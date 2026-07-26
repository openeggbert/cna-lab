#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"
#include "CnaTamagotchi/Domain/CreatureCatalog.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchPanel.hpp"

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

struct ButtonPosition final {
    int x;
};

constexpr std::array<IconPosition, 8> IconPositions{{
    {151, 225}, {230, 209}, {310, 209}, {389, 225},
    {151, 510}, {230, 526}, {310, 526}, {389, 510},
}};

constexpr int ButtonY = 601;
constexpr int ButtonHitRadius = 29;
constexpr std::array<ButtonPosition, 3> ButtonPositions{{
    {202}, {270}, {338},
}};

std::int64_t unixSecondsNow() noexcept
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::filesystem::path defaultSavePath()
{
    return std::filesystem::current_path() / "saves" / "slot-1.json";
}

using Glyph = std::array<std::string_view, 5>;

constexpr Glyph Blank{{"000", "000", "000", "000", "000"}};
constexpr Glyph LetterA{{"010", "101", "111", "101", "101"}};
constexpr Glyph LetterC{{"011", "100", "100", "100", "011"}};
constexpr Glyph LetterD{{"110", "101", "101", "101", "110"}};
constexpr Glyph LetterE{{"111", "100", "110", "100", "111"}};
constexpr Glyph LetterF{{"111", "100", "110", "100", "100"}};
constexpr Glyph LetterG{{"011", "100", "101", "101", "011"}};
constexpr Glyph LetterH{{"101", "101", "111", "101", "101"}};
constexpr Glyph LetterI{{"111", "010", "010", "010", "111"}};
constexpr Glyph LetterL{{"100", "100", "100", "100", "111"}};
constexpr Glyph LetterM{{"101", "111", "111", "101", "101"}};
constexpr Glyph LetterO{{"010", "101", "101", "101", "010"}};
constexpr Glyph LetterP{{"110", "101", "110", "100", "100"}};
constexpr Glyph LetterR{{"110", "101", "110", "101", "101"}};
constexpr Glyph LetterS{{"011", "100", "010", "001", "110"}};
constexpr Glyph LetterT{{"111", "010", "010", "010", "010"}};
constexpr Glyph LetterU{{"101", "101", "101", "101", "111"}};
constexpr Glyph LetterW{{"101", "101", "101", "111", "101"}};
constexpr Glyph LetterK{{"101", "101", "110", "101", "101"}};
constexpr Glyph LetterN{{"101", "111", "111", "111", "101"}};
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
    case 'H': return LetterH;
    case 'I': return LetterI;
    case 'L': return LetterL;
    case 'M': return LetterM;
    case 'O': return LetterO;
    case 'P': return LetterP;
    case 'R': return LetterR;
    case 'S': return LetterS;
    case 'T': return LetterT;
    case 'U': return LetterU;
    case 'W': return LetterW;
    case 'K': return LetterK;
    case 'N': return LetterN;
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
    if (!smokeTest_) {
        savePath_ = defaultSavePath();
        loadSave();
        saveNow();
    }
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
    const auto elapsedMilliseconds =
        gameTime.getElapsedGameTimeProperty().getTotalMillisecondsProperty();
    const float elapsedSeconds = static_cast<float>(elapsedMilliseconds) / 1000.0F;
    backgroundTimeSeconds_ += elapsedSeconds;

    const bool selectNext = keyboard.IsKeyDown(Keys::A) || keyboard.IsKeyDown(Keys::Right);
    const bool selectPrevious = keyboard.IsKeyDown(Keys::Left);
    const bool confirm = keyboard.IsKeyDown(Keys::B) || keyboard.IsKeyDown(Keys::Enter)
        || keyboard.IsKeyDown(Keys::Space);
    const bool cancel = keyboard.IsKeyDown(Keys::C) || keyboard.IsKeyDown(Keys::Back)
        || keyboard.IsKeyDown(Keys::Escape);

    bool saveChanged = false;
    if (selectNext && !selectNextWasDown_) {
        saveChanged = pressButton(DeviceButton::A);
    }
    if (selectPrevious && !selectPreviousWasDown_) {
        moveSelectionBackward();
    }
    if (confirm && !confirmWasDown_) {
        saveChanged = pressButton(DeviceButton::B) || saveChanged;
    }
    if (cancel && !cancelWasDown_) {
        saveChanged = pressButton(DeviceButton::C) || saveChanged;
    }

    std::optional<DeviceButton> pointerButton;
    const Touch::TouchCollection touches = Touch::TouchPanel::GetState();
    for (int index = 0; index < touches.getCountProperty(); ++index) {
        const Touch::TouchLocation& touch = touches[static_cast<std::size_t>(index)];
        if (touch.getStateProperty() == Touch::TouchLocationState::Pressed) {
            const Vector2& position = touch.getPositionProperty();
            pointerButton = buttonAtWindowPosition(position.X, position.Y);
            if (pointerButton.has_value()) {
                break;
            }
        }
    }

    const MouseState mouse = Mouse::GetState();
    const bool mouseLeftDown = mouse.getLeftButtonProperty() == ButtonState::Pressed;
    if (!pointerButton.has_value() && mouseLeftDown && !mouseLeftWasDown_) {
        pointerButton = buttonAtWindowPosition(
            static_cast<float>(mouse.getXProperty()), static_cast<float>(mouse.getYProperty()));
    }
    if (pointerButton.has_value()) {
        saveChanged = pressButton(*pointerButton) || saveChanged;
    }

    selectNextWasDown_ = selectNext;
    selectPreviousWasDown_ = selectPrevious;
    confirmWasDown_ = confirm;
    cancelWasDown_ = cancel;
    mouseLeftWasDown_ = mouseLeftDown;

    simulationSeconds_ += elapsedSeconds;
    while (simulationSeconds_ >= 60.0F) {
        static_cast<void>(simulation_.advance(pet_, 1));
        simulationSeconds_ -= 60.0F;
        lastSavedUnixSeconds_ += 60;
        saveChanged = true;
    }
    if (saveChanged) {
        saveNow();
    }
    refreshDisplay();
}

