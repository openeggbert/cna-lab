#include "TamagotchiCna/Display/P1MedicineAnimation.hpp"

#include <array>
#include <string_view>

namespace TamagotchiCna::Display {
namespace {

using Frame = std::array<std::string_view, MonochromeDisplay::Height>;

constexpr Frame FrontWithDose{{
    "................................",
    "................................",
    "................................",
    ".............######........##...",
    "............#......#.......##...",
    "...........#.##..##.#.....#.....",
    "...........#........#...........",
    "...........#...##...#...........",
    "...........#........#...........",
    "...........#........#...........",
    "............#......#............",
    ".............######.............",
    "................................",
    "................................",
    "................................",
    "................................",
}};

constexpr Frame SideAWithDose{{
    ".............#####..........##..",
    "............#.....#.........####",
    "............##.....#.......#####",
    ".............##.....#......####.",
    "..............##.##.#........##.",
    "...............#....#...........",
    "...........#.#.#....#....#......",
    "...........#####....#...........",
    "...........#........#...........",
    "............#......#............",
    ".............######.............",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
}};

constexpr Frame SideBWithDose{{
    "..............#####.........##..",
    ".............#.....#........####",
    "............#.....##.......#####",
    "...........#.....##........####.",
    "...........#.##.##...........##.",
    "...........#....#...............",
    "...........#....#.#.#....#......",
    "...........#....#####...........",
    "...........#........#...........",
    "............#......#............",
    ".............######.............",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
}};

constexpr std::array<float, P1MedicineAnimation::PhaseCount> PhaseEndSeconds{{
    2.0F / 30.0F,
    4.0F / 30.0F,
    7.0F / 30.0F,
    9.0F / 30.0F,
    11.0F / 30.0F,
    13.0F / 30.0F,
    P1MedicineAnimation::DurationSeconds,
}};

} // namespace

std::size_t P1MedicineAnimation::phaseAt(const float elapsedSeconds) noexcept
{
    for (std::size_t phase = 0U; phase < PhaseEndSeconds.size(); ++phase) {
        if (elapsedSeconds < PhaseEndSeconds[phase]) {
            return phase;
        }
    }
    return PhaseCount - 1U;
}

bool P1MedicineAnimation::complete(const float elapsedSeconds) noexcept
{
    return elapsedSeconds >= DurationSeconds;
}

void P1MedicineAnimation::render(MonochromeDisplay& destination,
                                 const std::size_t phase) noexcept
{
    destination.clear();
    const std::size_t boundedPhase = phase % PhaseCount;
    if (boundedPhase == 1U || boundedPhase == 5U) {
        destination.drawSprite(0, 0, SideAWithDose);
    } else if (boundedPhase == 3U) {
        destination.drawSprite(0, 0, SideBWithDose);
    } else {
        destination.drawSprite(0, 0, FrontWithDose);
    }
}

} // namespace TamagotchiCna::Display
