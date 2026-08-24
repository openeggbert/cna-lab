#include "IronGang/Core/PerformanceProfiler.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

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
                   << "  \"schema_version\": 2,\n"
                   << "  \"backend\": \"" << EscapeJson(context.backend) << "\",\n"
                   << "  \"build_configuration\": \"" << EscapeJson(context.buildConfiguration) << "\",\n"
                   << "  \"scenario\": \"" << EscapeJson(context.scenario) << "\",\n"
                   << "  \"resolution\": {\"width\": " << context.width << ", \"height\": " << context.height
                   << "},\n"
                   << "  \"timing\": {\"vertical_sync_requested\": "
                   << (context.verticalSyncRequested ? "true" : "false")
                   << ", \"fixed_timestep\": " << (context.fixedTimeStep ? "true" : "false")
                   << ", \"target_frame_ms\": " << context.targetFrameMilliseconds << "},\n"
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
                   << ", \"coverage\": \"Iron Gang-owned meshes, lightmaps, and HUD atlas plus imported CNA model buffers and effect-bound textures; backend effect programs, swapchain/depth/render-target/transient allocations, driver padding, and physical residency are not reported\"},\n"
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
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
        std::ifstream status("/proc/self/status");
        std::string key;
        while (status >> key)
        {
            if (key == "VmHWM:")
            {
                std::uint64_t kibibytes = 0;
                std::string unit;
                status >> kibibytes >> unit;
                return kibibytes * 1024ULL;
            }
            std::string remainder;
            std::getline(status, remainder);
        }
#endif
        return 0;
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
