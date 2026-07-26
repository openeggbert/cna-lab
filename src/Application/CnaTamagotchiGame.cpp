#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"
#include "CnaTamagotchi/Domain/CreatureCatalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
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
constexpr int DisplayPixelWidth = Display::MonochromeDisplay::Width * DisplayScale;
constexpr int DisplayPixelHeight = Display::MonochromeDisplay::Height * DisplayScale;
constexpr int DisplayX = (WindowWidth - DisplayPixelWidth) / 2;
constexpr int DisplayY = 302;

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

using Glyph = std::array<std::string_view, 5>;

constexpr Glyph Blank{{"000", "000", "000", "000", "000"}};
constexpr Glyph LetterA{{"010", "101", "111", "101", "101"}};
constexpr Glyph LetterC{{"011", "100", "100", "100", "011"}};
constexpr Glyph LetterD{{"110", "101", "101", "101", "110"}};
constexpr Glyph LetterE{{"111", "100", "110", "100", "111"}};
constexpr Glyph LetterF{{"111", "100", "110", "100", "100"}};
constexpr Glyph LetterG{{"011", "100", "101", "101", "011"}};
constexpr Glyph LetterI{{"111", "010", "010", "010", "111"}};
constexpr Glyph LetterL{{"100", "100", "100", "100", "111"}};
constexpr Glyph LetterM{{"101", "111", "111", "101", "101"}};
constexpr Glyph LetterO{{"010", "101", "101", "101", "010"}};
constexpr Glyph LetterR{{"110", "101", "110", "101", "101"}};
constexpr Glyph LetterS{{"011", "100", "010", "001", "110"}};
constexpr Glyph LetterT{{"111", "010", "010", "010", "010"}};
constexpr Glyph LetterW{{"101", "101", "101", "111", "101"}};
constexpr Glyph Digit0{{"111", "101", "101", "101", "111"}};
constexpr Glyph Digit1{{"010", "110", "010", "010", "111"}};
constexpr Glyph Digit2{{"110", "001", "010", "100", "111"}};
constexpr Glyph Digit3{{"110", "001", "010", "001", "110"}};
constexpr Glyph Digit4{{"101", "101", "111", "001", "001"}};
constexpr Glyph Digit5{{"111", "100", "110", "001", "110"}};
constexpr Glyph Digit6{{"011", "100", "111", "101", "111"}};
constexpr Glyph Digit7{{"111", "001", "010", "010", "010"}};
constexpr Glyph Digit8{{"111", "101", "111", "101", "111"}};
constexpr Glyph Digit9{{"111", "101", "111", "001", "110"}};

const Glyph& glyphFor(const char character) noexcept
{
    switch (character) {
    case 'A': return LetterA;
    case 'C': return LetterC;
    case 'D': return LetterD;
    case 'E': return LetterE;
    case 'F': return LetterF;
    case 'G': return LetterG;
    case 'I': return LetterI;
    case 'L': return LetterL;
    case 'M': return LetterM;
    case 'O': return LetterO;
    case 'R': return LetterR;
    case 'S': return LetterS;
    case 'T': return LetterT;
    case 'W': return LetterW;
    case '0': return Digit0;
    case '1': return Digit1;
    case '2': return Digit2;
    case '3': return Digit3;
    case '4': return Digit4;
    case '5': return Digit5;
    case '6': return Digit6;
    case '7': return Digit7;
    case '8': return Digit8;
    case '9': return Digit9;
    default: return Blank;
    }
}

void drawText(Display::MonochromeDisplay& display, const int firstX, const int firstY,
              const std::string_view text) noexcept
{
    constexpr int GlyphWidth = 3;
    constexpr int GlyphHeight = 5;
    constexpr int GlyphAdvance = GlyphWidth + 1;
    for (int index = 0; index < static_cast<int>(text.size()); ++index) {
        const Glyph& glyph = glyphFor(text[static_cast<std::size_t>(index)]);
        for (int y = 0; y < GlyphHeight; ++y) {
            for (int x = 0; x < GlyphWidth; ++x) {
                if (glyph[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '1') {
                    display.setPixel(firstX + index * GlyphAdvance + x, firstY + y, true);
                }
            }
        }
    }
}

} // namespace

