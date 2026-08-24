#pragma once

namespace CopperBoots
{
    enum class CameraVerticalPolicy
    {
        Follow,
        Locked,
    };

    class Camera2D
    {
    public:
        Camera2D(float viewportWidth, float viewportHeight);

        void SetWorldBounds(float width, float height);
        void SnapTo(float focusX, float focusY);
        void Update(float focusX, float focusY, float horizontalVelocity,
                    float seconds);
        void SetVerticalPolicy(CameraVerticalPolicy policy) noexcept
        {
            verticalPolicy_ = policy;
        }
        void SetShakeOffset(float x, float y) noexcept;
        void ClearShake() noexcept { SetShakeOffset(0.0F, 0.0F); }

        [[nodiscard]] float X() const noexcept { return ClampX(x_ + shakeX_); }
        [[nodiscard]] float Y() const noexcept { return ClampY(y_ + shakeY_); }
        [[nodiscard]] float BaseX() const noexcept { return x_; }
        [[nodiscard]] float BaseY() const noexcept { return y_; }
        [[nodiscard]] float ViewportWidth() const noexcept { return viewportWidth_; }
        [[nodiscard]] float ViewportHeight() const noexcept { return viewportHeight_; }

    private:
        [[nodiscard]] float ClampX(float value) const noexcept;
        [[nodiscard]] float ClampY(float value) const noexcept;

        float viewportWidth_;
        float viewportHeight_;
        float worldWidth_;
        float worldHeight_;
        float x_ = 0.0F;
        float y_ = 0.0F;
        float shakeX_ = 0.0F;
        float shakeY_ = 0.0F;
        CameraVerticalPolicy verticalPolicy_ = CameraVerticalPolicy::Follow;
    };
}
