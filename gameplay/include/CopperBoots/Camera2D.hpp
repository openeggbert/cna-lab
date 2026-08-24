#pragma once

namespace CopperBoots
{
    class Camera2D
    {
    public:
        Camera2D(float viewportWidth, float viewportHeight);

        void SetWorldBounds(float width, float height);
        void SnapTo(float focusX, float focusY);
        void Update(float focusX, float focusY, float horizontalVelocity,
                    float seconds);

        [[nodiscard]] float X() const noexcept { return x_; }
        [[nodiscard]] float Y() const noexcept { return y_; }
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
    };
}

