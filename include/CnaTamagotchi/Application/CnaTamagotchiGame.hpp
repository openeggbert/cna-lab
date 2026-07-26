#pragma once

#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"

#include <memory>
#include <optional>

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
    explicit CnaTamagotchiGame(bool smokeTest = false);

    GetTypeNameHPP()

protected:
    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    [[nodiscard]] Microsoft::Xna::Framework::Color backgroundColor() const;
    void seedDemoDisplay() noexcept;
    void drawDevice();

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    Display::MonochromeDisplay display_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    std::optional<Microsoft::Xna::Framework::Graphics::Texture2D> pixelTexture_;
    float backgroundTimeSeconds_{0.0F};
    bool smokeTest_{false};
    unsigned int drawnFrames_{0};
};

} // namespace CnaTamagotchi::Application
