#pragma once

#include "IronGang/Core/GameConfig.hpp"
#include "IronGang/Core/Log.hpp"
#include "IronGang/Core/PerformanceProfiler.hpp"
#include "IronGang/Core/SimulationClock.hpp"
#include "IronGang/Cutscenes/CutscenePlayer.hpp"
#include "IronGang/Dialogue/DialogueSystem.hpp"
#include "IronGang/Gameplay/Pedestrian.hpp"
#include "IronGang/Gameplay/InputContext.hpp"
#include "IronGang/Gameplay/PlayerController.hpp"
#include "IronGang/Gameplay/PoliceSystem.hpp"
#include "IronGang/Gameplay/TrafficVehicle.hpp"
#include "IronGang/Gameplay/VehicleController.hpp"
#include "IronGang/Graphics/GpuFrameTimer.hpp"
#include "IronGang/Graphics/PrototypeRenderer.hpp"
#include "IronGang/Missions/CampaignDefinition.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"
#include "IronGang/Persistence/AutosavePolicy.hpp"
#include "IronGang/Persistence/SaveGame.hpp"
#include "IronGang/Persistence/UserSettings.hpp"
#include "IronGang/Physics/PhysicsWorld.hpp"
#include "IronGang/UI/MenuModel.hpp"
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
        // plan_04 IG-04-002/008: --log-level wins over the configuration file's own logSeverity,
        // because someone passing it on the command line is debugging this run specifically.
        void SetLogSeverityOverride(LogSeverity severity) noexcept { logSeverityOverride_ = severity; }
        void SetSmokeFrames(int frames) noexcept { smokeFramesRemaining_ = frames; }
        void SetVerticalSync(bool enabled);
        void EnablePerformanceProfile(std::string reportPath);
        // plan_30 IG-30-013: write draw frame @p frame (1-based) to @p path as a PNG plus a
        // "<path>.summary.json" sidecar, then carry on. A capture failure is logged, never fatal --
        // a diagnostic must not be able to take the game down with it.
        void RequestScreenshot(std::string path, int frame);
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
        // plan_28 IG-28-007: every rebindable key is read through the player's bindings, so the
        // keys are one table rather than scattered literals -- and rebinding one actually works.
        [[nodiscard]] bool IsDown(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
                                  GameAction action) const;
        [[nodiscard]] bool WasPressed(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
                                      GameAction action) const;
        void HandleInteraction();
        void CaptureRequestedScreenshot(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
        // plan_26 IG-26-010: applies the cutscene's dialogue track to the conversation, at most
        // once per cue.
        void ApplyCutsceneDialogueCue();
        void SavePrototype();
        void LoadPrototype();
        // Writes the autosave file (a slot of its own, so an autosave never overwrites a save the
        // player made by hand) and reports what triggered it.
        void WriteAutosave(AutosaveTrigger trigger);
        // Everything a save records about where things are right now.
        [[nodiscard]] WorldStateSnapshot CaptureWorldState() const;
        [[nodiscard]] SaveSnapshot CaptureSnapshot() const;
        // What the game is listening to this frame (plan_28 IG-28-008). One place decides;
        // movement, interaction, the world's own advance, and whether a save is safe all ask it.
        [[nodiscard]] InputContext CurrentInputContext() const;
        // Why saving would be unsafe this frame, or None (plan_29 IG-29-010/011).
        [[nodiscard]] SaveBlockReason CurrentSaveBlockReason() const;
        void ResetPrototype();
        // plan_24 IG-24-010/043: R after a mission failure returns to the mission's last
        // checkpoint -- state and variables from PrototypeMission, player/vehicle/district from
        // the world snapshot taken when that checkpoint was recorded -- and clears the police
        // response, which would otherwise re-trigger the same failure within a frame. Falls back
        // to ResetPrototype() when the mission has no checkpoint to return to.
        void RetryMission();
        // plan_20 IG-20-003: marks the nearest `maxSkinnedPedestrians` for skinned drawing and
        // leaves the rest as boxes. Here rather than in the renderer because the camera position
        // is the game's knowledge.
        void MarkNearestPedestriansSkinned(std::vector<ActorPose>& pedestrians,
                                           const Microsoft::Xna::Framework::Vector3& viewer) const;
        // plan_24 IG-24-021: loads a campaign mission by id and starts it. Falls back to whatever
        // mission is already loaded if the file cannot be read, so a broken mission file costs
        // that mission rather than the session.
        bool StartMission(const std::string& missionId);
        // Records the current mission as complete and starts whatever that unlocks.
        void AdvanceCampaign();
        // plan_28 IG-28-003/004: fills the pause menu for the state the game is in right now.
        void BuildPauseMenu();
        // Runs the selected entry. Kept separate from the input handling so what each entry does
        // is one readable list rather than a branch inside a key handler.
        void ApplyMenuAction(MenuAction action);
        // Takes the world half of a checkpoint the moment the mission records a new one.
        void CaptureMissionCheckpointWorld();
        // Puts the player, the vehicle, and the district back the way a snapshot describes them.
        // Shared by loading a save and retrying from a checkpoint, which restore the same things.
        void ApplyWorldSnapshot(const WorldStateSnapshot& snapshot);
        void DrawDistrictMap(Microsoft::Xna::Framework::Graphics::SpriteBatch& spriteBatch,
                             Microsoft::Xna::Framework::Graphics::SpriteFont& font,
                             Microsoft::Xna::Framework::Graphics::Texture2D& pixel,
                             int viewportWidth,
                             int viewportHeight) const;
        void UpdateWindowTitle(float deltaSeconds);
        [[nodiscard]] std::string SavePath() const;
        [[nodiscard]] std::string AutosavePath() const;
        // Player preferences, deliberately a different file from the campaign save (plan_29
        // IG-29-005): different lifetime, different owner, and it must survive deleting saves.
        [[nodiscard]] std::string SettingsPath() const;
        // Writes the settings file and reports a failure once, rather than silently losing a
        // preference the player just set.
        void PersistSettings();
        // Master volume applied to a sound about to play.
        [[nodiscard]] float EffectiveVolume(float requestedVolume) const;
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
        CampaignDefinition campaign_;
        CampaignState campaignState_;
        std::string currentMissionId_;
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
        // plan_21 IG-21-003/007: one signal drives the crossing; each stop line reads either its
        // phase or its opposing one, so the two directions cannot both be green.
        TrafficSignal trafficSignal_;
        // Where the player's vehicle was last frame, so crossing a stop line can be detected as a
        // segment rather than a position -- at speed a car is behind the line one frame and past
        // it the next (plan_22 IG-22-001).
        Vector3 previousVehiclePosition_{};

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
        // plan_28 IG-28-004's first slice: Esc pauses instead of quitting outright, and the paused
        // world does not advance. Quitting moved behind the pause screen.
        bool paused_{false};
        // Rebuilt each time the game is paused, because what is available changes: there is
        // nothing to load before the first save, and nothing to restart before a mission starts.
        MenuModel pauseMenu_;
        UserSettings settings_;
        // World half of the mission's last checkpoint (see CaptureMissionCheckpointWorld). The
        // mission half lives in PrototypeMission; this is the world it was recorded in, and both
        // halves round-trip through the save file (plan_29 IG-29-029).
        GameConfig config_;
        SimulationClock simulationClock_;
        // Set once the clock has first refused a frame's delta, so a stall is reported once
        // instead of on every frame that follows it.
        bool reportedClockStall_{false};
        std::optional<LogSeverity> logSeverityOverride_;
        AutosaveScheduler autosave_;
        std::optional<WorldStateSnapshot> missionCheckpointWorld_;
        std::string missionCheckpointWorldStateId_;
        float titleRefreshTimer_{0.0F};
        std::string transientStatus_;
        // The cutscene dialogue cue already applied, so re-applying it every frame cannot undo a
        // player's own Advance() after the cutscene has handed control back.
        std::string appliedCutsceneCue_;
        float transientStatusSeconds_{0.0F};
        int smokeFramesRemaining_{-1};

        // Gate M12: enabled only by --profile, so ordinary per-frame play pays no clock reads or
        // sample-vector growth beyond negligible IsEnabled() checks. Infrequent load paths retain
        // isolated Clock calls. The pending values retain the real synchronous phases across the
        // loading screen's cosmetic delay; that delay is never counted as load work.
        PerformanceProfiler performanceProfiler_;
        std::unique_ptr<GpuFrameTimer> gpuFrameTimer_;
        std::string performanceReportPath_;
        std::string screenshotPath_;
        int screenshotFrame_{0};
        int drawFrameIndex_{0};
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
