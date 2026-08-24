#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "CopperBoots/SimulationClock.hpp"
#include "CopperBoots/GameSettings.hpp"
#include "CopperBoots/ProceduralAudio.hpp"
#include "CopperBoots/ProgressSave.hpp"
#include "CopperBoots/ParallaxLayer.hpp"
#include "CopperBoots/InputActionAdapter.hpp"
#include "CopperBoots/WorldSimulation.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadState.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"

namespace CopperBoots
{
    class CopperBootsGame final : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit CopperBootsGame(bool smokeTest = false,
                                 bool audioEnabled = true,
                                 bool settingsEnabled = true,
                                 bool displaySmokeTest = false,
                                 std::size_t initialStage = 0);

        [[nodiscard]] const std::string& GetTypeName() const override;

    protected:
        void LoadContent() override;
        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

    private:
        [[nodiscard]] PlayerInput ReadPlayerInput(
            const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
            const Microsoft::Xna::Framework::Input::GamePadState& gamepad);
        void DrawWorld();
        void LoadGeneratedAudio();
        void PlayWorldAudio(const WorldEvents& events);
        void PlayAudioCue(AudioCue cue);
        void SaveSettings();
        void LoadCampaignStage(std::size_t stageIndex);
        void UpdateProgress(const WorldEvents& events);
        void UpdateDisplaySmokeTest();
        void ValidateDisplaySmokeState();
        void DrawParallax(float cameraX);
        void DrawParallaxLayer(const ParallaxLayer& layer, float cameraX);
        void DrawTiles(float cameraX, float cameraY);
        void DrawPlatforms(float cameraX, float cameraY);
        void DrawCogs(float cameraX, float cameraY);
        void DrawCrawlers(float cameraX, float cameraY);
        void DrawPlatingPickups(float cameraX, float cameraY);
        void DrawCapacitorPickups(float cameraX, float cameraY);
        void DrawProjectiles(float cameraX, float cameraY);
        void DrawRouteEndpoints(float cameraX, float cameraY);
        void DrawPlayer(float cameraX, float cameraY);
        void DrawHud();
        void DrawDebugOverlay(float cameraX, float cameraY);
        void DrawRouteTransitionOverlay();
        void DrawPauseOverlay();
        void DrawCompletionOverlay();
        void DrawText(std::string_view text, int x, int y,
                      const Microsoft::Xna::Framework::Color& color);
        void DrawNumber(int value, int digits, int x, int y,
                        const Microsoft::Xna::Framework::Color& color);
        void DrawSignedNumber(int value, int digits, int x, int y,
                              const Microsoft::Xna::Framework::Color& color);
        void DrawGlyph(char glyph, int x, int y,
                       const Microsoft::Xna::Framework::Color& color);
        [[nodiscard]] static std::array<std::uint8_t, 5> GlyphRows(char glyph);
        void FillRectangle(const Microsoft::Xna::Framework::Rectangle& rectangle,
                           const Microsoft::Xna::Framework::Color& color);
        void OutlineRectangle(const Microsoft::Xna::Framework::Rectangle& rectangle,
                              const Microsoft::Xna::Framework::Color& color);
        [[nodiscard]] Microsoft::Xna::Framework::Rectangle PresentationRectangle();

        static constexpr int LogicalWidth = 320;
        static constexpr int LogicalHeight = 180;

        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        GameSettings settings_;
        ProgressData progress_;
        std::size_t currentStageIndex_ = 0;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> solidTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> logicalTarget_;
        Microsoft::Xna::Framework::Graphics::SamplerState pointSampler_;
        WorldSimulation world_;
        SimulationClock clock_;
        InputActionAdapter inputAdapter_;
        std::array<std::unique_ptr<
            Microsoft::Xna::Framework::Audio::SoundEffect>, AudioCueCount>
            soundEffects_;
        bool audioEnabled_ = true;
        bool audioAvailable_ = false;
        bool settingsEnabled_ = true;
        bool progressEnabled_ = true;
        bool smokeTest_ = false;
        bool displaySmokeTest_ = false;
        bool paused_ = false;
        bool debugOverlay_ = false;
        bool debugToggleDown_ = false;
        bool fullscreenToggleDown_ = false;
        bool presentationToggleDown_ = false;
        int spriteDrawCount_ = 0;
        int worldSpriteDrawCount_ = 0;
        double frameMilliseconds_ = 0.0;
        double updateMilliseconds_ = 0.0;
        double drawMilliseconds_ = 0.0;
        std::uint32_t drawnFrames_ = 0;
        int displaySmokeStep_ = 0;
        int displaySmokeValidatedFrames_ = 0;
    };
}
