#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Input;

namespace CnaTamagotchi::Application {
namespace {

constexpr int WindowWidth = 540;
constexpr int WindowHeight = 760;
constexpr float BackgroundCycleSeconds = 32.0F;
constexpr int DisplayScale = 8;
constexpr int DisplayX = (WindowWidth - Display::MonochromeDisplay::Width * DisplayScale) / 2;
constexpr int DisplayY = 268;

struct Rgb final {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

const Color ShellOutline(151, 87, 119, 255);
const Color ShellMain(242, 174, 199, 255);
const Color ShellHighlight(255, 216, 228, 255);
const Color ShellShadow(194, 116, 151, 255);
const Color Ink(69, 55, 62, 255);
const Color LcdBezel(77, 91, 62, 255);
const Color LcdOff(188, 202, 143, 255);
const Color LcdOn(34, 44, 31, 255);

struct IconPosition final {
    int x;
    int y;
};

constexpr std::array<IconPosition, 8> IconPositions{{
    {151, 225}, {230, 209}, {310, 209}, {389, 225},
    {151, 510}, {230, 526}, {310, 526}, {389, 510},
}};

} // namespace

CnaTamagotchiGame::CnaTamagotchiGame(const bool smokeTest)
    : graphics_(this),
      smokeTest_(smokeTest)
{
    graphics_.setPreferredBackBufferWidthProperty(WindowWidth);
    graphics_.setPreferredBackBufferHeightProperty(WindowHeight);
    Game::getWindowProperty().setTitleProperty("CNA Tamagotchi");
    seedDemoDisplay();
}

void CnaTamagotchiGame::LoadContent()
{
    spriteBatch_ = std::make_unique<SpriteBatch>(getGraphicsDeviceProperty());
    pixelTexture_.emplace(getGraphicsDeviceProperty(), 1, 1);
    const Color white(255, 255, 255, 255);
    pixelTexture_->SetData(&white, 1);
}

void CnaTamagotchiGame::Update(GameTime& gameTime)
{
    const KeyboardState keyboard = Keyboard::GetState();
    if (keyboard.IsKeyDown(Keys::Escape)) {
        Exit();
        return;
    }

    const auto elapsedMilliseconds =
        gameTime.getElapsedGameTimeProperty().getTotalMillisecondsProperty();
    backgroundTimeSeconds_ += static_cast<float>(elapsedMilliseconds) / 1000.0F;
}

void CnaTamagotchiGame::Draw(const GameTime& gameTime)
{
    (void)gameTime;
    getGraphicsDeviceProperty().Clear(backgroundColor());

    spriteBatch_->Begin();
    drawDevice();
    spriteBatch_->End();

    if (smokeTest_ && ++drawnFrames_ >= 3U) {
        Exit();
    }
}

Color CnaTamagotchiGame::backgroundColor() const
{
    constexpr std::array<Rgb, 4> colours{{
        {255U, 249U, 238U}, // ivory
        {255U, 240U, 223U}, // pale peach
        {255U, 246U, 223U}, // warm cream
        {253U, 235U, 206U}, // misty apricot
    }};

    const float cyclePosition = std::fmod(backgroundTimeSeconds_, BackgroundCycleSeconds)
        / BackgroundCycleSeconds * static_cast<float>(colours.size());
    const auto current = static_cast<std::size_t>(cyclePosition) % colours.size();
    const auto next = (current + 1U) % colours.size();
    const float progress = cyclePosition - static_cast<float>(static_cast<unsigned int>(cyclePosition));
    const float eased = progress * progress * (3.0F - 2.0F * progress);

    const auto blend = [eased](const std::uint8_t from, const std::uint8_t to) {
        return static_cast<std::uint8_t>(std::lerp(
            static_cast<float>(from), static_cast<float>(to), eased));
    };

    return Color(
        blend(colours[current].red, colours[next].red),
        blend(colours[current].green, colours[next].green),
        blend(colours[current].blue, colours[next].blue),
        255U);
}

void CnaTamagotchiGame::seedDemoDisplay() noexcept
{
    display_.clear();

    constexpr std::array<std::string_view, 11> pet{{
        "    ##    ",
        "  ######  ",
        " ######## ",
        "## ## ## ##",
        "##########",
        "## #### ##",
        "##########",
        "  ##  ##  ",
        "  ##  ##  ",
        " ##    ## ",
        "##      ##",
    }};

    for (int y = 0; y < static_cast<int>(pet.size()); ++y) {
        for (int x = 0; x < static_cast<int>(pet[static_cast<std::size_t>(y)].size()); ++x) {
            if (pet[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '#') {
                display_.setPixel(11 + x, 6 + y, true);
            }
        }
    }

    // Tiny floor and two stars make the first 1-bit screen immediately legible.
    for (int x = 5; x < 27; ++x) {
        display_.setPixel(x, 20, true);
    }
    display_.setPixel(5, 4, true);
    display_.setPixel(4, 4, true);
    display_.setPixel(5, 3, true);
    display_.setPixel(27, 3, true);
    display_.setPixel(28, 3, true);
    display_.setPixel(28, 4, true);
}

void CnaTamagotchiGame::drawDevice()
{
    const auto drawRect = [this](const Rectangle& rectangle, const Color color) {
        spriteBatch_->Draw(*pixelTexture_, rectangle, color);
    };

    const auto drawEllipse = [&drawRect](const int centreX, const int centreY,
                                        const int radiusX, const int radiusY,
                                        const Color colour) {
        for (int y = -radiusY; y <= radiusY; ++y) {
            const float normalizedY = static_cast<float>(y) / static_cast<float>(radiusY);
            const int halfWidth = static_cast<int>(std::sqrt(std::max(
                0.0F, 1.0F - normalizedY * normalizedY)) * static_cast<float>(radiusX));
            drawRect(Rectangle(centreX - halfWidth, centreY + y, halfWidth * 2 + 1, 1), colour);
        }
    };

    const auto drawRing = [&drawEllipse](const int centreX, const int centreY,
                                        const int radius, const Color outer, const Color inner) {
        drawEllipse(centreX, centreY, radius, radius, outer);
        drawEllipse(centreX, centreY, radius - 3, radius - 3, inner);
    };

    // Drop shadow, shell rim, and inner egg. The geometry is original rather
    // than reproducing a specific commercial shell pattern.
    drawEllipse(278, 684, 151, 15, Color(178, 129, 111, 44));
    drawEllipse(270, 392, 204, 310, ShellOutline);
    drawEllipse(270, 388, 193, 299, ShellMain);
    drawEllipse(238, 340, 128, 223, ShellHighlight);

    // Small keychain tab: recognisable, but deliberately generic.
    drawRing(270, 83, 20, ShellOutline, backgroundColor());
    drawRing(270, 83, 14, ShellMain, backgroundColor());

    // The recessed LCD is exactly 32 × 24 logical pixels at 8× scale.
    drawRect(Rectangle(DisplayX - 12, DisplayY - 12, 280, 216), LcdBezel);
    drawRect(Rectangle(DisplayX - 6, DisplayY - 6, 268, 204), ShellShadow);
    drawRect(Rectangle(DisplayX, DisplayY, 256, 192), LcdOff);

    for (int y = 0; y < Display::MonochromeDisplay::Height; ++y) {
        for (int x = 0; x < Display::MonochromeDisplay::Width; ++x) {
            if (display_.pixel(x, y)) {
                drawRect(Rectangle(
                    DisplayX + x * DisplayScale,
                    DisplayY + y * DisplayScale,
                    DisplayScale,
                    DisplayScale),
                    LcdOn);
            }
        }
    }

    // Eight care symbols in two shell bands. The first bowl is active.
    for (std::size_t index = 0; index < IconPositions.size(); ++index) {
        const IconPosition position = IconPositions[index];
        const Color icon = index == 0U ? Ink : ShellShadow;
        const Color cutout = ShellMain;

        switch (index) {
        case 0: // bowl
            drawEllipse(position.x, position.y + 3, 13, 7, icon);
            drawRect(Rectangle(position.x - 15, position.y - 5, 30, 4), icon);
            drawRect(Rectangle(position.x - 10, position.y - 1, 20, 4), cutout);
            break;
        case 1: // ball
            drawRing(position.x, position.y, 12, icon, cutout);
            drawRect(Rectangle(position.x - 2, position.y - 12, 4, 24), icon);
            break;
        case 2: // moon
            drawEllipse(position.x, position.y, 12, 12, icon);
            drawEllipse(position.x + 5, position.y - 4, 11, 11, cutout);
            break;
        case 3: // bell
            drawEllipse(position.x, position.y + 5, 12, 9, icon);
            drawRect(Rectangle(position.x - 10, position.y - 5, 20, 12), icon);
            drawEllipse(position.x, position.y + 13, 3, 3, icon);
            break;
        case 4: // cleaning droplet
            drawEllipse(position.x, position.y + 4, 9, 12, icon);
            drawRect(Rectangle(position.x - 3, position.y - 12, 6, 19), icon);
            break;
        case 5: // health cross
            drawRect(Rectangle(position.x - 4, position.y - 13, 8, 26), icon);
            drawRect(Rectangle(position.x - 13, position.y - 4, 26, 8), icon);
            break;
        case 6: // heart/status
            drawEllipse(position.x - 6, position.y - 4, 7, 7, icon);
            drawEllipse(position.x + 6, position.y - 4, 7, 7, icon);
            drawRect(Rectangle(position.x - 9, position.y - 3, 18, 12), icon);
            drawEllipse(position.x, position.y + 8, 5, 5, icon);
            break;
        case 7: // memory star
            drawRect(Rectangle(position.x - 3, position.y - 14, 6, 28), icon);
            drawRect(Rectangle(position.x - 14, position.y - 3, 28, 6), icon);
            drawRect(Rectangle(position.x - 9, position.y - 9, 18, 18), icon);
            break;
        default:
            break;
        }
    }

    // Three physical controls. Their functions will be wired in the care-loop milestone.
    for (const int x : {202, 270, 338}) {
        drawEllipse(x, 605, 25, 25, ShellOutline);
        drawEllipse(x, 601, 20, 20, ShellHighlight);
        drawEllipse(x - 3, 597, 8, 8, Color(255, 238, 244, 255));
    }
}

GetTypeNameCPP(CnaTamagotchiGame, "CnaTamagotchiGame")

} // namespace CnaTamagotchi::Application
