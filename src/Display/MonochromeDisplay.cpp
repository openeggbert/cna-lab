#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"

#include <algorithm>

namespace CnaTamagotchi::Display {
namespace {

using Glyph = std::array<std::string_view, 5>;

constexpr Glyph Blank{{"000", "000", "000", "000", "000"}};
constexpr Glyph LetterA{{"010", "101", "111", "101", "101"}};
constexpr Glyph LetterC{{"011", "100", "100", "100", "011"}};
constexpr Glyph LetterD{{"110", "101", "101", "101", "110"}};
constexpr Glyph LetterE{{"111", "100", "110", "100", "111"}};
constexpr Glyph LetterF{{"111", "100", "110", "100", "100"}};
constexpr Glyph LetterG{{"011", "100", "101", "101", "011"}};
constexpr Glyph LetterH{{"101", "101", "111", "101", "101"}};
constexpr Glyph LetterI{{"111", "010", "010", "010", "111"}};
constexpr Glyph LetterK{{"101", "101", "110", "101", "101"}};
constexpr Glyph LetterL{{"100", "100", "100", "100", "111"}};
constexpr Glyph LetterM{{"101", "111", "111", "101", "101"}};
constexpr Glyph LetterN{{"101", "111", "111", "111", "101"}};
constexpr Glyph LetterO{{"010", "101", "101", "101", "010"}};
constexpr Glyph LetterP{{"110", "101", "110", "100", "100"}};
constexpr Glyph LetterR{{"110", "101", "110", "101", "101"}};
constexpr Glyph LetterS{{"011", "100", "010", "001", "110"}};
constexpr Glyph LetterT{{"111", "010", "010", "010", "010"}};
constexpr Glyph LetterU{{"101", "101", "101", "101", "111"}};
constexpr Glyph LetterW{{"101", "101", "101", "111", "101"}};
constexpr Glyph Digit0{{"111", "101", "101", "101", "111"}};
constexpr Glyph Digit1{{"010", "110", "010", "010", "111"}};
constexpr Glyph Digit2{{"110", "001", "010", "100", "111"}};
constexpr Glyph Digit3{{"110", "001", "010", "001", "110"}};
constexpr Glyph Digit4{{"101", "101", "111", "001", "001"}};
constexpr Glyph Digit5{{"111", "100", "110", "001", "110"}};
constexpr Glyph Digit6{{"011", "100", "111", "101", "111"}};
constexpr Glyph Digit7{{"111", "001", "010", "010", "010"}};
constexpr Glyph Digit8{{"111", "101", "111", "101", "111"}};
constexpr Glyph Digit9{{"111", "101", "111", "001", "110"}};

const Glyph& glyphFor(const char character) noexcept
{
    switch (character) {
    case 'A': return LetterA;
    case 'C': return LetterC;
    case 'D': return LetterD;
    case 'E': return LetterE;
    case 'F': return LetterF;
    case 'G': return LetterG;
    case 'H': return LetterH;
    case 'I': return LetterI;
    case 'K': return LetterK;
    case 'L': return LetterL;
    case 'M': return LetterM;
    case 'N': return LetterN;
    case 'O': return LetterO;
    case 'P': return LetterP;
    case 'R': return LetterR;
    case 'S': return LetterS;
    case 'T': return LetterT;
    case 'U': return LetterU;
    case 'W': return LetterW;
    case '0': return Digit0;
    case '1': return Digit1;
    case '2': return Digit2;
    case '3': return Digit3;
    case '4': return Digit4;
    case '5': return Digit5;
    case '6': return Digit6;
    case '7': return Digit7;
    case '8': return Digit8;
    case '9': return Digit9;
    default: return Blank;
    }
}

} // namespace

void MonochromeDisplay::clear(const bool value) noexcept
{
    std::ranges::fill(pixels_, value);
}

bool MonochromeDisplay::pixel(const int x, const int y) const noexcept
{
    if (!isInside(x, y)) {
        return false;
    }

    return pixels_[static_cast<std::size_t>(y * Width + x)];
}

void MonochromeDisplay::setPixel(const int x, const int y, const bool value) noexcept
{
    if (!isInside(x, y)) {
        return;
    }

    pixels_[static_cast<std::size_t>(y * Width + x)] = value;
}

void MonochromeDisplay::fillRectangle(const int x, const int y, const int width,
                                      const int height, const bool value) noexcept
{
    if (width <= 0 || height <= 0) {
        return;
    }

    const int firstX = std::clamp(x, 0, Width);
    const int firstY = std::clamp(y, 0, Height);
    const auto unclippedLastX = static_cast<long long>(x) + static_cast<long long>(width);
    const auto unclippedLastY = static_cast<long long>(y) + static_cast<long long>(height);
    const int lastX = static_cast<int>(std::clamp(
        unclippedLastX, 0LL, static_cast<long long>(Width)));
    const int lastY = static_cast<int>(std::clamp(
        unclippedLastY, 0LL, static_cast<long long>(Height)));
    for (int currentY = firstY; currentY < lastY; ++currentY) {
        for (int currentX = firstX; currentX < lastX; ++currentX) {
            pixels_[static_cast<std::size_t>(currentY * Width + currentX)] = value;
        }
    }
}

void MonochromeDisplay::drawSprite(const int x, const int y,
                                   const std::span<const std::string_view> rows,
                                   const char onPixel) noexcept
{
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const std::string_view pixels = rows[static_cast<std::size_t>(row)];
        for (int column = 0; column < static_cast<int>(pixels.size()); ++column) {
            if (pixels[static_cast<std::size_t>(column)] == onPixel) {
                setPixel(x + column, y + row, true);
            }
        }
    }
}

void MonochromeDisplay::drawText(const int x, const int y, const std::string_view text) noexcept
{
    constexpr int GlyphWidth = 3;
    constexpr int GlyphHeight = 5;
    constexpr int GlyphAdvance = GlyphWidth + 1;
    for (int index = 0; index < static_cast<int>(text.size()); ++index) {
        const Glyph& glyph = glyphFor(text[static_cast<std::size_t>(index)]);
        for (int row = 0; row < GlyphHeight; ++row) {
            for (int column = 0; column < GlyphWidth; ++column) {
                if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] == '1') {
                    setPixel(x + index * GlyphAdvance + column, y + row, true);
                }
            }
        }
    }
}

} // namespace CnaTamagotchi::Display
