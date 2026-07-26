#pragma once

#include <array>
#include <cstddef>

namespace CnaTamagotchi::Display {

// The rendering model for the retro LCD. It deliberately has no CNA
// dependency, so domain and display tests will run without a GPU/window.
class MonochromeDisplay final {
public:
    static constexpr int Width = 32;
    static constexpr int Height = 24;

    void clear(bool value = false) noexcept;
    [[nodiscard]] bool pixel(int x, int y) const noexcept;
    void setPixel(int x, int y, bool value) noexcept;

private:
    [[nodiscard]] static constexpr bool isInside(const int x, const int y) noexcept
    {
        return x >= 0 && x < Width && y >= 0 && y < Height;
    }

    std::array<bool, static_cast<std::size_t>(Width * Height)> pixels_{};
};

} // namespace CnaTamagotchi::Display
