#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"

#include <algorithm>

namespace CnaTamagotchi::Display {

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

} // namespace CnaTamagotchi::Display
