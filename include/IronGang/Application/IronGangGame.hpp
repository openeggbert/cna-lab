#pragma once

#include "IronGang/Core/PerformanceProfiler.hpp"
#include "IronGang/Cutscenes/CutscenePlayer.hpp"
#include "IronGang/Dialogue/DialogueSystem.hpp"
#include "IronGang/Gameplay/Pedestrian.hpp"
#include "IronGang/Gameplay/PlayerController.hpp"
#include "IronGang/Gameplay/PoliceSystem.hpp"
#include "IronGang/Gameplay/TrafficVehicle.hpp"
#include "IronGang/Gameplay/VehicleController.hpp"
#include "IronGang/Graphics/GpuFrameTimer.hpp"
#include "IronGang/Graphics/PrototypeRenderer.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"
#include "IronGang/Physics/PhysicsWorld.hpp"
#include "IronGang/World/DistrictManager.hpp"

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace IronGang
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

    class IronGangGame final : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit IronGangGame(std::string assetRoot);
        void SetSmokeFrames(int frames) noexcept { smokeFramesRemaining_ = frames; }
        void SetVerticalSync(bool enabled);
        void EnablePerformanceProfile(std::string reportPath);
        void SetPerformanceScenario(PerformanceScenario scenario) noexcept { performanceScenario_ = scenario; }
        [[nodiscard]] bool WritePerformanceReport(std::string& error) const;

        [[nodiscard]] const std::string& GetTypeName() const override;

    protected:
        void Initialize() override;
        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;
        void EndDraw() override;

    private:
        [[nodiscard]] bool WasPressed(const Microsoft::Xna::Framework::Input::KeyboardState& current,
                                      Microsoft::Xna::Framework::Input::Keys key) const;
        void HandleInteraction();
        void SavePrototype();
        void LoadPrototype();
        void ResetPrototype();
        void DrawDistrictMap(Microsoft::Xna::Framework::Graphics::SpriteBatch& spriteBatch,
                             Microsoft::Xna::Framework::Graphics::SpriteFont& font,
                             Microsoft::Xna::Framework::Graphics::Texture2D& pixel,
                             int viewportWidth,
                             int viewportHeight) const;
        void UpdateWindowTitle(float deltaSeconds);
        [[nodiscard]] std::string SavePath() const;
        // Checks the current district's exit trigger against whichever of player/vehicle is
        // active and requests a transition if it was entered (plan_13 IG-13-002/006).
        void CheckDistrictExit();
        void BeginDistrictTransition();
        // Repositions player/vehicle at the new district's spawn points once DistrictManager
        // reports the loading screen's minimum display time has elapsed (IG-13-008/009/017/018).
        void HandleDistrictArrival();
        // Gate M9: (re)populates trafficVehicles_/pedestrians_ from the current district's
        // WaypointPath data and resets police_ -- called whenever the world/district changes
        // (Initialize, district arrival, load, reset), since none of this ambient state is part
        // of SaveGame (plan_19/20/21/22's own scope note: no NPC/wanted persistence yet).
        void RespawnTrafficAndPedestrians();
        void RecordRenderWorkload();
        void RecordPhysicsWorkload();
        [[nodiscard]] std::string_view CurrentPerformancePhase() const noexcept;
        void CaptureSwapIntervalAcceptance();
        void CaptureGraphicsRuntimeIdentity();
        void RecordDistrictLoadSample(const char* reason,
                                      DistrictId sourceDistrict,
                                      double worldPhysicsMilliseconds,
                                      double rendererUploadMilliseconds,
                                      std::uint64_t residentBytesBefore,
                                      std::uint64_t trackedVideoMemoryBytesBefore);
        [[nodiscard]] std::uint64_t GetTrackedRendererVideoMemoryBytes() const;

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
        // Gate M10 (plan_28-ui-hud-menus-accessibility-and-input-rebinding.md): a real on-screen
        // HUD replacing the window-title-only display, using a hand-built bitmap SpriteFont (see
        // IronGang/UI/BitmapFont.hpp) since CNA has no XNB font content pipeline. Both are
        // optional (constructed in Initialize(), not default-constructible/assignable in a way
        // that suits a plain member) rather than needed for any fallback -- unlike Model loads,
        // there is no "missing asset" case here, the font is always built in-process.
        std::optional<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
        std::optional<Microsoft::Xna::Framework::Graphics::SpriteFont> hudFont_;
        std::optional<Microsoft::Xna::Framework::Graphics::Texture2D> mapPixel_;
        // Gate M9 (plan_19/20/21/22-...): ambient traffic/pedestrians and the one police-response
        // scenario. Empty in districts with no WaypointPath data (e.g. Countryside), so they are
        // naturally inert there -- see PrototypeWorld::GetTrafficLoop()/GetSidewalkPaths().
        std::vector<TrafficVehicle> trafficVehicles_;
        std::vector<Pedestrian> pedestrians_;
        PoliceSystem police_;

        // Gate M10 audio (plan_27-audio-music-ambience-and-radio.md): real CC0 sound assets
        // (Nox Sound Design's "Essentials Series", itch.io, tracked in
        // assets/licenses/asset-registry.csv), each optional and loaded with the same
        // try/catch-with-fallback convention as every other optional asset -- a missing file, or
        // no audio hardware at all (NoAudioHardwareException, a real risk in this sandboxed
        // environment), degrades to silence rather than a crash. No ambience/siren this pass --
        // the chosen pack has no matching content; see NEXT.md.
        std::optional<Microsoft::Xna::Framework::Audio::SoundEffect> engineSound_;
        std::optional<Microsoft::Xna::Framework::Audio::SoundEffectInstance> engineSoundInstance_;
        std::optional<Microsoft::Xna::Framework::Audio::SoundEffect> footstepSound_;
        std::optional<Microsoft::Xna::Framework::Audio::SoundEffect> hornSound_;
        float footstepTimer_{0.0F};
        static constexpr float kFootstepIntervalSeconds = 0.4F;
        Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_{};
        bool mapVisible_{false};
        bool playerDriving_{false};
        float titleRefreshTimer_{0.0F};
        std::string transientStatus_;
        float transientStatusSeconds_{0.0F};
        int smokeFramesRemaining_{-1};

        // Gate M12: enabled only by --profile, so ordinary per-frame play pays no clock reads or
        // sample-vector growth beyond negligible IsEnabled() checks. Infrequent load paths retain
        // isolated Clock calls. The pending values retain the real synchronous phases across the
        // loading screen's cosmetic delay; that delay is never counted as load work.
        PerformanceProfiler performanceProfiler_;
        std::unique_ptr<GpuFrameTimer> gpuFrameTimer_;
        std::string performanceReportPath_;
        double pendingDistrictWorldPhysicsMilliseconds_{0.0};
        std::uint64_t pendingDistrictResidentBytesBefore_{0};
        std::uint64_t pendingDistrictVideoMemoryBytesBefore_{0};
        DistrictId pendingDistrictSource_{DistrictId::WarehouseBlock};
        PerformanceScenario performanceScenario_{PerformanceScenario::InteractiveOrIntro};
        int performanceScenarioUpdate_{0};
        std::size_t peakPhysicsBodyCount_{0};
        std::size_t peakTrafficVehicleCount_{0};
        std::size_t peakPedestrianCount_{0};
        int peakPoliceVehicleCount_{0};
        bool swapIntervalApplyResultKnown_{false};
        bool swapIntervalApplySucceeded_{false};
        std::optional<int> appliedSwapInterval_;
        std::string swapIntervalUnavailableReason_;
        bool graphicsRuntimeIdentityKnown_{false};
        std::string graphicsRuntimeVendor_;
        std::string graphicsRuntimeRenderer_;
        std::string graphicsRuntimeVersion_;
        std::string graphicsRuntimeUnavailableReason_;

        // Gate M6 vehicle entry/exit animation state (see VehicleTransitionState's own comment).
        VehicleTransitionState vehicleTransitionState_{VehicleTransitionState::None};
        float vehicleTransitionSecondsRemaining_{0.0F};
        static constexpr float kVehicleTransitionSeconds = 0.5F; // matches EnterVehicle/ExitVehicle's authored clip duration
    };
}
