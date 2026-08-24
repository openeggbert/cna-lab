#pragma once

#include "explore2d/Persistence.hpp"
#include "explore2d/Renderer.hpp"

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace Microsoft::Xna::Framework {
class GameTime;
class GraphicsDeviceManager;
namespace Graphics {
class SamplerState;
class SpriteBatch;
class Texture2D;
}
}

namespace explore2d::cna {

struct HostConfig final {
    std::string windowTitle{"Explore2D"};
    int presentationScale{2};
    std::filesystem::path savePath{"explore2d.e2dsave"};
    std::size_t exitAfterFrames{};
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
    Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_{};
    std::size_t updateCounter_{};
    std::size_t renderedFrames_{};

    [[nodiscard]] bool pressed(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard, int virtualKey) const;
    [[nodiscard]] bool down(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard, int virtualKey) const;
    void updateWorldInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard);
    void quickSave();
    void quickLoad();
};

} // namespace explore2d::cna
