#pragma once

#include "IronShadows/Dialogue/DialogueSystem.hpp"
#include "IronShadows/Gameplay/PlayerController.hpp"
#include "IronShadows/Gameplay/VehicleController.hpp"
#include "IronShadows/Graphics/PrototypeRenderer.hpp"
#include "IronShadows/Missions/PrototypeMission.hpp"
#include "IronShadows/World/PrototypeWorld.hpp"

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

#include <memory>
#include <string>

namespace IronShadows
{
    class IronShadowsGame final : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit IronShadowsGame(std::string assetRoot);
        void SetSmokeFrames(int frames) noexcept { smokeFramesRemaining_ = frames; }

        [[nodiscard]] const std::string& GetTypeName() const override;

    protected:
        void Initialize() override;
        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

    private:
        [[nodiscard]] bool WasPressed(const Microsoft::Xna::Framework::Input::KeyboardState& current,
                                      Microsoft::Xna::Framework::Input::Keys key) const;
        void HandleInteraction();
        void SavePrototype();
        void LoadPrototype();
        void ResetPrototype();
        void UpdateWindowTitle(float deltaSeconds);
        [[nodiscard]] std::string SavePath() const;

        std::unique_ptr<Microsoft::Xna::Framework::GraphicsDeviceManager> graphicsDeviceManager_;
        std::string assetRoot_;
        PrototypeWorld world_;
        PlayerController player_;
        VehicleController vehicle_;
        PrototypeMission mission_;
        DialogueSystem dialogue_;
        PrototypeRenderer renderer_;
        Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_{};
        bool playerDriving_{false};
        float titleRefreshTimer_{0.0F};
        std::string transientStatus_;
        float transientStatusSeconds_{0.0F};
        int smokeFramesRemaining_{-1};
    };
}
