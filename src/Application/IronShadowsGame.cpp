#include "IronShadows/Application/IronShadowsGame.hpp"

#include "IronShadows/Persistence/SaveGame.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "System/TimeSpan.hpp"

#include <cmath>
#include <iostream>
#include <numbers>
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
        player_.Reset(world_.GetPlayerSpawn());
        vehicle_.Reset(world_.GetVehicleSpawn(), world_.GetVehicleSpawnYaw());
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

        renderer_.Initialize(getGraphicsDeviceProperty(), world_);
        UpdateWindowTitle(10.0F);
    }

    bool IronShadowsGame::WasPressed(const KeyboardState& current, Keys key) const
    {
        return current.IsKeyDown(key) && previousKeyboard_.IsKeyUp(key);
    }

    void IronShadowsGame::HandleInteraction()
    {
        if (!playerDriving_)
        {
            if (DistanceSquaredXZ(player_.GetPosition(), vehicle_.GetPosition()) <= 9.0F)
            {
                playerDriving_ = true;
                player_.SetPosition(vehicle_.GetPosition());
                transientStatus_ = "Entered sedan";
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
        if (!world_.CanOccupy(exitPosition, 0.35F))
        {
            exitPosition = vehicle_.GetPosition() - right * 2.2F;
            exitPosition.Y = 1.70F;
        }
        if (world_.CanOccupy(exitPosition, 0.35F))
        {
            playerDriving_ = false;
            player_.SetPosition(exitPosition);
            player_.SetYaw(vehicle_.GetYaw());
            transientStatus_ = "Exited sedan";
        }
        else
        {
            transientStatus_ = "No safe space to exit";
        }
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

        mission_.SetState(snapshot->missionState);
        player_.Reset(snapshot->playerPosition, snapshot->playerYaw);
        vehicle_.Restore(snapshot->vehiclePosition, snapshot->vehicleYaw, snapshot->vehicleSpeed);
        playerDriving_ = snapshot->playerDriving;
        transientStatus_ = "Loaded prototype state";
        transientStatusSeconds_ = 3.0F;
    }

    void IronShadowsGame::ResetPrototype()
    {
        player_.Reset(world_.GetPlayerSpawn());
        vehicle_.Reset(world_.GetVehicleSpawn(), world_.GetVehicleSpawnYaw());
        mission_.Reset();
        dialogue_.Start();
        playerDriving_ = false;
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

        if (WasPressed(keyboard, Keys::Enter) && dialogue_.IsActive())
        {
            dialogue_.Advance();
            if (const DialogueLine* line = dialogue_.GetCurrentLine())
            {
                std::cout << line->speaker << ": " << line->text << '\n';
            }
        }

        if (WasPressed(keyboard, Keys::E) && !dialogue_.IsActive())
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

        if (!dialogue_.IsActive())
        {
            if (playerDriving_)
            {
                VehicleInput input;
                input.throttle = (keyboard.IsKeyDown(Keys::W) || keyboard.IsKeyDown(Keys::Up) ? 1.0F : 0.0F) -
                                 (keyboard.IsKeyDown(Keys::S) || keyboard.IsKeyDown(Keys::Down) ? 1.0F : 0.0F);
                input.steering = (keyboard.IsKeyDown(Keys::D) || keyboard.IsKeyDown(Keys::Right) ? 1.0F : 0.0F) -
                                 (keyboard.IsKeyDown(Keys::A) || keyboard.IsKeyDown(Keys::Left) ? 1.0F : 0.0F);
                input.handbrake = keyboard.IsKeyDown(Keys::Space);
                vehicle_.Update(deltaSeconds, input, world_);
                player_.SetPosition(vehicle_.GetPosition());
                player_.SetYaw(vehicle_.GetYaw());
            }
            else
            {
                OnFootInput input;
                input.forward = (keyboard.IsKeyDown(Keys::W) || keyboard.IsKeyDown(Keys::Up) ? 1.0F : 0.0F) -
                                (keyboard.IsKeyDown(Keys::S) || keyboard.IsKeyDown(Keys::Down) ? 1.0F : 0.0F);
                input.strafe = (keyboard.IsKeyDown(Keys::D) ? 1.0F : 0.0F) -
                               (keyboard.IsKeyDown(Keys::A) ? 1.0F : 0.0F);
                input.turn = (keyboard.IsKeyDown(Keys::Right) ? 1.0F : 0.0F) -
                             (keyboard.IsKeyDown(Keys::Left) ? 1.0F : 0.0F);
                input.sprint = keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift);
                player_.Update(deltaSeconds, input, world_);
            }
        }

        mission_.Update(dialogue_.IsFinished(),
                        player_.GetPosition(),
                        vehicle_.GetPosition(),
                        playerDriving_,
                        world_.GetWarehouseGoal());

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
        if (const DialogueLine* line = dialogue_.GetCurrentLine())
        {
            title << line->speaker << ": " << line->text << " | Enter: continue";
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
        device.Clear(Color(112, 145, 164, 255), 1.0F);
        device.SetDepthTestEnabled(true);

        const auto& viewport = device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
            ? static_cast<float>(viewport.getWidthProperty()) /
              static_cast<float>(viewport.getHeightProperty())
            : 1.0F;

        Vector3 target;
        Vector3 camera;
        if (playerDriving_)
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
