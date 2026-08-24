#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
}

namespace IronGang
{
    // Small game-facing wrapper around CNA's asynchronous renderer timer contract. It owns the
    // pending-query state that the contract deliberately does not expose, preventing a new frame
    // from overwriting a result the GPU has not finished yet. Unsupported renderers remain inert
    // and provide an actionable reason instead of substituting a CPU wall-clock value.
    class GpuFrameTimer final
    {
    public:
        explicit GpuFrameTimer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
        ~GpuFrameTimer();

        GpuFrameTimer(const GpuFrameTimer&) = delete;
        GpuFrameTimer& operator=(const GpuFrameTimer&) = delete;

        [[nodiscard]] bool IsSupported() const noexcept;
        [[nodiscard]] const std::string& GetUnsupportedReason() const noexcept;
        [[nodiscard]] std::size_t GetDiscardedSampleCount() const noexcept;

        // Poll never blocks. A false return means either no range is pending or its GPU result is
        // not ready; in both cases milliseconds is untouched.
        bool Poll(double& milliseconds);
        void Begin();
        void End();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
