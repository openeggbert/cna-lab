#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"

#include <array>
#include <iostream>

using CnaTamagotchi::Display::LcdPalette;
using CnaTamagotchi::Display::MonochromeDisplay;

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

} // namespace

int main()
{
    testPixelsAreBounded();
    testRectangleIsClipped();
    testSpriteBlitIsClipped();
    testTextUsesTheFramebufferFont();
    testPalettesRemainOneBit();

    if (failures == 0) {
        std::cout << "MonochromeDisplayTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
