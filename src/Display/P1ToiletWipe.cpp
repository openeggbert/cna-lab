#include "TamagotchiCna/Display/P1ToiletWipe.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace TamagotchiCna::Display {
namespace {

constexpr std::array<std::string_view, 4> WaterRows{{
    "..##.#",
    ".##.#.",
    "##.#..",
    ".##.#.",
}};

} // namespace

std::size_t P1ToiletWipe::phaseAt(const float elapsedSeconds) noexcept
{
    if (elapsedSeconds <= 0.0F) {
        return 0U;
    }

    constexpr float MovingBeforeFinalSeconds =
        (MovingPhaseCount - 1U) * MovingPhaseSeconds;
    if (elapsedSeconds < MovingBeforeFinalSeconds) {
        return std::min(
            static_cast<std::size_t>(elapsedSeconds / MovingPhaseSeconds),
            MovingPhaseCount - 2U);
    }
    if (elapsedSeconds < MovingBeforeFinalSeconds + FinalBandHoldSeconds) {
        return MovingPhaseCount - 1U;
    }
    return EmptyPhase;
}

bool P1ToiletWipe::complete(const float elapsedSeconds) noexcept
{
    return elapsedSeconds >= DurationSeconds;
}

void P1ToiletWipe::render(MonochromeDisplay& destination,
                          const MonochromeDisplay& source,
                          const std::size_t phase) noexcept
{
    destination.clear();
    if (phase >= EmptyPhase) {
        return;
    }

    const int shift = 2 * (static_cast<int>(phase) + 1);
    for (int y = 0; y < MonochromeDisplay::Height; ++y) {
        for (int x = 0; x < MonochromeDisplay::Width - shift; ++x) {
            destination.setPixel(x, y, source.pixel(x + shift, y));
        }
    }

    const int waterOriginX = MonochromeDisplay::Width - shift;
    for (int y = 0; y < MonochromeDisplay::Height; ++y) {
        const std::string_view row = WaterRows[static_cast<std::size_t>(y % 4)];
        for (int x = 0; x < static_cast<int>(row.size()); ++x) {
            if (row[static_cast<std::size_t>(x)] == '#') {
                destination.setPixel(waterOriginX + x, y, true);
            }
        }
    }
}

} // namespace TamagotchiCna::Display
