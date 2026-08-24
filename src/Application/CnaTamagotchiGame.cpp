#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"
#include "CnaTamagotchi/Application/DeviceShellRenderer.hpp"
#include "CnaTamagotchi/Display/P1LightScreen.hpp"
#include "CnaTamagotchi/Domain/P1Program.hpp"
#include "CnaTamagotchi/Domain/P1SpriteCatalog.hpp"
#include "CnaTamagotchi/Persistence/SaveLocation.hpp"
#include "CnaTamagotchi/Presentation/DeviceShell.hpp"

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
constexpr int IconAtlasCellWidth = 152;
constexpr int IconAtlasCellHeight = 144;
constexpr int IconDrawWidth = 40;
constexpr int IconDrawHeight = 32;

struct Rgb final {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

Color asColor(const Display::LcdColour colour) noexcept
{
    return Color(colour.red, colour.green, colour.blue, 255U);
}

Color asColor(const Presentation::ShellRgba colour) noexcept
{
    return Color(colour.red, colour.green, colour.blue, colour.alpha);
}

constexpr int IconCount = 8;
constexpr int SelectableIconCount = IconCount - 1;
constexpr float ResetHoldSeconds = 1.5F;

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
        "assets/p1-icon-atlas-smooth.png",
        "../assets/p1-icon-atlas-smooth.png",
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
    // The classic P1 pause trick is to leave the device in Clock SET after
    // pressing A+C from the clock. Freeze presentation timers as well as the
    // pet simulation so returning with C resumes the exact visible phase.
    const bool clockSetPaused = screen_ == Screen::ClockSetup;
    if (!clockSetPaused) {
        backgroundTimeSeconds_ += elapsedSeconds;
        if (feedbackSeconds_ > 0.0F) {
            feedbackSeconds_ = std::max(0.0F, feedbackSeconds_ - elapsedSeconds);
            if (feedbackSeconds_ == 0.0F) {
                feedback_ = Feedback::None;
            }
        }
        if (transientVisual_ == TransientVisual::ToiletWipe) {
            transientVisualSeconds_ += elapsedSeconds;
            if (Display::P1ToiletWipe::complete(transientVisualSeconds_)) {
                transientVisual_ = TransientVisual::None;
                transientVisualSeconds_ = 0.0F;
                selectedIcon_ = -1;
                iconSelectionSeconds_ = 0.0F;
            }
        }
    }

    const bool selectNext = keyboard.IsKeyDown(Keys::A) || keyboard.IsKeyDown(Keys::Right);
    const bool selectPrevious = keyboard.IsKeyDown(Keys::Left);
    const float iconTimeout = activeProgramme().display.iconSelectionTimeoutSeconds;
    if (screen_ == Screen::Home && selectedIcon_ >= 0
        && transientVisual_ == TransientVisual::None && iconTimeout > 0.0F) {
        iconSelectionSeconds_ += elapsedSeconds;
        if (iconSelectionSeconds_ >= iconTimeout) {
            selectedIcon_ = -1;
            iconSelectionSeconds_ = 0.0F;
        }
    }
    const float menuTimeout = activeProgramme().display.menuTimeoutSeconds;
    if (screen_ == Screen::Light && menuTimeout > 0.0F) {
        menuInactivitySeconds_ += elapsedSeconds;
        if (menuInactivitySeconds_ >= menuTimeout) {
            screen_ = Screen::Home;
            selectedIcon_ = -1;
            iconSelectionSeconds_ = 0.0F;
            menuInactivitySeconds_ = 0.0F;
        }
    }

