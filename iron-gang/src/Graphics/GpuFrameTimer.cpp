#include "IronGang/Graphics/GpuFrameTimer.hpp"

#include "CNA/Internal/Renderers/Common/IGraphicsRenderer.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

#include <cstdint>
#include <limits>

namespace IronGang
{
    struct GpuFrameTimer::Impl
    {
        explicit Impl(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device)
        {
            auto& renderer = device.GetRenderer();
            if (!renderer.SupportsGpuTimerEXT())
            {
                unsupportedReason = "the " + std::string(device.GetGraphicsRendererName()) +
                    " renderer has no GPU timer query (GL ES needs GL_EXT_disjoint_timer_query)";
                return;
            }
            timer = renderer.CreateGpuTimerEXT();
            if (!timer)
            {
                unsupportedReason = "the " + std::string(device.GetGraphicsRendererName()) +
                    " renderer reported GPU timing support but did not create a timer";
            }
        }

        std::unique_ptr<CNA::Internal::Renderers::IGpuTimerRenderer> timer;
        std::string unsupportedReason;
        bool open{false};
        bool pending{false};
        std::size_t discardedSamples{0};
    };

    GpuFrameTimer::GpuFrameTimer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device)
        : impl_(std::make_unique<Impl>(device))
    {
    }

    GpuFrameTimer::~GpuFrameTimer() = default;

    bool GpuFrameTimer::IsSupported() const noexcept
    {
        return impl_->timer != nullptr;
    }

    const std::string& GpuFrameTimer::GetUnsupportedReason() const noexcept
    {
        return impl_->unsupportedReason;
    }

    std::size_t GpuFrameTimer::GetDiscardedSampleCount() const noexcept
    {
        return impl_->discardedSamples;
    }

    bool GpuFrameTimer::Poll(double& milliseconds)
    {
        if (!impl_->timer || !impl_->pending || !impl_->timer->IsResultAvailable())
        {
            return false;
        }
        const std::uint64_t nanoseconds = impl_->timer->ElapsedNanoseconds();
        impl_->pending = false;
        // EasyGL's current metagl seam retrieves timer results through a 32-bit GLuint. The
        // all-ones value is its explicit saturation result and was observed once on llvmpipe as
        // 4294.967 ms even though the complete host frame took 57 ms. Preserve the evidence as a
        // discarded count, but never let that sentinel contaminate averages or percentiles.
        if (nanoseconds == std::numeric_limits<std::uint32_t>::max())
        {
            ++impl_->discardedSamples;
            return false;
        }
        milliseconds = static_cast<double>(nanoseconds) / 1.0e6;
        return true;
    }

    void GpuFrameTimer::Begin()
    {
        if (impl_->timer && !impl_->pending && !impl_->open)
        {
            impl_->timer->Begin();
            impl_->open = true;
        }
    }

    void GpuFrameTimer::End()
    {
        if (impl_->timer && impl_->open)
        {
            impl_->timer->End();
            impl_->open = false;
            impl_->pending = true;
        }
    }
}
