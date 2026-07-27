#include "IronShadows/Application/IronShadowsGame.hpp"

#include "IronShadows/Persistence/SaveGame.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "System/TimeSpan.hpp"

#include <cmath>
#include <iostream>
#include <numbers>
#include <optional>
#include <sstream>
#include <utility>

namespace IronShadows
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Input;

    IronShadowsGame::IronShadowsGame(std::string assetRoot)
        : graphicsDeviceManager_(std::make_unique<GraphicsDeviceManager>(this)),
          assetRoot_(std::move(assetRoot))
    {
        graphicsDeviceManager_->setPreferredBackBufferWidthProperty(1280);
        graphicsDeviceManager_->setPreferredBackBufferHeightProperty(720);
        graphicsDeviceManager_->setSynchronizeWithVerticalRetraceProperty(true);
        setTargetElapsedTimeProperty(System::TimeSpan::FromMilliseconds(1000.0 / 60.0));
        getWindowProperty().setTitleProperty("Iron Shadows - starting");
        setIsMouseVisibleProperty(true);
    }

    const std::string& IronShadowsGame::GetTypeName() const
    {
        static const std::string typeName = "IronShadows.IronShadowsGame";
        return typeName;
    }

    void IronShadowsGame::Initialize()
    {
        Game::Initialize();
        districtManager_.Initialize(physics_);
        player_.Reset(districtManager_.GetWorld().GetPlayerSpawn(), 0.0F, physics_);
        vehicle_.Reset(districtManager_.GetWorld().GetVehicleSpawn(),
                       districtManager_.GetWorld().GetVehicleSpawnYaw(), physics_);

        // Gate M7 (plan_24-mission-framework-and-scripting.md IS-24-001/004): the mission's
        // states/objectives/transitions now live in data; PrototypeMission ships with a
        // hardcoded default reproducing the same flow, so a missing/corrupt file degrades to
        // that rather than breaking the mission.
        std::string missionError;
        if (!mission_.LoadMission(assetRoot_ + "/missions/prologue.mission.json", missionError))
        {
            std::cerr << "[IronShadows] " << missionError << " -- using built-in fallback mission.\n";
        }
        mission_.Reset();

        std::string dialogueError;
        if (!dialogue_.LoadFromFile(assetRoot_ + "/dialogues/prologue.dialogue.txt", dialogueError))
        {
            std::cerr << "[IronShadows] " << dialogueError << " -- using built-in fallback.\n";
            dialogue_.LoadFallbackPrologue();
        }
        dialogue_.Start();
        if (const DialogueLine* line = dialogue_.GetCurrentLine())
        {
            std::cout << line->speaker << ": " << line->text << '\n';
        }

        // Gate M8 (plan_26-cutscenes-and-cinematic-sequencing.md IS-26-001/003): a short,
        // skippable camera-only sequence panning from an establishing shot of the warehouse
        // delivery target to the exact framing of the normal gameplay follow-camera at the
        // player's spawn point (computed by hand to match Draw()'s own camera formula exactly,
        // so the cut back to gameplay has no visible pop). Falls back to an identical hardcoded
        // sequence (same convention as dialogue/mission) if the file fails to load.
        CutsceneSequence introSequence;
        std::string cutsceneError;
        if (!LoadCutsceneSequence(assetRoot_ + "/cutscenes/prologue_intro.cutscene.json", introSequence,
                                  cutsceneError))
        {
            std::cerr << "[IronShadows] " << cutsceneError << " -- using built-in fallback cutscene.\n";
            introSequence = CutsceneSequence{
                "prologue_intro_fallback",
                1,
                2.5F,
                {
                    {0.0F, Vector3(25.0F, 12.0F, -34.0F), Vector3(0.0F, 2.0F, -34.0F)},
                    {2.5F, Vector3(0.0F, 4.65F, 27.5F), Vector3(0.0F, 1.25F, 20.0F)},
                }};
        }
        cutscene_.Start(std::move(introSequence));

        // Load the warehouse and sedan as generated CNJ models (MC3 -> glTF -> CNJ) if they have
        // been built via scripts/build-assets.sh; otherwise fall back to procedural geometry, so
        // a fresh checkout that has not run the asset pipeline still runs.
        getContentProperty().setRootDirectoryProperty(assetRoot_ + "/generated/models/cnj");

        std::optional<Graphics::Model> warehouseModel;
        try
        {
            warehouseModel = getContentProperty().Load<Graphics::Model>("warehouse");
            std::cout << "[IronShadows] Loaded generated warehouse.cnj\n";
        }
        catch (const std::exception& contentError)
        {
            std::cerr << "[IronShadows] " << contentError.what()
                      << " -- using procedural warehouse box. Run scripts/build-assets.sh"
                         " assets/source/mc3/warehouse.mc3.xml assets/generated/models to"
                         " generate it.\n";
        }

        // The sedan is authored as four single-object MC3 files (body/cabin/windshield/wheel)
        // instead of one multi-object scene: the current MC3 -> glTF -> CNJ pipeline does not
        // bake per-object node transforms into vertex data, so a multi-part scene loaded as one
        // CNJ Model would lose each part's relative position (see PrototypeRenderer.hpp's
        // VehicleModelSet comment). Iron Shadows composes the four parts itself instead. All four
        // must load for the sedan to use CNJ content; otherwise it stays fully procedural rather
        // than mixing generated and procedural parts.
        std::optional<VehicleModelSet> vehicleModels;
        try
        {
            VehicleModelSet models{
                getContentProperty().Load<Graphics::Model>("vehicle_body"),
                getContentProperty().Load<Graphics::Model>("vehicle_cabin"),
                getContentProperty().Load<Graphics::Model>("vehicle_windshield"),
                getContentProperty().Load<Graphics::Model>("vehicle_wheel")};
            vehicleModels = std::move(models);
            std::cout << "[IronShadows] Loaded generated vehicle_{body,cabin,windshield,wheel}.cnj\n";
        }
        catch (const std::exception& contentError)
        {
            std::cerr << "[IronShadows] " << contentError.what()
                      << " -- using procedural sedan. Run scripts/build-assets.sh"
                         " assets/source/mc3/vehicle_<part>.mc3.xml assets/generated/models"
                         " for body/cabin/windshield/wheel to generate it.\n";
        }

        // Gate M6: a hand-authored (not MC3 -- Mesh Craft has no rigging/skinning authoring
        // support) skinned test character, replacing the procedural on-foot player box when
        // available. See assets/source/gltf/test_character.gltf's own provenance note.
        std::optional<Graphics::Model> characterModel;
        try
        {
            characterModel = getContentProperty().Load<Graphics::Model>("test_character");
            std::cout << "[IronShadows] Loaded generated test_character.cnj\n";
        }
        catch (const std::exception& contentError)
        {
            std::cerr << "[IronShadows] " << contentError.what()
                      << " -- using procedural player box. Run"
                         " cna_tool_gltf_to_cnj assets/source/gltf/test_character.gltf"
                         " assets/generated/models/cnj test_character 1.0 to generate it.\n";
        }

        renderer_.Initialize(getGraphicsDeviceProperty(), districtManager_.GetWorld(),
                             std::move(warehouseModel), std::move(vehicleModels), std::move(characterModel));
        UpdateWindowTitle(10.0F);
    }

    bool IronShadowsGame::WasPressed(const KeyboardState& current, Keys key) const
    {
        return current.IsKeyDown(key) && previousKeyboard_.IsKeyUp(key);
    }

    void IronShadowsGame::HandleInteraction()
    {
        // Gate M6: ignore a new interaction while an enter/exit clip is already playing, rather
        // than starting a second transition mid-animation.
        if (vehicleTransitionState_ != VehicleTransitionState::None)
        {
            return;
        }

        if (!playerDriving_)
        {
            if (DistanceSquaredXZ(player_.GetPosition(), vehicle_.GetPosition()) <= 9.0F)
            {
                // Stay visible and on-foot for kVehicleTransitionSeconds while "EnterVehicle"
                // plays; playerDriving_ only flips (hiding the character) once it finishes -- see
                // the vehicle-transition tick in Update().
                vehicleTransitionState_ = VehicleTransitionState::Entering;
                vehicleTransitionSecondsRemaining_ = kVehicleTransitionSeconds;
                player_.SetPosition(vehicle_.GetPosition(), physics_);
                transientStatus_ = "Entering sedan";
                transientStatusSeconds_ = 2.0F;
            }
            else
            {
                transientStatus_ = "Move closer to the sedan";
                transientStatusSeconds_ = 2.0F;
            }
            return;
        }

        const Vector3 right = RightFromYaw(vehicle_.GetYaw());
        Vector3 exitPosition = vehicle_.GetPosition() + right * 2.2F;
        exitPosition.Y = 1.70F;
        if (!districtManager_.GetWorld().CanOccupy(exitPosition, 0.35F))
        {
            exitPosition = vehicle_.GetPosition() - right * 2.2F;
            exitPosition.Y = 1.70F;
        }
        if (districtManager_.GetWorld().CanOccupy(exitPosition, 0.35F))
        {
            // Become visible immediately (playerDriving_ = false) so "ExitVehicle" is seen right
            // where the car is, then play it for kVehicleTransitionSeconds before normal on-foot
            // Walk/Idle selection resumes.
            playerDriving_ = false;
            player_.SetPosition(exitPosition, physics_);
            player_.SetYaw(vehicle_.GetYaw(), physics_);
            vehicleTransitionState_ = VehicleTransitionState::Exiting;
            vehicleTransitionSecondsRemaining_ = kVehicleTransitionSeconds;
            transientStatus_ = "Exited sedan";
        }
        else
        {
            transientStatus_ = "No safe space to exit";
        }
        transientStatusSeconds_ = 2.0F;
    }

    void IronShadowsGame::CheckDistrictExit()
    {
        if (districtManager_.IsTransitioning())
        {
            return;
        }
        const DistrictExit& exit = districtManager_.GetWorld().GetDistrictExit();
        const Vector3& checkPosition = playerDriving_ ? vehicle_.GetPosition() : player_.GetPosition();
        if (exit.trigger.bounds.ContainsXZ(checkPosition))
        {
            districtManager_.RequestTransition(physics_);
            transientStatus_.clear();
        }
    }

    void IronShadowsGame::HandleDistrictArrival()
    {
        if (!districtManager_.ConsumeArrival())
        {
            return;
        }

        const PrototypeWorld& world = districtManager_.GetWorld();
        player_.Reset(world.GetPlayerSpawn(), 0.0F, physics_);
        vehicle_.Reset(world.GetVehicleSpawn(), world.GetVehicleSpawnYaw(), physics_);
        if (playerDriving_)
        {
            // Carry the player's vehicle across (IS-13-018): if they were driving, keep them in
            // the car at its freshly-spawned position/yaw in the new district instead of leaving
            // them on foot at the pedestrian spawn point.
            player_.SetPosition(vehicle_.GetPosition(), physics_);
            player_.SetYaw(vehicle_.GetYaw(), physics_);
        }

        renderer_.RebuildStaticGeometry(getGraphicsDeviceProperty(), world);
        transientStatus_ = "Arrived";
        transientStatusSeconds_ = 2.0F;
    }

    void IronShadowsGame::SavePrototype()
    {
        SaveSnapshot snapshot;
        snapshot.missionState = mission_.GetState();
        snapshot.playerPosition = player_.GetPosition();
        snapshot.playerYaw = player_.GetYaw();
        snapshot.vehiclePosition = vehicle_.GetPosition();
        snapshot.vehicleYaw = vehicle_.GetYaw();
        snapshot.vehicleSpeed = vehicle_.GetSpeed();
        snapshot.playerDriving = playerDriving_;
        snapshot.districtId = districtManager_.GetWorld().GetId();

        std::string error;
        if (SaveGame::Write(SavePath(), snapshot, error))
        {
            transientStatus_ = "Saved prototype state";
        }
        else
        {
            transientStatus_ = "Save failed: " + error;
        }
        transientStatusSeconds_ = 3.0F;
    }

    void IronShadowsGame::LoadPrototype()
    {
        std::string error;
        const std::optional<SaveSnapshot> snapshot = SaveGame::Read(SavePath(), error);
        if (!snapshot)
        {
            transientStatus_ = "Load failed: " + error;
            transientStatusSeconds_ = 3.0F;
            return;
        }

        if (snapshot->districtId != districtManager_.GetWorld().GetId())
        {
            districtManager_.LoadDistrict(snapshot->districtId, physics_);
            renderer_.RebuildStaticGeometry(getGraphicsDeviceProperty(), districtManager_.GetWorld());
        }

        mission_.SetState(snapshot->missionState);
        player_.Reset(snapshot->playerPosition, snapshot->playerYaw, physics_);
        vehicle_.Restore(snapshot->vehiclePosition, snapshot->vehicleYaw, snapshot->vehicleSpeed, physics_);
        playerDriving_ = snapshot->playerDriving;
        // Gate M8 save-safety: a save is never taken mid-cutscene by design (nothing writes one
        // there), but force it to its terminal state anyway so loading can never leave the game
        // with an active cutscene camera fighting a restored, unrelated player/vehicle position.
        cutscene_.Skip();
        vehicleTransitionState_ = VehicleTransitionState::None; // no mid-clip state is ever saved
        transientStatus_ = "Loaded prototype state";
        transientStatusSeconds_ = 3.0F;
    }

    void IronShadowsGame::ResetPrototype()
    {
        districtManager_.LoadDistrict(DistrictId::WarehouseBlock, physics_);
        renderer_.RebuildStaticGeometry(getGraphicsDeviceProperty(), districtManager_.GetWorld());

        player_.Reset(districtManager_.GetWorld().GetPlayerSpawn(), 0.0F, physics_);
        vehicle_.Reset(districtManager_.GetWorld().GetVehicleSpawn(),
                       districtManager_.GetWorld().GetVehicleSpawnYaw(), physics_);
        mission_.Reset();
        dialogue_.Start();
        // The intro cutscene deliberately does not replay on reset (unlike dialogue) -- "R" is a
        // developer/QA shortcut back to a clean gameplay-ready state, not a full replay of the
        // opening cinematic; skipping straight to its terminal state keeps that state consistent.
        cutscene_.Skip();
        playerDriving_ = false;
        vehicleTransitionState_ = VehicleTransitionState::None;
        transientStatus_ = "Prototype reset";
        transientStatusSeconds_ = 2.0F;
    }

    std::string IronShadowsGame::SavePath() const
    {
        return "runtime/iron_shadows_prototype.save";
    }

    void IronShadowsGame::Update(GameTime& gameTime)
    {
        Game::Update(gameTime);
        const float deltaSeconds = static_cast<float>(
            gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
        const KeyboardState keyboard = Keyboard::GetState();

        if (keyboard.IsKeyDown(Keys::Escape))
        {
            Exit();
            return;
        }

        districtManager_.Update(deltaSeconds);
        HandleDistrictArrival();
        const bool transitioning = districtManager_.IsTransitioning();

        // Gate M8: ticks independently of dialogue's own pace -- the cutscene has its own short,
        // fixed duration and naturally hands control back on its own, but pressing Enter while
        // dialogue has already finished (a player racing through the opening) skips straight to
        // its terminal camera state instead (IS-26-004).
        if (!transitioning)
        {
            cutscene_.Update(deltaSeconds);
        }

        if (WasPressed(keyboard, Keys::Enter))
        {
            if (dialogue_.IsActive())
            {
                dialogue_.Advance();
                if (const DialogueLine* line = dialogue_.GetCurrentLine())
                {
                    std::cout << line->speaker << ": " << line->text << '\n';
                }
            }
            else if (cutscene_.IsActive())
            {
                cutscene_.Skip();
            }
        }

        if (WasPressed(keyboard, Keys::E) && !dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning)
        {
            HandleInteraction();
        }
        if (WasPressed(keyboard, Keys::F5))
        {
            SavePrototype();
        }
        if (WasPressed(keyboard, Keys::F9))
        {
            LoadPrototype();
        }
        if (WasPressed(keyboard, Keys::R))
        {
            ResetPrototype();
        }

        if (!dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning)
        {
            if (playerDriving_)
            {
                VehicleInput input;
                input.throttle = (keyboard.IsKeyDown(Keys::W) || keyboard.IsKeyDown(Keys::Up) ? 1.0F : 0.0F) -
                                 (keyboard.IsKeyDown(Keys::S) || keyboard.IsKeyDown(Keys::Down) ? 1.0F : 0.0F);
                input.steering = (keyboard.IsKeyDown(Keys::D) || keyboard.IsKeyDown(Keys::Right) ? 1.0F : 0.0F) -
                                 (keyboard.IsKeyDown(Keys::A) || keyboard.IsKeyDown(Keys::Left) ? 1.0F : 0.0F);
                input.handbrake = keyboard.IsKeyDown(Keys::Space);
                vehicle_.Update(deltaSeconds, input, physics_);
                player_.SetPosition(vehicle_.GetPosition(), physics_);
                player_.SetYaw(vehicle_.GetYaw(), physics_);
            }
            else if (vehicleTransitionState_ == VehicleTransitionState::None)
            {
                // Suppressed while an enter/exit clip is playing (see the vehicle-transition tick
                // below) -- the character briefly stops responding to on-foot input during the
                // clip, matching how dialogue already freezes movement.
                OnFootInput input;
                input.forward = (keyboard.IsKeyDown(Keys::W) || keyboard.IsKeyDown(Keys::Up) ? 1.0F : 0.0F) -
                                (keyboard.IsKeyDown(Keys::S) || keyboard.IsKeyDown(Keys::Down) ? 1.0F : 0.0F);
                input.strafe = (keyboard.IsKeyDown(Keys::D) ? 1.0F : 0.0F) -
                               (keyboard.IsKeyDown(Keys::A) ? 1.0F : 0.0F);
                input.turn = (keyboard.IsKeyDown(Keys::Right) ? 1.0F : 0.0F) -
                             (keyboard.IsKeyDown(Keys::Left) ? 1.0F : 0.0F);
                input.sprint = keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift);
                player_.Update(deltaSeconds, input, physics_);

                // Gate M6: locomotion clip switching, crossfaded over ModelAnimationComponentEXT's
                // BlendDurationEXT (see ModelAnimationSystem3DEXT's own header comment). A no-op
                // if the skinned test character model failed to load.
                const bool playerIsMoving = input.forward != 0.0F || input.strafe != 0.0F;
                renderer_.UpdateCharacterAnimation(deltaSeconds, playerIsMoving ? "Walk" : "Idle");
            }
            if (vehicleTransitionState_ == VehicleTransitionState::None)
            {
                // Skipped mid-clip: position doesn't change during an enter/exit animation (input
                // is suppressed above), but avoid starting a second, unrelated transition on top
                // of an in-progress one regardless.
                CheckDistrictExit();
            }
        }

        // Gate M6 dialogue pose: the on-foot Walk/Idle call above only fires when dialogue is NOT
        // active (movement/physics are frozen during dialogue), so this covers the opposite case.
        // Not drawn while driving, so skipped there -- matches drawPlayer's own condition.
        if (!transitioning && dialogue_.IsActive() && !playerDriving_)
        {
            renderer_.UpdateCharacterAnimation(deltaSeconds, "Dialogue");
        }

        // Gate M6 vehicle entry/exit: play the one-shot clip for kVehicleTransitionSeconds, then
        // finalize (Entering hides the character by flipping playerDriving_ true; Exiting already
        // flipped playerDriving_ false in HandleInteraction(), so there is nothing left to flip).
        if (!transitioning && vehicleTransitionState_ != VehicleTransitionState::None)
        {
            const char* clipName =
                vehicleTransitionState_ == VehicleTransitionState::Entering ? "EnterVehicle" : "ExitVehicle";
            renderer_.UpdateCharacterAnimation(deltaSeconds, clipName);
            vehicleTransitionSecondsRemaining_ -= deltaSeconds;
            if (vehicleTransitionSecondsRemaining_ <= 0.0F)
            {
                if (vehicleTransitionState_ == VehicleTransitionState::Entering)
                {
                    playerDriving_ = true;
                }
                vehicleTransitionState_ = VehicleTransitionState::None;
            }
        }

        if (!transitioning)
        {
            mission_.Update(dialogue_.IsFinished(),
                            player_.GetPosition(),
                            vehicle_.GetPosition(),
                            playerDriving_,
                            districtManager_.GetWorld().GetWarehouseGoal());
        }

        if (transientStatusSeconds_ > 0.0F)
        {
            transientStatusSeconds_ -= deltaSeconds;
        }
        UpdateWindowTitle(deltaSeconds);
        previousKeyboard_ = keyboard;
    }

    void IronShadowsGame::UpdateWindowTitle(float deltaSeconds)
    {
        titleRefreshTimer_ -= deltaSeconds;
        if (titleRefreshTimer_ > 0.0F)
        {
            return;
        }
        titleRefreshTimer_ = 0.20F;

        std::ostringstream title;
        title << "Iron Shadows | ";
        if (districtManager_.IsTransitioning())
        {
            title << "Loading...";
        }
        else if (const DialogueLine* line = dialogue_.GetCurrentLine())
        {
            title << line->speaker << ": " << line->text << " | Enter: continue";
        }
        else if (cutscene_.IsActive())
        {
            title << "..." << " | Enter: skip";
        }
        else
        {
            title << (playerDriving_ ? "Driving" : "On foot")
                  << " | Objective: " << mission_.GetObjectiveText();
            if (playerDriving_)
            {
                title << " | " << static_cast<int>(std::round(vehicle_.GetSpeedKph())) << " km/h";
            }
            if (transientStatusSeconds_ > 0.0F && !transientStatus_.empty())
            {
                title << " | " << transientStatus_;
            }
        }
        getWindowProperty().setTitleProperty(title.str());
    }

    void IronShadowsGame::Draw(const GameTime& gameTime)
    {
        (void)gameTime;
        Graphics::GraphicsDevice& device = getGraphicsDeviceProperty();

        // IS-13-013: a solid-color loading screen for a district transition's minimum display
        // time. The 3D scene is deliberately not drawn here -- physics has already swapped to
        // the new district's static bodies by this point (DistrictManager::RequestTransition()
        // runs synchronously), so the old renderer geometry would be stale/mismatched.
        if (districtManager_.IsTransitioning())
        {
            device.Clear(Color(18, 18, 22, 255), 1.0F);
            if (smokeFramesRemaining_ > 0)
            {
                --smokeFramesRemaining_;
                if (smokeFramesRemaining_ == 0)
                {
                    Exit();
                }
            }
            return;
        }

        device.Clear(Color(112, 145, 164, 255), 1.0F);
        device.SetDepthTestEnabled(true);

        const auto& viewport = device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
            ? static_cast<float>(viewport.getWidthProperty()) /
              static_cast<float>(viewport.getHeightProperty())
            : 1.0F;

        Vector3 target;
        Vector3 camera;
        if (cutscene_.IsActive())
        {
            // Gate M8: the intro cutscene's own camera keyframes override the normal
            // player/vehicle follow camera entirely while it plays. Its final keyframe is
            // authored to exactly match the "else" branch below at the player's spawn position,
            // so the cut back to gameplay has no visible pop.
            camera = cutscene_.GetCameraPosition();
            target = cutscene_.GetCameraLookAt();
        }
        else if (playerDriving_)
        {
            target = vehicle_.GetPosition() + Vector3(0.0F, 1.0F, 0.0F);
            camera = target - vehicle_.GetForward() * 10.5F + Vector3(0.0F, 4.8F, 0.0F);
        }
        else
        {
            target = player_.GetPosition() + Vector3(0.0F, -0.45F, 0.0F);
            camera = target - player_.GetForward() * 7.5F + Vector3(0.0F, 3.4F, 0.0F);
        }

        const Matrix view = Matrix::CreateLookAt(camera, target, Vector3::Up);
        const Matrix projection = Matrix::CreatePerspectiveFieldOfView(
            std::numbers::pi_v<float> / 4.0F,
            aspect,
            0.1F,
            250.0F);

        renderer_.Draw(device,
                       view,
                       projection,
                       player_.GetPosition(),
                       player_.GetYaw(),
                       !playerDriving_,
                       vehicle_.GetPosition(),
                       vehicle_.GetYaw());

        if (smokeFramesRemaining_ > 0)
        {
            --smokeFramesRemaining_;
            if (smokeFramesRemaining_ == 0)
            {
                Exit();
            }
        }
    }
}
