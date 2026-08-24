#pragma once

#include "explore2d/Persistence.hpp"
#include "explore2d/Renderer.hpp"

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace Microsoft::Xna::Framework {
class GameTime;
class GraphicsDeviceManager;
namespace Graphics {
class SamplerState;
class SpriteBatch;
class Texture2D;
}
namespace Audio { class SoundEffect; }
}

namespace explore2d::cna {

struct HostConfig final {
    std::string windowTitle{"Explore2D"};
    int presentationScale{2};
    std::filesystem::path savePath{"explore2d.e2dsave"};
    std::filesystem::path settingsPath{"explore2d.e2dsettings"};
    std::size_t exitAfterFrames{};
    bool audioEnabled{true};
};

class AdventureGame final : public Microsoft::Xna::Framework::Game {
public:
    AdventureGame(
        WorldDefinition world,
        SessionConfig sessionConfig = {},
        RendererTheme rendererTheme = {},
        HostConfig hostConfig = {});
    ~AdventureGame() override;

    [[nodiscard]] AdventureSession& session() noexcept { return session_; }
    [[nodiscard]] const AdventureSession& session() const noexcept { return session_; }

protected:
    void Initialize() override;
    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;
    void UnloadContent() override;

private:
    enum class ShellMode : std::uint8_t { title, titleSettings, playing, pause, pauseSettings, help };

    WorldDefinition world_;
    SessionConfig sessionConfig_;
    RendererTheme rendererTheme_;
    HostConfig hostConfig_;
    std::unique_ptr<Microsoft::Xna::Framework::GraphicsDeviceManager> graphics_;
    AdventureSession session_;
    AdventureRenderer renderer_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> frameTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SamplerState> pointClamp_;
    std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> activeSound_;
    float activeSoundRemaining_{};
    bool audioAvailable_{true};
    Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_{};
    std::size_t updateCounter_{};
    std::size_t renderedFrames_{};
    ShellMode shellMode_{ShellMode::title};
    std::size_t titleSelection_{};
    std::size_t pauseSelection_{};
    std::size_t settingsSelection_{};
    ShellMode helpReturnMode_{ShellMode::playing};

    [[nodiscard]] bool pressed(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard, int virtualKey) const;
    [[nodiscard]] bool down(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard, int virtualKey) const;
    void updateWorldInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard);
    void updateTitleInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard);
    void updatePauseInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard);
    void updateSettingsInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard, bool fromPause);
    void cycleLanguage(int delta);
    void loadLanguageSetting();
    void saveLanguageSetting() const;
    void quickSave();
    void quickLoad();
    void playSoundEffect(std::string_view id);
    void drainSessionSounds();
    void updateSound(float seconds);
};

} // namespace explore2d::cna
