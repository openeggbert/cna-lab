#pragma once

#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"

#include <cstddef>

namespace CnaTamagotchi::Display {

// One complete, manually transcribed international-P1 Marutchi Medicine
// action. The seven visible phases occupy sixteen frames of a 30 fps trace.
// Other forms deliberately keep their fallback until separately observed.
class P1MedicineAnimation final {
public:
    static constexpr std::size_t PhaseCount = 7U;
    static constexpr float DurationSeconds = 16.0F / 30.0F;

    [[nodiscard]] static std::size_t phaseAt(float elapsedSeconds) noexcept;
    [[nodiscard]] static bool complete(float elapsedSeconds) noexcept;
    static void render(MonochromeDisplay& destination, std::size_t phase) noexcept;
};

} // namespace CnaTamagotchi::Display
