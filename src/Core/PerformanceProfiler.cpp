#include "IronGang/Core/PerformanceProfiler.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace IronGang
{
    namespace
    {
        const char* MetricName(PerformanceMetric metric)
        {
            switch (metric)
            {
            case PerformanceMetric::FrameInterval:
                return "frame_interval";
            case PerformanceMetric::UpdateCpu:
                return "update_cpu";
            case PerformanceMetric::PhysicsCpu:
                return "physics_cpu";
            case PerformanceMetric::AiCpu:
                return "ai_cpu";
            case PerformanceMetric::AudioCpu:
                return "audio_cpu";
            case PerformanceMetric::RenderCpu:
                return "render_cpu";
            case PerformanceMetric::PresentCpu:
                return "present_cpu";
            case PerformanceMetric::GpuRender:
                return "gpu_render";
            case PerformanceMetric::DistrictWorldPhysicsCpu:
                return "district_world_physics_cpu";
            case PerformanceMetric::DistrictRendererUploadCpu:
                return "district_renderer_upload_cpu";
            case PerformanceMetric::DistrictLoadCpu:
                return "district_load_cpu";
            case PerformanceMetric::StartupCpu:
                return "startup_cpu";
            case PerformanceMetric::Count:
                break;
            }
            return "unknown";
        }

        const char* RenderWorkloadMetricName(RenderWorkloadMetric metric)
        {
            switch (metric)
            {
            case RenderWorkloadMetric::DrawCalls:
                return "draw_calls";
            case RenderWorkloadMetric::StateChanges:
                return "state_change_calls";
            case RenderWorkloadMetric::Vertices:
                return "vertices";
            case RenderWorkloadMetric::Triangles:
                return "triangles";
            case RenderWorkloadMetric::Instances:
                return "geometry_instances";
            case RenderWorkloadMetric::VisibleObjects:
                return "visible_objects";
            case RenderWorkloadMetric::Count:
                break;
            }
            return "unknown";
        }

        const char* PhysicsWorkloadMetricName(PhysicsWorkloadMetric metric)
        {
            switch (metric)
            {
            case PhysicsWorkloadMetric::Bodies:
                return "bodies";
            case PhysicsWorkloadMetric::ActiveRigidBodies:
                return "active_rigid_bodies";
            case PhysicsWorkloadMetric::RigidBodyContactManifolds:
                return "rigid_body_contact_manifolds";
            case PhysicsWorkloadMetric::CharacterContacts:
                return "character_contacts";
            case PhysicsWorkloadMetric::FixedSteps:
                return "fixed_steps";
            case PhysicsWorkloadMetric::PublicRaycasts:
                return "public_raycasts";
            case PhysicsWorkloadMetric::CharacterCollisionUpdates:
                return "character_collision_updates";
            case PhysicsWorkloadMetric::VehicleWheelRaycasts:
                return "vehicle_wheel_raycasts";
            case PhysicsWorkloadMetric::Count:
                break;
            }
            return "unknown";
        }

        const char* AiWorkloadMetricName(AiWorkloadMetric metric)
        {
            switch (metric)
            {
            case AiWorkloadMetric::TrafficVehicles:
                return "traffic_vehicles";
            case AiWorkloadMetric::Pedestrians:
                return "pedestrians";
            case AiWorkloadMetric::FleeingPedestrians:
                return "fleeing_pedestrians";
            case AiWorkloadMetric::PolicePatrols:
                return "police_patrols";
            case AiWorkloadMetric::TrafficUpdates:
                return "traffic_updates";
            case AiWorkloadMetric::TrafficObstacleChecks:
                return "traffic_obstacle_checks";
            case AiWorkloadMetric::PedestrianUpdates:
                return "pedestrian_updates";
            case AiWorkloadMetric::PedestrianThreatChecks:
                return "pedestrian_threat_checks";
            case AiWorkloadMetric::PoliceWitnessChecks:
                return "police_witness_checks";
            case AiWorkloadMetric::PolicePatrolUpdates:
                return "police_patrol_updates";
            case AiWorkloadMetric::Count:
                break;
            }
            return "unknown";
        }

        const char* AudioWorkloadMetricName(AudioWorkloadMetric metric)
        {
            switch (metric)
            {
            case AudioWorkloadMetric::LoadedSoundAssets:
                return "loaded_sound_assets";
            case AudioWorkloadMetric::TrackedLoopInstances:
                return "tracked_loop_instances";
            case AudioWorkloadMetric::TrackedPlayingLoopVoices:
                return "tracked_playing_loop_voices";
            case AudioWorkloadMetric::StreamedAudioAssets:
                return "streamed_audio_assets";
            case AudioWorkloadMetric::OneShotPlayRequests:
                return "one_shot_play_requests";
            case AudioWorkloadMetric::OneShotPlaySuccesses:
                return "one_shot_play_successes";
            case AudioWorkloadMetric::LoopPlayCommands:
                return "loop_play_commands";
            case AudioWorkloadMetric::LoopStopCommands:
                return "loop_stop_commands";
            case AudioWorkloadMetric::LoopParameterUpdates:
                return "loop_parameter_updates";
            case AudioWorkloadMetric::Count:
                break;
            }
            return "unknown";
        }

        std::string EscapeJson(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size());
            for (const char character : value)
            {
                switch (character)
                {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped += character;
                    break;
                }
            }
            return escaped;
        }

        bool PassesP95(const PerformanceStatistics& statistics, double budgetMilliseconds)
        {
            return statistics.sampleCount > 0 && statistics.p95Milliseconds <= budgetMilliseconds;
        }

        double Percentage(std::size_t count, std::size_t total)
        {
            return total == 0 ? 0.0 : 100.0 * static_cast<double>(count) / static_cast<double>(total);
        }

        void WriteByteDelta(std::ostream& output, std::uint64_t before, std::uint64_t after)
        {
            if (after < before)
            {
                output << '-' << (before - after);
            }
            else
            {
                output << (after - before);
            }
        }

        std::uint64_t ReadLinuxStatusBytes(const char* requestedKey)
        {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
            std::ifstream status("/proc/self/status");
            std::string key;
            while (status >> key)
            {
                if (key == requestedKey)
                {
                    std::uint64_t kibibytes = 0;
                    std::string unit;
                    status >> kibibytes >> unit;
                    return kibibytes * 1024ULL;
                }
                std::string remainder;
                std::getline(status, remainder);
            }
#else
            (void)requestedKey;
#endif
            return 0;
        }
    }

    const char* PerformanceScenarioName(PerformanceScenario scenario) noexcept
    {
        switch (scenario)
        {
        case PerformanceScenario::InteractiveOrIntro:
            return "interactive_or_intro";
        case PerformanceScenario::Intro:
            return "intro";
        case PerformanceScenario::Idle:
            return "idle";
        case PerformanceScenario::Walk:
            return "walk";
        case PerformanceScenario::Drive:
            return "drive";
        case PerformanceScenario::Mixed:
            return "mixed";
        }
        return "unknown";
    }

    std::optional<PerformanceScenario> ParsePerformanceScenario(std::string_view name) noexcept
    {
        if (name == "intro")
        {
            return PerformanceScenario::Intro;
        }
        if (name == "idle")
        {
            return PerformanceScenario::Idle;
        }
        if (name == "walk")
        {
            return PerformanceScenario::Walk;
        }
        if (name == "drive")
        {
            return PerformanceScenario::Drive;
        }
        if (name == "mixed")
        {
            return PerformanceScenario::Mixed;
        }
        return std::nullopt;
    }

    void PerformanceProfiler::SetEnabled(bool enabled) noexcept
    {
        enabled_ = enabled;
        if (!enabled_)
        {
            previousFrameStart_.reset();
        }
    }

    void PerformanceProfiler::BeginFrame()
    {
        if (!enabled_)
        {
            return;
        }

        const Clock::time_point now = Clock::now();
        if (previousFrameStart_)
        {
            Record(PerformanceMetric::FrameInterval,
                   std::chrono::duration<double, std::milli>(now - *previousFrameStart_).count());
        }
        previousFrameStart_ = now;
    }

    void PerformanceProfiler::Record(PerformanceMetric metric, double milliseconds)
    {
        if (!enabled_ || metric == PerformanceMetric::Count || !std::isfinite(milliseconds) || milliseconds < 0.0)
        {
            return;
        }
        samples_[MetricIndex(metric)].push_back(milliseconds);
    }

    void PerformanceProfiler::RecordRenderWorkload(RenderWorkloadMetric metric, std::uint64_t count)
    {
        if (!enabled_ || metric == RenderWorkloadMetric::Count)
        {
            return;
        }
        renderWorkloadSamples_[RenderWorkloadMetricIndex(metric)].push_back(static_cast<double>(count));
    }

    void PerformanceProfiler::RecordPhysicsWorkload(PhysicsWorkloadMetric metric, std::uint64_t count)
    {
        if (!enabled_ || metric == PhysicsWorkloadMetric::Count)
        {
            return;
        }
        physicsWorkloadSamples_[PhysicsWorkloadMetricIndex(metric)].push_back(static_cast<double>(count));
    }

    void PerformanceProfiler::RecordAiWorkload(const AiWorkloadSample& sample)
    {
        if (!enabled_)
        {
            return;
        }
        const std::array<std::uint64_t, static_cast<std::size_t>(AiWorkloadMetric::Count)> values{
            sample.trafficVehicles,
            sample.pedestrians,
            sample.fleeingPedestrians,
            sample.policePatrols,
            sample.trafficUpdates,
            sample.trafficObstacleChecks,
            sample.pedestrianUpdates,
            sample.pedestrianThreatChecks,
            sample.policeWitnessChecks,
            sample.policePatrolUpdates,
        };
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            aiWorkloadSamples_[index].push_back(static_cast<double>(values[index]));
        }
    }

    void PerformanceProfiler::RecordAudioWorkload(const AudioWorkloadSample& sample)
    {
        if (!enabled_)
        {
            return;
        }
        const std::array<std::uint64_t, static_cast<std::size_t>(AudioWorkloadMetric::Count)> values{
            sample.loadedSoundAssets,
            sample.trackedLoopInstances,
            sample.trackedPlayingLoopVoices,
            sample.streamedAudioAssets,
            sample.oneShotPlayRequests,
            sample.oneShotPlaySuccesses,
            sample.loopPlayCommands,
            sample.loopStopCommands,
            sample.loopParameterUpdates,
        };
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            audioWorkloadSamples_[index].push_back(static_cast<double>(values[index]));
        }
    }

    void PerformanceProfiler::RecordDistrictLoad(DistrictLoadSample sample)
    {
        if (!enabled_ || !std::isfinite(sample.worldPhysicsMilliseconds) ||
            !std::isfinite(sample.rendererUploadMilliseconds) || sample.worldPhysicsMilliseconds < 0.0 ||
            sample.rendererUploadMilliseconds < 0.0)
        {
            return;
        }

        Record(PerformanceMetric::DistrictWorldPhysicsCpu, sample.worldPhysicsMilliseconds);
        Record(PerformanceMetric::DistrictRendererUploadCpu, sample.rendererUploadMilliseconds);
        Record(PerformanceMetric::DistrictLoadCpu,
               sample.worldPhysicsMilliseconds + sample.rendererUploadMilliseconds);
        districtTransitionFrameSampleIndices_.push_back(
            samples_[MetricIndex(PerformanceMetric::FrameInterval)].size());
        districtLoadSamples_.push_back(std::move(sample));
    }

    PerformanceStatistics PerformanceProfiler::GetStatistics(PerformanceMetric metric) const
    {
        PerformanceStatistics result;
        if (metric == PerformanceMetric::Count)
        {
            return result;
        }

        const std::vector<double>& samples = samples_[MetricIndex(metric)];
        result.sampleCount = samples.size();
        if (samples.empty())
        {
            return result;
        }

        double total = 0.0;
        for (const double sample : samples)
        {
            total += sample;
            result.maximumMilliseconds = std::max(result.maximumMilliseconds, sample);
        }
        result.averageMilliseconds = total / static_cast<double>(samples.size());

        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t percentileIndex = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1U;
        result.p95Milliseconds = sorted[percentileIndex];
        return result;
    }

    FramePacingStatistics PerformanceProfiler::GetFramePacingStatistics() const
    {
        FramePacingStatistics result;
        const std::vector<double>& samples = samples_[MetricIndex(PerformanceMetric::FrameInterval)];
        result.sampleCount = samples.size();
        for (const double sample : samples)
        {
            if (sample <= kRecommendedFrameBudgetMilliseconds)
            {
                ++result.atOrBelowRecommendedBudgetCount;
            }
            else if (sample <= kMinimumFrameBudgetMilliseconds)
            {
                ++result.aboveRecommendedAtOrBelowMinimumBudgetCount;
            }
            else if (sample <= kFrameHitchThresholdMilliseconds)
            {
                ++result.aboveMinimumAtOrBelowHitchCount;
            }
            else if (sample <= kSevereFrameHitchThresholdMilliseconds)
            {
                ++result.aboveHitchAtOrBelowSevereHitchCount;
            }
            else
            {
                ++result.aboveSevereHitchCount;
            }

            if (sample > kMinimumFrameBudgetMilliseconds)
            {
                ++result.minimumBudgetMissCount;
            }
            if (sample > kFrameHitchThresholdMilliseconds)
            {
                ++result.hitchCount;
            }
            if (sample > kSevereFrameHitchThresholdMilliseconds)
            {
                ++result.severeHitchCount;
            }
        }

        result.districtTransitionCount = districtTransitionFrameSampleIndices_.size();
        for (const std::size_t sampleIndex : districtTransitionFrameSampleIndices_)
        {
            if (sampleIndex >= samples.size())
            {
                continue;
            }
            const double sample = samples[sampleIndex];
            ++result.measuredDistrictTransitionCount;
            if (sample > kFrameHitchThresholdMilliseconds)
            {
                ++result.districtTransitionHitchCount;
            }
            if (!result.maximumDistrictTransitionMilliseconds ||
                sample > *result.maximumDistrictTransitionMilliseconds)
            {
                result.maximumDistrictTransitionMilliseconds = sample;
            }
        }
        return result;
    }

    RenderWorkloadStatistics
    PerformanceProfiler::GetRenderWorkloadStatistics(RenderWorkloadMetric metric) const
    {
        RenderWorkloadStatistics result;
        if (metric == RenderWorkloadMetric::Count)
        {
            return result;
        }

        const std::vector<double>& samples = renderWorkloadSamples_[RenderWorkloadMetricIndex(metric)];
        result.sampleCount = samples.size();
        if (samples.empty())
        {
            return result;
        }

        double total = 0.0;
        for (const double sample : samples)
        {
            total += sample;
            result.maximum = std::max(result.maximum, sample);
        }
        result.average = total / static_cast<double>(samples.size());

        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t percentileIndex = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1U;
        result.p95 = sorted[percentileIndex];
        return result;
    }

    PhysicsWorkloadStatistics
    PerformanceProfiler::GetPhysicsWorkloadStatistics(PhysicsWorkloadMetric metric) const
    {
        PhysicsWorkloadStatistics result;
        if (metric == PhysicsWorkloadMetric::Count)
        {
            return result;
        }

        const std::vector<double>& samples = physicsWorkloadSamples_[PhysicsWorkloadMetricIndex(metric)];
        result.sampleCount = samples.size();
        if (samples.empty())
        {
            return result;
        }

        double total = 0.0;
        for (const double sample : samples)
        {
            total += sample;
            result.maximum = std::max(result.maximum, sample);
        }
        result.average = total / static_cast<double>(samples.size());

        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t percentileIndex = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1U;
        result.p95 = sorted[percentileIndex];
        return result;
    }

    AiWorkloadStatistics PerformanceProfiler::GetAiWorkloadStatistics(AiWorkloadMetric metric) const
    {
        AiWorkloadStatistics result;
        if (metric == AiWorkloadMetric::Count)
        {
            return result;
        }

        const std::vector<double>& samples = aiWorkloadSamples_[AiWorkloadMetricIndex(metric)];
        result.sampleCount = samples.size();
        if (samples.empty())
        {
            return result;
        }

        double total = 0.0;
        for (const double sample : samples)
        {
            total += sample;
            result.maximum = std::max(result.maximum, sample);
        }
        result.average = total / static_cast<double>(samples.size());

        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t percentileIndex = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1U;
        result.p95 = sorted[percentileIndex];
        return result;
    }

    AudioWorkloadStatistics
    PerformanceProfiler::GetAudioWorkloadStatistics(AudioWorkloadMetric metric) const
    {
        AudioWorkloadStatistics result;
        if (metric == AudioWorkloadMetric::Count)
        {
            return result;
        }

        const std::vector<double>& samples = audioWorkloadSamples_[AudioWorkloadMetricIndex(metric)];
        result.sampleCount = samples.size();
        if (samples.empty())
        {
            return result;
        }

        double total = 0.0;
        for (const double sample : samples)
        {
            total += sample;
            result.maximum = std::max(result.maximum, sample);
        }
        result.average = total / static_cast<double>(samples.size());

        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t percentileIndex = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1U;
        result.p95 = sorted[percentileIndex];
        return result;
    }

    bool PerformanceProfiler::WriteJsonReport(const std::string& path,
                                               const PerformanceReportContext& context,
                                               std::string& error) const
    {
        try
        {
            const std::filesystem::path reportPath(path);
            if (reportPath.has_parent_path())
            {
                std::filesystem::create_directories(reportPath.parent_path());
            }

            std::ofstream output(reportPath, std::ios::trunc);
            if (!output)
            {
                error = "Could not open performance report for writing: " + path;
                return false;
            }

            const PerformanceStatistics frame = GetStatistics(PerformanceMetric::FrameInterval);
            const FramePacingStatistics framePacing = GetFramePacingStatistics();
            const PerformanceStatistics update = GetStatistics(PerformanceMetric::UpdateCpu);
            const PerformanceStatistics physics = GetStatistics(PerformanceMetric::PhysicsCpu);
            const PerformanceStatistics ai = GetStatistics(PerformanceMetric::AiCpu);
            const PerformanceStatistics audio = GetStatistics(PerformanceMetric::AudioCpu);
            const PerformanceStatistics render = GetStatistics(PerformanceMetric::RenderCpu);
            const PerformanceStatistics districtLoad = GetStatistics(PerformanceMetric::DistrictLoadCpu);
            const bool ramKnown = context.peakResidentBytes > 0;
            const bool ramPass = ramKnown && context.peakResidentBytes <= kMinimumRamBudgetBytes;
            const bool trackedVramPass = context.trackedVideoMemoryBytes <= kMinimumVramBudgetBytes;

            output << std::fixed << std::setprecision(3);
            output << "{\n"
                   << "  \"schema_version\": 8,\n"
                   << "  \"backend\": \"" << EscapeJson(context.backend) << "\",\n"
                   << "  \"build_configuration\": \"" << EscapeJson(context.buildConfiguration) << "\",\n"
                   << "  \"scenario\": \"" << EscapeJson(context.scenario) << "\",\n"
                   << "  \"resolution\": {\"width\": " << context.width << ", \"height\": " << context.height
                   << "},\n"
                   << "  \"timing\": {\"vertical_sync_requested\": "
                   << (context.verticalSyncRequested ? "true" : "false")
                   << ", \"fixed_timestep\": " << (context.fixedTimeStep ? "true" : "false")
                   << ", \"target_frame_ms\": " << context.targetFrameMilliseconds << "},\n"
                   << "  \"swap_interval\": {\"requested\": " << context.requestedSwapInterval
                   << ", \"apply_result_known\": "
                   << (context.swapIntervalApplyResultKnown ? "true" : "false")
                   << ", \"apply_succeeded\": ";
            if (context.swapIntervalApplyResultKnown)
            {
                output << (context.swapIntervalApplySucceeded ? "true" : "false");
            }
            else
            {
                output << "null";
            }
            output << ", \"applied\": ";
            if (context.appliedSwapInterval)
            {
                output << *context.appliedSwapInterval;
            }
            else
            {
                output << "null";
            }
            output << ", \"proof\": \"platform SetSwapInterval acknowledgement; not physical vblank or compositor proof\""
                   << ", \"unavailable_reason\": \""
                   << EscapeJson(context.swapIntervalUnavailableReason) << "\"},\n"
                   << "  \"gpu_timing\": {\"supported\": "
                   << (context.gpuTimerSupported ? "true" : "false")
                   << ", \"non_blocking\": true, \"scope\": \"draw_commands_excluding_present\""
                   << ", \"discarded_samples\": "
                   << context.gpuTimerDiscardedSamples << ", \"unsupported_reason\": \""
                   << EscapeJson(context.gpuTimerUnsupportedReason) << "\"},\n"
                   << "  \"budgets\": {\n"
                   << "    \"minimum_frame_p95_ms\": " << kMinimumFrameBudgetMilliseconds << ",\n"
                   << "    \"recommended_frame_p95_ms\": " << kRecommendedFrameBudgetMilliseconds << ",\n"
                   << "    \"update_cpu_p95_ms\": " << kMinimumUpdateCpuBudgetMilliseconds << ",\n"
                   << "    \"physics_cpu_p95_ms\": " << kMinimumPhysicsCpuBudgetMilliseconds << ",\n"
                   << "    \"ai_cpu_p95_ms\": " << kMinimumAiCpuBudgetMilliseconds << ",\n"
                   << "    \"audio_cpu_p95_ms\": " << kMinimumAudioCpuBudgetMilliseconds << ",\n"
                   << "    \"render_cpu_p95_ms\": " << kMinimumRenderCpuBudgetMilliseconds << ",\n"
                   << "    \"district_load_p95_ms\": " << kDistrictLoadBudgetMilliseconds << ",\n"
                   << "    \"ram_bytes\": " << kMinimumRamBudgetBytes << ",\n"
                   << "    \"vram_bytes\": " << kMinimumVramBudgetBytes << "\n"
                   << "  },\n"
                   << "  \"measurements\": {\n";

            for (std::size_t index = 0; index < static_cast<std::size_t>(PerformanceMetric::Count); ++index)
            {
                const auto metric = static_cast<PerformanceMetric>(index);
                const PerformanceStatistics statistics = GetStatistics(metric);
                output << "    \"" << MetricName(metric) << "\": {\"samples\": " << statistics.sampleCount
                       << ", \"average_ms\": " << statistics.averageMilliseconds
                       << ", \"p95_ms\": " << statistics.p95Milliseconds
                       << ", \"maximum_ms\": " << statistics.maximumMilliseconds << "}";
                output << (index + 1U == static_cast<std::size_t>(PerformanceMetric::Count) ? "\n" : ",\n");
            }

            output << "  },\n"
                   << "  \"frame_pacing\": {\n"
                   << "    \"scope\": \"wall-clock intervals between consecutive BeginFrame calls; the first frame establishes a baseline and has no sample\",\n"
                   << "    \"boundary_scope\": \"a district-transition boundary is the first frame-interval sample recorded after RecordDistrictLoad\",\n"
                   << "    \"samples\": " << framePacing.sampleCount << ",\n"
                   << "    \"histogram\": {\n"
                   << "      \"at_or_below_recommended_budget\": {\"upper_bound_ms\": "
                   << kRecommendedFrameBudgetMilliseconds << ", \"count\": "
                   << framePacing.atOrBelowRecommendedBudgetCount << "},\n"
                   << "      \"above_recommended_at_or_below_minimum_budget\": {\"lower_bound_exclusive_ms\": "
                   << kRecommendedFrameBudgetMilliseconds << ", \"upper_bound_ms\": "
                   << kMinimumFrameBudgetMilliseconds << ", \"count\": "
                   << framePacing.aboveRecommendedAtOrBelowMinimumBudgetCount << "},\n"
                   << "      \"above_minimum_at_or_below_hitch\": {\"lower_bound_exclusive_ms\": "
                   << kMinimumFrameBudgetMilliseconds << ", \"upper_bound_ms\": "
                   << kFrameHitchThresholdMilliseconds << ", \"count\": "
                   << framePacing.aboveMinimumAtOrBelowHitchCount << "},\n"
                   << "      \"above_hitch_at_or_below_severe_hitch\": {\"lower_bound_exclusive_ms\": "
                   << kFrameHitchThresholdMilliseconds << ", \"upper_bound_ms\": "
                   << kSevereFrameHitchThresholdMilliseconds << ", \"count\": "
                   << framePacing.aboveHitchAtOrBelowSevereHitchCount << "},\n"
                   << "      \"above_severe_hitch\": {\"lower_bound_exclusive_ms\": "
                   << kSevereFrameHitchThresholdMilliseconds << ", \"count\": "
                   << framePacing.aboveSevereHitchCount << "}\n"
                   << "    },\n"
                   << "    \"minimum_budget_misses\": {\"threshold_ms\": "
                   << kMinimumFrameBudgetMilliseconds << ", \"comparison\": \"greater_than\", \"count\": "
                   << framePacing.minimumBudgetMissCount << ", \"percent\": "
                   << Percentage(framePacing.minimumBudgetMissCount, framePacing.sampleCount) << "},\n"
                   << "    \"hitches\": {\"threshold_ms\": " << kFrameHitchThresholdMilliseconds
                   << ", \"comparison\": \"greater_than\", \"count\": " << framePacing.hitchCount
                   << ", \"percent\": " << Percentage(framePacing.hitchCount, framePacing.sampleCount)
                   << "},\n"
                   << "    \"severe_hitches\": {\"threshold_ms\": "
                   << kSevereFrameHitchThresholdMilliseconds
                   << ", \"comparison\": \"greater_than\", \"count\": "
                   << framePacing.severeHitchCount << ", \"percent\": "
                   << Percentage(framePacing.severeHitchCount, framePacing.sampleCount) << "},\n"
                   << "    \"district_transition_boundaries\": {\"transitions\": "
                   << framePacing.districtTransitionCount << ", \"measured_samples\": "
                   << framePacing.measuredDistrictTransitionCount << ", \"hitch_count\": "
                   << framePacing.districtTransitionHitchCount << ", \"maximum_ms\": ";
            if (framePacing.maximumDistrictTransitionMilliseconds)
            {
                output << *framePacing.maximumDistrictTransitionMilliseconds;
            }
            else
            {
                output << "null";
            }
            output << "}\n"
                   << "  },\n"
                   << "  \"district_load\": {\n"
                   << "    \"content_path\": \"procedural in-memory PrototypeWorld; no district file/package is read during a transition\",\n"
                   << "    \"unload_activation_scope\": \"destroy old static physics bodies, construct target world, and build target static physics bodies; exit-trigger samples also include player/vehicle arrival placement\",\n"
                   << "    \"renderer_upload_scope\": \"CPU time to rebuild target static geometry/lightmap and issue resource uploads; not GPU-completion time\",\n"
                   << "    \"io_ms\": null, \"decompression_ms\": null, \"parse_ms\": null,\n"
                   << "    \"unavailable_reason\": \"districts have no serialized runtime package yet; null means not applicable, not measured zero\",\n"
                   << "    \"samples\": [\n";

            for (std::size_t index = 0; index < districtLoadSamples_.size(); ++index)
            {
                const DistrictLoadSample& sample = districtLoadSamples_[index];
                const bool residentKnown = sample.residentBytesBefore > 0 && sample.residentBytesAfter > 0;
                output << "      {\"reason\": \"" << EscapeJson(sample.reason)
                       << "\", \"source\": \"" << EscapeJson(sample.sourceDistrict)
                       << "\", \"target\": \"" << EscapeJson(sample.targetDistrict)
                       << "\", \"world_physics_ms\": " << sample.worldPhysicsMilliseconds
                       << ", \"renderer_upload_ms\": " << sample.rendererUploadMilliseconds
                       << ", \"total_ms\": "
                       << (sample.worldPhysicsMilliseconds + sample.rendererUploadMilliseconds)
                       << ", \"asset_counts\": {\"district_files\": 0, \"procedural_world_objects\": "
                       << sample.proceduralWorldObjectCount << ", \"static_physics_bodies\": "
                       << sample.staticPhysicsBodyCount << "}, \"memory\": {\"resident_known\": "
                       << (residentKnown ? "true" : "false")
                       << ", \"resident_before_bytes\": " << sample.residentBytesBefore
                       << ", \"resident_after_bytes\": " << sample.residentBytesAfter
                       << ", \"resident_delta_bytes\": ";
                if (residentKnown)
                {
                    WriteByteDelta(output, sample.residentBytesBefore, sample.residentBytesAfter);
                }
                else
                {
                    output << "null";
                }
                output << ", \"tracked_video_memory_before_bytes\": "
                       << sample.trackedVideoMemoryBytesBefore
                       << ", \"tracked_video_memory_after_bytes\": "
                       << sample.trackedVideoMemoryBytesAfter
                       << ", \"tracked_video_memory_delta_bytes\": ";
                WriteByteDelta(output,
                               sample.trackedVideoMemoryBytesBefore,
                               sample.trackedVideoMemoryBytesAfter);
                output << "}}" << (index + 1U == districtLoadSamples_.size() ? "\n" : ",\n");
            }

            output << "    ]\n"
                   << "  },\n"
                   << "  \"render_workload\": {\n"
                   << "    \"scope\": \"Iron Gang 3D front-end submissions; excludes Clear, HUD SpriteBatch internal batching, Present, and backend state deduplication\",\n"
                   << "    \"visibility_policy\": \"no frustum or occlusion culling; every submitted scene object counts as visible\",\n";

            for (std::size_t index = 0; index < static_cast<std::size_t>(RenderWorkloadMetric::Count); ++index)
            {
                const auto metric = static_cast<RenderWorkloadMetric>(index);
                const RenderWorkloadStatistics statistics = GetRenderWorkloadStatistics(metric);
                output << "    \"" << RenderWorkloadMetricName(metric) << "\": {\"samples\": "
                       << statistics.sampleCount << ", \"average\": " << statistics.average
                       << ", \"p95\": " << statistics.p95
                       << ", \"maximum\": " << statistics.maximum << "}";
                output << (index + 1U == static_cast<std::size_t>(RenderWorkloadMetric::Count) ? "\n" : ",\n");
            }

            output << "  },\n"
                   << "  \"physics_workload\": {\n"
                   << "    \"scope\": \"per game Update; body/contact fields are current state and step/query fields are operations consumed since the previous sample\",\n"
                   << "    \"contact_scope\": \"Jolt rigid-body/subshape contact manifolds plus actual CharacterVirtual contacts; contact points within a manifold are not counted separately\",\n"
                   << "    \"query_scope\": \"public PhysicsWorld raycasts, actual vehicle suspension raycasts, and CharacterVirtual collision-update batches are separate because their granularities differ\",\n";

            for (std::size_t index = 0; index < static_cast<std::size_t>(PhysicsWorkloadMetric::Count); ++index)
            {
                const auto metric = static_cast<PhysicsWorkloadMetric>(index);
                const PhysicsWorkloadStatistics statistics = GetPhysicsWorkloadStatistics(metric);
                output << "    \"" << PhysicsWorkloadMetricName(metric) << "\": {\"samples\": "
                       << statistics.sampleCount << ", \"average\": " << statistics.average
                       << ", \"p95\": " << statistics.p95
                       << ", \"maximum\": " << statistics.maximum << "}";
                output << (index + 1U == static_cast<std::size_t>(PhysicsWorkloadMetric::Count) ? "\n" : ",\n");
            }

            output << "  },\n"
                   << "  \"ai_workload\": {\n"
                   << "    \"scope\": \"per game Update; state counts are current after the Update (including AI-suspended transition frames) and operation counts are exact loop work for that update\",\n"
                   << "    \"cpu_scope\": \"ai_cpu covers traffic, pedestrian, witness, and police updates; mission state progression is excluded\",\n"
                   << "    \"route_scope\": \"traffic and pedestrians follow fixed WaypointPaths; no road graph or path-request queue exists yet\",\n";

            for (std::size_t index = 0; index < static_cast<std::size_t>(AiWorkloadMetric::Count); ++index)
            {
                const auto metric = static_cast<AiWorkloadMetric>(index);
                const AiWorkloadStatistics statistics = GetAiWorkloadStatistics(metric);
                output << "    \"" << AiWorkloadMetricName(metric) << "\": {\"samples\": "
                       << statistics.sampleCount << ", \"average\": " << statistics.average
                       << ", \"p95\": " << statistics.p95
                       << ", \"maximum\": " << statistics.maximum << "}";
                output << (index + 1U == static_cast<std::size_t>(AiWorkloadMetric::Count) ? "\n" : ",\n");
            }

            output << "  },\n"
                   << "  \"audio_workload\": {\n"
                   << "    \"scope\": \"per game Update; exact Iron Gang-owned SoundEffect assets, tracked loop state, and playback/control commands\",\n"
                   << "    \"voice_scope\": \"tracked_playing_loop_voices covers only retained SoundEffectInstances; CNA exposes no lifetime query for fire-and-forget SoundEffect::Play voices\",\n"
                   << "    \"backend_scope\": \"decoder time, mixer callback time, active backend channels, and bus cost are unavailable through CNA and are not reported as zero\",\n";

            for (std::size_t index = 0; index < static_cast<std::size_t>(AudioWorkloadMetric::Count); ++index)
            {
                const auto metric = static_cast<AudioWorkloadMetric>(index);
                const AudioWorkloadStatistics statistics = GetAudioWorkloadStatistics(metric);
                output << "    \"" << AudioWorkloadMetricName(metric) << "\": {\"samples\": "
                       << statistics.sampleCount << ", \"average\": " << statistics.average
                       << ", \"p95\": " << statistics.p95
                       << ", \"maximum\": " << statistics.maximum << "}";
                output << (index + 1U == static_cast<std::size_t>(AudioWorkloadMetric::Count) ? "\n" : ",\n");
            }

            output << "  },\n"
                   << "  \"memory\": {\"peak_resident_bytes\": " << context.peakResidentBytes
                   << ", \"known\": " << (ramKnown ? "true" : "false")
                   << ", \"budget_pass\": " << (ramPass ? "true" : "false") << "},\n"
                   << "  \"video_memory\": {\"tracked_bytes\": " << context.trackedVideoMemoryBytes
                   << ", \"game_owned_bytes\": " << context.trackedGameOwnedVideoMemoryBytes
                   << ", \"imported_model_buffer_bytes\": " << context.trackedImportedModelBufferBytes
                   << ", \"imported_model_texture_bytes\": " << context.trackedImportedModelTextureBytes
                   << ", \"tracking_complete\": "
                   << (context.videoMemoryTrackingComplete ? "true" : "false")
                   << ", \"tracked_budget_pass\": " << (trackedVramPass ? "true" : "false")
                   << ", \"coverage\": \"Iron Gang-owned meshes, lightmaps, and HUD/map textures plus imported CNA model buffers and effect-bound textures; backend effect programs, swapchain/depth/render-target/transient allocations, driver padding, and physical residency are not reported\"},\n"
                   << "  \"workload\": {\"physics_bodies\": " << context.physicsBodyCount
                   << ", \"traffic_vehicles\": " << context.trafficVehicleCount
                   << ", \"pedestrians\": " << context.pedestrianCount
                   << ", \"police_vehicles\": " << context.policeVehicleCount << "},\n"
                   << "  \"checks\": {\n"
                   << "    \"minimum_frame_rate_pass\": "
                   << (PassesP95(frame, kMinimumFrameBudgetMilliseconds) ? "true" : "false") << ",\n"
                   << "    \"recommended_frame_rate_pass\": "
                   << (PassesP95(frame, kRecommendedFrameBudgetMilliseconds) ? "true" : "false") << ",\n"
                   << "    \"cpu_subsystems_pass\": "
                   << (PassesP95(update, kMinimumUpdateCpuBudgetMilliseconds) &&
                               PassesP95(physics, kMinimumPhysicsCpuBudgetMilliseconds) &&
                               PassesP95(ai, kMinimumAiCpuBudgetMilliseconds) &&
                               PassesP95(audio, kMinimumAudioCpuBudgetMilliseconds) &&
                               PassesP95(render, kMinimumRenderCpuBudgetMilliseconds)
                           ? "true"
                           : "false")
                   << ",\n"
                   << "    \"district_load_pass\": ";
            if (districtLoad.sampleCount == 0)
            {
                output << "null\n";
            }
            else
            {
                output << (PassesP95(districtLoad, kDistrictLoadBudgetMilliseconds) ? "true" : "false") << "\n";
            }
            output << "  }\n"
                   << "}\n";

            if (!output)
            {
                error = "Failed while writing performance report: " + path;
                return false;
            }
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = "Could not write performance report '" + path + "': " + exception.what();
            return false;
        }
    }

    std::uint64_t PerformanceProfiler::ReadPeakResidentBytes()
    {
        return ReadLinuxStatusBytes("VmHWM:");
    }

    std::uint64_t PerformanceProfiler::ReadCurrentResidentBytes()
    {
        return ReadLinuxStatusBytes("VmRSS:");
    }

    ScopedPerformanceSample::ScopedPerformanceSample(PerformanceProfiler& profiler, PerformanceMetric metric)
        : profiler_(profiler.IsEnabled() ? &profiler : nullptr), metric_(metric)
    {
        if (profiler_ != nullptr)
        {
            start_ = PerformanceProfiler::Clock::now();
        }
    }

    ScopedPerformanceSample::~ScopedPerformanceSample()
    {
        if (profiler_ != nullptr)
        {
            profiler_->Record(metric_, std::chrono::duration<double, std::milli>(
                                                   PerformanceProfiler::Clock::now() - start_)
                                                   .count());
        }
    }
}
