#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"
#include "CnaTamagotchi/Domain/P1Program.hpp"
#include "CnaTamagotchi/Domain/P1SpriteCatalog.hpp"
#include "CnaTamagotchi/Persistence/SaveLocation.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

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
constexpr int WindowHeight = 700;
constexpr float BackgroundCycleSeconds = 32.0F;
constexpr int DisplayScale = 8;
constexpr int DisplayPixelWidth = Display::MonochromeDisplay::Width * DisplayScale;
constexpr int DisplayPixelHeight = Display::MonochromeDisplay::Height * DisplayScale;
constexpr int DisplayX = (WindowWidth - DisplayPixelWidth) / 2;
constexpr int DisplayY = 280;
constexpr int IconBandHeight = 34;
constexpr int LcdModulePadding = 12;
constexpr int IconAtlasCellWidth = 38;
constexpr int IconAtlasCellHeight = 36;
constexpr int IconDrawWidth = 32;
constexpr int IconDrawHeight = 22;

struct Rgb final {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

// The selected external P1 reference is a translucent turquoise shell with
// warm yellow controls.  These are deliberately authored colour values, not
// sampled image data or a shipped reference asset.
const Color ShellOutline(0, 83, 96, 255);
const Color ShellMain(0, 172, 184, 255);
const Color ShellHighlight(92, 224, 225, 255);
const Color ShellShadow(0, 124, 139, 255);
const Color ButtonMain(248, 203, 65, 255);
const Color ButtonHighlight(255, 238, 139, 255);
const Color Ink(28, 65, 71, 255);

Color asColor(const Display::LcdColour colour) noexcept
{
    return Color(colour.red, colour.green, colour.blue, 255U);
}

struct ButtonPosition final {
    int x;
    int y;
};

constexpr int IconCount = 8;
constexpr int SelectableIconCount = IconCount - 1;
constexpr int ButtonY = 555;
constexpr int ResetButtonX = 408;
constexpr int ResetButtonY = 542;
constexpr int ResetButtonRadius = 10;
constexpr float ResetHoldSeconds = 1.5F;
constexpr int ButtonHitRadius = 29;
constexpr std::array<ButtonPosition, 3> ButtonPositions{{
    {202, ButtonY}, {270, ButtonY + 12}, {338, ButtonY},
}};

const Domain::ProgramDefinition& activeProgramme() noexcept
{
    // The application has one selected programme today. All programme-specific
    // visible values come from this definition, so a later P2 package can use
    // the same controller and simulator rather than a copied application.
    return Domain::Programs::internationalP1();
}

std::int64_t unixSecondsNow() noexcept
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::filesystem::path defaultSavePath()
{
    std::error_code error;
    const std::filesystem::path workingDirectory = std::filesystem::current_path(error);
    if (error) {
        return std::filesystem::path("saves") / "slot-1.json";
    }
    return Persistence::SaveLocation::resolveSlot(
        workingDirectory, Persistence::SaveLocation::platformDataDirectory());
}

std::filesystem::path iconAtlasPath()
{
    constexpr std::array<std::string_view, 2> candidates{{
        "assets/p1-icon-atlas.png",
        "../assets/p1-icon-atlas.png",
    }};
    std::error_code error;
    for (const std::string_view candidate : candidates) {
        const std::filesystem::path path(candidate);
        if (std::filesystem::exists(path, error) && !error) {
            return path;
        }
        error.clear();
    }
    return std::filesystem::path(candidates.front());
}

} // namespace

