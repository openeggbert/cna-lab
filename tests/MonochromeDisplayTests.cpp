#include "TamagotchiCna/Display/MonochromeDisplay.hpp"
#include "TamagotchiCna/Display/P1LightScreen.hpp"
#include "TamagotchiCna/Display/P1MedicineAnimation.hpp"
#include "TamagotchiCna/Display/P1ToiletWipe.hpp"

#include <array>
#include <iostream>

using TamagotchiCna::Display::LcdPalette;
using TamagotchiCna::Display::MonochromeDisplay;
using TamagotchiCna::Display::P1LightScreen;
using TamagotchiCna::Display::P1MedicineAnimation;
using TamagotchiCna::Display::P1ToiletWipe;

namespace {

int failures = 0;

void expect(const bool condition, const char* const message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

int litPixelCount(const MonochromeDisplay& display)
{
    int count = 0;
    for (int y = 0; y < MonochromeDisplay::Height; ++y) {
        for (int x = 0; x < MonochromeDisplay::Width; ++x) {
            count += display.pixel(x, y) ? 1 : 0;
        }
    }
    return count;
}

bool matchesRows(const MonochromeDisplay& display,
                 const std::array<std::string_view, 16>& rows)
{
    for (int y = 0; y < MonochromeDisplay::Height; ++y) {
        if (rows[static_cast<std::size_t>(y)].size() != MonochromeDisplay::Width) return false;
        for (int x = 0; x < MonochromeDisplay::Width; ++x) {
            const bool expected = rows[static_cast<std::size_t>(y)]
                [static_cast<std::size_t>(x)] == '#';
            if (display.pixel(x, y) != expected) return false;
        }
    }
    return true;
}

void testPixelsAreBounded()
{
    MonochromeDisplay display;
    display.setPixel(-1, 0, true);
    display.setPixel(MonochromeDisplay::Width, 0, true);
    display.setPixel(0, MonochromeDisplay::Height, true);
    display.setPixel(12, 7, true);

    expect(display.pixel(12, 7), "an in-range pixel must be settable");
    expect(!display.pixel(-1, 0), "negative pixel reads must remain off");
    expect(!display.pixel(MonochromeDisplay::Width, 0),
        "right-edge pixel reads must remain off");
    expect(litPixelCount(display) == 1, "out-of-range writes must not affect the framebuffer");
}

void testRectangleIsClipped()
{
    MonochromeDisplay display;
    display.fillRectangle(-2, -1, 5, 3, true);
    expect(litPixelCount(display) == 6, "a clipped rectangle must fill only its visible area");
    expect(display.pixel(0, 0) && display.pixel(2, 1),
        "the visible rectangle bounds must be filled");

    display.setPixel(31, 15, true);
    display.fillRectangle(31, 15, 4, 4, false);
    expect(!display.pixel(31, 15), "a clipped clear rectangle must affect the final pixel");
    expect(litPixelCount(display) == 6, "clearing at the edge must not disturb other pixels");
}

void testSpriteBlitIsClipped()
{
    MonochromeDisplay display;
    constexpr std::array<std::string_view, 3> sprite{{"# #", " # ", "###"}};
    display.drawSprite(-1, 14, sprite);

    expect(display.pixel(1, 14), "visible sprite pixels must be drawn");
    expect(display.pixel(0, 15), "sprite rows must keep their original alignment");
    expect(litPixelCount(display) == 2, "off-screen sprite rows and columns must be clipped");
}

void testTextUsesTheFramebufferFont()
{
    MonochromeDisplay display;
    display.drawText(0, 0, "A");
    expect(display.pixel(1, 0) && display.pixel(0, 1) && display.pixel(2, 1),
        "the built-in font must draw the expected glyph shape");
    expect(litPixelCount(display) == 10, "the A glyph must have its documented pixel count");

    display.clear();
    display.drawText(31, 14, "A");
    expect(litPixelCount(display) == 1, "text must be clipped at the LCD boundary");
}

void testPalettesRemainOneBit()
{
    const auto olive = MonochromeDisplay::coloursFor(LcdPalette::ClassicOlive);
    const auto amber = MonochromeDisplay::coloursFor(LcdPalette::Amber);

    expect(olive.on.red == 34U && olive.off.green == 202U,
        "the classic palette must preserve the documented olive colours");
    expect(olive.on.red != olive.off.red || olive.on.green != olive.off.green
               || olive.on.blue != olive.off.blue,
        "a palette must keep the on and off LCD states visually distinct");
    expect(amber.on.red != olive.on.red || amber.on.green != olive.on.green
               || amber.on.blue != olive.on.blue,
        "palette selection must change the renderer colours, not framebuffer data");
}

void testP1ToiletWipeMovesTheWholeFramebuffer()
{
    MonochromeDisplay source;
    source.setPixel(2, 0, true);
    source.setPixel(10, 0, true);
    MonochromeDisplay destination;

    P1ToiletWipe::render(destination, source, 0U);

    expect(destination.pixel(0, 0) && destination.pixel(8, 0),
           "the first Toilet phase must shift every source pixel two cells left");
    expect(!destination.pixel(2, 0) && !destination.pixel(10, 0),
           "the Toilet wipe must not retain the source at its old position");
    expect(litPixelCount(destination) == 18,
           "the clipped first water band must add its exact 16 visible pixels");
}

void testP1ToiletWipeKeepsTheObservedWaterPattern()
{
    MonochromeDisplay source;
    MonochromeDisplay destination;
    constexpr std::array<std::string_view, 4> expectedRows{{
        "..##.#", ".##.#.", "##.#..", ".##.#.",
    }};

    P1ToiletWipe::render(destination, source, 8U);
    expect(litPixelCount(destination) == 48,
           "a fully visible P1 Toilet water band must contain 48 lit cells");
    for (int y = 0; y < MonochromeDisplay::Height; ++y) {
        for (int x = 0; x < 6; ++x) {
            const bool expected = expectedRows[static_cast<std::size_t>(y % 4)]
                [static_cast<std::size_t>(x)] == '#';
            expect(destination.pixel(14 + x, y) == expected,
                   "every repeated water-band cell must remain exact");
        }
    }

    P1ToiletWipe::render(destination, source, P1ToiletWipe::MovingPhaseCount - 1U);
    expect(litPixelCount(destination) == 48 && destination.pixel(2, 0)
               && destination.pixel(5, 0),
           "the last moving phase must hold the complete band at the left edge");
    P1ToiletWipe::render(destination, source, P1ToiletWipe::EmptyPhase);
    expect(litPixelCount(destination) == 0,
           "the observed blank phase must follow the departing water band");
}

void testP1ToiletWipeTimingIsDeterministic()
{
    expect(P1ToiletWipe::phaseAt(0.0F) == 0U
               && P1ToiletWipe::phaseAt(0.10F) == 1U,
           "the moving Toilet phases must advance at a tenth-second cadence");
    expect(P1ToiletWipe::phaseAt(1.79F) == 15U,
           "the complete left-edge water band must receive its observed longer hold");
    expect(P1ToiletWipe::phaseAt(1.80F) == P1ToiletWipe::EmptyPhase,
           "the left-edge band must transition into the blank phase");
    expect(!P1ToiletWipe::complete(1.89F) && P1ToiletWipe::complete(1.90F),
           "the verified wipe core must finish after its blank hold");
}

void testP1MarutchiMedicineKeepsItsObservedFramesAndTiming()
{
    constexpr std::array<std::string_view, 16> expectedFront{{
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
    constexpr std::array<std::string_view, 16> expectedSideA{{
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
    constexpr std::array<std::string_view, 16> expectedSideB{{
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

    MonochromeDisplay display;
    P1MedicineAnimation::render(display, 0U);
    expect(matchesRows(display, expectedFront),
        "the Marutchi Medicine front phase must retain every observed cell");
    P1MedicineAnimation::render(display, 1U);
    expect(matchesRows(display, expectedSideA),
        "the first Marutchi Medicine side phase must retain every observed cell");
    P1MedicineAnimation::render(display, 3U);
    expect(matchesRows(display, expectedSideB),
        "the second Marutchi Medicine side phase must retain every observed cell");
    P1MedicineAnimation::render(display, 5U);
    expect(matchesRows(display, expectedSideA),
        "the final side phase must repeat the independently observed first pose");

    expect(P1MedicineAnimation::phaseAt(0.0F) == 0U
            && P1MedicineAnimation::phaseAt(2.0F / 30.0F) == 1U
            && P1MedicineAnimation::phaseAt(4.0F / 30.0F) == 2U
            && P1MedicineAnimation::phaseAt(7.0F / 30.0F) == 3U,
        "Medicine phases must retain their observed two/three-frame boundaries");
    expect(!P1MedicineAnimation::complete(15.0F / 30.0F)
            && P1MedicineAnimation::complete(16.0F / 30.0F),
        "the observed seven-phase Medicine action must complete after sixteen frames");
}

void testP1LightMenuKeepsBothExactStableSelections()
{
    constexpr std::array<std::string_view, 16> expectedOn{{
        "................................",
        "...#......###....##...#.........",
        "...##....#...#...#.#..#.........",
        ".#####...#...#...#.#..#.........",
        ".######..#...#...#..#.#.........",
        ".#####...#...#...#..#.#.........",
        "...##.....###....#...##.........",
        "...#............................",
        "................................",
        "..........###..####.####........",
        ".........#...#.#....#...........",
        ".........#...#.###..###.........",
        ".........#...#.#....#...........",
        ".........#...#.#....#...........",
        "..........###..#....#...........",
        "................................",
    }};
    constexpr std::array<std::string_view, 16> expectedOff{{
        "................................",
        "..........###....##...#.........",
        ".........#...#...#.#..#.........",
        ".........#...#...#.#..#.........",
        ".........#...#...#..#.#.........",
        ".........#...#...#..#.#.........",
        "..........###....#...##.........",
        "................................",
        "................................",
        "...#......###..####.####........",
        "...##....#...#.#....#...........",
        ".#####...#...#.###..###.........",
        ".######..#...#.#....#...........",
        ".#####...#...#.#....#...........",
        "...##.....###..#....#...........",
        "...#............................",
    }};

    MonochromeDisplay display;
    P1LightScreen::renderMenu(display, false);
    expect(matchesRows(display, expectedOn) && litPixelCount(display) == 90,
           "the selected-ON Light menu must retain every observed P1 LCD cell");
    P1LightScreen::renderMenu(display, true);
    expect(matchesRows(display, expectedOff) && litPixelCount(display) == 90,
           "the selected-OFF Light menu must retain every observed P1 LCD cell");
}

void testP1LightsOutUsesTheShiftedInvertedSleepCycle()
{
    constexpr std::array<std::string_view, 6> small{{
        "....###", "......#", ".....#.", "....#..", "..#.###", "#......",
    }};
    constexpr std::array<std::string_view, 6> large{{
        "####", "...#", "..#.", ".#..", "#...", "####",
    }};
    const auto matchesInvertedSprite = [](const MonochromeDisplay& display,
                                          const int normalOriginX,
                                          const int originY,
                                          const std::span<const std::string_view> rows) {
        for (int y = 0; y < MonochromeDisplay::Height; ++y) {
            for (int x = 0; x < MonochromeDisplay::Width; ++x) {
                const int row = y - originY;
                const int column = x - normalOriginX + P1LightScreen::LightsOutSleepShiftX;
                const bool hole = row >= 0 && row < static_cast<int>(rows.size())
                    && column >= 0
                    && column < static_cast<int>(rows[static_cast<std::size_t>(row)].size())
                    && rows[static_cast<std::size_t>(row)]
                           [static_cast<std::size_t>(column)] == '#';
                if (display.pixel(x, y) != !hole) return false;
            }
        }
        return true;
    };

    MonochromeDisplay display;
    P1LightScreen::renderLightsOut(display, 24, 0, small);
    expect(litPixelCount(display) == 501
               && matchesInvertedSprite(display, 24, 0, small),
           "the small lights-out Z must be an exact eleven-cell hole at x=16");
    P1LightScreen::renderLightsOut(display, 25, 2, large);
    expect(litPixelCount(display) == 500
               && matchesInvertedSprite(display, 25, 2, large),
           "the large lights-out Z must be an exact twelve-cell hole at x=17");
}

} // namespace

int main()
{
    testPixelsAreBounded();
    testRectangleIsClipped();
    testSpriteBlitIsClipped();
    testTextUsesTheFramebufferFont();
    testPalettesRemainOneBit();
    testP1ToiletWipeMovesTheWholeFramebuffer();
    testP1ToiletWipeKeepsTheObservedWaterPattern();
    testP1ToiletWipeTimingIsDeterministic();
    testP1MarutchiMedicineKeepsItsObservedFramesAndTiming();
    testP1LightMenuKeepsBothExactStableSelections();
    testP1LightsOutUsesTheShiftedInvertedSleepCycle();

    if (failures == 0) {
        std::cout << "MonochromeDisplayTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