    const bool confirm = keyboard.IsKeyDown(Keys::B) || keyboard.IsKeyDown(Keys::Enter)
        || keyboard.IsKeyDown(Keys::Space);
    const bool cancel = keyboard.IsKeyDown(Keys::C) || keyboard.IsKeyDown(Keys::Back)
        || keyboard.IsKeyDown(Keys::Escape);
    const bool clockChord = keyboard.IsKeyDown(Keys::A) && keyboard.IsKeyDown(Keys::C);
    const bool cycleShell = keyboard.IsKeyDown(Keys::V);
    bool resetHeld = keyboard.IsKeyDown(Keys::R);
    std::array<bool, 3> buttonsHeld{{
        selectNext || selectPrevious,
        confirm,
        cancel,
    }};
    const auto markButtonHeld = [&buttonsHeld](const DeviceButton button) {
        buttonsHeld[static_cast<std::size_t>(button)] = true;
    };

    bool saveChanged = false;
    if (cycleShell && !shellCycleWasDown_ && screen_ != Screen::SaveRecovery) {
        shellId_ = std::string(Presentation::nextDeviceShellId(shellId_));
        saveChanged = true;
    }

    const bool pressedClockChord = clockChord && !clockChordWasDown_;
    if (pressedClockChord) {
        if (screen_ == Screen::ClockView) {
            beginClockSetup(true);
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
            saveChanged = pressButton(DeviceButton::A) || saveChanged;
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
        if (touch.getStateProperty() == Touch::TouchLocationState::Pressed
            || touch.getStateProperty() == Touch::TouchLocationState::Moved) {
            const std::optional<DeviceButton> touchButton =
                buttonAtWindowPosition(position.X, position.Y);
            if (touchButton.has_value()) {
                markButtonHeld(*touchButton);
                if (touch.getStateProperty() == Touch::TouchLocationState::Pressed
                    && !pointerButton.has_value()) {
                    pointerButton = touchButton;
                }
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
        const float mouseX = static_cast<float>(mouse.getXProperty());
        const float mouseY = static_cast<float>(mouse.getYProperty());
        resetHeld = resetHeld || resetAtWindowPosition(mouseX, mouseY);
        const std::optional<DeviceButton> mouseButton = buttonAtWindowPosition(mouseX, mouseY);
        if (mouseButton.has_value()) {
            markButtonHeld(*mouseButton);
        }
    }
    if (!pointerButton.has_value() && mouseLeftDown && !mouseLeftWasDown_) {
        pointerButton = buttonAtWindowPosition(
            static_cast<float>(mouse.getXProperty()), static_cast<float>(mouse.getYProperty()));
    }
    if (pointerButton.has_value()) {
        saveChanged = pressButton(*pointerButton) || saveChanged;
    }

    pressedButtons_ = buttonsHeld;
    resetPressed_ = resetHeld;

    selectNextWasDown_ = selectNext;
    selectPreviousWasDown_ = selectPrevious;
    confirmWasDown_ = confirm;
    cancelWasDown_ = cancel;
    mouseLeftWasDown_ = mouseLeftDown;
    clockChordWasDown_ = clockChord;
    shellCycleWasDown_ = cycleShell;

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
    if (saveChanged) {
        saveDirty_ = true;
        if (screen_ != Screen::ClockSetup) {
            saveNow();
        }
    }
    refreshDisplay();
}

bool CnaTamagotchiGame::pressButton(const DeviceButton button)
{
    if (transientVisual_ != TransientVisual::None) {
        return false;
    }

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
        if (button == DeviceButton::B) {
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
        screen_ = clockSetupReturnsToClockView_ ? Screen::ClockView : Screen::Home;
        clockSetupReturnsToClockView_ = false;
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
            iconSelectionSeconds_ = 0.0F;
        } else if (screen_ == Screen::Food) {
            foodSelection_ = (foodSelection_ + 1)
                % static_cast<int>(activeProgramme().food.size());
        } else if (screen_ == Screen::Light) {
            lightSelection_ = (lightSelection_ + 1) % 2;
            menuInactivitySeconds_ = 0.0F;
        } else if (screen_ == Screen::Status) {
            statusPage_ = (statusPage_ + 1) % 4;
        } else if (screen_ == Screen::Game && gameResolved_) {
            if (gameRound_ >= activeProgramme().game.rounds) {
                screen_ = Screen::Home;
                selectedIcon_ = -1;
                iconSelectionSeconds_ = 0.0F;
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
            iconSelectionSeconds_ = 0.0F;
            setFeedback(fed ? Feedback::Success : Feedback::Blocked);
            return fed;
        }
        if (screen_ == Screen::Light) {
            const bool changed = simulation_.setLightOff(pet_, lightSelection_ == 1);
            screen_ = Screen::Home;
            selectedIcon_ = -1;
            iconSelectionSeconds_ = 0.0F;
            setFeedback(changed ? Feedback::Success : Feedback::Blocked);
            menuInactivitySeconds_ = 0.0F;
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
                    iconSelectionSeconds_ = 0.0F;
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
            menuInactivitySeconds_ = 0.0F;
            return false;
        }
        if (selectedIcon_ == 3) {
            const bool changed = simulation_.giveMedicine(pet_);
            setFeedback(changed ? Feedback::Success : Feedback::Blocked);
            return changed;
        }
        if (selectedIcon_ == 4) {
            toiletWipeSource_ = display_;
            const bool changed = simulation_.cleanWaste(pet_);
            if (changed) {
                setFeedback(Feedback::None);
                transientVisual_ = TransientVisual::ToiletWipe;
                transientVisualSeconds_ = 0.0F;
            } else {
                setFeedback(Feedback::Blocked);
            }
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
        iconSelectionSeconds_ = 0.0F;
        menuInactivitySeconds_ = 0.0F;
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
    for (std::size_t index = 0; index < DeviceShellGeometry::Buttons.size(); ++index) {
        const ShellPoint& button = DeviceShellGeometry::Buttons[index];
        const float deltaX = deviceX - static_cast<float>(button.x);
        const float deltaY = deviceY - static_cast<float>(button.y);
        if (deltaX * deltaX + deltaY * deltaY
            <= static_cast<float>(DeviceShellGeometry::ButtonHitRadius
                * DeviceShellGeometry::ButtonHitRadius)) {
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
    const float deltaX = deviceX - static_cast<float>(DeviceShellGeometry::ResetX);
    const float deltaY = deviceY - static_cast<float>(DeviceShellGeometry::ResetY);
    return deltaX * deltaX + deltaY * deltaY
        <= static_cast<float>(DeviceShellGeometry::ResetRadius * DeviceShellGeometry::ResetRadius);
}

void CnaTamagotchiGame::moveSelectionBackward() noexcept
{
    if (screen_ == Screen::Home) {
        selectedIcon_ = selectedIcon_ < 0 ? SelectableIconCount - 1
                                          : (selectedIcon_ + SelectableIconCount - 1)
                % SelectableIconCount;
        iconSelectionSeconds_ = 0.0F;
    } else if (screen_ == Screen::Food) {
        foodSelection_ = (foodSelection_ + 1)
            % static_cast<int>(activeProgramme().food.size());
    } else if (screen_ == Screen::Light) {
        lightSelection_ = (lightSelection_ + 1) % 2;
        menuInactivitySeconds_ = 0.0F;
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
    shellId_ = data.shellId;
    seed_ = data.seed;
    lastSavedUnixSeconds_ = data.lastSavedUnixSeconds;
    screen_ = Screen::Home;
    selectedIcon_ = -1;
    iconSelectionSeconds_ = 0.0F;
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
        .shellId = shellId_,
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

void CnaTamagotchiGame::beginClockSetup(const bool returnToClockView) noexcept
{
    clockSetupMinutes_ = pet_.clockMinutesOfDay;
    clockSetupReturnsToClockView_ = returnToClockView;
    screen_ = Screen::ClockSetup;
}

void CnaTamagotchiGame::resetPetToEgg() noexcept
{
    pet_ = Domain::ProgramPetState{};
    screen_ = Screen::Home;
    selectedIcon_ = -1;
    iconSelectionSeconds_ = 0.0F;
    clockSetupReturnsToClockView_ = false;
    foodSelection_ = 0;
    menuInactivitySeconds_ = 0.0F;
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
        Display::P1LightScreen::renderMenu(display_, lightSelection_ == 1);
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

    if (transientVisual_ == TransientVisual::ToiletWipe) {
        Display::P1ToiletWipe::render(display_, toiletWipeSource_,
            Display::P1ToiletWipe::phaseAt(transientVisualSeconds_));
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

    if (pet_.asleep && pet_.lightOff) {
        const Domain::P1Sprite& sleep = Domain::P1SpriteCatalog::sleepIndicator();
        const Domain::P1SpriteFrame& sleepFrame = sleep.idleFrame(
            static_cast<std::size_t>(backgroundTimeSeconds_ / sleep.idleFrameSeconds));
        Display::P1LightScreen::renderLightsOut(display_, sleepFrame.originX,
            sleepFrame.originY, sleepFrame.visibleRows());
        return;
    }

    const Domain::P1Sprite& sprite = pet_.sick
        ? Domain::P1SpriteCatalog::sickSpriteForCharacter(pet_.characterId)
        : Domain::P1SpriteCatalog::spriteForCharacter(pet_.characterId);
    // P1 home animation consists of independently transcribed LCD phases.
    // Each phase carries its observed origin, rather than turning one modern
    // sprite into a synthetic bobbing animation. Unobserved sleeping forms
    // retain the first quiet pose; the observed Marutchi body keeps its normal
    // independent cycle.
    const bool freezeUnobservedSleepPose = pet_.asleep && pet_.characterId != "marutchi";
    const std::size_t idleFrame = freezeUnobservedSleepPose ? 0U : static_cast<std::size_t>(
        backgroundTimeSeconds_ / sprite.idleFrameSeconds);
    const Domain::P1SpriteFrame& frame = sprite.idleFrame(idleFrame);
    display_.drawSprite(frame.originX, frame.originY, frame.visibleRows());

    if (pet_.asleep) {
        const Domain::P1Sprite& sleep = Domain::P1SpriteCatalog::sleepIndicator();
        const Domain::P1SpriteFrame& sleepFrame = sleep.idleFrame(static_cast<std::size_t>(
            backgroundTimeSeconds_ / sleep.idleFrameSeconds));
        display_.drawSprite(sleepFrame.originX, sleepFrame.originY, sleepFrame.visibleRows());
    }

    if (pet_.sick) {
        const Domain::P1SpriteFrame& sickness = Domain::P1SpriteCatalog::sicknessIndicator();
        display_.drawSprite(sickness.originX, sickness.originY, sickness.visibleRows());
    }

    const int visibleWaste = std::min(pet_.wasteCount, 2);
    const Domain::P1Sprite& wasteSprite = Domain::P1SpriteCatalog::waste();
    const std::size_t wastePhase = static_cast<std::size_t>(
        backgroundTimeSeconds_ / wasteSprite.idleFrameSeconds);
    const Domain::P1SpriteFrame& wasteFrame = wasteSprite.idleFrame(wastePhase);
    for (int waste = 0; waste < visibleWaste; ++waste) {
        const int y = wasteFrame.originY - waste * 8;
        display_.drawSprite(wasteFrame.originX, y, wasteFrame.visibleRows());
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

    const Display::LcdPaletteColours lcdColours =
        Display::MonochromeDisplay::coloursFor(lcdPalette_);
    const Color lcdBezel = asColor(lcdColours.bezel);
    const Color lcdOff = asColor(lcdColours.off);
    const Color lcdOn = asColor(lcdColours.on);
    const Color iconBandTop(
        std::min(255, static_cast<int>(lcdColours.off.red) + 13),
        std::min(255, static_cast<int>(lcdColours.off.green) + 13),
        std::min(255, static_cast<int>(lcdColours.off.blue) + 7), 255);
    const Color iconBandBottom(
        std::max(0, static_cast<int>(lcdColours.off.red) - 10),
        std::max(0, static_cast<int>(lcdColours.off.green) - 10),
        std::max(0, static_cast<int>(lcdColours.off.blue) - 5), 255);

    const Presentation::DeviceShellStyle& shellStyle =
        Presentation::deviceShellStyle(shellId_);
    DeviceShellRenderer::drawBody(
        *spriteBatch_, *pixelTexture_, shellStyle, backgroundColor());

    // The active game bitmap is exactly 32 × 16 pixels. The eight permanent
    // icon cells live in the physically connected top/bottom LCD surround;
    // they must never consume rows from that game bitmap.
    const int moduleX = DisplayX - LcdModulePadding;
    const int moduleY = DisplayY - IconBandHeight - LcdModulePadding;
    const int moduleWidth = DisplayPixelWidth + LcdModulePadding * 2;
    const int moduleHeight = DisplayPixelHeight + IconBandHeight * 2 + LcdModulePadding * 2;
    drawRect(Rectangle(moduleX, moduleY, moduleWidth, moduleHeight),
        asColor(shellStyle.outline));
    drawRect(Rectangle(moduleX + 4, moduleY + 4, moduleWidth - 8, moduleHeight - 8),
        asColor(shellStyle.bodyShadow));
    drawRect(Rectangle(moduleX + 8, moduleY + 8, moduleWidth - 16, moduleHeight - 16),
        lcdBezel);
    drawRect(Rectangle(DisplayX, DisplayY - IconBandHeight,
        DisplayPixelWidth, IconBandHeight), iconBandTop);
    drawRect(Rectangle(DisplayX, DisplayY, DisplayPixelWidth, DisplayPixelHeight), lcdOff);
    drawRect(Rectangle(DisplayX, DisplayY + DisplayPixelHeight,
        DisplayPixelWidth, IconBandHeight), iconBandBottom);

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

    // A real P1 selects an icon by energising that LCD segment: the chosen
    // pictogram becomes fully black, without a modern box, outline, or cursor.
    // Keep dormant segments faintly visible here so all eight controls remain
    // legible on a desktop display, while retaining the same strong on/off
    // contrast. The atlas is black plus alpha, so tint alpha alone controls
    // the apparent LCD density without introducing a coloured cell background.
    for (int index = 0; index < IconCount; ++index) {
        const int slot = index % 4;
        const bool topBand = index < 4;
        const int bandY = topBand ? DisplayY - IconBandHeight : DisplayY + DisplayPixelHeight;
        const int slotWidth = DisplayPixelWidth / 4;
        const int iconX = DisplayX + slot * slotWidth + (slotWidth - IconDrawWidth) / 2;
        const int iconY = bandY + (IconBandHeight - IconDrawHeight) / 2;
        const Rectangle source(slot * IconAtlasCellWidth, (index / 4) * IconAtlasCellHeight,
            IconAtlasCellWidth, IconAtlasCellHeight);
        const Rectangle destination(iconX, iconY, IconDrawWidth, IconDrawHeight);

        const bool urgent = (index == 3 && pet_.sick)
            || (index == 4 && pet_.wasteCount > 0)
            || (index == 7 && pet_.attentionReason != Domain::ProgramAttentionReason::None);
        const bool energised = index == selectedIcon_ || urgent;
        constexpr std::uint8_t DormantIconOpacity = 58U;
        const Color iconTint(255, 255, 255,
            energised ? 255 : static_cast<int>(DormantIconOpacity));
        spriteBatch_->Draw(*iconAtlasTexture_, destination, source, iconTint);
    }

    // Controls are rendered last so their moulded rims sit above the body.
    DeviceShellRenderer::drawControls(*spriteBatch_, *pixelTexture_, shellStyle,
        DeviceShellControlState{.buttons = pressedButtons_, .resetPressed = resetPressed_});
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