CnaTamagotchiGame::CnaTamagotchiGame(const bool smokeTest,
                                     const Display::LcdPalette lcdPalette)
    : graphics_(this),
      lcdPalette_(lcdPalette),
      smokeTest_(smokeTest)
{
    graphics_.setPreferredBackBufferWidthProperty(WindowWidth);
    graphics_.setPreferredBackBufferHeightProperty(WindowHeight);
    Game::getWindowProperty().setTitleProperty("Tamagotchi CNA");
    if (!smokeTest_) {
        savePath_ = defaultSavePath();
        saveDirty_ = loadSave();
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
    iconAtlasTexture_.emplace(iconAtlasPath().string(), getGraphicsDeviceProperty());
}

void CnaTamagotchiGame::Update(GameTime& gameTime)
{
    const KeyboardState keyboard = Keyboard::GetState();
    const auto elapsedMilliseconds =
        gameTime.getElapsedGameTimeProperty().getTotalMillisecondsProperty();
    const float elapsedSeconds = static_cast<float>(elapsedMilliseconds) / 1000.0F;
    backgroundTimeSeconds_ += elapsedSeconds;
    if (feedbackSeconds_ > 0.0F) {
        feedbackSeconds_ = std::max(0.0F, feedbackSeconds_ - elapsedSeconds);
        if (feedbackSeconds_ == 0.0F) {
            feedback_ = Feedback::None;
        }
    }

    const bool selectNext = keyboard.IsKeyDown(Keys::A) || keyboard.IsKeyDown(Keys::Right);
    const bool selectPrevious = keyboard.IsKeyDown(Keys::Left);
    const bool confirm = keyboard.IsKeyDown(Keys::B) || keyboard.IsKeyDown(Keys::Enter)
        || keyboard.IsKeyDown(Keys::Space);
    const bool cancel = keyboard.IsKeyDown(Keys::C) || keyboard.IsKeyDown(Keys::Back)
        || keyboard.IsKeyDown(Keys::Escape);
    const bool clockChord = keyboard.IsKeyDown(Keys::A) && keyboard.IsKeyDown(Keys::C);
    bool resetHeld = keyboard.IsKeyDown(Keys::R);

    bool saveChanged = false;
    const bool pressedClockChord = clockChord && !clockChordWasDown_;
    if (pressedClockChord) {
        if (screen_ == Screen::ClockView) {
            beginClockSetup();
        } else if (screen_ == Screen::Home && pet_.stage == Domain::ProgramStage::End) {
            startNewEgg();
        } else if (screen_ == Screen::Home && selectedIcon_ < 0) {
            // P1 reserves this chord for the sound setting on the home LCD;
            // the actual audio layer will consume this session-local setting
            // when the P1 beep traces are implemented.
            soundEnabled_ = !soundEnabled_;
            setFeedback(Feedback::Success);
        }
    } else if (!clockChord) {
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
    }

    std::optional<DeviceButton> pointerButton;
    const Touch::TouchCollection touches = Touch::TouchPanel::GetState();
    for (int index = 0; index < touches.getCountProperty(); ++index) {
        const Touch::TouchLocation& touch = touches[static_cast<std::size_t>(index)];
        const Vector2& position = touch.getPositionProperty();
        if (touch.getStateProperty() == Touch::TouchLocationState::Pressed) {
            pointerButton = buttonAtWindowPosition(position.X, position.Y);
            if (pointerButton.has_value()) {
                break;
            }
        }
        if (touch.getStateProperty() == Touch::TouchLocationState::Pressed
            || touch.getStateProperty() == Touch::TouchLocationState::Moved) {
            resetHeld = resetHeld || resetAtWindowPosition(position.X, position.Y);
        }
    }

    const MouseState mouse = Mouse::GetState();
    const bool mouseLeftDown = mouse.getLeftButtonProperty() == ButtonState::Pressed;
    if (mouseLeftDown) {
        resetHeld = resetHeld || resetAtWindowPosition(
            static_cast<float>(mouse.getXProperty()), static_cast<float>(mouse.getYProperty()));
    }
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
    clockChordWasDown_ = clockChord;

    if (screen_ != Screen::SaveRecovery && screen_ != Screen::ResetConfirm
        && screen_ != Screen::ClockSetup) {
        if (resetHeld) {
            resetHoldSeconds_ += elapsedSeconds;
            if (resetHoldSeconds_ >= ResetHoldSeconds) {
                screen_ = Screen::ResetConfirm;
                resetHoldSeconds_ = 0.0F;
            }
        } else {
            resetHoldSeconds_ = 0.0F;
        }
        simulationSeconds_ += elapsedSeconds;
        while (simulationSeconds_ >= 60.0F) {
            static_cast<void>(simulation_.advance(activeProgramme(), pet_, 1));
            simulationSeconds_ -= 60.0F;
            lastSavedUnixSeconds_ += 60;
            // Do not rewrite the slot and its backup once per simulated
            // minute. A later care action stores this current timestamp; if
            // the process exits first, deterministic offline catch-up
            // replays the unsaved minutes from the prior timestamp.
        }
    } else {
        resetHoldSeconds_ = 0.0F;
    }
    if (saveChanged && screen_ != Screen::ClockSetup) {
        saveDirty_ = true;
        saveNow();
    }
    refreshDisplay();
}

bool CnaTamagotchiGame::pressButton(const DeviceButton button)
{
    if (screen_ == Screen::ResetConfirm) {
        if (button == DeviceButton::B) {
            const bool completed = resetCurrentSession();
            setFeedback(completed ? Feedback::Success : Feedback::Blocked);
            return completed;
        }
        if (button == DeviceButton::C) {
            screen_ = Screen::Home;
        }
        return false;
    }

    if (screen_ == Screen::SaveRecovery) {
        if (button == DeviceButton::A && recoveryBackupAvailable_) {
            recoveryChoice_ = recoveryChoice_ == RecoveryChoice::RestoreBackup
                ? RecoveryChoice::NewEgg : RecoveryChoice::RestoreBackup;
            return false;
        }
        if (button == DeviceButton::B) {
            const bool completed = recoveryChoice_ == RecoveryChoice::RestoreBackup
                ? restoreBackup() : archiveAndStartFreshEgg();
            setFeedback(completed ? Feedback::Success : Feedback::Blocked);
            return completed;
        }
        return false;
    }

    if (screen_ == Screen::ClockView) {
        if (button == DeviceButton::B || button == DeviceButton::C) {
            screen_ = Screen::Home;
        }
        return false;
    }

    if (screen_ == Screen::ClockSetup) {
        if (button == DeviceButton::A) {
            clockSetupMinutes_ = (clockSetupMinutes_ + 60) % (24 * 60);
            return false;
        }
        if (button == DeviceButton::B) {
            clockSetupMinutes_ = (clockSetupMinutes_ + 1) % (24 * 60);
            return false;
        }

        pet_.clockMinutesOfDay = clockSetupMinutes_;
        lastSavedUnixSeconds_ = unixSecondsNow();
        simulationSeconds_ = 0.0F;
        screen_ = Screen::Home;
        return true;
    }

    if (pet_.stage == Domain::ProgramStage::End && screen_ == Screen::Home) {
        if (button == DeviceButton::B) {
            screen_ = Screen::Status;
            statusPage_ = 0;
        }
        return false;
    }

    switch (button) {
    case DeviceButton::A:
        if (screen_ == Screen::Home) {
            selectedIcon_ = (selectedIcon_ + 1 + SelectableIconCount) % SelectableIconCount;
        } else if (screen_ == Screen::Food) {
            foodSelection_ = (foodSelection_ + 1)
                % static_cast<int>(activeProgramme().food.size());
        } else if (screen_ == Screen::Light) {
            lightSelection_ = (lightSelection_ + 1) % 2;
        } else if (screen_ == Screen::Status) {
            statusPage_ = (statusPage_ + 1) % 4;
        } else if (screen_ == Screen::Game && gameResolved_) {
            if (gameRound_ >= activeProgramme().game.rounds) {
                screen_ = Screen::Home;
                selectedIcon_ = -1;
            } else {
                startNextCharacterRound();
            }
            return false;
        } else if (screen_ == Screen::Game) {
            resolveCharacterRound(0);
            return true;
        }
        return false;
    case DeviceButton::B:
        if (screen_ == Screen::Home && selectedIcon_ < 0) {
            if (pet_.attentionReason == Domain::ProgramAttentionReason::None) {
                screen_ = Screen::ClockView;
            }
            return false;
        }
        if (screen_ == Screen::Food) {
            const bool fed = simulation_.feed(activeProgramme(), pet_, foodSelection_);
            screen_ = Screen::Home;
            selectedIcon_ = -1;
            setFeedback(fed ? Feedback::Success : Feedback::Blocked);
            return fed;
        }
        if (screen_ == Screen::Light) {
            const bool changed = simulation_.setLightOff(pet_, lightSelection_ == 1);
            screen_ = Screen::Home;
            selectedIcon_ = -1;
            setFeedback(changed ? Feedback::Success : Feedback::Blocked);
            return changed;
        }
        if (screen_ == Screen::Status) {
            statusPage_ = (statusPage_ + 1) % 4;
            return false;
        }
        if (screen_ == Screen::Game) {
            if (gameResolved_) {
                if (gameRound_ >= activeProgramme().game.rounds) {
                    screen_ = Screen::Home;
                    selectedIcon_ = -1;
                } else {
                    startNextCharacterRound();
                }
                return false;
            }
            resolveCharacterRound(1);
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
            if (!pet_.asleep
                && pet_.attentionReason != Domain::ProgramAttentionReason::Discipline) {
                startCharacterGame();
                screen_ = Screen::Game;
                return true;
            }
            setFeedback(Feedback::Blocked);
            return false;
        }
        if (selectedIcon_ == 1) {
            screen_ = Screen::Light;
            lightSelection_ = pet_.lightOff ? 1 : 0;
            return false;
        }
        if (selectedIcon_ == 3) {
            const bool changed = simulation_.giveMedicine(pet_);
            setFeedback(changed ? Feedback::Success : Feedback::Blocked);
            return changed;
        }
        if (selectedIcon_ == 4) {
            const bool changed = simulation_.cleanWaste(pet_);
            setFeedback(changed ? Feedback::Success : Feedback::Blocked);
            return changed;
        }
        if (selectedIcon_ == 6) {
            const bool changed = simulation_.discipline(pet_);
            setFeedback(changed ? Feedback::Success : Feedback::Blocked);
            return changed;
        }
        return false;
    case DeviceButton::C:
        if (screen_ != Screen::Home) {
            screen_ = Screen::Home;
        }
        selectedIcon_ = -1;
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
        const float deltaY = deviceY - static_cast<float>(ButtonPositions[index].y);
        if (deltaX * deltaX + deltaY * deltaY
            <= static_cast<float>(ButtonHitRadius * ButtonHitRadius)) {
            return static_cast<DeviceButton>(index);
        }
    }
    return std::nullopt;
}

bool CnaTamagotchiGame::resetAtWindowPosition(const float x, const float y) const noexcept
{
    const Rectangle clientBounds = getWindowProperty().getClientBoundsProperty();
    if (clientBounds.Width <= 0 || clientBounds.Height <= 0) {
        return false;
    }

    const float deviceX = x * static_cast<float>(WindowWidth)
        / static_cast<float>(clientBounds.Width);
    const float deviceY = y * static_cast<float>(WindowHeight)
        / static_cast<float>(clientBounds.Height);
    const float deltaX = deviceX - static_cast<float>(ResetButtonX);
    const float deltaY = deviceY - static_cast<float>(ResetButtonY);
    return deltaX * deltaX + deltaY * deltaY
        <= static_cast<float>(ResetButtonRadius * ResetButtonRadius);
}

void CnaTamagotchiGame::moveSelectionBackward() noexcept
{
    if (screen_ == Screen::Home) {
        selectedIcon_ = selectedIcon_ < 0 ? SelectableIconCount - 1
                                          : (selectedIcon_ + SelectableIconCount - 1)
                % SelectableIconCount;
    } else if (screen_ == Screen::Food) {
        foodSelection_ = (foodSelection_ + 1)
            % static_cast<int>(activeProgramme().food.size());
    } else if (screen_ == Screen::Light) {
        lightSelection_ = (lightSelection_ + 1) % 2;
    } else if (screen_ == Screen::SaveRecovery && recoveryBackupAvailable_) {
        recoveryChoice_ = recoveryChoice_ == RecoveryChoice::RestoreBackup
            ? RecoveryChoice::NewEgg : RecoveryChoice::RestoreBackup;
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

bool CnaTamagotchiGame::loadSave()
{
    std::error_code error;
    const bool saveExists = std::filesystem::exists(savePath_, error);
    if (error) {
        recoveryBackupAvailable_ = false;
        recoveryChoice_ = RecoveryChoice::NewEgg;
        screen_ = Screen::SaveRecovery;
        return false;
    }
    if (!saveExists) {
        startFreshEgg();
        // A newly reset P1 must be clock-configured before it gets a durable
        // elapsed-time anchor. Do not save an egg that is not yet running.
        return false;
    }

    const Persistence::LoadResult loaded = saveRepository_.load(savePath_);
    if (!loaded.data) {
        legacySaveAwaitingArchive_ = loaded.isLegacyPrototype();
        recoveryBackupAvailable_ = !legacySaveAwaitingArchive_
            && saveRepository_.load(savePath_.string() + ".bak").success();
        recoveryChoice_ = recoveryBackupAvailable_ ? RecoveryChoice::RestoreBackup
                                                    : RecoveryChoice::NewEgg;
        screen_ = Screen::SaveRecovery;
        return false;
    }

    return activateSave(*loaded.data);
}

bool CnaTamagotchiGame::activateSave(const Persistence::SaveData& data)
{
    if (data.programId != activeProgramme().id) {
        legacySaveAwaitingArchive_ = false;
        recoveryBackupAvailable_ = false;
        recoveryChoice_ = RecoveryChoice::NewEgg;
        screen_ = Screen::SaveRecovery;
        return false;
    }

    pet_ = data.pet;
    seed_ = data.seed;
    lastSavedUnixSeconds_ = data.lastSavedUnixSeconds;
    screen_ = Screen::Home;
    selectedIcon_ = -1;
    simulationSeconds_ = 0.0F;

    const std::int64_t now = unixSecondsNow();
    if (now <= lastSavedUnixSeconds_) {
        return false;
    }

    const std::int64_t elapsedSeconds = now - lastSavedUnixSeconds_;
    const std::int64_t elapsedMinutes = elapsedSeconds / 60;
    const int appliedMinutes = static_cast<int>(std::min(
        elapsedMinutes, static_cast<std::int64_t>(std::numeric_limits<int>::max())));
    const Domain::ProgramAdvanceReport report = simulation_.advance(
        activeProgramme(), pet_, appliedMinutes);

    // Keep sub-minute time in the saved timestamp. Without this, repeatedly
    // opening and closing the application every few seconds would prevent a
    // full simulated minute from ever being reached.
    lastSavedUnixSeconds_ += static_cast<std::int64_t>(report.appliedMinutes) * 60;
    simulationSeconds_ = static_cast<float>(elapsedSeconds % 60);
    return report.appliedMinutes > 0;
}

bool CnaTamagotchiGame::restoreBackup()
{
    if (!recoveryBackupAvailable_) {
        return false;
    }

    const Persistence::LoadResult backup = saveRepository_.load(savePath_.string() + ".bak");
    if (!backup.data || !saveRepository_.restoreBackup(savePath_).success) {
        return false;
    }

    recoveryBackupAvailable_ = false;
    legacySaveAwaitingArchive_ = false;
    recoveryChoice_ = RecoveryChoice::NewEgg;
    static_cast<void>(activateSave(*backup.data));
    return true;
}

bool CnaTamagotchiGame::archiveAndStartFreshEgg()
{
    const Persistence::SaveResult archive = legacySaveAwaitingArchive_
        ? saveRepository_.archiveLegacySave(savePath_)
        : saveRepository_.archiveCorruptSave(savePath_);
    if (!archive.success) {
        return false;
    }

    recoveryBackupAvailable_ = false;
    legacySaveAwaitingArchive_ = false;
    recoveryChoice_ = RecoveryChoice::NewEgg;
    startFreshEgg();
    return true;
}

bool CnaTamagotchiGame::resetCurrentSession()
{
    if (savePath_.empty() || !saveRepository_.archiveResetSave(savePath_).success) {
        return false;
    }

    startFreshEgg();
    return true;
}

void CnaTamagotchiGame::saveNow()
{
    if (smokeTest_ || !saveDirty_ || screen_ == Screen::ClockSetup) {
        return;
    }

    // This is an elapsed-time anchor, rather than merely the instant of the
    // file write. It never moves backwards, and it retains sub-minute time.
    const Persistence::SaveData data{
        .programId = std::string(activeProgramme().id),
        .lastSavedUnixSeconds = lastSavedUnixSeconds_,
        .seed = seed_,
        .pet = pet_,
    };
    const Persistence::SaveResult result = saveRepository_.save(savePath_, data);
    if (result.success) {
        saveDirty_ = false;
    }
}

void CnaTamagotchiGame::startCharacterGame() noexcept
{
    gameRound_ = 0;
    gameWins_ = 0;
    startNextCharacterRound();
}

void CnaTamagotchiGame::startNextCharacterRound() noexcept
{
    // The direction stays hidden until the A/B prediction is committed.
    // The persisted seed keeps the local sequence stable across restarts.
    gameTarget_ = 0;
    gameChoice_ = 0;
    gameResolved_ = false;
    gameWon_ = false;
}

void CnaTamagotchiGame::resolveCharacterRound(const int choice) noexcept
{
    // The P1 base data has a per-form success fraction. Derive the displayed
    // direction after the user's prediction so it carries that exact chance,
    // rather than accidentally treating every form as a 50/50 target coin.
    seed_ = seed_ * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
    const auto character = std::find_if(activeProgramme().creatures.begin(),
        activeProgramme().creatures.end(), [this](const Domain::CreatureDefinition& definition) {
            return definition.id == pet_.characterId;
        });
    const int numerator = character == activeProgramme().creatures.end()
        ? 1 : character->characterGameWinNumerator;
    const int denominator = character == activeProgramme().creatures.end()
        ? 2 : character->characterGameWinDenominator;
    gameChoice_ = choice;
    gameWon_ = denominator > 0
        && static_cast<int>(seed_ % static_cast<std::uint64_t>(denominator)) < numerator;
    gameTarget_ = gameWon_ ? gameChoice_ : 1 - gameChoice_;
    gameResolved_ = true;
    if (gameWon_) {
        ++gameWins_;
    }
    ++gameRound_;
    if (gameRound_ == activeProgramme().game.rounds) {
        static_cast<void>(simulation_.completeGame(activeProgramme(), pet_, gameWins_));
    }
}

void CnaTamagotchiGame::startNewEgg() noexcept
{
    // A P1 death/new-egg chord keeps the already configured device clock. It
    // clears the departed generation only; reset is the separate path that
    // requires SET again.
    seed_ = seed_ * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
    const int retainedClockMinutes = pet_.clockMinutesOfDay;
    resetPetToEgg();
    pet_.clockMinutesOfDay = retainedClockMinutes;
}

void CnaTamagotchiGame::startFreshEgg() noexcept
{
    seed_ = static_cast<std::uint64_t>(unixSecondsNow());
    resetPetToEgg();
    beginClockSetup();
}

void CnaTamagotchiGame::beginClockSetup() noexcept
{
    clockSetupMinutes_ = pet_.clockMinutesOfDay;
    screen_ = Screen::ClockSetup;
}

void CnaTamagotchiGame::resetPetToEgg() noexcept
{
    pet_ = Domain::ProgramPetState{};
    screen_ = Screen::Home;
    selectedIcon_ = -1;
    foodSelection_ = 0;
    lightSelection_ = 0;
    statusPage_ = 0;
    gameChoice_ = 0;
    gameTarget_ = 0;
    gameRound_ = 0;
    gameWins_ = 0;
    gameResolved_ = false;
    gameWon_ = false;
    lastSavedUnixSeconds_ = unixSecondsNow();
    simulationSeconds_ = 0.0F;
    saveDirty_ = false;
}

void CnaTamagotchiGame::setFeedback(const Feedback feedback) noexcept
{
    feedback_ = feedback;
    feedbackSeconds_ = feedback == Feedback::None ? 0.0F : 0.8F;
}

void CnaTamagotchiGame::refreshDisplay() noexcept
{
    display_.clear();

    const auto drawHeartMeter = [this](const int firstX, const int firstY, const int value) {
        const int filled = std::clamp(value, 0, 4);
        for (int heart = 0; heart < filled; ++heart) {
            const int x = firstX + heart * 2;
            display_.setPixel(x, firstY, true);
            display_.setPixel(x + 1, firstY, true);
            display_.setPixel(x, firstY + 1, true);
            display_.setPixel(x + 1, firstY + 1, true);
        }
    };

    const auto drawClock = [this](const int minutes, const bool setup) {
        const int hour24 = minutes / 60;
        const int hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
        const int minute = minutes % 60;
        const std::string hours = (hour12 < 10 ? "0" : "") + std::to_string(hour12);
        const std::string minutesText = (minute < 10 ? "0" : "") + std::to_string(minute);
        if (setup) {
            display_.drawText(10, 1, "SET");
        }
        display_.drawText(3, 8, hours);
        display_.setPixel(16, 9, true);
        display_.setPixel(16, 11, true);
        display_.drawText(21, 8, minutesText);
    };

    if (screen_ == Screen::SaveRecovery) {
        if (recoveryBackupAvailable_) {
            display_.drawText(8, 3, "REST");
            display_.drawText(10, 8, "NEW");
            const int markerY = recoveryChoice_ == RecoveryChoice::RestoreBackup ? 4 : 9;
            display_.setPixel(4, markerY, true);
            display_.setPixel(5, markerY + 1, true);
            display_.setPixel(4, markerY + 2, true);
        } else {
            display_.drawText(legacySaveAwaitingArchive_ ? 10 : 8, 3,
                legacySaveAwaitingArchive_ ? "OLD" : "NONE");
            display_.drawText(10, 8, "NEW");
            display_.setPixel(4, 9, true);
            display_.setPixel(5, 10, true);
            display_.setPixel(4, 11, true);
        }
        return;
    }

    if (screen_ == Screen::ResetConfirm) {
        display_.drawText(6, 3, "RESET");
        display_.drawText(8, 8, "SURE");
        return;
    }

    if (screen_ == Screen::ClockView) {
        drawClock(pet_.clockMinutesOfDay, false);
        return;
    }

    if (screen_ == Screen::ClockSetup) {
        drawClock(clockSetupMinutes_, true);
        return;
    }

    if (screen_ == Screen::Food) {
        const auto& food = activeProgramme().food;
        display_.drawText(3, 3, food[0].lcdLabel);
        display_.drawText(3, 8, food[1].lcdLabel);
        const int markerY = foodSelection_ == 0 ? 4 : 9;
        display_.setPixel(1, markerY, true);
        display_.setPixel(2, markerY + 1, true);
        display_.setPixel(1, markerY + 2, true);
        return;
    }

    if (screen_ == Screen::Light) {
        display_.drawText(7, 1, "LIGHT");
        display_.drawText(11, 5, "ON");
        display_.drawText(10, 10, "OFF");
        const int markerY = lightSelection_ == 0 ? 6 : 11;
        display_.setPixel(6, markerY, true);
        display_.setPixel(7, markerY + 1, true);
        display_.setPixel(6, markerY + 2, true);
        return;
    }

    if (screen_ == Screen::Status) {
        if (statusPage_ == 0) {
            const std::string age = "AGE" + std::to_string(pet_.age);
            const std::string weight = "WGT" + std::to_string(pet_.weight);
            display_.drawText(2, 3, age);
            display_.drawText(2, 8, weight);
        } else if (statusPage_ == 1) {
            display_.drawText(1, 3, "DIS");
            drawHeartMeter(20, 5, pet_.disciplineBars);
        } else if (statusPage_ == 2) {
            display_.drawText(1, 5, "HUN");
            drawHeartMeter(20, 7, pet_.hungerHearts);
        } else {
            display_.drawText(1, 5, "HAP");
            drawHeartMeter(20, 7, pet_.happinessHearts);
        }
        return;
    }

    if (screen_ == Screen::Game) {
        if (gameResolved_) {
            if (gameRound_ >= activeProgramme().game.rounds) {
                const bool completeWin = gameWins_ >= activeProgramme().game.winsNeededForHappiness;
                display_.drawText(completeWin ? 10 : 8, 4, completeWin ? "WIN" : "LOSE");
                display_.drawText(12, 9, "W" + std::to_string(gameWins_));
            } else {
                display_.drawText(gameWon_ ? 10 : 8, 5, gameWon_ ? "OK" : "NO");
                display_.drawText(12, 9, "R" + std::to_string(gameRound_));
            }
            return;
        }

        display_.drawText(8, 3, "CHAR");
        // The P1 Character game is a five-round left/right prediction. The
        // selected position is marked below the two target positions.
        for (const int x : {8, 23}) {
            display_.setPixel(x, 8, true);
            display_.setPixel(x + 1, 8, true);
            display_.setPixel(x, 9, true);
            display_.setPixel(x + 1, 9, true);
            display_.setPixel(x, 10, true);
            display_.setPixel(x + 1, 10, true);
            display_.setPixel(x - 1, 11, true);
            display_.setPixel(x, 11, true);
            display_.setPixel(x + 1, 11, true);
            display_.setPixel(x + 2, 11, true);
        }
        const int markerX = gameChoice_ == 0 ? 8 : 23;
        display_.setPixel(markerX, 12, true);
        return;
    }

    if (pet_.stage == Domain::ProgramStage::End) {
        constexpr std::array<std::string_view, 8> Angel{{
            "      ##        ", "    ######      ", "   ## ## ##     ", "  ##  ##  ##    ",
            " ##  ######  ## ", "  ##  ####  ##  ", "    ##  ##      ", "   ##    ##     ",
        }};
        display_.drawSprite(8, 4, Angel);
        for (const auto [x, y] : std::array<std::pair<int, int>, 7>{{
                 {2, 3}, {5, 7}, {4, 12}, {27, 3}, {29, 7}, {26, 12}, {30, 14},
             }}) {
            display_.setPixel(x, y, true);
        }
        return;
    }

    const Domain::P1Sprite& sprite = Domain::P1SpriteCatalog::spriteForCharacter(pet_.characterId);
    // P1 home animation consists of independently transcribed LCD phases.
    // Each phase carries its observed origin, rather than turning one modern
    // sprite into a synthetic bobbing animation. Sleeping leaves the first
    // quiet frame on screen.
    const std::size_t idleFrame = pet_.asleep ? 0U : static_cast<std::size_t>(
        backgroundTimeSeconds_ / sprite.idleFrameSeconds);
    const Domain::P1SpriteFrame& frame = sprite.idleFrame(idleFrame);
    display_.drawSprite(frame.originX, frame.originY, frame.visibleRows());

    if (pet_.asleep) {
        display_.setPixel(27, 4, true);
        display_.setPixel(28, 4, true);
        display_.setPixel(28, 5, true);
    }

    if (pet_.sick) {
        display_.setPixel(28, 7, true);
        display_.setPixel(27, 8, true);
        display_.setPixel(28, 8, true);
        display_.setPixel(29, 8, true);
        display_.setPixel(28, 9, true);
    }

    const int visibleWaste = std::min(pet_.wasteCount, 2);
    for (int waste = 0; waste < visibleWaste; ++waste) {
        const int x = 26 + waste * 3;
        display_.setPixel(x + 1, 10, true);
        display_.setPixel(x, 11, true);
        display_.setPixel(x + 1, 11, true);
        display_.setPixel(x + 2, 11, true);
        display_.setPixel(x + 1, 12, true);
    }

    if (feedback_ == Feedback::Success) {
        display_.setPixel(24, 5, true);
        display_.setPixel(23, 6, true);
        display_.setPixel(24, 6, true);
        display_.setPixel(25, 6, true);
        display_.setPixel(24, 7, true);
    } else if (feedback_ == Feedback::Blocked) {
        display_.setPixel(23, 5, true);
        display_.setPixel(25, 5, true);
        display_.setPixel(24, 6, true);
        display_.setPixel(23, 7, true);
        display_.setPixel(25, 7, true);
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

    const auto drawEgg = [&drawRect](const int centreX, const int centreY,
                                     const int radiusX, const int radiusY,
                                     const Color colour) {
        for (int y = -radiusY; y <= radiusY; ++y) {
            const float normalizedY = static_cast<float>(y) / static_cast<float>(radiusY);
            const float ovalWidth = std::sqrt(std::max(
                0.0F, 1.0F - normalizedY * normalizedY));
            // A narrower crown and a slightly lower widest point make this an
            // egg rather than the previous tall, symmetric capsule.
            const float lowerHalf = (normalizedY + 1.0F) * 0.5F;
            const float taper = 0.82F + lowerHalf * 0.26F;
            const int halfWidth = static_cast<int>(
                ovalWidth * taper * static_cast<float>(radiusX));
            drawRect(Rectangle(centreX - halfWidth, centreY + y, halfWidth * 2 + 1, 1), colour);
        }
    };

    const auto drawRing = [&drawEllipse](const int centreX, const int centreY,
                                        const int radius, const Color outer, const Color inner) {
        drawEllipse(centreX, centreY, radius, radius, outer);
        drawEllipse(centreX, centreY, radius - 3, radius - 3, inner);
    };
    const Display::LcdPaletteColours lcdColours =
        Display::MonochromeDisplay::coloursFor(lcdPalette_);
    const Color lcdBezel = asColor(lcdColours.bezel);
    const Color lcdOff = asColor(lcdColours.off);
    const Color lcdOn = asColor(lcdColours.on);

    // Drop shadow, shell rim, and inner egg. The short, tapered silhouette
    // and translucent turquoise treatment follow the selected P1 device
    // visually, while the geometry remains an independently drawn UI.
    drawEllipse(278, 638, 164, 15, Color(38, 108, 112, 44));
    drawEgg(270, 348, 220, 272, ShellOutline);
    drawEgg(270, 346, 208, 260, ShellMain);
    drawEllipse(238, 318, 135, 190, ShellHighlight);

    // Small keychain tab: recognisable, but deliberately generic.
    drawRing(270, 72, 20, ShellOutline, backgroundColor());
    drawRing(270, 72, 14, ShellMain, backgroundColor());

    // The active game bitmap is exactly 32 × 16 pixels. The eight permanent
    // icon cells live in the physically connected top/bottom LCD surround;
    // they must never consume rows from that game bitmap.
    const int moduleX = DisplayX - LcdModulePadding;
    const int moduleY = DisplayY - IconBandHeight - LcdModulePadding;
    const int moduleWidth = DisplayPixelWidth + LcdModulePadding * 2;
    const int moduleHeight = DisplayPixelHeight + IconBandHeight * 2 + LcdModulePadding * 2;
    drawRect(Rectangle(moduleX, moduleY, moduleWidth, moduleHeight), lcdBezel);
    drawRect(Rectangle(moduleX + 6, moduleY + 6, moduleWidth - 12, moduleHeight - 12), ShellShadow);
    drawRect(Rectangle(DisplayX, DisplayY - IconBandHeight,
        DisplayPixelWidth, DisplayPixelHeight + IconBandHeight * 2), lcdOff);
    drawRect(Rectangle(DisplayX, DisplayY, DisplayPixelWidth, DisplayPixelHeight), lcdOff);

    for (int y = 0; y < Display::MonochromeDisplay::Height; ++y) {
        for (int x = 0; x < Display::MonochromeDisplay::Width; ++x) {
            if (display_.pixel(x, y)) {
                drawRect(Rectangle(
                    DisplayX + x * DisplayScale,
                    DisplayY + y * DisplayScale,
                    DisplayScale,
                    DisplayScale),
                    lcdOn);
            }
        }
    }

    // The atlas is a transparency mask derived from the reference device's
    // eight face icons. CNA applies one of two outline colours at draw time:
    // muted grey when inactive and near-black when selected or urgent.
    const Color iconInactive = Ink;
    for (int index = 0; index < IconCount; ++index) {
        const int slot = index % 4;
        const bool topBand = index < 4;
        const int bandY = topBand ? DisplayY - IconBandHeight : DisplayY + DisplayPixelHeight;
        const int slotWidth = DisplayPixelWidth / 4;
        const bool urgent = (index == 3 && pet_.sick)
            || (index == 4 && pet_.wasteCount > 0)
            || (index == 7 && pet_.attentionReason != Domain::ProgramAttentionReason::None);
        const bool active = index == selectedIcon_ || urgent;
        const Rectangle source((index % 4) * IconAtlasCellWidth,
            (index / 4) * IconAtlasCellHeight, IconAtlasCellWidth, IconAtlasCellHeight);
        // Keep the photographed face art centred and close to its native
        // aspect ratio.  Stretching a 38x36 mask across a 48x32 cell made
        // the icons squat and put their outlines against the band edges.
        const Rectangle destination(DisplayX + slot * slotWidth
                + (slotWidth - IconDrawWidth) / 2,
            bandY + (IconBandHeight - IconDrawHeight) / 2,
            IconDrawWidth, IconDrawHeight);
        spriteBatch_->Draw(*iconAtlasTexture_, destination, source,
            active ? lcdOn : iconInactive);
    }

    // Three physical controls: A changes selection, B confirms, C clears it.
    for (const ButtonPosition button : ButtonPositions) {
        drawEllipse(button.x, button.y + 4, 25, 25, ShellOutline);
        drawEllipse(button.x, button.y, 20, 20, ButtonMain);
        drawEllipse(button.x - 3, button.y - 4, 8, 8, ButtonHighlight);
    }

    // A small recessed reset pinhole sits apart from the three care controls.
    // Hold it (or the desktop R key) before the LCD asks for B/C confirmation.
    drawEllipse(ResetButtonX, ResetButtonY, ResetButtonRadius, ResetButtonRadius, ShellOutline);
    drawEllipse(ResetButtonX, ResetButtonY, ResetButtonRadius - 3, ResetButtonRadius - 3,
        ShellShadow);
}

void CnaTamagotchiGame::OnExiting(System::Object* const sender,
                                  const System::EventArgs& args)
{
    if (saveDirty_ && screen_ != Screen::ClockSetup) {
        saveNow();
    }
    Game::OnExiting(sender, args);
}

GetTypeNameCPP(CnaTamagotchiGame, "CnaTamagotchiGame")

} // namespace CnaTamagotchi::Application
