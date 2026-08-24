#pragma once

#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"

#include <span>
#include <string_view>

namespace CnaTamagotchi::Display {

// Exact stable P1 Light menu and lights-out presentation. The normal sleep Z
// data remains in P1SpriteCatalog; lights-out inverts it over a filled LCD and
// shifts it eight cells left, as observed on a sleeping Marutchi.
class P1LightScreen final {
public:
    static constexpr int LightsOutSleepShiftX = 8;

    static void renderMenu(MonochromeDisplay& destination,
                           bool offSelected) noexcept;
    static void renderLightsOut(MonochromeDisplay& destination,
                                int sleepOriginX,
                                int sleepOriginY,
                                std::span<const std::string_view> sleepRows) noexcept;
};

} // namespace CnaTamagotchi::Display
