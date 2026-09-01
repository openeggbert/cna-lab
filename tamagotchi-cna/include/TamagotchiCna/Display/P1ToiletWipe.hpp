#pragma once

#include "TamagotchiCna/Display/MonochromeDisplay.hpp"

#include <cstddef>

namespace TamagotchiCna::Display {

// The observed P1 Toilet core scrolls the complete 32x16 image left by two
// cells per phase while a six-cell diagonal water band enters from the right.
// The separate post-flush character celebration is not part of this class.
class P1ToiletWipe final {
public:
    static constexpr float MovingPhaseSeconds = 0.10F;
    static constexpr std::size_t MovingPhaseCount = 16U;
    static constexpr float FinalBandHoldSeconds = 0.30F;
    static constexpr float EmptyHoldSeconds = 0.10F;
    static constexpr std::size_t EmptyPhase = MovingPhaseCount;
    static constexpr float DurationSeconds =
        (MovingPhaseCount - 1U) * MovingPhaseSeconds
        + FinalBandHoldSeconds + EmptyHoldSeconds;

    [[nodiscard]] static std::size_t phaseAt(float elapsedSeconds) noexcept;
    [[nodiscard]] static bool complete(float elapsedSeconds) noexcept;
    static void render(MonochromeDisplay& destination,
                       const MonochromeDisplay& source,
                       std::size_t phase) noexcept;
};

} // namespace TamagotchiCna::Display
