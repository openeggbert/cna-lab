#pragma once

#include "CnaTamagotchi/Display/MonochromeDisplay.hpp"
#include "CnaTamagotchi/Domain/PetSimulation.hpp"

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
    void refreshDisplay() noexcept;
    void drawDevice();

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    Display::MonochromeDisplay display_;
    Domain::PetState pet_;
    Domain::PetSimulation simulation_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    std::optional<Microsoft::Xna::Framework::Graphics::Texture2D> pixelTexture_;
    float backgroundTimeSeconds_{0.0F};
    float simulationSeconds_{0.0F};
    int selectedIcon_{0};
    bool selectNextWasDown_{false};
    bool selectPreviousWasDown_{false};
    bool confirmWasDown_{false};
    bool cancelWasDown_{false};
    bool smokeTest_{false};
    unsigned int drawnFrames_{0};
};

} // namespace CnaTamagotchi::Application
