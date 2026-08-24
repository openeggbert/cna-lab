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

    struct RenderWorkloadStatistics
    {
        std::size_t sampleCount{0};
        double average{0.0};
        double p95{0.0};
        double maximum{0.0};
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

        void SetEnabled(bool enabled) noexcept;
        [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }

        // Call once at the beginning of every Draw(). The interval between consecutive calls is
        // the end-to-end presented-frame cadence; the first call establishes the baseline only.
        void BeginFrame();
        void Record(PerformanceMetric metric, double milliseconds);
        void RecordRenderWorkload(RenderWorkloadMetric metric, std::uint64_t count);

        [[nodiscard]] PerformanceStatistics GetStatistics(PerformanceMetric metric) const;
        [[nodiscard]] RenderWorkloadStatistics
        GetRenderWorkloadStatistics(RenderWorkloadMetric metric) const;
        [[nodiscard]] bool WriteJsonReport(const std::string& path,
                                           const PerformanceReportContext& context,
                                           std::string& error) const;

        // Linux reads VmHWM from /proc/self/status. Other platforms return 0 (unknown) rather
        // than fabricating a value; platform-specific implementations can be added later.
        [[nodiscard]] static std::uint64_t ReadPeakResidentBytes();

    private:
        static constexpr std::size_t MetricIndex(PerformanceMetric metric)
        {
            return static_cast<std::size_t>(metric);
        }

        static constexpr std::size_t RenderWorkloadMetricIndex(RenderWorkloadMetric metric)
        {
            return static_cast<std::size_t>(metric);
        }

        bool enabled_{false};
        std::optional<Clock::time_point> previousFrameStart_;
        std::array<std::vector<double>, static_cast<std::size_t>(PerformanceMetric::Count)> samples_;
        std::array<std::vector<double>, static_cast<std::size_t>(RenderWorkloadMetric::Count)>
            renderWorkloadSamples_;
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
