#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace CnaTamagotchi::Display {

struct LcdColour final {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

struct LcdPaletteColours final {
    LcdColour bezel;
    LcdColour off;
    LcdColour on;
};

// LCD chemistry varied between devices and viewing conditions. These presets
// affect only the shell renderer; the framebuffer remains strictly one bit.
enum class LcdPalette {
    ClassicOlive,
    Amber,
    IceBlue,
    HighContrast,
};

// The rendering model for the retro LCD. It deliberately has no CNA
// dependency, so domain and display tests will run without a GPU/window.
class MonochromeDisplay final {
public:
    static constexpr int Width = 32;
    static constexpr int Height = 16;

    void clear(bool value = false) noexcept;
    [[nodiscard]] bool pixel(int x, int y) const noexcept;
    void setPixel(int x, int y, bool value) noexcept;
    void fillRectangle(int x, int y, int width, int height, bool value) noexcept;
    void drawSprite(int x, int y, std::span<const std::string_view> rows,
                    char onPixel = '#') noexcept;
    void drawText(int x, int y, std::string_view text) noexcept;

    [[nodiscard]] static constexpr LcdPaletteColours coloursFor(
        LcdPalette palette) noexcept
    {
        switch (palette) {
        case LcdPalette::ClassicOlive:
            return {{77U, 91U, 62U}, {188U, 202U, 143U}, {34U, 44U, 31U}};
        case LcdPalette::Amber:
            return {{91U, 72U, 39U}, {235U, 199U, 115U}, {63U, 48U, 18U}};
        case LcdPalette::IceBlue:
            return {{54U, 80U, 87U}, {174U, 214U, 213U}, {24U, 42U, 48U}};
        case LcdPalette::HighContrast:
            return {{50U, 50U, 50U}, {231U, 231U, 221U}, {18U, 18U, 18U}};
        }

        return {{77U, 91U, 62U}, {188U, 202U, 143U}, {34U, 44U, 31U}};
    }

private:
    [[nodiscard]] static constexpr bool isInside(const int x, const int y) noexcept
    {
        return x >= 0 && x < Width && y >= 0 && y < Height;
    }

    std::array<bool, static_cast<std::size_t>(Width * Height)> pixels_{};
};

} // namespace CnaTamagotchi::Display
