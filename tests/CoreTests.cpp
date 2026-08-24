#include "IronGang/Cutscenes/CutscenePlayer.hpp"
#include "IronGang/Cutscenes/CutsceneSequence.hpp"
#include "IronGang/Core/PerformanceProfiler.hpp"
#include "IronGang/Dialogue/DialogueSystem.hpp"
#include "IronGang/Gameplay/Pedestrian.hpp"
#include "IronGang/Gameplay/PlayerController.hpp"
#include "IronGang/Gameplay/PoliceSystem.hpp"
#include "IronGang/Gameplay/TrafficVehicle.hpp"
#include "IronGang/Gameplay/VehicleController.hpp"
#include "IronGang/Missions/MissionDefinition.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"
#include "IronGang/Persistence/SaveGame.hpp"
#include "IronGang/Physics/PhysicsWorld.hpp"
#include "IronGang/Graphics/LightmapMesh.hpp"
#include "IronGang/Graphics/SunLight.hpp"
#include "IronGang/Graphics/VideoMemoryAccounting.hpp"
#include "IronGang/UI/BitmapFont.hpp"
#include "IronGang/UI/DistrictMap.hpp"
#include "IronGang/World/DistrictManager.hpp"
#include "IronGang/World/PrototypeWorld.hpp"
#include "IronGang/World/WaypointPath.hpp"

