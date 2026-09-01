#include "TamagotchiCna/Display/P1LightScreen.hpp"

#include <array>

namespace TamagotchiCna::Display {
namespace {

constexpr std::array<std::string_view, 16> MenuRows{{
    "................................",
    "..........###....##...#.........",
    ".........#...#...#.#..#.........",
    ".........#...#...#.#..#.........",
    ".........#...#...#..#.#.........",
    ".........#...#...#..#.#.........",
    "..........###....#...##.........",
    "................................",
    "................................",
    "..........###..####.####........",
    ".........#...#.#....#...........",
    ".........#...#.###..###.........",
    ".........#...#.#....#...........",
    ".........#...#.#....#...........",
    "..........###..#....#...........",
    "................................",
}};

constexpr std::array<std::string_view, 7> SelectionMarker{{
    "..#...",
    "..##..",
    "#####.",
    "######",
    "#####.",
    "..##..",
    "..#...",
}};

} // namespace

void P1LightScreen::renderMenu(MonochromeDisplay& destination,
                               const bool offSelected) noexcept
{
    destination.clear();
    destination.drawSprite(0, 0, MenuRows);
    destination.drawSprite(1, offSelected ? 9 : 1, SelectionMarker);
}

void P1LightScreen::renderLightsOut(MonochromeDisplay& destination,
                                    const int sleepOriginX,
                                    const int sleepOriginY,
                                    const std::span<const std::string_view> sleepRows) noexcept
{
    destination.clear(true);
    for (int row = 0; row < static_cast<int>(sleepRows.size()); ++row) {
        const std::string_view pixels = sleepRows[static_cast<std::size_t>(row)];
        for (int column = 0; column < static_cast<int>(pixels.size()); ++column) {
            if (pixels[static_cast<std::size_t>(column)] == '#') {
                destination.setPixel(
                    sleepOriginX - LightsOutSleepShiftX + column,
                    sleepOriginY + row, false);
            }
        }
    }
}

} // namespace TamagotchiCna::Display
