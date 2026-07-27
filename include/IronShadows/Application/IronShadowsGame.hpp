#pragma once

#include "IronShadows/Cutscenes/CutscenePlayer.hpp"
#include "IronShadows/Dialogue/DialogueSystem.hpp"
#include "IronShadows/Gameplay/Pedestrian.hpp"
#include "IronShadows/Gameplay/PlayerController.hpp"
#include "IronShadows/Gameplay/PoliceSystem.hpp"
#include "IronShadows/Gameplay/TrafficVehicle.hpp"
#include "IronShadows/Gameplay/VehicleController.hpp"
#include "IronShadows/Graphics/PrototypeRenderer.hpp"
#include "IronShadows/Missions/PrototypeMission.hpp"
#include "IronShadows/Physics/PhysicsWorld.hpp"
#include "IronShadows/World/DistrictManager.hpp"

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

#include <memory>
#include <string>
#include <vector>

namespace IronShadows
{
    // Gate M6 vehicle entry/exit: which one-shot animation clip (if any) is currently playing
    // while the character briefly stays visible (Entering) or becomes visible again (Exiting)
    // around the otherwise-instant playerDriving_ show/hide cut.
    enum class VehicleTransitionState
    {
        None,
        Entering,
        Exiting,
    };

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
        // Checks the current district's exit trigger against whichever of player/vehicle is
        // active and requests a transition if it was entered (plan_13 IS-13-002/006).
        void CheckDistrictExit();
        // Repositions player/vehicle at the new district's spawn points once DistrictManager
        // reports the loading screen's minimum display time has elapsed (IS-13-008/009/017/018).
        void HandleDistrictArrival();
        // Gate M9: (re)populates trafficVehicles_/pedestrians_ from the current district's
        // WaypointPath data and resets police_ -- called whenever the world/district changes
        // (Initialize, district arrival, load, reset), since none of this ambient state is part
        // of SaveGame (plan_19/20/21/22's own scope note: no NPC/wanted persistence yet).
        void RespawnTrafficAndPedestrians();

        std::unique_ptr<Microsoft::Xna::Framework::GraphicsDeviceManager> graphicsDeviceManager_;
        std::string assetRoot_;
        DistrictManager districtManager_;
        Physics::PhysicsWorld physics_;
        PlayerController player_;
        VehicleController vehicle_;
        PrototypeMission mission_;
        DialogueSystem dialogue_;
        CutscenePlayer cutscene_;
        PrototypeRenderer renderer_;
        // Gate M9 (plan_19/20/21/22-...): ambient traffic/pedestrians and the one police-response
        // scenario. Empty in districts with no WaypointPath data (e.g. Countryside), so they are
        // naturally inert there -- see PrototypeWorld::GetTrafficLoop()/GetSidewalkPaths().
        std::vector<TrafficVehicle> trafficVehicles_;
        std::vector<Pedestrian> pedestrians_;
        PoliceSystem police_;
        Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_{};
        bool playerDriving_{false};
        float titleRefreshTimer_{0.0F};
        std::string transientStatus_;
        float transientStatusSeconds_{0.0F};
        int smokeFramesRemaining_{-1};

        // Gate M6 vehicle entry/exit animation state (see VehicleTransitionState's own comment).
        VehicleTransitionState vehicleTransitionState_{VehicleTransitionState::None};
        float vehicleTransitionSecondsRemaining_{0.0F};
        static constexpr float kVehicleTransitionSeconds = 0.5F; // matches EnterVehicle/ExitVehicle's authored clip duration
    };
}