#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void TestWorldCollision()
    {
        IronGang::PrototypeWorld world;
        const IronGang::Vector3 insideHotel(-18.0F, 1.70F, 18.0F);
        Require(!world.CanOccupy(insideHotel, 0.35F), "hotel collider must reject occupancy");
        Require(world.CanOccupy(world.GetPlayerSpawn(), 0.35F), "player spawn must be free");
    }

    void TestDistrictMapProjection()
    {
        const std::vector<IronGang::WorldBox> boxes{
            {"ground", {0.0F, 0.0F, 0.0F}, {100.0F, 1.0F, 100.0F}, IronGang::Color::White, false},
        };
        const Microsoft::Xna::Framework::Rectangle screenBounds(100, 200, 400, 400);
        const IronGang::DistrictMapProjection projection =
            IronGang::BuildDistrictMapProjection(boxes, screenBounds);

        const Microsoft::Xna::Framework::Vector2 northwest =
            projection.ProjectPoint({-50.0F, 0.0F, -50.0F});
        const Microsoft::Xna::Framework::Vector2 southeast =
            projection.ProjectPoint({50.0F, 0.0F, 50.0F});
        Require(std::abs(northwest.X - 100.0F) < 0.001F && std::abs(northwest.Y - 200.0F) < 0.001F,
                "district map must project world northwest to screen top-left");
        Require(std::abs(southeast.X - 499.0F) < 0.001F && std::abs(southeast.Y - 599.0F) < 0.001F,
                "district map point projection must stay inside the screen rectangle");

        const IronGang::WorldBox centered{"building", {0.0F, 1.0F, 0.0F},
                                          {20.0F, 2.0F, 10.0F}, IronGang::Color::White, true};
        const Microsoft::Xna::Framework::Rectangle projected = projection.ProjectBox(centered);
        Require(projected.X == 260 && projected.Y == 380 && projected.Width == 80 && projected.Height == 40,
                "district map box projection must preserve authored X/Z extents");
    }

    void TestVehicleMotion()
    {
        IronGang::PrototypeWorld world;
        IronGang::Physics::PhysicsWorld physics;
        (void)world.BuildPhysicsStaticBodies(physics);
        IronGang::VehicleController vehicle;
        vehicle.Reset(world.GetVehicleSpawn(), world.GetVehicleSpawnYaw(), physics);
        const IronGang::Vector3 before = vehicle.GetPosition();
        IronGang::VehicleInput input;
        input.throttle = 1.0F;
        for (int i = 0; i < 180; ++i)
        {
            vehicle.Update(1.0F / 60.0F, input, physics);
        }
        Require(vehicle.GetSpeed() > 1.0F, "vehicle must accelerate");
        Require(IronGang::DistanceSquaredXZ(before, vehicle.GetPosition()) > 1.0F,
                "vehicle must move from spawn");
    }

    void TestPlayerMotion()
    {
        IronGang::PrototypeWorld world;
        IronGang::Physics::PhysicsWorld physics;
        (void)world.BuildPhysicsStaticBodies(physics);
        IronGang::PlayerController player;
        player.Reset(world.GetPlayerSpawn(), 0.0F, physics);
        const IronGang::Vector3 before = player.GetPosition();
        IronGang::OnFootInput input;
        input.forward = 1.0F;
        for (int i = 0; i < 90; ++i)
        {
            player.Update(1.0F / 60.0F, input, physics);
        }
        Require(IronGang::DistanceSquaredXZ(before, player.GetPosition()) > 1.0F,
                "player must move from spawn when walking forward");

        // Walking straight at the hotel (a static collider spanning X in [-26,-10], Z in
        // [10,26]) for a long time must not tunnel through its ~16-unit thickness.
        IronGang::PlayerController blocked;
        blocked.Reset({-5.0F, 1.70F, 18.0F}, 0.0F, physics);
        IronGang::OnFootInput towardHotel;
        towardHotel.strafe = -1.0F;
        for (int i = 0; i < 300; ++i)
        {
            blocked.Update(1.0F / 60.0F, towardHotel, physics);
        }
        Require(blocked.GetPosition().X > -20.0F,
                "walking into the hotel collider for 5 seconds must not tunnel through it");
    }

    // Gate M5 / plan_13 (IG-13-042 equivalent): transitioning between the two prototype
    // districts must swap the loaded world, replace its static physics bodies without leaking
    // the old ones, and honor the loading screen's minimum display time exactly once.
    void TestDistrictTransition()
    {
        IronGang::Physics::PhysicsWorld physics;
        IronGang::DistrictManager districts;
        districts.Initialize(physics);
        Require(districts.GetWorld().GetId() == IronGang::DistrictId::WarehouseBlock,
                "DistrictManager must start in the warehouse block");
        // The two districts have different numbers of static bodies (different scenes), so only
        // a full round trip back to the same district is a valid leak check -- not the raw count
        // after a one-way swap.
        const std::size_t warehouseBodyCount = physics.GetBodyCount();

        districts.RequestTransition(physics);
        Require(districts.GetWorld().GetId() == IronGang::DistrictId::Countryside,
                "the warehouse block's exit must lead to the countryside");
        Require(districts.IsTransitioning(), "requesting a transition must show a loading screen");
        Require(!districts.ConsumeArrival(),
                "arrival must not be reported before the loading screen's minimum time elapses");

        for (int i = 0; i < 60; ++i)
        {
            districts.Update(1.0F / 60.0F);
        }
        Require(!districts.IsTransitioning(), "the loading screen must finish after its minimum display time");
        Require(districts.ConsumeArrival(), "arrival must be reported once the loading screen finishes");
        Require(!districts.ConsumeArrival(), "arrival must not be reported a second time");
        const std::size_t countrysideBodyCount = physics.GetBodyCount();

        // The countryside's own exit must lead back to the warehouse block.
        districts.RequestTransition(physics);
        Require(districts.GetWorld().GetId() == IronGang::DistrictId::WarehouseBlock,
                "the countryside's exit must lead back to the warehouse block");
        for (int i = 0; i < 60; ++i)
        {
            districts.Update(1.0F / 60.0F);
        }
        Require(districts.ConsumeArrival(), "arrival must be reported once the return trip's loading screen finishes");
        Require(physics.GetBodyCount() == warehouseBodyCount,
                "round-tripping warehouse -> countryside -> warehouse must not leak static physics bodies");

        // And a second visit to the countryside must reproduce the same body count again.
        districts.RequestTransition(physics);
        for (int i = 0; i < 60; ++i)
        {
            districts.Update(1.0F / 60.0F);
        }
        Require(districts.ConsumeArrival(), "arrival must be reported for the second countryside visit");
        Require(physics.GetBodyCount() == countrysideBodyCount,
                "revisiting the countryside must not leak static physics bodies");
    }

    void TestMissionFlow()
    {
        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::ReachVehicle,
                "dialogue completion must start reach-vehicle objective");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::EnterVehicle,
                "reaching the car must request entry");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::DriveToWarehouse,
                "entering the car must start driving objective");
        mission.Update(true,
                       world.GetWarehouseGoal().bounds.center,
                       world.GetWarehouseGoal().bounds.center,
                       true,
                       world.GetWarehouseGoal());
        Require(mission.IsCompleted(), "entering warehouse trigger must complete mission");
    }

    // Gate M7 / plan_24 (IG-24-001/004/026): the mission's states/objectives/transitions now
    // come from a data file (assets/missions/prologue.mission.json), not a hardcoded switch --
    // this drives the exact same flow TestMissionFlow() above exercises against the hardcoded
    // fallback, but through PrototypeMission::LoadMission() against the real, committed file.
    void TestMissionLoadsCommittedFile()
    {
        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;

        std::string error;
        Require(mission.LoadMission(std::string(IRON_GANG_SOURCE_ASSET_DIR) + "/missions/prologue.mission.json",
                                    error),
                "loading the committed mission file must succeed: " + error);
        mission.Reset();

        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::ReachVehicle,
                "loaded mission: dialogue completion must start reach-vehicle objective");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::EnterVehicle,
                "loaded mission: reaching the car must request entry");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::DriveToWarehouse,
                "loaded mission: entering the car must start driving objective");
        mission.Update(true,
                       world.GetWarehouseGoal().bounds.center,
                       world.GetWarehouseGoal().bounds.center,
                       true,
                       world.GetWarehouseGoal());
        Require(mission.IsCompleted(), "loaded mission: entering warehouse trigger must complete mission");
    }

    void WriteTempJson(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    // Gate M7: LoadMissionDefinition must reject malformed mission data with an actionable
    // error rather than silently accepting it (IG-24-003's smallest form: inline validation).
    void TestMissionValidationRejectsMalformedData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_bad_mission.json";
        std::string error;
        IronGang::MissionDefinition definition;

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a","next":"missing"}]})JSON");
        Require(!IronGang::LoadMissionDefinition(path.string(), definition, error),
                "a \"next\" referencing an unknown state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"nowhere","states":[{"id":"a"}]})JSON");
        Require(!IronGang::LoadMissionDefinition(path.string(), definition, error),
                "an initialState referencing an unknown state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a"},{"id":"a"}]})JSON");
        Require(!IronGang::LoadMissionDefinition(path.string(), definition, error),
                "a duplicate state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a","condition":"not_a_real_condition"}]})JSON");
        Require(!IronGang::LoadMissionDefinition(path.string(), definition, error),
                "an unrecognized condition name must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[]})JSON");
        Require(!IronGang::LoadMissionDefinition(path.string(), definition, error),
                "a mission with no states must be rejected");

        Require(!IronGang::LoadMissionDefinition((path.string() + ".does-not-exist"), definition, error),
                "a missing file must be rejected, not crash");

        // One well-formed, minimal two-state mission must still succeed after all the rejections
        // above -- proves failures didn't corrupt LoadMissionDefinition's own state.
        WriteTempJson(path, R"JSON({
            "id": "test_mission",
            "version": 1,
            "initialState": "start",
            "states": [
                { "id": "start", "objective": "Go", "condition": "player_driving", "next": "done" },
                { "id": "done", "objective": "Done" }
            ]
        })JSON");
        Require(IronGang::LoadMissionDefinition(path.string(), definition, error),
                "a well-formed minimal mission must load successfully: " + error);
        Require(definition.initialState == "start", "initialState must round-trip correctly");
        Require(definition.states.size() == 2, "both states must be parsed");
        const IronGang::MissionStateDefinition* start = definition.FindState("start");
        Require(start != nullptr && start->condition == IronGang::MissionCondition::PlayerDriving,
                "the named condition must resolve to the matching MissionCondition enum value");
        Require(start->next == "done", "next must round-trip correctly");
        const IronGang::MissionStateDefinition* done = definition.FindState("done");
        Require(done != nullptr && done->next.empty() && done->condition == IronGang::MissionCondition::None,
                "a state with no \"next\"/\"condition\" fields must default to terminal/None");

        std::filesystem::remove(path);
    }

    // Gate M8 / plan_26 (IG-26-001/004): the minimal in-engine sequence player must advance its
    // camera track over time, interpolating between keyframes, and finish (hand control back) at
    // the correct instant.
    void TestCutscenePlayerAdvancesAndFinishes()
    {
        IronGang::CutsceneSequence sequence;
        sequence.id = "test_sequence";
        sequence.duration = 2.0F;
        sequence.cameraKeyframes = {
            {0.0F, IronGang::Vector3(0.0F, 0.0F, 0.0F), IronGang::Vector3(10.0F, 0.0F, 0.0F)},
            {2.0F, IronGang::Vector3(4.0F, 8.0F, 0.0F), IronGang::Vector3(20.0F, 0.0F, 0.0F)},
        };

        IronGang::CutscenePlayer player;
        Require(!player.IsActive(), "a player must not be active before Start()");
        player.Start(sequence);
        Require(player.IsActive(), "Start() with a non-empty sequence must become active");

        // Halfway through the 2-second clip: position/lookAt must both be exactly halfway
        // between the two keyframes (linear interpolation).
        player.Update(1.0F);
        Require(player.IsActive(), "the player must still be active before its duration elapses");
        Require(std::abs(player.GetCameraPosition().X - 2.0F) < 1e-4F &&
                std::abs(player.GetCameraPosition().Y - 4.0F) < 1e-4F,
                "camera position must be linearly interpolated halfway between keyframes");
        Require(std::abs(player.GetCameraLookAt().X - 15.0F) < 1e-4F,
                "camera look-at must be linearly interpolated halfway between keyframes");

        // Advancing past the duration must finish the sequence and hold the terminal keyframe.
        player.Update(1.5F);
        Require(!player.IsActive(), "the player must finish once elapsed time reaches duration");
        Require(std::abs(player.GetCameraPosition().X - 4.0F) < 1e-4F &&
                std::abs(player.GetCameraPosition().Y - 8.0F) < 1e-4F,
                "the terminal camera position must match the last keyframe exactly");
    }

    // Skip must apply the SAME terminal state a natural finish would (IG-26-004), not some other
    // arbitrary "stopped" state.
    void TestCutscenePlayerSkipAppliesTerminalState()
    {
        IronGang::CutsceneSequence sequence;
        sequence.duration = 5.0F;
        sequence.cameraKeyframes = {
            {0.0F, IronGang::Vector3(0.0F, 0.0F, 0.0F), IronGang::Vector3(1.0F, 0.0F, 0.0F)},
            {5.0F, IronGang::Vector3(9.0F, 0.0F, 0.0F), IronGang::Vector3(2.0F, 0.0F, 0.0F)},
        };

        IronGang::CutscenePlayer player;
        player.Start(sequence);
        player.Update(0.2F); // barely started
        player.Skip();

        Require(!player.IsActive(), "Skip() must finish the sequence immediately");
        Require(std::abs(player.GetCameraPosition().X - 9.0F) < 1e-4F,
                "Skip() must jump straight to the last keyframe's camera position");
        Require(std::abs(player.GetCameraLookAt().X - 2.0F) < 1e-4F,
                "Skip() must jump straight to the last keyframe's camera look-at");
    }

    void TestCutsceneValidationRejectsMalformedData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_bad_cutscene.json";
        std::string error;
        IronGang::CutsceneSequence sequence;

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), sequence, error),
                "an empty cameraKeyframes array must be rejected");

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[
            {"time":0.5,"position":[0,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), sequence, error),
                "a first keyframe not at time 0 must be rejected");

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[
            {"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]},
            {"time":0.0,"position":[1,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), sequence, error),
                "keyframes that are not strictly ascending in time must be rejected");

        WriteTempJson(path, R"JSON({"duration":1.0,"cameraKeyframes":[
            {"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]},
            {"time":2.0,"position":[1,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), sequence, error),
                "a duration shorter than the last keyframe's time must be rejected");

        WriteTempJson(path, R"JSON({"cameraKeyframes":[{"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]}]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), sequence, error),
                "a missing \"duration\" must be rejected");

        Require(!IronGang::LoadCutsceneSequence((path.string() + ".does-not-exist"), sequence, error),
                "a missing file must be rejected, not crash");

        WriteTempJson(path, R"JSON({
            "id": "test_cutscene",
            "version": 1,
            "duration": 1.5,
            "cameraKeyframes": [
                { "time": 0.0, "position": [0, 1, 2], "lookAt": [3, 4, 5] },
                { "time": 1.5, "position": [6, 7, 8], "lookAt": [9, 10, 11] }
            ]
        })JSON");
        Require(IronGang::LoadCutsceneSequence(path.string(), sequence, error),
                "a well-formed minimal cutscene must load successfully: " + error);
        Require(sequence.cameraKeyframes.size() == 2, "both keyframes must be parsed");
        Require(std::abs(sequence.cameraKeyframes[0].position.Y - 1.0F) < 1e-4F,
                "keyframe vector components must round-trip correctly");

        std::filesystem::remove(path);
    }

    void TestDialogueFallback()
    {
        IronGang::DialogueSystem dialogue;
        dialogue.LoadFallbackPrologue();
        dialogue.Start();
        Require(dialogue.IsActive(), "fallback dialogue must start");
        Require(dialogue.GetCurrentLine() != nullptr, "fallback dialogue must have a current line");
        for (std::size_t i = 0; i < dialogue.GetLineCount(); ++i)
        {
            dialogue.Advance();
        }
        Require(dialogue.IsFinished(), "dialogue must finish after all lines advance");
    }

    // Gate M9 / plan_19 (IG-19-001/002): AdvanceAlongPath must move toward the current target at
    // the given speed, and only advance (wrapping, since loop=true here) to the next target once
    // it arrives within arrivalRadius -- checked against hand-computed positions/yaws. Values
    // below are hand-verified via a standalone diagnostic, not just derived on paper.
    void TestWaypointPathAdvancesAndWraps()
    {
        IronGang::WaypointPath path;
        path.points = {IronGang::Vector3(0.0F, 0.0F, 0.0F), IronGang::Vector3(10.0F, 0.0F, 0.0F)};
        path.loop = true;

        // Starting exactly AT points[0] with targetIndex 0 already means "arrived" (distance 0),
        // so the very first call immediately advances to points[1] before moving -- matching how
        // TrafficVehicle::Reset()/Pedestrian::Reset() both start a mover exactly at its first
        // waypoint.
        IronGang::Vector3 position(0.0F, 0.0F, 0.0F);
        std::size_t targetIndex = 0;
        float yaw = 0.0F;

        yaw = IronGang::AdvanceAlongPath(path, position, targetIndex, 5.0F, 1.0F, 0.5F, yaw);
        Require(targetIndex == 1, "starting exactly at the current target must advance to the next one immediately");
        Require(std::abs(position.X - 5.0F) < 1e-4F, "first step must then move halfway toward the new target");
        Require(std::abs(yaw - std::numbers::pi_v<float> / 2.0F) < 1e-4F,
                "yaw facing +X must be +90 degrees under ForwardFromYaw's convention");

        yaw = IronGang::AdvanceAlongPath(path, position, targetIndex, 5.0F, 1.0F, 0.5F, yaw);
        Require(std::abs(position.X - 10.0F) < 1e-4F, "second step must land exactly on the target");
        Require(targetIndex == 1, "the arrival check only runs at the START of a call, one frame later");

        // Third call: now within arrivalRadius (distance 0) at the START of the call, and index 1
        // is the path's last point, so a looping path must wrap back to index 0.
        yaw = IronGang::AdvanceAlongPath(path, position, targetIndex, 5.0F, 1.0F, 0.5F, yaw);
        Require(targetIndex == 0, "reaching the last point of a looping path must wrap back to index 0");
        Require(std::abs(position.X - 5.0F) < 1e-4F, "third step must move back toward the wrapped target");
        Require(std::abs(yaw - (-std::numbers::pi_v<float> / 2.0F)) < 1e-4F,
                "yaw facing -X must be -90 degrees under ForwardFromYaw's convention");
    }

    // Gate M9 / plan_21 (IG-21-001/002): a TrafficVehicle must accelerate toward its cruise speed
    // when clear ahead, and brake to a stop when something is within the minimum gap.
    void TestTrafficVehicleAcceleratesAndBrakes()
    {
        IronGang::WaypointPath path;
        path.points = {IronGang::Vector3(0.0F, 0.0F, 0.0F), IronGang::Vector3(1000.0F, 0.0F, 0.0F)};
        path.loop = false;

        IronGang::TrafficVehicle vehicle;
        vehicle.Reset(path, 0, 10.0F);
        Require(std::abs(vehicle.GetForwardSpeed()) < 1e-4F, "a freshly reset vehicle must start at rest");

        constexpr float kNoObstacleAhead = 1000.0F;
        vehicle.Update(1.0F, kNoObstacleAhead);
        Require(std::abs(vehicle.GetForwardSpeed() - 6.0F) < 1e-4F,
                "accelerating for 1s at 6 units/s^2 from rest must reach 6 units/s");

        // Something 1 unit ahead is well inside the minimum gap (3 units) -- must brake to a full
        // stop in this same 1s step (braking is 12 units/s^2, more than enough to cancel 6).
        vehicle.Update(1.0F, 1.0F);
        Require(std::abs(vehicle.GetForwardSpeed()) < 1e-4F,
                "braking for an obstacle inside the minimum gap must reach zero speed");

        // Clear again: must resume accelerating from rest exactly as the first step did.
        vehicle.Update(1.0F, kNoObstacleAhead);
        Require(std::abs(vehicle.GetForwardSpeed() - 6.0F) < 1e-4F,
                "a vehicle that braked to a stop must accelerate again once the obstacle clears");
    }

    // Gate M9 / plan_20 (IG-20-001/002/003): a Pedestrian must flee directly away from a threat
    // for a fixed duration (even after the threat itself is no longer reported present), then
    // resume its normal path once that duration elapses.
    void TestPedestrianFleesAndResumesPath()
    {
        IronGang::WaypointPath path;
        path.points = {IronGang::Vector3(0.0F, 0.9F, 0.0F), IronGang::Vector3(0.0F, 0.9F, 50.0F)};
        path.loop = true;

        IronGang::Pedestrian pedestrian;
        pedestrian.Reset(path, 0, 1.6F);

        const IronGang::Vector3 threatPosition(0.0F, 0.9F, -5.0F);
        pedestrian.Update(1.0F, true, threatPosition);
        Require(pedestrian.IsFleeing(), "a pedestrian with a threat present must start fleeing");
        // away = (0,0,5), distance 5, direction (0,0,1); step = 1.6 * 2.5 * 1.0 = 4.0.
        Require(std::abs(pedestrian.GetPosition().Z - 4.0F) < 1e-4F,
                "fleeing must move directly away from the threat position at the flee speed");
        Require(std::abs(pedestrian.GetYaw() - std::numbers::pi_v<float>) < 1e-4F,
                "yaw facing -Z (away from the threat, which is further -Z) must be 180 degrees");

        // The threat is no longer reported (hasThreat=false) for the next few calls, but the
        // pedestrian must keep fleeing off its own timer, away from the ORIGINAL threat position.
        for (int i = 0; i < 3; ++i)
        {
            pedestrian.Update(1.0F, false, IronGang::Vector3());
            Require(pedestrian.IsFleeing(), "the flee state must persist for its full duration on its own timer");
        }
        Require(std::abs(pedestrian.GetPosition().Z - 16.0F) < 1e-4F,
                "each subsequent flee second must add another 4.0 units of distance");

        // The flee timer (4s total) expires on this 5th call (1 trigger + 4 no-threat calls);
        // the pedestrian must resume following its own WaypointPath instead of still fleeing.
        pedestrian.Update(1.0F, false, IronGang::Vector3());
        Require(!pedestrian.IsFleeing(), "the flee state must end once its fixed duration elapses");
    }

    // Gate M9 / plan_22 (IG-22-001/002/003/004): the full witnessed-offense -> dispatch -> chase
    // -> escalate -> resolve cycle, with every timer/threshold exercised at its exact boundary.
    void TestPoliceSystemFullCycle()
    {
        IronGang::PoliceSystem police;
        police.Reset();
        Require(police.GetState() == IronGang::PoliceState::Clear, "a fresh PoliceSystem must start Clear");

        const IronGang::Vector3 origin(0.0F, 0.0F, 0.0F);
        const IronGang::Vector3 spawnPosition(20.0F, 0.0F, 0.0F);

        // Not driving: even a very close, very fast "witness" must never trigger a chase.
        police.Update(1.0F, false, origin, 120.0F, {IronGang::Vector3(1.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Clear,
                "an offense while not driving must never be witnessed");

        // Driving fast, but the only witness is far outside the witness radius (15 units).
        police.Update(1.0F, true, origin, 120.0F, {IronGang::Vector3(1000.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Clear,
                "a witness outside the witness radius must not trigger a chase");

        // Driving over the speed threshold with a witness inside the radius: must dispatch.
        police.Update(1.0F, true, origin, 100.0F, {IronGang::Vector3(5.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Dispatched,
                "speeding witnessed within radius must dispatch a patrol car");
        Require(police.GetActivePatrolCount() == 1, "dispatch must spawn exactly one patrol car");
        Require(std::abs(police.GetPatrolPosition(0).X - spawnPosition.X) < 1e-4F,
                "the dispatched patrol car must appear at the given spawn position");

        // Dispatched has a fixed delay (2s) before patrol cars actually start moving/chasing.
        police.Update(1.0F, true, origin, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Dispatched,
                "the dispatch delay must not elapse after only 1 of its 2 seconds");
        police.Update(1.5F, true, origin, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Chasing,
                "the dispatch delay must elapse and start the chase");

        // One chase tick at normal speed: hand-verified pursuit math (patrol starts 20 units from
        // the player at (0,0,0); closes 9 units/s for 1s).
        police.Update(1.0F, true, origin, 0.0F, {}, spawnPosition);
        Require(std::abs(police.GetPatrolPosition(0).X - 11.0F) < 1e-4F,
                "the patrol car must close the distance to the player at its own patrol speed");

        // A single long step (19s more, 20s total chase time) must both escalate (a second patrol
        // car appears) and let both patrol cars close in (clamped so neither overshoots the
        // player's position).
        police.Update(19.0F, true, origin, 0.0F, {}, spawnPosition);
        Require(police.GetActivePatrolCount() == 2,
                "a chase running for the full escalation timer must add a second patrol car");
        Require(police.GetPatrolPosition(0).Length() < 1e-4F,
                "the first patrol car must have closed the full remaining distance to the player");
        Require(police.GetPatrolPosition(1).Length() < 1e-4F,
                "the newly escalated patrol car must also close its own distance to the player");
        Require(police.GetState() == IronGang::PoliceState::Chasing, "escalating must not end the chase");

        // The player "escapes" to somewhere far away; patrol cars can only close 9 units/s, so
        // they stay far behind (closestDistance stays well over the 40-unit resolve distance) for
        // 3 full seconds -- the chase must then resolve back to Clear.
        const IronGang::Vector3 farAway(1000.0F, 0.0F, 0.0F);
        police.Update(1.0F, true, farAway, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Chasing,
                "the resolve distance must be sustained, not trigger instantly");
        police.Update(1.0F, true, farAway, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Chasing,
                "resolve must require the full sustain duration (2 of 3 seconds so far)");
        police.Update(1.0F, true, farAway, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Clear,
                "sustaining the resolve distance for the full 3 seconds must clear the chase");
        Require(police.GetActivePatrolCount() == 0, "clearing a chase must remove all patrol cars");
    }

    // Gate M10 / plan_28 (UI HUD): the hand-built bitmap font's glyph-atlas bit-unpacking is the
    // part most likely to have an off-by-one or bit-order bug, so it is verified directly against
    // the exact 'A' glyph bit pattern (hand-derived from the embedded font8x8 data, cross-checked
    // via a standalone ASCII-art diagnostic before being embedded) -- independent of any
    // GraphicsDevice/rendering, since BuildFont8x8AtlasPixels() needs neither.
    void TestBitmapFontGlyphAtlas()
    {
        const auto pixels = IronGang::BuildFont8x8AtlasPixels();
        Require(pixels.size() ==
                    static_cast<std::size_t>(IronGang::kFont8x8AtlasWidth) *
                        static_cast<std::size_t>(IronGang::kFont8x8AtlasHeight),
                "the atlas pixel buffer must be exactly atlasWidth * atlasHeight pixels");

        auto pixelAt = [&](int x, int y) -> const IronGang::Color&
        {
            return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(IronGang::kFont8x8AtlasWidth) +
                         static_cast<std::size_t>(x)];
        };

        // 'A' is glyph index (0x41 - 0x20) = 33 -> column 33%16=1, row 33/16=2 -> origin (8,16).
        // Its 8 rows (from font8x8_basic, hand-verified via ASCII art): 0x0C,0x1E,0x33,0x33,0x3F,
        // 0x33,0x33,0x00 -- row 0 = 0b00001100 (bits 2,3 set), row 2 = 0b00110011 (bits 0,1,4,5 set).
        constexpr int originX = 8;
        constexpr int originY = 16;

        Require(pixelAt(originX + 2, originY + 0).getAProperty() == 255,
                "'A' row 0, column 2 (bit 2 of 0x0C) must be opaque");
        Require(pixelAt(originX + 3, originY + 0).getAProperty() == 255,
                "'A' row 0, column 3 (bit 3 of 0x0C) must be opaque");
        Require(pixelAt(originX + 0, originY + 0).getAProperty() == 0, "'A' row 0, column 0 must be transparent");
        Require(pixelAt(originX + 5, originY + 0).getAProperty() == 0, "'A' row 0, column 5 must be transparent");

        Require(pixelAt(originX + 0, originY + 2).getAProperty() == 255,
                "'A' row 2, column 0 (bit 0 of 0x33) must be opaque");
        Require(pixelAt(originX + 1, originY + 2).getAProperty() == 255,
                "'A' row 2, column 1 (bit 1 of 0x33) must be opaque");
        Require(pixelAt(originX + 2, originY + 2).getAProperty() == 0,
                "'A' row 2, column 2 (bit 2 of 0x33, unset) must be transparent");
        Require(pixelAt(originX + 4, originY + 2).getAProperty() == 255,
                "'A' row 2, column 4 (bit 4 of 0x33) must be opaque");

        // Opaque pixels must be white (this font has no grayscale/anti-aliasing).
        const IronGang::Color& litPixel = pixelAt(originX + 2, originY + 0);
        Require(litPixel.getRProperty() == 255 && litPixel.getGProperty() == 255 && litPixel.getBProperty() == 255,
                "an opaque glyph pixel must be pure white");
    }

    // Gate M10 / plan_39 IG-39-011 (dynamic sun): ComputeSunBrightness() is a plain deterministic
    // function of the hand-authored kSunDirection/kSunIntensity/kSunAmbientFloor constants --
    // hand-computed here (upDot = -kSunDirection.Y = 0.5997; brightness = 0.35 + 0.75*0.5997 =
    // 0.799775) so a future change to those constants can't silently drift without a test noticing.
    void TestSunBrightnessMatchesHandComputedValue()
    {
        const float brightness = IronGang::ComputeSunBrightness();
        Require(std::abs(brightness - 0.799775F) < 1e-4F,
                "ComputeSunBrightness() must match the hand-computed value for the authored sun direction");
    }

    // Gate M10 / plan_39 IG-39-011 (baked lighting): LightmapMeshBuilder must bake one flat-shaded
    // tile per box face into the atlas at the hand-computed brightness for that face's normal, and
    // point each face's vertices at the exact center of its own tile. Expected levels below were
    // cross-checked with a standalone Python calculation against ComputeBrightnessForNormal()'s
    // own formula before being embedded here.
    void TestLightmapMeshBuilderBakesPerFaceBrightness()
    {
        IronGang::LightmapMeshBuilder builder;
        builder.AddBox(IronGang::Vector3(0.0F, 0.0F, 0.0F), IronGang::Vector3(2.0F, 2.0F, 2.0F),
                       IronGang::Color(200, 200, 200, 255));
        builder.Finalize();

        Require(builder.GetVertices().size() == 24,
                "one box must produce 24 vertices -- 4 per face, 6 faces, no vertex sharing across faces");
        Require(builder.GetIndices().size() == 36, "one box must produce 36 indices (6 per face * 6 faces)");
        Require(builder.GetAtlasWidth() == IronGang::kLightmapAtlasColumns * IronGang::kLightmapTileSize,
                "atlas width must be columns * tile size");
        Require(builder.GetAtlasHeight() == IronGang::kLightmapTileSize,
                "6 tiles fit in a single row of the 32-column atlas, so atlas height must be exactly one tile tall");

        // AddBox() adds faces in a fixed order: front, back, left, right, bottom, top (tile
        // indices 0-5). Expected levels hand-computed for kSunDirection/kSunIntensity/
        // kSunAmbientFloor (SunLight.hpp): front/left/bottom face away from the sun (level 89,
        // ambient floor only); back/right/top face partially toward it (right and top land on the
        // exact same value here since kSunDirection's X and Y components are equal).
        const auto& pixels = builder.GetAtlasPixels();
        const int atlasWidth = builder.GetAtlasWidth();
        auto tileLevel = [&](int tileIndex) -> int
        {
            const int column = tileIndex % IronGang::kLightmapAtlasColumns;
            const int row = tileIndex / IronGang::kLightmapAtlasColumns;
            const std::size_t pixelIndex =
                static_cast<std::size_t>(row * IronGang::kLightmapTileSize) *
                    static_cast<std::size_t>(atlasWidth) +
                static_cast<std::size_t>(column * IronGang::kLightmapTileSize);
            return pixels[pixelIndex].getRProperty();
        };
        Require(tileLevel(0) == 89, "front face tile must bake to level 89 (ambient floor only)");
        Require(tileLevel(1) == 190, "back face tile must bake to level 190");
        Require(tileLevel(2) == 89, "left face tile must bake to level 89 (ambient floor only)");
        Require(tileLevel(3) == 203, "right face tile must bake to level 203");
        Require(tileLevel(4) == 89, "bottom face tile must bake to level 89 (ambient floor only)");
        Require(tileLevel(5) == 203, "top face tile must bake to level 203");

        // The right face's 4 vertices (indices 12-15) must all sample the exact center of tile 3.
        const auto& rightFaceVertex = builder.GetVertices()[12];
        constexpr float kExpectedU = 14.0F / 128.0F; // (column 3 * 4 + 2) / atlasWidth 128
        constexpr float kExpectedV = 2.0F / 4.0F;    // (row 0 * 4 + 2) / atlasHeight 4
        Require(std::abs(rightFaceVertex.TextureCoordinate.X - kExpectedU) < 1e-4F &&
                    std::abs(rightFaceVertex.TextureCoordinate.Y - kExpectedV) < 1e-4F,
                "the right face's vertices must sample the exact center of its own tile");

        for (const auto& vertex : builder.GetVertices())
        {
            Require(vertex.Color.getRProperty() == 200 && vertex.Color.getGProperty() == 200 &&
                        vertex.Color.getBProperty() == 200,
                    "every vertex must carry the box's own authored color -- baked lighting lives in "
                    "the atlas texture, not vertex color");
        }
    }

    // Gate M11 / plan_39 IG-39-061 (save/load playthrough): saving mid-mission and loading into a
    // FRESH PrototypeMission must not just round-trip the state enum (TestSaveRoundTrip already
    // covers that in isolation) -- the loaded mission must actually keep progressing correctly
    // afterward, proving a save/load cycle doesn't leave it stuck or in a state it can't advance
    // from.
    void TestSaveLoadMidMissionPlaythrough()
    {
        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::DriveToWarehouse,
                "setup: mission must reach DriveToWarehouse before saving");

        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_playthrough_test.save";
        IronGang::SaveSnapshot snapshot;
        snapshot.missionState = mission.GetState();
        snapshot.playerPosition = world.GetVehicleSpawn();
        snapshot.vehiclePosition = world.GetVehicleSpawn();
        snapshot.playerDriving = true;
        snapshot.districtId = world.GetId();

        std::string error;
        Require(IronGang::SaveGame::Write(path.string(), snapshot, error), "save write failed: " + error);
        const auto loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "save read failed: " + error);

        IronGang::PrototypeMission resumedMission;
        resumedMission.SetState(loaded->missionState);
        Require(resumedMission.GetState() == IronGang::PrototypeMissionState::DriveToWarehouse,
                "the loaded mission state must match what was saved");

        // Continuing from the loaded state must still complete the mission correctly.
        resumedMission.Update(true, world.GetWarehouseGoal().bounds.center, world.GetWarehouseGoal().bounds.center,
                              true, world.GetWarehouseGoal());
        Require(resumedMission.IsCompleted(), "a mission resumed from a save must still be able to complete");

        std::filesystem::remove(path);
    }

    // Gate M11 / plan_39 IG-39-062 (cutscene-skip playthrough): CutscenePlayer and PrototypeMission
    // are independent by construction (Mission::Update() takes no cutscene state at all) -- proves
    // that rather than just assuming it: skipping a cutscene immediately must not prevent the
    // mission from progressing normally right afterward.
    void TestCutsceneSkipDoesNotBlockMissionProgression()
    {
        IronGang::CutsceneSequence sequence;
        sequence.duration = 2.5F;
        sequence.cameraKeyframes = {
            {0.0F, IronGang::Vector3(0.0F, 0.0F, 0.0F), IronGang::Vector3(1.0F, 0.0F, 0.0F)},
            {2.5F, IronGang::Vector3(1.0F, 0.0F, 0.0F), IronGang::Vector3(2.0F, 0.0F, 0.0F)},
        };
        IronGang::CutscenePlayer cutscene;
        cutscene.Start(sequence);
        cutscene.Skip();
        Require(!cutscene.IsActive(), "Skip() must finish the cutscene immediately");

        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::ReachVehicle,
                "mission progression must work normally immediately after a cutscene is skipped");
    }

    // Gate M11 / plan_39 IG-39-063 (mission-failure retry): this prototype's one mission has no
    // real failure state (a locked, deliberately simple linear delivery mission -- see
    // plan_24-mission-framework-and-scripting.md's own non-goal on bespoke scripting), so "retry"
    // is proven at the level that actually exists: Reset() (the "R" key in IronGangGame) must
    // return a mid-mission run all the way back to the mission's own initial state, and the
    // mission must still be able to complete again afterward.
    void TestMissionResetActsAsRetry()
    {
        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::DriveToWarehouse,
                "setup: mission must be mid-flight before retrying");

        mission.Reset();
        Require(mission.GetState() == IronGang::PrototypeMissionState::Introduction,
                "Reset() must return a mid-mission run to the mission's own initial state");

        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        mission.Update(true, world.GetWarehouseGoal().bounds.center, world.GetWarehouseGoal().bounds.center, true,
                      world.GetWarehouseGoal());
        Require(mission.IsCompleted(), "a retried mission must be able to complete again");
    }

    // Gate M11 / plan_39 IG-39-064 (vehicle-loss recovery): this prototype has no vehicle-
    // destruction mechanic (no combat/damage system exists yet, plan_23), so "recovery" is proven
    // at the level that actually exists: if the player saves while separated from their own
    // vehicle (on foot, vehicle parked somewhere else), loading must restore BOTH independently
    // rather than collapsing one onto the other -- the vehicle is never actually "lost" as long as
    // its own saved position survives the round trip.
    void TestVehicleStatePersistsIndependentlyOfPlayer()
    {
        const std::filesystem::path path =
            std::filesystem::current_path() / "iron_gang_vehicle_recovery_test.save";
        IronGang::SaveSnapshot snapshot;
        snapshot.missionState = IronGang::PrototypeMissionState::ReachVehicle;
        snapshot.playerPosition = {50.0F, 1.70F, -10.0F}; // far from the vehicle
        snapshot.playerYaw = 1.2F;
        snapshot.vehiclePosition = {0.0F, 0.65F, 11.0F};
        snapshot.vehicleYaw = 0.3F;
        snapshot.vehicleSpeed = 0.0F;
        snapshot.playerDriving = false;
        snapshot.districtId = IronGang::DistrictId::WarehouseBlock;

        std::string error;
        Require(IronGang::SaveGame::Write(path.string(), snapshot, error), "save write failed: " + error);
        const auto loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "save read failed: " + error);

        Require(IronGang::DistanceSquaredXZ(loaded->playerPosition, snapshot.playerPosition) < 1e-4F,
                "the player's own position must survive independently of the vehicle's");
        Require(IronGang::DistanceSquaredXZ(loaded->vehiclePosition, snapshot.vehiclePosition) < 1e-4F,
                "the vehicle's own position must survive independently of the player's, even when parked far away");
        Require(!loaded->playerDriving,
                "a save made on foot, away from the vehicle, must not silently mark the player as driving");

        std::filesystem::remove(path);
    }

    // Gate M11 / plan_39 IG-39-066 (district-transition mid-mission): leaving and returning to the
    // original district must not disturb an in-progress mission's state. DistrictManager and
    // PrototypeMission are independent objects by construction, but this is worth a direct
    // regression test rather than an assumption.
    void TestDistrictTransitionPreservesMissionState()
    {
        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::EnterVehicle,
                "setup: mission must be mid-flight before transitioning districts");

        IronGang::Physics::PhysicsWorld physics;
        IronGang::DistrictManager districts;
        districts.Initialize(physics);

        districts.RequestTransition(physics); // WarehouseBlock -> Countryside
        for (int i = 0; i < 60; ++i)
        {
            districts.Update(1.0F / 60.0F);
        }
        Require(districts.ConsumeArrival(), "arrival must be reported once the loading screen finishes");
        Require(mission.GetState() == IronGang::PrototypeMissionState::EnterVehicle,
                "leaving the district must not disturb the mission's state");

        districts.RequestTransition(physics); // Countryside -> WarehouseBlock
        for (int i = 0; i < 60; ++i)
        {
            districts.Update(1.0F / 60.0F);
        }
        Require(districts.ConsumeArrival(), "arrival must be reported once the return trip's loading screen finishes");
        Require(mission.GetState() == IronGang::PrototypeMissionState::EnterVehicle,
                "returning to the original district must not disturb the mission's state either");
    }

    void TestSaveRoundTrip()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_core_test.save";
        IronGang::SaveSnapshot source;
        source.missionState = IronGang::PrototypeMissionState::DriveToWarehouse;
        source.playerPosition = {1.0F, 1.7F, 2.0F};
        source.playerYaw = 0.25F;
        source.vehiclePosition = {3.0F, 0.65F, 4.0F};
        source.vehicleYaw = -0.5F;
        source.vehicleSpeed = 8.0F;
        source.playerDriving = true;
        source.districtId = IronGang::DistrictId::Countryside;

        std::string error;
        Require(IronGang::SaveGame::Write(path.string(), source, error), "save write failed: " + error);
        const auto loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "save read failed: " + error);
        Require(loaded->missionState == source.missionState, "mission state round-trip failed");
        Require(std::abs(loaded->vehicleSpeed - source.vehicleSpeed) < 0.001F,
                "vehicle speed round-trip failed");
        Require(loaded->playerDriving, "driving flag round-trip failed");
        Require(loaded->districtId == source.districtId, "district id round-trip failed");
        std::filesystem::remove(path);
    }

    void TestPerformanceProfilerStatisticsAndReport()
    {
        const auto introScenario = IronGang::ParsePerformanceScenario("intro");
        const auto idleScenario = IronGang::ParsePerformanceScenario("idle");
        const auto walkScenario = IronGang::ParsePerformanceScenario("walk");
        const auto driveScenario = IronGang::ParsePerformanceScenario("drive");
        const auto mixedScenario = IronGang::ParsePerformanceScenario("mixed");
        Require(introScenario == IronGang::PerformanceScenario::Intro &&
                    idleScenario == IronGang::PerformanceScenario::Idle &&
                    walkScenario == IronGang::PerformanceScenario::Walk &&
                    driveScenario == IronGang::PerformanceScenario::Drive &&
                    mixedScenario == IronGang::PerformanceScenario::Mixed,
                "every documented performance scenario must parse to its distinct enum value");
        Require(!IronGang::ParsePerformanceScenario("unknown"),
                "an unknown performance scenario must be rejected");
        Require(std::string(IronGang::PerformanceScenarioName(*driveScenario)) == "drive",
                "performance scenario report names must round-trip through the parser");

        IronGang::PerformanceProfiler profiler;
        profiler.SetEnabled(true);
        for (int sample = 1; sample <= 20; ++sample)
        {
            profiler.Record(IronGang::PerformanceMetric::FrameInterval, static_cast<double>(sample));
        }
        IronGang::DistrictLoadSample districtLoad;
        districtLoad.reason = "exit_transition";
        districtLoad.sourceDistrict = "warehouse_block";
        districtLoad.targetDistrict = "countryside";
        districtLoad.worldPhysicsMilliseconds = 4.5;
        districtLoad.rendererUploadMilliseconds = 8.0;
        districtLoad.proceduralWorldObjectCount = 17;
        districtLoad.staticPhysicsBodyCount = 9;
        districtLoad.residentBytesBefore = 1000;
        districtLoad.residentBytesAfter = 900;
        districtLoad.trackedVideoMemoryBytesBefore = 200;
        districtLoad.trackedVideoMemoryBytesAfter = 250;
        profiler.RecordDistrictLoad(std::move(districtLoad));
        profiler.Record(IronGang::PerformanceMetric::GpuRender, 3.25);
        profiler.RecordRenderWorkload(IronGang::RenderWorkloadMetric::DrawCalls, 10);
        profiler.RecordRenderWorkload(IronGang::RenderWorkloadMetric::DrawCalls, 12);
        profiler.RecordRenderWorkload(IronGang::RenderWorkloadMetric::StateChanges, 31);
        profiler.RecordRenderWorkload(IronGang::RenderWorkloadMetric::Vertices, 2400);
        profiler.RecordRenderWorkload(IronGang::RenderWorkloadMetric::Triangles, 1200);
        profiler.RecordRenderWorkload(IronGang::RenderWorkloadMetric::Instances, 8);
        profiler.RecordRenderWorkload(IronGang::RenderWorkloadMetric::VisibleObjects, 42);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::Bodies, 7);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::Bodies, 9);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::ActiveRigidBodies, 2);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::RigidBodyContactManifolds, 3);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::CharacterContacts, 1);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::FixedSteps, 1);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::PublicRaycasts, 0);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::CharacterCollisionUpdates, 1);
        profiler.RecordPhysicsWorkload(IronGang::PhysicsWorkloadMetric::VehicleWheelRaycasts, 4);

        const IronGang::PerformanceStatistics frame =
            profiler.GetStatistics(IronGang::PerformanceMetric::FrameInterval);
        Require(frame.sampleCount == 20, "performance profiler must retain every recorded sample");
        Require(std::abs(frame.averageMilliseconds - 10.5) < 1e-9,
                "performance profiler average must match the hand-computed value");
        Require(std::abs(frame.p95Milliseconds - 19.0) < 1e-9,
                "performance profiler p95 must use the nearest-rank definition");
        Require(std::abs(frame.maximumMilliseconds - 20.0) < 1e-9,
                "performance profiler maximum must match the largest sample");
        const IronGang::PerformanceStatistics districtLoadTotal =
            profiler.GetStatistics(IronGang::PerformanceMetric::DistrictLoadCpu);
        Require(districtLoadTotal.sampleCount == 1 &&
                    std::abs(districtLoadTotal.averageMilliseconds - 12.5) < 1e-9,
                "district load total must equal its measured world/physics and renderer phases");
        const IronGang::RenderWorkloadStatistics drawCalls =
            profiler.GetRenderWorkloadStatistics(IronGang::RenderWorkloadMetric::DrawCalls);
        Require(drawCalls.sampleCount == 2 && std::abs(drawCalls.average - 11.0) < 1e-9 &&
                    std::abs(drawCalls.p95 - 12.0) < 1e-9 && std::abs(drawCalls.maximum - 12.0) < 1e-9,
                "render workload statistics must retain integer counts and use nearest-rank p95");
        const IronGang::PhysicsWorkloadStatistics bodies =
            profiler.GetPhysicsWorkloadStatistics(IronGang::PhysicsWorkloadMetric::Bodies);
        Require(bodies.sampleCount == 2 && std::abs(bodies.average - 8.0) < 1e-9 &&
                    std::abs(bodies.p95 - 9.0) < 1e-9 && std::abs(bodies.maximum - 9.0) < 1e-9,
                "physics workload statistics must retain per-update counts and use nearest-rank p95");

        IronGang::PerformanceReportContext context;
        context.backend = "TEST";
        context.buildConfiguration = "Debug";
        context.scenario = "unit_test";
        context.width = 1280;
        context.height = 720;
        context.verticalSyncRequested = false;
        context.requestedSwapInterval = 0;
        context.swapIntervalApplyResultKnown = true;
        context.swapIntervalApplySucceeded = true;
        context.appliedSwapInterval = 0;
        context.fixedTimeStep = true;
        context.targetFrameMilliseconds = 1000.0 / 60.0;
        context.peakResidentBytes = 64ULL * 1024ULL * 1024ULL;
        context.trackedVideoMemoryBytes = 8ULL * 1024ULL * 1024ULL;
        context.trackedGameOwnedVideoMemoryBytes = 5ULL * 1024ULL * 1024ULL;
        context.trackedImportedModelBufferBytes = 2ULL * 1024ULL * 1024ULL;
        context.trackedImportedModelTextureBytes = 1ULL * 1024ULL * 1024ULL;
        context.gpuTimerSupported = false;
        context.gpuTimerUnsupportedReason = "test renderer has no timer \"query\"";
        context.gpuTimerDiscardedSamples = 2;
        context.physicsBodyCount = 7;
        context.trafficVehicleCount = 2;
        context.pedestrianCount = 2;

        const std::filesystem::path path =
            std::filesystem::current_path() / "iron_gang_performance_report_test.json";
        std::string error;
        Require(profiler.WriteJsonReport(path.string(), context, error),
                "performance report write failed: " + error);
        std::ifstream input(path);
        const std::string report((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Require(report.find("\"backend\": \"TEST\"") != std::string::npos,
                "performance report must identify its graphics backend");
        Require(report.find("\"schema_version\": 5") != std::string::npos &&
                    report.find("\"draw_calls\": {\"samples\": 2, \"average\": 11.000, \"p95\": 12.000") !=
                        std::string::npos &&
                    report.find("\"state_change_calls\": {\"samples\": 1, \"average\": 31.000") !=
                        std::string::npos &&
                    report.find("\"visible_objects\": {\"samples\": 1, \"average\": 42.000") !=
                        std::string::npos &&
                    report.find("excludes Clear, HUD SpriteBatch internal batching") != std::string::npos,
                "performance report must expose scoped 3D workload counts without claiming backend counters");
        Require(report.find("\"bodies\": {\"samples\": 2, \"average\": 8.000, \"p95\": 9.000") !=
                        std::string::npos &&
                    report.find("\"rigid_body_contact_manifolds\": {\"samples\": 1, \"average\": 3.000") !=
                        std::string::npos &&
                    report.find("\"vehicle_wheel_raycasts\": {\"samples\": 1, \"average\": 4.000") !=
                        std::string::npos &&
                    report.find("contact points within a manifold are not counted separately") !=
                        std::string::npos &&
                    report.find("separate because their granularities differ") != std::string::npos,
                "performance report must preserve exact, separately-scoped physics state and query counts");
        Require(report.find("\"district_world_physics_cpu\": {\"samples\": 1, \"average_ms\": 4.500") !=
                        std::string::npos &&
                    report.find("\"district_renderer_upload_cpu\": {\"samples\": 1, \"average_ms\": 8.000") !=
                        std::string::npos &&
                    report.find("\"reason\": \"exit_transition\", \"source\": \"warehouse_block\", \"target\": \"countryside\"") !=
                        std::string::npos &&
                    report.find("\"district_files\": 0, \"procedural_world_objects\": 17, \"static_physics_bodies\": 9") !=
                        std::string::npos &&
                    report.find("\"resident_delta_bytes\": -100") != std::string::npos &&
                    report.find("\"tracked_video_memory_delta_bytes\": 50") != std::string::npos &&
                    report.find("null means not applicable, not measured zero") != std::string::npos,
                "district-load report must preserve real phase, asset-count, and signed memory-delta evidence");
        Require(report.find("\"present_cpu\"") != std::string::npos,
                "performance report must expose the EndDraw/Present diagnostic separately");
        Require(report.find("\"gpu_render\": {\"samples\": 1, \"average_ms\": 3.250") !=
                        std::string::npos &&
                    report.find("\"gpu_timing\": {\"supported\": false") != std::string::npos &&
                    report.find("\"discarded_samples\": 2") != std::string::npos &&
                    report.find("test renderer has no timer \\\"query\\\"") != std::string::npos,
                "performance report must distinguish an unavailable real GPU timer from zero GPU work");
        Require(report.find("\"vertical_sync_requested\": false") != std::string::npos &&
                    report.find("\"fixed_timestep\": true") != std::string::npos &&
                    report.find("\"target_frame_ms\": 16.667") != std::string::npos,
                "performance report must identify requested presentation and scheduler timing");
        Require(report.find("\"swap_interval\": {\"requested\": 0, \"apply_result_known\": true") !=
                        std::string::npos &&
                    report.find("\"apply_succeeded\": true, \"applied\": 0") != std::string::npos &&
                    report.find("not physical vblank or compositor proof") != std::string::npos,
                "performance report must separate platform swap acknowledgement from vblank proof");
        Require(report.find("\"minimum_frame_rate_pass\": true") != std::string::npos,
                "19ms p95 must pass the 30 FPS minimum budget");
        Require(report.find("\"recommended_frame_rate_pass\": false") != std::string::npos,
                "19ms p95 must fail the stricter 60 FPS recommended budget");
        Require(report.find("\"cpu_subsystems_pass\": false") != std::string::npos,
                "missing CPU subsystem samples must not be represented as a pass");
        Require(report.find("\"tracking_complete\": false") != std::string::npos,
                "partial VRAM accounting must never be represented as complete");
        Require(report.find("\"game_owned_bytes\": 5242880") != std::string::npos &&
                    report.find("\"imported_model_buffer_bytes\": 2097152") != std::string::npos &&
                    report.find("\"imported_model_texture_bytes\": 1048576") != std::string::npos,
                "performance report must expose each tracked VRAM category separately");
        std::filesystem::remove(path);
    }

    void TestVideoMemoryTextureStorageAccounting()
    {
        using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
        Require(IronGang::CalculateTextureStorageBytes(4, 2, 1, 1, 1, SurfaceFormat::Color) == 32,
                "RGBA8 texture accounting must use four bytes per texel");
        Require(IronGang::CalculateTextureStorageBytes(4, 4, 1, 1, 3, SurfaceFormat::Color) == 84,
                "2D texture accounting must include every mip level down to 1x1");
        Require(IronGang::CalculateTextureStorageBytes(8, 8, 1, 1, 1, SurfaceFormat::Dxt1) == 32,
                "DXT1 accounting must use one eight-byte block per 4x4 texels");
        Require(IronGang::CalculateTextureStorageBytes(4, 4, 1, 1, 2, SurfaceFormat::Dxt5) == 32,
                "compressed sub-4x4 mip levels must still occupy one complete block");
        Require(IronGang::CalculateTextureStorageBytes(2, 2, 1, 6, 2, SurfaceFormat::Color) == 120,
                "cube texture accounting must include all six faces and mip levels");
        Require(IronGang::CalculateTextureStorageBytes(4, 4, 4, 1, 3, SurfaceFormat::Color) == 292,
                "3D texture accounting must halve depth together with width and height");
        Require(IronGang::CalculateTextureStorageBytes(0, 4, 1, 1, 1, SurfaceFormat::Color) == 0,
                "invalid texture dimensions must not fabricate storage");
    }
}