bool CnaTamagotchiGame::pressButton(const DeviceButton button)
{
    if (pet_.lifeStage == Domain::LifeStage::Farewell) {
        if (button == DeviceButton::B) {
            startNewEgg();
            return true;
        }
        return false;
    }

    switch (button) {
    case DeviceButton::A:
        if (screen_ == Screen::Home) {
            selectedIcon_ = (selectedIcon_ + 1) % static_cast<int>(IconPositions.size());
        } else if (screen_ == Screen::Food) {
            foodSelection_ = (foodSelection_ + 1) % 2;
        } else if ((screen_ == Screen::Game || screen_ == Screen::NumberGame) && !gameResolved_) {
            gameChoice_ = (gameChoice_ + 1) % 2;
        }
        return false;
    case DeviceButton::B:
        if (screen_ == Screen::Food) {
            simulation_.applyAction(pet_, foodSelection_ == 0
                ? Domain::PetAction::Meal : Domain::PetAction::Snack);
            screen_ = Screen::Home;
            return true;
        }
        if (screen_ == Screen::Status) {
            statusPage_ = (statusPage_ + 1) % 3;
            return false;
        }
        if (screen_ == Screen::Game || screen_ == Screen::NumberGame) {
            if (gameResolved_) {
                screen_ = Screen::Home;
                return false;
            }
            gameWon_ = screen_ == Screen::Game
                ? gameChoice_ == gameTarget_
                : gameChoice_ == (nextNumber_ > currentNumber_ ? 1 : 0);
            gameResolved_ = true;
            if (gameWon_) {
                simulation_.applyAction(pet_, Domain::PetAction::Play);
            }
            return true;
        }
        if (selectedIcon_ == 0) {
            screen_ = Screen::Food;
            return false;
        }
        if (selectedIcon_ == 5) {
            screen_ = Screen::Status;
            return false;
        }
        if (selectedIcon_ == 2) {
            if (!pet_.asleep) {
                if (pet_.species == Domain::PetSpecies::Mossling) {
                    startNumberGame();
                    screen_ = Screen::NumberGame;
                } else {
                    startPeekGame();
                    screen_ = Screen::Game;
                }
                return true;
            }
            return false;
        }
        {
            constexpr std::array<std::optional<Domain::PetAction>, 8> iconActions{{
                std::nullopt, // food opens its own two-choice menu
                Domain::PetAction::ToggleLight,
                std::nullopt, // game opens its own choice screen
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
                return true;
            }
        }
        return false;
    case DeviceButton::C:
        if (screen_ != Screen::Home) {
            screen_ = Screen::Home;
        }
        return false;
    }

    return false;
}

std::optional<CnaTamagotchiGame::DeviceButton>
CnaTamagotchiGame::buttonAtWindowPosition(const float x, const float y) const noexcept
{
    const Rectangle clientBounds = getWindowProperty().getClientBoundsProperty();
    if (clientBounds.Width <= 0 || clientBounds.Height <= 0) {
        return std::nullopt;
    }

    const float deviceX = x * static_cast<float>(WindowWidth)
        / static_cast<float>(clientBounds.Width);
    const float deviceY = y * static_cast<float>(WindowHeight)
        / static_cast<float>(clientBounds.Height);
    for (std::size_t index = 0; index < ButtonPositions.size(); ++index) {
        const float deltaX = deviceX - static_cast<float>(ButtonPositions[index].x);
        const float deltaY = deviceY - static_cast<float>(ButtonY);
        if (deltaX * deltaX + deltaY * deltaY
            <= static_cast<float>(ButtonHitRadius * ButtonHitRadius)) {
            return static_cast<DeviceButton>(index);
        }
    }
    return std::nullopt;
}

void CnaTamagotchiGame::moveSelectionBackward() noexcept
{
    if (screen_ == Screen::Home) {
        selectedIcon_ = (selectedIcon_ + static_cast<int>(IconPositions.size()) - 1)
            % static_cast<int>(IconPositions.size());
    } else if (screen_ == Screen::Food) {
        foodSelection_ = (foodSelection_ + 1) % 2;
    } else if ((screen_ == Screen::Game || screen_ == Screen::NumberGame) && !gameResolved_) {
        gameChoice_ = (gameChoice_ + 1) % 2;
    }
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

void CnaTamagotchiGame::loadSave()
{
    const Persistence::LoadResult loaded = saveRepository_.load(savePath_);
    if (!loaded.data) {
        lastSavedUnixSeconds_ = unixSecondsNow();
        seed_ = static_cast<std::uint64_t>(lastSavedUnixSeconds_);
        return;
    }

    pet_ = loaded.data->pet;
    seed_ = loaded.data->seed;
    lastSavedUnixSeconds_ = loaded.data->lastSavedUnixSeconds;

    const std::int64_t now = unixSecondsNow();
    if (now <= lastSavedUnixSeconds_) {
        return;
    }

    const std::int64_t elapsedSeconds = now - lastSavedUnixSeconds_;
    const std::int64_t elapsedMinutes = elapsedSeconds / 60;
    const int appliedMinutes = static_cast<int>(std::min(
        elapsedMinutes, static_cast<std::int64_t>(std::numeric_limits<int>::max())));
    const Domain::SimulationReport report = simulation_.advance(pet_, appliedMinutes);
    if (report.wasClamped) {
        // The development safeguard deliberately discards excessive offline
        // time instead of letting repeated launches consume it in chunks.
        lastSavedUnixSeconds_ = now;
        simulationSeconds_ = 0.0F;
        return;
    }

    // Keep sub-minute time in the saved timestamp. Without this, repeatedly
    // opening and closing the application every few seconds would prevent a
    // full simulated minute from ever being reached.
    lastSavedUnixSeconds_ += static_cast<std::int64_t>(report.appliedMinutes) * 60;
    simulationSeconds_ = static_cast<float>(elapsedSeconds % 60);
}

void CnaTamagotchiGame::saveNow()
{
    if (smokeTest_) {
        return;
    }

    // This is an elapsed-time anchor, rather than merely the instant of the
    // file write. It never moves backwards, and it retains sub-minute time.
    const Persistence::SaveData data{
        .lastSavedUnixSeconds = lastSavedUnixSeconds_,
        .seed = seed_,
        .pet = pet_,
    };
    static_cast<void>(saveRepository_.save(savePath_, data));
}

void CnaTamagotchiGame::startPeekGame() noexcept
{
    // The seed is persisted, so the sequence remains stable across restarts
    // without relying on a platform random-number API.
    seed_ = seed_ * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
    gameTarget_ = static_cast<int>((seed_ >> 63U) & 1U);
    gameChoice_ = 0;
    gameResolved_ = false;
    gameWon_ = false;
}

void CnaTamagotchiGame::startNumberGame() noexcept
{
    seed_ = seed_ * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
    currentNumber_ = static_cast<int>(seed_ % 9U) + 1;
    seed_ = seed_ * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
    nextNumber_ = static_cast<int>(seed_ % 9U) + 1;
    if (nextNumber_ == currentNumber_) {
        nextNumber_ = currentNumber_ == 9 ? 8 : currentNumber_ + 1;
    }
    gameChoice_ = 0; // lower
    gameResolved_ = false;
    gameWon_ = false;
}

void CnaTamagotchiGame::startNewEgg() noexcept
{
    // A new egg begins a fresh deterministic generation while retaining no
    // accidental needs, illness, or care mistakes from the departed pet.
    seed_ = seed_ * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
    pet_ = Domain::PetState{};
    pet_.species = (seed_ & 1U) == 0U ? Domain::PetSpecies::Puffin
                                      : Domain::PetSpecies::Mossling;
    screen_ = Screen::Home;
    selectedIcon_ = 0;
    foodSelection_ = 0;
    statusPage_ = 0;
    gameChoice_ = 0;
    gameTarget_ = 0;
    currentNumber_ = 0;
    nextNumber_ = 0;
    gameResolved_ = false;
    gameWon_ = false;
    lastSavedUnixSeconds_ = unixSecondsNow();
    simulationSeconds_ = 0.0F;
}

void CnaTamagotchiGame::refreshDisplay() noexcept
{
    display_.clear();

    const auto drawHeartMeter = [this](const int firstX, const int firstY, const int value) {
        const int filled = std::clamp((value + 24) / 25, 0, 4);
        for (int heart = 0; heart < filled; ++heart) {
            const int x = firstX + heart * 2;
            display_.setPixel(x, firstY, true);
            display_.setPixel(x + 1, firstY, true);
            display_.setPixel(x, firstY + 1, true);
            display_.setPixel(x + 1, firstY + 1, true);
        }
    };

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
        } else if (statusPage_ == 1) {
            drawText(display_, 8, 0, "HUNG");
            drawHeartMeter(12, 6, pet_.needs.hunger);
            drawText(display_, 8, 8, "HAPP");
            drawHeartMeter(12, 14, pet_.needs.happiness);
        } else {
            drawText(display_, 8, 0, "DISC");
            drawHeartMeter(12, 6, pet_.needs.discipline);
            const std::string mistakes = "MIST" + std::to_string(pet_.careMistakes);
            drawText(display_, 2, 9, mistakes);
        }
        return;
    }

    if (screen_ == Screen::Game) {
        if (gameResolved_) {
            drawText(display_, gameWon_ ? 10 : 8, 5, gameWon_ ? "WIN" : "LOSE");
            return;
        }

        drawText(display_, 8, 0, "PICK");
        // Two deliberately abstract peek positions; the selected position is
        // marked below it, rather than using a modern text button.
        for (const int x : {8, 23}) {
            display_.setPixel(x, 7, true);
            display_.setPixel(x + 1, 7, true);
            display_.setPixel(x, 8, true);
            display_.setPixel(x + 1, 8, true);
            display_.setPixel(x, 9, true);
            display_.setPixel(x + 1, 9, true);
            display_.setPixel(x - 1, 10, true);
            display_.setPixel(x, 10, true);
            display_.setPixel(x + 1, 10, true);
            display_.setPixel(x + 2, 10, true);
        }
        const int markerX = gameChoice_ == 0 ? 8 : 23;
        display_.setPixel(markerX, 13, true);
        display_.setPixel(markerX + 1, 14, true);
        display_.setPixel(markerX, 15, true);
        return;
    }

    if (screen_ == Screen::NumberGame) {
        if (gameResolved_) {
            drawText(display_, gameWon_ ? 10 : 8, 5, gameWon_ ? "WIN" : "LOSE");
            return;
        }

        drawText(display_, 10, 0, "NUM");
        drawText(display_, 14, 5, std::to_string(currentNumber_));
        if (gameChoice_ == 0) { // lower
            display_.setPixel(16, 11, true);
            display_.setPixel(15, 12, true);
            display_.setPixel(16, 12, true);
            display_.setPixel(17, 12, true);
            display_.setPixel(16, 13, true);
            display_.setPixel(16, 14, true);
        } else { // higher
            display_.setPixel(16, 11, true);
            display_.setPixel(16, 12, true);
            display_.setPixel(15, 13, true);
            display_.setPixel(16, 13, true);
            display_.setPixel(17, 13, true);
            display_.setPixel(16, 14, true);
        }
        return;
    }

    if (pet_.lifeStage == Domain::LifeStage::Farewell) {
        drawText(display_, 10, 0, "NEW");
        const Domain::CreatureSprite& sprite = Domain::CreatureCatalog::spriteFor(
            Domain::CreatureCatalog::formFor(pet_));
        for (int y = 0; y < static_cast<int>(sprite.rows.size()); ++y) {
            for (int x = 0; x < static_cast<int>(sprite.rows[static_cast<std::size_t>(y)].size()); ++x) {
                if (sprite.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '#') {
                    display_.setPixel(11 + x, 5 + y, true);
                }
            }
        }
        return;
    }

    drawHeartMeter(0, 0, pet_.needs.hunger);
    drawHeartMeter(12, 0, pet_.needs.happiness);
    drawHeartMeter(24, 0, pet_.needs.discipline);

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

    if (pet_.sick) {
        display_.setPixel(2, 6, true);
        display_.setPixel(1, 7, true);
        display_.setPixel(2, 7, true);
        display_.setPixel(3, 7, true);
        display_.setPixel(2, 8, true);
    }

    const int visibleWaste = std::min(pet_.wasteCount, 2);
    for (int waste = 0; waste < visibleWaste; ++waste) {
        const int x = 2 + waste * 4;
        display_.setPixel(x + 1, 11, true);
        display_.setPixel(x, 12, true);
        display_.setPixel(x + 1, 12, true);
        display_.setPixel(x + 2, 12, true);
        display_.setPixel(x, 13, true);
        display_.setPixel(x + 1, 13, true);
        display_.setPixel(x + 2, 13, true);
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
        const bool active = (index == 3 && pet_.sick)
            || (index == 4 && pet_.wasteCount > 0)
            || (index == 7 && pet_.attentionReason != Domain::AttentionReason::None);
        const Color icon = selected || active ? Ink : ShellShadow;
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
    for (const ButtonPosition button : ButtonPositions) {
        drawEllipse(button.x, ButtonY + 4, 25, 25, ShellOutline);
        drawEllipse(button.x, ButtonY, 20, 20, ShellHighlight);
        drawEllipse(button.x - 3, ButtonY - 4, 8, 8, Color(255, 238, 244, 255));
    }
}

void CnaTamagotchiGame::OnExiting(System::Object* const sender,
                                  const System::EventArgs& args)
{
    saveNow();
    Game::OnExiting(sender, args);
}

GetTypeNameCPP(CnaTamagotchiGame, "CnaTamagotchiGame")

} // namespace CnaTamagotchi::Application
