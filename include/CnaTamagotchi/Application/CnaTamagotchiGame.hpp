#pragma once

#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"
#include "CnaTamagotchi/Domain/ProgramSimulation.hpp"
#include "CnaTamagotchi/Persistence/SaveRepository.hpp"
#include "CnaTamagotchi/Presentation/DeviceShell.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace CnaTamagotchi::Application {

// CNA adapter only. Simulation and persistence must stay out of this class.
class CnaTamagotchiGame final : public Microsoft::Xna::Framework::Game {
public:
    explicit CnaTamagotchiGame(
        bool smokeTest = false,
        Display::LcdPalette lcdPalette = Display::LcdPalette::ClassicOlive);

    GetTypeNameHPP()

protected:
    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;
    void OnExiting(System::Object* sender, const System::EventArgs& args) override;

private:
    enum class Screen {
        Home,
        Food,
        Light,
        Status,
        Game,
        ClockView,
        ClockSetup,
        SaveRecovery,
        ResetConfirm,
    };

    enum class DeviceButton {
        A,
        B,
        C,
    };

    enum class Feedback {
        None,
        Success,
        Blocked,
    };

    enum class RecoveryChoice {
        RestoreBackup,
        NewEgg,
    };

    [[nodiscard]] Microsoft::Xna::Framework::Color backgroundColor() const;
    [[nodiscard]] bool pressButton(DeviceButton button);
    [[nodiscard]] std::optional<DeviceButton> buttonAtWindowPosition(float x, float y) const noexcept;
    [[nodiscard]] bool resetAtWindowPosition(float x, float y) const noexcept;
    void moveSelectionBackward() noexcept;
    [[nodiscard]] bool loadSave();
    [[nodiscard]] bool activateSave(const Persistence::SaveData& data);
    [[nodiscard]] bool restoreBackup();
    [[nodiscard]] bool archiveAndStartFreshEgg();
    [[nodiscard]] bool resetCurrentSession();
    void saveNow();
    void startCharacterGame() noexcept;
    void startNextCharacterRound() noexcept;
    void resolveCharacterRound(int choice) noexcept;
    void startNewEgg() noexcept;
    void startFreshEgg() noexcept;
    void beginClockSetup() noexcept;
    void resetPetToEgg() noexcept;
    void setFeedback(Feedback feedback) noexcept;
    void refreshDisplay() noexcept;
    void drawDevice();

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    Display::MonochromeDisplay display_;
    Domain::ProgramPetState pet_;
    Domain::ProgramSimulation simulation_;
    Persistence::SaveRepository saveRepository_;
    std::filesystem::path savePath_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    std::optional<Microsoft::Xna::Framework::Graphics::Texture2D> pixelTexture_;
    std::optional<Microsoft::Xna::Framework::Graphics::Texture2D> iconAtlasTexture_;
    std::string shellId_{Presentation::DefaultDeviceShellId};
    Display::LcdPalette lcdPalette_{Display::LcdPalette::ClassicOlive};
    float backgroundTimeSeconds_{0.0F};
    float simulationSeconds_{0.0F};
    Screen screen_{Screen::Home};
    int selectedIcon_{-1};
    int foodSelection_{0};
    int lightSelection_{0};
    int statusPage_{0};
    int gameChoice_{0};
    int gameTarget_{0};
    int gameRound_{0};
    int gameWins_{0};
    int clockSetupMinutes_{12 * 60};
    float feedbackSeconds_{0.0F};
    float resetHoldSeconds_{0.0F};
    std::int64_t lastSavedUnixSeconds_{0};
    std::uint64_t seed_{0};
    bool gameResolved_{false};
    bool gameWon_{false};
    bool soundEnabled_{true};
    bool recoveryBackupAvailable_{false};
    bool legacySaveAwaitingArchive_{false};
    RecoveryChoice recoveryChoice_{RecoveryChoice::NewEgg};
    Feedback feedback_{Feedback::None};
    bool selectNextWasDown_{false};
    bool selectPreviousWasDown_{false};
    bool confirmWasDown_{false};
    bool cancelWasDown_{false};
    bool mouseLeftWasDown_{false};
    bool clockChordWasDown_{false};
    bool shellCycleWasDown_{false};
    bool saveDirty_{false};
    bool smokeTest_{false};
    unsigned int drawnFrames_{0};
};

} // namespace CnaTamagotchi::Application