CnaTamagotchiGame::CnaTamagotchiGame(const bool smokeTest)
    : graphics_(this),
      smokeTest_(smokeTest)
{
    graphics_.setPreferredBackBufferWidthProperty(WindowWidth);
    graphics_.setPreferredBackBufferHeightProperty(WindowHeight);
    Game::getWindowProperty().setTitleProperty("CNA Tamagotchi");
    refreshDisplay();
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
    const float elapsedSeconds = static_cast<float>(elapsedMilliseconds) / 1000.0F;
    backgroundTimeSeconds_ += elapsedSeconds;

    const bool selectNext = keyboard.IsKeyDown(Keys::A) || keyboard.IsKeyDown(Keys::Right);
    const bool selectPrevious = keyboard.IsKeyDown(Keys::Left);
    const bool confirm = keyboard.IsKeyDown(Keys::B) || keyboard.IsKeyDown(Keys::Enter)
        || keyboard.IsKeyDown(Keys::Space);
    const bool cancel = keyboard.IsKeyDown(Keys::C) || keyboard.IsKeyDown(Keys::Back);

    if (selectNext && !selectNextWasDown_) {
        if (screen_ == Screen::Home) {
            selectedIcon_ = (selectedIcon_ + 1) % static_cast<int>(IconPositions.size());
        } else if (screen_ == Screen::Food) {
            foodSelection_ = (foodSelection_ + 1) % 2;
        }
    }
    if (selectPrevious && !selectPreviousWasDown_) {
        if (screen_ == Screen::Home) {
            selectedIcon_ = (selectedIcon_ + static_cast<int>(IconPositions.size()) - 1)
                % static_cast<int>(IconPositions.size());
        } else if (screen_ == Screen::Food) {
            foodSelection_ = (foodSelection_ + 1) % 2;
        }
    }
    if (confirm && !confirmWasDown_) {
        if (screen_ == Screen::Food) {
            simulation_.applyAction(pet_, foodSelection_ == 0
                ? Domain::PetAction::Meal : Domain::PetAction::Snack);
            screen_ = Screen::Home;
        } else if (screen_ == Screen::Status) {
            statusPage_ = (statusPage_ + 1) % 2;
        } else if (selectedIcon_ == 0) {
            screen_ = Screen::Food;
        } else if (selectedIcon_ == 5) {
            screen_ = Screen::Status;
        } else {
            constexpr std::array<std::optional<Domain::PetAction>, 8> iconActions{{
                std::nullopt, // food opens its own two-choice menu
                Domain::PetAction::ToggleLight,
                Domain::PetAction::Play,
                Domain::PetAction::Medicine,
                Domain::PetAction::Clean,
                std::nullopt, // status opens its own display
                Domain::PetAction::Discipline,
                std::nullopt, // attention is an automatic indicator
            }};
            const std::optional<Domain::PetAction> action =
                iconActions[static_cast<std::size_t>(selectedIcon_)];
            if (action.has_value()) {
                simulation_.applyAction(pet_, *action);
            }
        }
    }
    if (cancel && !cancelWasDown_) {
        if (screen_ != Screen::Home) {
            screen_ = Screen::Home;
        }
    }

    selectNextWasDown_ = selectNext;
    selectPreviousWasDown_ = selectPrevious;
    confirmWasDown_ = confirm;
    cancelWasDown_ = cancel;

    simulationSeconds_ += elapsedSeconds;
    while (simulationSeconds_ >= 60.0F) {
        static_cast<void>(simulation_.advance(pet_, 1));
        simulationSeconds_ -= 60.0F;
    }
    refreshDisplay();
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

void CnaTamagotchiGame::refreshDisplay() noexcept
{
    display_.clear();

    if (screen_ == Screen::Food) {
        drawText(display_, 8, 0, "FOOD");
        drawText(display_, 6, 6, "MEAL");
        drawText(display_, 6, 11, "SNACK");
        const int markerY = foodSelection_ == 0 ? 7 : 12;
        display_.setPixel(1, markerY, true);
        display_.setPixel(2, markerY + 1, true);
        display_.setPixel(1, markerY + 2, true);
        return;
    }

    if (screen_ == Screen::Status) {
        if (statusPage_ == 0) {
            const std::string age = "AGE" + std::to_string(pet_.ageMinutes / (24 * 60));
            const std::string weight = "WGT" + std::to_string(pet_.weight);
            drawText(display_, 2, 1, age);
            drawText(display_, 2, 9, weight);
        } else {
            const std::string mistakes = "CARE" + std::to_string(pet_.careMistakes);
            const std::string discipline = "DISC" + std::to_string(
                std::clamp((pet_.needs.discipline + 24) / 25, 0, 4));
            drawText(display_, 2, 1, mistakes);
            drawText(display_, 2, 9, discipline);
        }
        return;
    }

    const auto drawHeartMeter = [this](const int firstX, const int value) {
        const int filled = std::clamp((value + 24) / 25, 0, 4);
        for (int heart = 0; heart < filled; ++heart) {
            const int x = firstX + heart * 2;
            display_.setPixel(x, 0, true);
            display_.setPixel(x + 1, 0, true);
            display_.setPixel(x, 1, true);
            display_.setPixel(x + 1, 1, true);
        }
    };
    drawHeartMeter(0, pet_.needs.hunger);
    drawHeartMeter(12, pet_.needs.happiness);
    drawHeartMeter(24, pet_.needs.discipline);

    const Domain::CreatureForm form = Domain::CreatureCatalog::formFor(pet_);
    const Domain::CreatureSprite& sprite = Domain::CreatureCatalog::spriteFor(form);
    for (int y = 0; y < static_cast<int>(sprite.rows.size()); ++y) {
        for (int x = 0; x < static_cast<int>(sprite.rows[static_cast<std::size_t>(y)].size()); ++x) {
            if (sprite.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '#') {
                display_.setPixel(11 + x, 4 + y, true);
            }
        }
    }

    // Tiny floor and two stars keep the first 1-bit screen immediately legible.
    for (int x = 5; x < 27; ++x) {
        display_.setPixel(x, 14, true);
    }
    display_.setPixel(5, 4, true);
    display_.setPixel(4, 4, true);
    display_.setPixel(5, 3, true);
    display_.setPixel(27, 3, true);
    display_.setPixel(28, 3, true);
    display_.setPixel(28, 4, true);

    if (pet_.asleep) {
        display_.setPixel(27, 6, true);
        display_.setPixel(28, 6, true);
        display_.setPixel(28, 7, true);
    }

    if (pet_.attentionReason != Domain::AttentionReason::None) {
        display_.setPixel(29, 3, true);
        display_.setPixel(30, 3, true);
        display_.setPixel(29, 4, true);
        display_.setPixel(30, 4, true);
    }
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

    // The recessed LCD is exactly 32 × 16 logical pixels at 8× scale.
    drawRect(Rectangle(DisplayX - 12, DisplayY - 12,
        DisplayPixelWidth + 24, DisplayPixelHeight + 24), LcdBezel);
    drawRect(Rectangle(DisplayX - 6, DisplayY - 6,
        DisplayPixelWidth + 12, DisplayPixelHeight + 12), ShellShadow);
    drawRect(Rectangle(DisplayX, DisplayY, DisplayPixelWidth, DisplayPixelHeight), LcdOff);

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
        const bool selected = static_cast<int>(index) == selectedIcon_;
        if (selected) {
            drawEllipse(position.x, position.y, 18, 18, ShellHighlight);
        }
        const Color icon = selected ? Ink : ShellShadow;
        const Color cutout = ShellMain;

        switch (index) {
        case 0: // bowl
            drawEllipse(position.x, position.y + 3, 13, 7, icon);
            drawRect(Rectangle(position.x - 15, position.y - 5, 30, 4), icon);
            drawRect(Rectangle(position.x - 10, position.y - 1, 20, 4), cutout);
            break;
        case 1: // moon / light
            drawEllipse(position.x, position.y, 12, 12, icon);
            drawEllipse(position.x + 5, position.y - 4, 11, 11, cutout);
            break;
        case 2: // ball / game
            drawRing(position.x, position.y, 12, icon, cutout);
            drawRect(Rectangle(position.x - 2, position.y - 12, 4, 24), icon);
            break;
        case 3: // health cross / medicine
            drawRect(Rectangle(position.x - 4, position.y - 13, 8, 26), icon);
            drawRect(Rectangle(position.x - 13, position.y - 4, 26, 8), icon);
            break;
        case 4: // cleaning droplet
            drawEllipse(position.x, position.y + 4, 9, 12, icon);
            drawRect(Rectangle(position.x - 3, position.y - 12, 6, 19), icon);
            break;
        case 5: // heart / status
            drawEllipse(position.x - 6, position.y - 4, 7, 7, icon);
            drawEllipse(position.x + 6, position.y - 4, 7, 7, icon);
            drawRect(Rectangle(position.x - 9, position.y - 3, 18, 12), icon);
            drawEllipse(position.x, position.y + 8, 5, 5, icon);
            break;
        case 6: // bell / discipline
            drawEllipse(position.x, position.y + 5, 12, 9, icon);
            drawRect(Rectangle(position.x - 10, position.y - 5, 20, 12), icon);
            drawEllipse(position.x, position.y + 13, 3, 3, icon);
            break;
        case 7: // attention marker
            drawRect(Rectangle(position.x - 3, position.y - 14, 6, 28), icon);
            drawRect(Rectangle(position.x - 14, position.y - 3, 28, 6), icon);
            drawRect(Rectangle(position.x - 9, position.y - 9, 18, 18), icon);
            break;
        default:
            break;
        }
    }

    // Three physical controls: A changes selection, B confirms, C clears it.
    for (const int x : {202, 270, 338}) {
        drawEllipse(x, 605, 25, 25, ShellOutline);
        drawEllipse(x, 601, 20, 20, ShellHighlight);
        drawEllipse(x - 3, 597, 8, 8, Color(255, 238, 244, 255));
    }
}

GetTypeNameCPP(CnaTamagotchiGame, "CnaTamagotchiGame")

} // namespace CnaTamagotchi::Application
