#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"
#include "CnaTamagotchi/Domain/CreatureCatalog.hpp"
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

Color asColor(const Display::LcdColour colour) noexcept
{
    return Color(colour.red, colour.green, colour.blue, 255U);
}

struct ButtonPosition final {
    int x;
};

constexpr int IconCount = 8;
constexpr int ButtonY = 555;
constexpr int ResetButtonX = 408;
constexpr int ResetButtonY = 542;
constexpr int ResetButtonRadius = 10;
constexpr float ResetHoldSeconds = 1.5F;
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
    std::error_code error;
    const std::filesystem::path workingDirectory = std::filesystem::current_path(error);
    if (error) {
        return std::filesystem::path("saves") / "slot-1.json";
    }
    return Persistence::SaveLocation::resolveSlot(
        workingDirectory, Persistence::SaveLocation::platformDataDirectory());
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
    Game::getWindowProperty().setTitleProperty("CNA Tamagotchi");
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
    bool resetHeld = keyboard.IsKeyDown(Keys::R);

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

    if (screen_ != Screen::SaveRecovery && screen_ != Screen::ResetConfirm) {
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
            static_cast<void>(simulation_.advance(pet_, 1));
            simulationSeconds_ -= 60.0F;
            lastSavedUnixSeconds_ += 60;
            saveChanged = true;
        }
    } else {
        resetHoldSeconds_ = 0.0F;
    }
    if (saveChanged) {
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

    if (pet_.lifeStage == Domain::LifeStage::Farewell) {
        if (button == DeviceButton::B) {
            startNewEgg();
            setFeedback(Feedback::Success);
            return true;
        }
        return false;
    }

    switch (button) {
    case DeviceButton::A:
        if (screen_ == Screen::Home) {
            selectedIcon_ = (selectedIcon_ + 1) % IconCount;
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
            setFeedback(Feedback::Success);
            return true;
        }
        if (screen_ == Screen::Status) {
            statusPage_ = (statusPage_ + 1) % 4;
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
            setFeedback(Feedback::Blocked);
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
                const bool hasEffect = (*action != Domain::PetAction::ToggleLight || pet_.asleep)
                    && (*action != Domain::PetAction::Medicine || pet_.sick)
                    && (*action != Domain::PetAction::Clean
                        || pet_.wasteCount > 0 || pet_.needs.hygiene < 100)
                    && (*action != Domain::PetAction::Discipline
                        || pet_.attentionReason == Domain::AttentionReason::Discipline);
                if (!hasEffect) {
                    setFeedback(Feedback::Blocked);
                    return false;
                }
                simulation_.applyAction(pet_, *action);
                setFeedback(Feedback::Success);
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
        selectedIcon_ = (selectedIcon_ + IconCount - 1) % IconCount;
    } else if (screen_ == Screen::Food) {
        foodSelection_ = (foodSelection_ + 1) % 2;
    } else if ((screen_ == Screen::Game || screen_ == Screen::NumberGame) && !gameResolved_) {
        gameChoice_ = (gameChoice_ + 1) % 2;
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
        return true;
    }

    const Persistence::LoadResult loaded = saveRepository_.load(savePath_);
    if (!loaded.data) {
        recoveryBackupAvailable_ = saveRepository_.load(savePath_.string() + ".bak").success();
        recoveryChoice_ = recoveryBackupAvailable_ ? RecoveryChoice::RestoreBackup
                                                    : RecoveryChoice::NewEgg;
        screen_ = Screen::SaveRecovery;
        return false;
    }

    return activateSave(*loaded.data);
}

bool CnaTamagotchiGame::activateSave(const Persistence::SaveData& data)
{
    pet_ = data.pet;
    seed_ = data.seed;
    lastSavedUnixSeconds_ = data.lastSavedUnixSeconds;
    screen_ = Screen::Home;
    selectedIcon_ = 0;
    simulationSeconds_ = 0.0F;

    const std::int64_t now = unixSecondsNow();
    if (now <= lastSavedUnixSeconds_) {
        return false;
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
        return true;
    }

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
    recoveryChoice_ = RecoveryChoice::NewEgg;
    static_cast<void>(activateSave(*backup.data));
    return true;
}

bool CnaTamagotchiGame::archiveAndStartFreshEgg()
{
    if (!saveRepository_.archiveCorruptSave(savePath_).success) {
        return false;
    }

    recoveryBackupAvailable_ = false;
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
    if (smokeTest_ || !saveDirty_) {
        return;
    }

    // This is an elapsed-time anchor, rather than merely the instant of the
    // file write. It never moves backwards, and it retains sub-minute time.
    const Persistence::SaveData data{
        .lastSavedUnixSeconds = lastSavedUnixSeconds_,
        .seed = seed_,
        .pet = pet_,
    };
    const Persistence::SaveResult result = saveRepository_.save(savePath_, data);
    if (result.success) {
        saveDirty_ = false;
    }
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
    resetPetToEgg();
}

void CnaTamagotchiGame::startFreshEgg() noexcept
{
    seed_ = static_cast<std::uint64_t>(unixSecondsNow());
    resetPetToEgg();
}

void CnaTamagotchiGame::resetPetToEgg() noexcept
{
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

void CnaTamagotchiGame::setFeedback(const Feedback feedback) noexcept
{
    feedback_ = feedback;
    feedbackSeconds_ = feedback == Feedback::None ? 0.0F : 0.8F;
}

void CnaTamagotchiGame::refreshDisplay() noexcept
{
    display_.clear();

    // The original-style device face keeps all eight care pictograms inside
    // the 32 × 16 LCD: four above the creature and four below it. A selected
    // or urgent icon is inverted, which remains legible on a true one-bit LCD.
    const auto drawLcdIcon = [this](const int index) {
        const int slotX = (index % 4) * 8;
        const int slotY = index < 4 ? 0 : 13;
        const bool urgent = (index == 3 && pet_.sick)
            || (index == 4 && pet_.wasteCount > 0)
            || (index == 7 && pet_.attentionReason != Domain::AttentionReason::None);
        const bool inverted = index == selectedIcon_ || urgent;
        if (inverted) {
            display_.fillRectangle(slotX, slotY, 8, 3, true);
        }
        const auto pixel = [this, slotX, slotY, inverted](const int x, const int y) {
            display_.setPixel(slotX + x, slotY + y, !inverted);
        };

        switch (index) {
        case 0: // fork and knife / Food
            pixel(2, 0); pixel(3, 0); pixel(4, 0); pixel(3, 1); pixel(3, 2);
            pixel(6, 0); pixel(6, 1); pixel(5, 2); pixel(6, 2);
            break;
        case 1: // small sun / Light
            pixel(1, 0); pixel(3, 0); pixel(5, 0); pixel(2, 1); pixel(3, 1);
            pixel(4, 1); pixel(3, 2);
            break;
        case 2: // ball / Game
            pixel(2, 0); pixel(3, 0); pixel(1, 1); pixel(2, 1); pixel(3, 1);
            pixel(4, 1); pixel(2, 2); pixel(3, 2);
            break;
        case 3: // compact medicine cross
            pixel(3, 0); pixel(2, 1); pixel(3, 1); pixel(4, 1); pixel(3, 2);
            break;
        case 4: // toilet / Clean
            pixel(1, 0); pixel(2, 0); pixel(3, 0); pixel(4, 0); pixel(2, 1);
            pixel(3, 1); pixel(4, 1); pixel(3, 2); pixel(4, 2);
            break;
        case 5: // scale / Status
            pixel(3, 0); pixel(1, 1); pixel(2, 1); pixel(3, 1); pixel(4, 1);
            pixel(5, 1); pixel(2, 2); pixel(4, 2);
            break;
        case 6: // bell / Discipline
            pixel(3, 0); pixel(2, 1); pixel(3, 1); pixel(4, 1); pixel(1, 2);
            pixel(2, 2); pixel(3, 2); pixel(4, 2); pixel(5, 2);
            break;
        case 7: // clock / Attention
            pixel(2, 0); pixel(3, 0); pixel(4, 0); pixel(1, 1); pixel(3, 1);
            pixel(4, 1); pixel(2, 2); pixel(3, 2); pixel(4, 2);
            break;
        default:
            break;
        }
    };
    for (int index = 0; index < IconCount; ++index) {
        drawLcdIcon(index);
    }

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

    if (screen_ == Screen::SaveRecovery) {
        if (recoveryBackupAvailable_) {
            display_.drawText(8, 3, "REST");
            display_.drawText(10, 8, "NEW");
            const int markerY = recoveryChoice_ == RecoveryChoice::RestoreBackup ? 4 : 9;
            display_.setPixel(4, markerY, true);
            display_.setPixel(5, markerY + 1, true);
            display_.setPixel(4, markerY + 2, true);
        } else {
            display_.drawText(8, 3, "NONE");
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

    if (screen_ == Screen::Food) {
        display_.drawText(6, 3, "MEAL");
        display_.drawText(6, 8, "SNACK");
        const int markerY = foodSelection_ == 0 ? 4 : 9;
        display_.setPixel(1, markerY, true);
        display_.setPixel(2, markerY + 1, true);
        display_.setPixel(1, markerY + 2, true);
        return;
    }

    if (screen_ == Screen::Status) {
        if (statusPage_ == 0) {
            const std::string age = "AGE" + std::to_string(pet_.ageMinutes / (24 * 60));
            const std::string weight = "WGT" + std::to_string(pet_.weight);
            display_.drawText(2, 3, age);
            display_.drawText(2, 8, weight);
        } else if (statusPage_ == 1) {
            display_.drawText(1, 3, "HUN");
            drawHeartMeter(20, 5, pet_.needs.hunger);
            display_.drawText(1, 8, "HAP");
            drawHeartMeter(20, 10, pet_.needs.happiness);
        } else if (statusPage_ == 2) {
            display_.drawText(1, 3, "DIS");
            drawHeartMeter(20, 5, pet_.needs.discipline);
            const std::string mistakes = "MIST" + std::to_string(pet_.careMistakes);
            display_.drawText(2, 8, mistakes);
        } else {
            display_.drawText(8, 3, "LINE");
            display_.drawText(pet_.species == Domain::PetSpecies::Mossling ? 10 : 8,
                8, pet_.species == Domain::PetSpecies::Mossling ? "NUM" : "PEEK");
        }
        return;
    }

    if (screen_ == Screen::Game) {
        if (gameResolved_) {
            display_.drawText(gameWon_ ? 10 : 8, 6, gameWon_ ? "WIN" : "LOSE");
            return;
        }

        display_.drawText(8, 3, "PICK");
        // Two deliberately abstract peek positions; the selected position is
        // marked below it, rather than using a modern text button.
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

    if (screen_ == Screen::NumberGame) {
        if (gameResolved_) {
            display_.drawText(gameWon_ ? 10 : 8, 6, gameWon_ ? "WIN" : "LOSE");
            return;
        }

        display_.drawText(10, 3, "NUM");
        display_.drawText(14, 8, std::to_string(currentNumber_));
        if (gameChoice_ == 0) { // lower
            display_.setPixel(16, 10, true);
            display_.setPixel(15, 11, true);
            display_.setPixel(16, 11, true);
            display_.setPixel(17, 11, true);
            display_.setPixel(16, 12, true);
        } else { // higher
            display_.setPixel(16, 10, true);
            display_.setPixel(16, 11, true);
            display_.setPixel(15, 12, true);
            display_.setPixel(16, 12, true);
            display_.setPixel(17, 12, true);
        }
        return;
    }

    if (pet_.lifeStage == Domain::LifeStage::Farewell) {
        display_.drawText(10, 6, "NEW");
        return;
    }

    drawHeartMeter(1, 3, pet_.needs.hunger);
    drawHeartMeter(1, 6, pet_.needs.happiness);
    drawHeartMeter(1, 9, pet_.needs.discipline);

    const Domain::CreatureForm form = Domain::CreatureCatalog::formFor(pet_);
    const Domain::CreatureSprite& sprite = Domain::CreatureCatalog::spriteFor(form);
    display_.drawSprite(13, 3, sprite.rows);

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

    // Drop shadow, shell rim, and inner egg. The shorter, tapered silhouette
    // is original, while retaining the broad handheld proportions of a
    // 1990s virtual-pet device.
    drawEllipse(278, 638, 164, 15, Color(178, 129, 111, 44));
    drawEgg(270, 348, 220, 272, ShellOutline);
    drawEgg(270, 346, 208, 260, ShellMain);
    drawEllipse(238, 318, 135, 190, ShellHighlight);

    // Small keychain tab: recognisable, but deliberately generic.
    drawRing(270, 72, 20, ShellOutline, backgroundColor());
    drawRing(270, 72, 14, ShellMain, backgroundColor());

    // The recessed LCD is exactly 32 × 16 logical pixels at 8× scale.
    drawRect(Rectangle(DisplayX - 12, DisplayY - 12,
        DisplayPixelWidth + 24, DisplayPixelHeight + 24), lcdBezel);
    drawRect(Rectangle(DisplayX - 6, DisplayY - 6,
        DisplayPixelWidth + 12, DisplayPixelHeight + 12), ShellShadow);
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

    // Three physical controls: A changes selection, B confirms, C clears it.
    for (const ButtonPosition button : ButtonPositions) {
        drawEllipse(button.x, ButtonY + 4, 25, 25, ShellOutline);
        drawEllipse(button.x, ButtonY, 20, 20, ShellHighlight);
        drawEllipse(button.x - 3, ButtonY - 4, 8, 8, Color(255, 238, 244, 255));
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
    if (saveDirty_) {
        saveNow();
    }
    Game::OnExiting(sender, args);
}

GetTypeNameCPP(CnaTamagotchiGame, "CnaTamagotchiGame")

} // namespace CnaTamagotchi::Application
