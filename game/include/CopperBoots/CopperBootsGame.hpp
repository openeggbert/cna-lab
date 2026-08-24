#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "CopperBoots/SimulationClock.hpp"
#include "CopperBoots/WorldSimulation.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

namespace CopperBoots
{
    class CopperBootsGame final : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit CopperBootsGame(bool smokeTest = false);

        [[nodiscard]] const std::string& GetTypeName() const override;

    protected:
        void LoadContent() override;
        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

    private:
        [[nodiscard]] PlayerInput ReadPlayerInput(
            const Microsoft::Xna::Framework::Input::KeyboardState& keyboard);
        void DrawWorld();
        void DrawParallax(float cameraX);
        void DrawTiles(float cameraX, float cameraY);
        void DrawCogs(float cameraX, float cameraY);
        void DrawPlayer(float cameraX, float cameraY);
        void FillRectangle(const Microsoft::Xna::Framework::Rectangle& rectangle,
                           const Microsoft::Xna::Framework::Color& color);
        [[nodiscard]] Microsoft::Xna::Framework::Rectangle PresentationRectangle();

        static constexpr int LogicalWidth = 320;
        static constexpr int LogicalHeight = 180;

        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> solidTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> logicalTarget_;
        Microsoft::Xna::Framework::Graphics::SamplerState pointSampler_;
        Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_;
        WorldSimulation world_;
        SimulationClock clock_;
        bool jumpLatched_ = false;
        bool smokeTest_ = false;
        std::uint32_t drawnFrames_ = 0;
    };
}