int main()
{
    try
    {
        TestWorldCollision();
        TestDistrictMapProjection();
        TestVehicleMotion();
        TestPlayerMotion();
        TestDistrictTransition();
        TestMissionFlow();
        TestMissionLoadsCommittedFile();
        TestMissionValidationRejectsMalformedData();
        TestCutscenePlayerAdvancesAndFinishes();
        TestCutscenePlayerSkipAppliesTerminalState();
        TestCutsceneValidationRejectsMalformedData();
        TestDialogueFallback();
        TestWaypointPathAdvancesAndWraps();
        TestTrafficVehicleAcceleratesAndBrakes();
        TestPedestrianFleesAndResumesPath();
        TestPoliceSystemFullCycle();
        TestBitmapFontGlyphAtlas();
        TestSunBrightnessMatchesHandComputedValue();
        TestLightmapMeshBuilderBakesPerFaceBrightness();
        TestSaveLoadMidMissionPlaythrough();
        TestCutsceneSkipDoesNotBlockMissionProgression();
        TestMissionResetActsAsRetry();
        TestVehicleStatePersistsIndependentlyOfPlayer();
        TestDistrictTransitionPreservesMissionState();
        TestSaveRoundTrip();
        TestPerformanceProfilerStatisticsAndReport();
        TestVideoMemoryTextureStorageAccounting();
        std::cout << "Iron Gang core tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Iron Gang core test failure: " << exception.what() << '\n';
        return 1;
    }
}
