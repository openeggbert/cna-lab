#include "CopperBoots/Camera2D.hpp"

#include <algorithm>
#include <cmath>

namespace CopperBoots
{
    Camera2D::Camera2D(const float viewportWidth, const float viewportHeight)
        : viewportWidth_(viewportWidth),
          viewportHeight_(viewportHeight),
          worldWidth_(viewportWidth),
          worldHeight_(viewportHeight)
    {
    }

    void Camera2D::SetWorldBounds(const float width, const float height)
    {
        worldWidth_ = std::max(width, viewportWidth_);
        worldHeight_ = std::max(height, viewportHeight_);
        x_ = ClampX(x_);
        y_ = ClampY(y_);
    }

    void Camera2D::SnapTo(const float focusX, const float focusY)
    {
        x_ = ClampX(focusX - viewportWidth_ * 0.5F);
        y_ = ClampY(focusY - viewportHeight_ * 0.55F);
    }

    void Camera2D::Update(const float focusX, const float focusY,
                          const float horizontalVelocity, const float seconds)
    {
        const float lookAhead = std::clamp(horizontalVelocity * 0.28F,
                                           -34.0F, 34.0F);
        const float targetX = ClampX(focusX + lookAhead - viewportWidth_ * 0.5F);
        const float targetY = verticalPolicy_ == CameraVerticalPolicy::Follow
            ? ClampY(focusY - viewportHeight_ * 0.55F)
            : y_;
        const float blend = 1.0F - std::exp(-8.0F * std::max(seconds, 0.0F));
        x_ = ClampX(x_ + (targetX - x_) * blend);
        y_ = ClampY(y_ + (targetY - y_) * blend);
    }

    void Camera2D::SetShakeOffset(const float x, const float y) noexcept
    {
        shakeX_ = x;
        shakeY_ = y;
    }

    float Camera2D::ClampX(const float value) const noexcept
    {
        return std::clamp(value, 0.0F,
                          std::max(0.0F, worldWidth_ - viewportWidth_));
    }

    float Camera2D::ClampY(const float value) const noexcept
    {
        return std::clamp(value, 0.0F,
                          std::max(0.0F, worldHeight_ - viewportHeight_));
    }
}
