#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace IronGang
{
    // Gate M12 / plan_35: the small set of timings needed to decide whether the first district
    // fits its performance budget. FrameInterval includes the scheduler, vertical sync, and the
    // preceding Present(); RenderCpu measures Iron Gang's CPU-side Draw() submission work and
    // GpuRender is the renderer's asynchronous command-range timer when the driver supports one.
    enum class PerformanceMetric : std::size_t
    {
        FrameInterval,
        UpdateCpu,
        PhysicsCpu,
        AiCpu,
        AudioCpu,
        RenderCpu,
        PresentCpu,
        GpuRender,
        DistrictWorldPhysicsCpu,
        DistrictRendererUploadCpu,
        DistrictLoadCpu,
        StartupCpu,
        Count,
    };

    // Per-frame integer workload submitted by Iron Gang's 3D renderer. These are deliberately
    // separate from PerformanceMetric: their values are counts, never milliseconds.
    enum class RenderWorkloadMetric : std::size_t
    {
        DrawCalls,
        StateChanges,
        Vertices,
        Triangles,
        Instances,
        VisibleObjects,
        Count,
    };

    // Per-update physics state and operation counts from PhysicsWorld's Iron Gang/Jolt seam.
    enum class PhysicsWorkloadMetric : std::size_t
    {
        Bodies,
        ActiveRigidBodies,
        RigidBodyContactManifolds,
        CharacterContacts,
        FixedSteps,
        PublicRaycasts,
        CharacterCollisionUpdates,
        VehicleWheelRaycasts,
        Count,
    };

    // Per-update ambient-AI state and exact loop work from IronGangGame's traffic/pedestrian/
    // police orchestration. Mission logic is deliberately outside this metric's CPU scope.
    enum class AiWorkloadMetric : std::size_t
    {
        TrafficVehicles,
        Pedestrians,
        FleeingPedestrians,
        PolicePatrols,
        TrafficUpdates,
        TrafficObstacleChecks,
        PedestrianUpdates,
        PedestrianThreatChecks,
        PoliceWitnessChecks,
        PolicePatrolUpdates,
        Count,
    };

    // Game-owned audio state and commands. CNA does not expose fire-and-forget one-shot voice
    // lifetime, decoder work, mixer buses, or backend callback cost, so none are guessed here.
    enum class AudioWorkloadMetric : std::size_t
    {
        LoadedSoundAssets,
        TrackedLoopInstances,
        TrackedPlayingLoopVoices,
        StreamedAudioAssets,
        OneShotPlayRequests,
        OneShotPlaySuccesses,
        LoopPlayCommands,
        LoopStopCommands,
        LoopParameterUpdates,
        Count,
    };

    // Named, repeatable M12 workloads. InteractiveOrIntro preserves the ordinary game path;
    // every other value is selected explicitly through --profile-scenario.
    enum class PerformanceScenario
    {
        InteractiveOrIntro,
        Intro,
        Idle,
        Walk,
        Drive,
        Mixed,
        Mission,
    };

    [[nodiscard]] const char* PerformanceScenarioName(PerformanceScenario scenario) noexcept;
    [[nodiscard]] std::optional<PerformanceScenario> ParsePerformanceScenario(std::string_view name) noexcept;

    struct PerformanceStatistics
    {
        std::size_t sampleCount{0};
        double averageMilliseconds{0.0};
        double p95Milliseconds{0.0};
        double maximumMilliseconds{0.0};
    };

    // Mutually exclusive frame-interval buckets plus explicit long-frame counts. A district
    // boundary sample is the first frame interval recorded after a synchronous district change.
    struct FramePacingStatistics
    {
        std::size_t sampleCount{0};
        std::size_t atOrBelowRecommendedBudgetCount{0};
        std::size_t aboveRecommendedAtOrBelowMinimumBudgetCount{0};
        std::size_t aboveMinimumAtOrBelowHitchCount{0};
        std::size_t aboveHitchAtOrBelowSevereHitchCount{0};
        std::size_t aboveSevereHitchCount{0};
        std::size_t minimumBudgetMissCount{0};
        std::size_t hitchCount{0};
        std::size_t severeHitchCount{0};
        std::size_t districtTransitionCount{0};
        std::size_t measuredDistrictTransitionCount{0};
        std::size_t districtTransitionHitchCount{0};
        std::optional<double> maximumDistrictTransitionMilliseconds;
    };

    struct RenderWorkloadStatistics
    {
        std::size_t sampleCount{0};
        double average{0.0};
        double p95{0.0};
        double maximum{0.0};
    };

    struct PhysicsWorkloadStatistics
    {
        std::size_t sampleCount{0};
        double average{0.0};
        double p95{0.0};
        double maximum{0.0};
    };

    struct AiWorkloadStatistics
    {
        std::size_t sampleCount{0};
        double average{0.0};
        double p95{0.0};
        double maximum{0.0};
    };

    struct AiWorkloadSample
    {
        std::uint64_t trafficVehicles{0};
        std::uint64_t pedestrians{0};
        std::uint64_t fleeingPedestrians{0};
        std::uint64_t policePatrols{0};
        std::uint64_t trafficUpdates{0};
        std::uint64_t trafficObstacleChecks{0};
        std::uint64_t pedestrianUpdates{0};
        std::uint64_t pedestrianThreatChecks{0};
        std::uint64_t policeWitnessChecks{0};
        std::uint64_t policePatrolUpdates{0};
    };

    struct AudioWorkloadStatistics
    {
        std::size_t sampleCount{0};
        double average{0.0};
        double p95{0.0};
        double maximum{0.0};
    };

    struct AudioWorkloadSample
    {
        std::uint64_t loadedSoundAssets{0};
        std::uint64_t trackedLoopInstances{0};
        std::uint64_t trackedPlayingLoopVoices{0};
        std::uint64_t streamedAudioAssets{0};
        std::uint64_t oneShotPlayRequests{0};
        std::uint64_t oneShotPlaySuccesses{0};
        std::uint64_t loopPlayCommands{0};
        std::uint64_t loopStopCommands{0};
        std::uint64_t loopParameterUpdates{0};
    };

    // One complete synchronous district change. District content is currently generated in
    // memory, so there are deliberately no fabricated I/O/decompression/parse durations here;
    // the report labels those phases not applicable and records the two phases that really run.
    struct DistrictLoadSample
    {
        std::string reason;
        std::string sourceDistrict;
        std::string targetDistrict;
        double worldPhysicsMilliseconds{0.0};
        double rendererUploadMilliseconds{0.0};
        std::size_t proceduralWorldObjectCount{0};
        std::size_t staticPhysicsBodyCount{0};
        std::uint64_t residentBytesBefore{0};
        std::uint64_t residentBytesAfter{0};
        std::uint64_t trackedVideoMemoryBytesBefore{0};
        std::uint64_t trackedVideoMemoryBytesAfter{0};
    };

    struct PerformanceReportContext
    {
        std::string backend;
        std::string buildConfiguration;
        std::string scenario;
        int width{0};
        int height{0};
        bool verticalSyncRequested{true};
        int requestedSwapInterval{1};
        std::string nativeWindowSystem{"Unknown"};
        bool nativeWindowAvailable{false};
        bool graphicsRuntimeIdentityKnown{false};
        std::string graphicsRuntimeVendor;
        std::string graphicsRuntimeRenderer;
        std::string graphicsRuntimeVersion;
        std::string graphicsRuntimeUnavailableReason{"graphics runtime identity was not captured"};
        bool swapIntervalApplyResultKnown{false};
        bool swapIntervalApplySucceeded{false};
        std::optional<int> appliedSwapInterval;
        std::string swapIntervalUnavailableReason;
        bool fixedTimeStep{true};
        double targetFrameMilliseconds{0.0};
        std::uint64_t peakResidentBytes{0};
        std::uint64_t trackedVideoMemoryBytes{0};
        std::uint64_t trackedGameOwnedVideoMemoryBytes{0};
        std::uint64_t trackedImportedModelBufferBytes{0};
        std::uint64_t trackedImportedModelTextureBytes{0};
        bool videoMemoryTrackingComplete{false};
        bool gpuTimerSupported{false};
        std::string gpuTimerUnsupportedReason;
        std::size_t gpuTimerDiscardedSamples{0};
        std::size_t physicsBodyCount{0};
        std::size_t trafficVehicleCount{0};
        std::size_t pedestrianCount{0};
        int policeVehicleCount{0};
    };

    inline constexpr double kMinimumFrameBudgetMilliseconds = 1000.0 / 30.0;
    inline constexpr double kRecommendedFrameBudgetMilliseconds = 1000.0 / 60.0;
    inline constexpr double kFrameHitchThresholdMilliseconds = 50.0;
    inline constexpr double kSevereFrameHitchThresholdMilliseconds = 100.0;
    inline constexpr double kMinimumUpdateCpuBudgetMilliseconds = 8.0;
    inline constexpr double kMinimumPhysicsCpuBudgetMilliseconds = 3.0;
    inline constexpr double kMinimumAiCpuBudgetMilliseconds = 2.0;
    inline constexpr double kMinimumAudioCpuBudgetMilliseconds = 1.0;
    inline constexpr double kMinimumRenderCpuBudgetMilliseconds = 8.0;
    inline constexpr double kDistrictLoadBudgetMilliseconds = 1000.0;
    inline constexpr std::uint64_t kMinimumRamBudgetBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    inline constexpr std::uint64_t kMinimumVramBudgetBytes = 512ULL * 1024ULL * 1024ULL;

    class PerformanceProfiler final
    {
    public:
        using Clock = std::chrono::steady_clock;
        using WallClock = std::chrono::system_clock;

        void SetEnabled(bool enabled) noexcept;
        [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }

        // Call once at the beginning of every Draw(). The interval between consecutive calls is
        // the end-to-end presented-frame cadence; the first call establishes the baseline only.
        void BeginFrame();
        void Record(PerformanceMetric metric, double milliseconds);
        void RecordRenderWorkload(RenderWorkloadMetric metric, std::uint64_t count);
        void RecordPhysicsWorkload(PhysicsWorkloadMetric metric, std::uint64_t count);
        void RecordAiWorkload(const AiWorkloadSample& sample);
        void RecordAudioWorkload(const AudioWorkloadSample& sample);
        void RecordDistrictLoad(DistrictLoadSample sample);

        [[nodiscard]] PerformanceStatistics GetStatistics(PerformanceMetric metric) const;
        [[nodiscard]] FramePacingStatistics GetFramePacingStatistics() const;
        [[nodiscard]] RenderWorkloadStatistics
        GetRenderWorkloadStatistics(RenderWorkloadMetric metric) const;
        [[nodiscard]] PhysicsWorkloadStatistics
        GetPhysicsWorkloadStatistics(PhysicsWorkloadMetric metric) const;
        [[nodiscard]] AiWorkloadStatistics GetAiWorkloadStatistics(AiWorkloadMetric metric) const;
        [[nodiscard]] AudioWorkloadStatistics
        GetAudioWorkloadStatistics(AudioWorkloadMetric metric) const;
        [[nodiscard]] bool WriteJsonReport(const std::string& path,
                                           const PerformanceReportContext& context,
                                           std::string& error) const;

        // Linux reads VmHWM from /proc/self/status. Other platforms return 0 (unknown) rather
        // than fabricating a value; platform-specific implementations can be added later.
        [[nodiscard]] static std::uint64_t ReadPeakResidentBytes();
        // Linux reads the current VmRSS for before/after district-load deltas. As above, zero
        // means unavailable rather than a real zero-byte resident set.
        [[nodiscard]] static std::uint64_t ReadCurrentResidentBytes();

    private:
        static constexpr std::size_t MetricIndex(PerformanceMetric metric)
        {
            return static_cast<std::size_t>(metric);
        }

        static constexpr std::size_t RenderWorkloadMetricIndex(RenderWorkloadMetric metric)
        {
            return static_cast<std::size_t>(metric);
        }

        static constexpr std::size_t PhysicsWorkloadMetricIndex(PhysicsWorkloadMetric metric)
        {
            return static_cast<std::size_t>(metric);
        }

        static constexpr std::size_t AiWorkloadMetricIndex(AiWorkloadMetric metric)
        {
            return static_cast<std::size_t>(metric);
        }

        static constexpr std::size_t AudioWorkloadMetricIndex(AudioWorkloadMetric metric)
        {
            return static_cast<std::size_t>(metric);
        }

        bool enabled_{false};
        std::optional<WallClock::time_point> captureStartedUtc_;
        std::optional<Clock::time_point> previousFrameStart_;
        std::array<std::vector<double>, static_cast<std::size_t>(PerformanceMetric::Count)> samples_;
        std::array<std::vector<double>, static_cast<std::size_t>(RenderWorkloadMetric::Count)>
            renderWorkloadSamples_;
        std::array<std::vector<double>, static_cast<std::size_t>(PhysicsWorkloadMetric::Count)>
            physicsWorkloadSamples_;
        std::array<std::vector<double>, static_cast<std::size_t>(AiWorkloadMetric::Count)>
            aiWorkloadSamples_;
        std::array<std::vector<double>, static_cast<std::size_t>(AudioWorkloadMetric::Count)>
            audioWorkloadSamples_;
        std::vector<DistrictLoadSample> districtLoadSamples_;
        std::vector<std::size_t> districtTransitionFrameSampleIndices_;
    };

    // RAII timing for whole Update()/Draw()/Initialize() scopes, including early-return paths.
    class ScopedPerformanceSample final
    {
    public:
        ScopedPerformanceSample(PerformanceProfiler& profiler, PerformanceMetric metric);
        ~ScopedPerformanceSample();

        ScopedPerformanceSample(const ScopedPerformanceSample&) = delete;
        ScopedPerformanceSample& operator=(const ScopedPerformanceSample&) = delete;

    private:
        PerformanceProfiler* profiler_{nullptr};
        PerformanceMetric metric_{PerformanceMetric::UpdateCpu};
        PerformanceProfiler::Clock::time_point start_{};
    };
}
