#include "IronShadows/Cutscenes/CutscenePlayer.hpp"
#include "IronShadows/Cutscenes/CutsceneSequence.hpp"
#include "IronShadows/Dialogue/DialogueSystem.hpp"
#include "IronShadows/Gameplay/Pedestrian.hpp"
#include "IronShadows/Gameplay/PlayerController.hpp"
#include "IronShadows/Gameplay/PoliceSystem.hpp"
#include "IronShadows/Gameplay/TrafficVehicle.hpp"
#include "IronShadows/Gameplay/VehicleController.hpp"
#include "IronShadows/Missions/MissionDefinition.hpp"
#include "IronShadows/Missions/PrototypeMission.hpp"
#include "IronShadows/Persistence/SaveGame.hpp"
#include "IronShadows/Physics/PhysicsWorld.hpp"
#include "IronShadows/UI/BitmapFont.hpp"
#include "IronShadows/World/DistrictManager.hpp"
#include "IronShadows/World/PrototypeWorld.hpp"
#include "IronShadows/World/WaypointPath.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

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
        IronShadows::PrototypeWorld world;
        const IronShadows::Vector3 insideHotel(-18.0F, 1.70F, 18.0F);
        Require(!world.CanOccupy(insideHotel, 0.35F), "hotel collider must reject occupancy");
        Require(world.CanOccupy(world.GetPlayerSpawn(), 0.35F), "player spawn must be free");
    }

    void TestVehicleMotion()
    {
        IronShadows::PrototypeWorld world;
        IronShadows::Physics::PhysicsWorld physics;
        (void)world.BuildPhysicsStaticBodies(physics);
        IronShadows::VehicleController vehicle;
        vehicle.Reset(world.GetVehicleSpawn(), world.GetVehicleSpawnYaw(), physics);
        const IronShadows::Vector3 before = vehicle.GetPosition();
        IronShadows::VehicleInput input;
        input.throttle = 1.0F;
        for (int i = 0; i < 180; ++i)
        {
            vehicle.Update(1.0F / 60.0F, input, physics);
        }
        Require(vehicle.GetSpeed() > 1.0F, "vehicle must accelerate");
        Require(IronShadows::DistanceSquaredXZ(before, vehicle.GetPosition()) > 1.0F,
                "vehicle must move from spawn");
    }

    void TestPlayerMotion()
    {
        IronShadows::PrototypeWorld world;
        IronShadows::Physics::PhysicsWorld physics;
        (void)world.BuildPhysicsStaticBodies(physics);
        IronShadows::PlayerController player;
        player.Reset(world.GetPlayerSpawn(), 0.0F, physics);
        const IronShadows::Vector3 before = player.GetPosition();
        IronShadows::OnFootInput input;
        input.forward = 1.0F;
        for (int i = 0; i < 90; ++i)
        {
            player.Update(1.0F / 60.0F, input, physics);
        }
        Require(IronShadows::DistanceSquaredXZ(before, player.GetPosition()) > 1.0F,
                "player must move from spawn when walking forward");

        // Walking straight at the hotel (a static collider spanning X in [-26,-10], Z in
        // [10,26]) for a long time must not tunnel through its ~16-unit thickness.
        IronShadows::PlayerController blocked;
        blocked.Reset({-5.0F, 1.70F, 18.0F}, 0.0F, physics);
        IronShadows::OnFootInput towardHotel;
        towardHotel.strafe = -1.0F;
        for (int i = 0; i < 300; ++i)
        {
            blocked.Update(1.0F / 60.0F, towardHotel, physics);
        }
        Require(blocked.GetPosition().X > -20.0F,
                "walking into the hotel collider for 5 seconds must not tunnel through it");
    }

    // Gate M5 / plan_13 (IS-13-042 equivalent): transitioning between the two prototype
    // districts must swap the loaded world, replace its static physics bodies without leaking
    // the old ones, and honor the loading screen's minimum display time exactly once.
    void TestDistrictTransition()
    {
        IronShadows::Physics::PhysicsWorld physics;
        IronShadows::DistrictManager districts;
        districts.Initialize(physics);
        Require(districts.GetWorld().GetId() == IronShadows::DistrictId::WarehouseBlock,
                "DistrictManager must start in the warehouse block");
        // The two districts have different numbers of static bodies (different scenes), so only
        // a full round trip back to the same district is a valid leak check -- not the raw count
        // after a one-way swap.
        const std::size_t warehouseBodyCount = physics.GetBodyCount();

        districts.RequestTransition(physics);
        Require(districts.GetWorld().GetId() == IronShadows::DistrictId::Countryside,
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
        Require(districts.GetWorld().GetId() == IronShadows::DistrictId::WarehouseBlock,
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
        IronShadows::PrototypeWorld world;
        IronShadows::PrototypeMission mission;
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronShadows::PrototypeMissionState::ReachVehicle,
                "dialogue completion must start reach-vehicle objective");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronShadows::PrototypeMissionState::EnterVehicle,
                "reaching the car must request entry");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.GetState() == IronShadows::PrototypeMissionState::DriveToWarehouse,
                "entering the car must start driving objective");
        mission.Update(true,
                       world.GetWarehouseGoal().bounds.center,
                       world.GetWarehouseGoal().bounds.center,
                       true,
                       world.GetWarehouseGoal());
        Require(mission.IsCompleted(), "entering warehouse trigger must complete mission");
    }

    // Gate M7 / plan_24 (IS-24-001/004/026): the mission's states/objectives/transitions now
    // come from a data file (assets/missions/prologue.mission.json), not a hardcoded switch --
    // this drives the exact same flow TestMissionFlow() above exercises against the hardcoded
    // fallback, but through PrototypeMission::LoadMission() against the real, committed file.
    void TestMissionLoadsCommittedFile()
    {
        IronShadows::PrototypeWorld world;
        IronShadows::PrototypeMission mission;

        std::string error;
        Require(mission.LoadMission(std::string(IRON_SHADOWS_SOURCE_ASSET_DIR) + "/missions/prologue.mission.json",
                                    error),
                "loading the committed mission file must succeed: " + error);
        mission.Reset();

        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronShadows::PrototypeMissionState::ReachVehicle,
                "loaded mission: dialogue completion must start reach-vehicle objective");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.GetState() == IronShadows::PrototypeMissionState::EnterVehicle,
                "loaded mission: reaching the car must request entry");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.GetState() == IronShadows::PrototypeMissionState::DriveToWarehouse,
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
    // error rather than silently accepting it (IS-24-003's smallest form: inline validation).
    void TestMissionValidationRejectsMalformedData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_shadows_bad_mission.json";
        std::string error;
        IronShadows::MissionDefinition definition;

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a","next":"missing"}]})JSON");
        Require(!IronShadows::LoadMissionDefinition(path.string(), definition, error),
                "a \"next\" referencing an unknown state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"nowhere","states":[{"id":"a"}]})JSON");
        Require(!IronShadows::LoadMissionDefinition(path.string(), definition, error),
                "an initialState referencing an unknown state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a"},{"id":"a"}]})JSON");
        Require(!IronShadows::LoadMissionDefinition(path.string(), definition, error),
                "a duplicate state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a","condition":"not_a_real_condition"}]})JSON");
        Require(!IronShadows::LoadMissionDefinition(path.string(), definition, error),
                "an unrecognized condition name must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[]})JSON");
        Require(!IronShadows::LoadMissionDefinition(path.string(), definition, error),
                "a mission with no states must be rejected");

        Require(!IronShadows::LoadMissionDefinition((path.string() + ".does-not-exist"), definition, error),
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
        Require(IronShadows::LoadMissionDefinition(path.string(), definition, error),
                "a well-formed minimal mission must load successfully: " + error);
        Require(definition.initialState == "start", "initialState must round-trip correctly");
        Require(definition.states.size() == 2, "both states must be parsed");
        const IronShadows::MissionStateDefinition* start = definition.FindState("start");
        Require(start != nullptr && start->condition == IronShadows::MissionCondition::PlayerDriving,
                "the named condition must resolve to the matching MissionCondition enum value");
        Require(start->next == "done", "next must round-trip correctly");
        const IronShadows::MissionStateDefinition* done = definition.FindState("done");
        Require(done != nullptr && done->next.empty() && done->condition == IronShadows::MissionCondition::None,
                "a state with no \"next\"/\"condition\" fields must default to terminal/None");

        std::filesystem::remove(path);
    }

    // Gate M8 / plan_26 (IS-26-001/004): the minimal in-engine sequence player must advance its
    // camera track over time, interpolating between keyframes, and finish (hand control back) at
    // the correct instant.
    void TestCutscenePlayerAdvancesAndFinishes()
    {
        IronShadows::CutsceneSequence sequence;
        sequence.id = "test_sequence";
        sequence.duration = 2.0F;
        sequence.cameraKeyframes = {
            {0.0F, IronShadows::Vector3(0.0F, 0.0F, 0.0F), IronShadows::Vector3(10.0F, 0.0F, 0.0F)},
            {2.0F, IronShadows::Vector3(4.0F, 8.0F, 0.0F), IronShadows::Vector3(20.0F, 0.0F, 0.0F)},
        };

        IronShadows::CutscenePlayer player;
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

    // Skip must apply the SAME terminal state a natural finish would (IS-26-004), not some other
    // arbitrary "stopped" state.
    void TestCutscenePlayerSkipAppliesTerminalState()
    {
        IronShadows::CutsceneSequence sequence;
        sequence.duration = 5.0F;
        sequence.cameraKeyframes = {
            {0.0F, IronShadows::Vector3(0.0F, 0.0F, 0.0F), IronShadows::Vector3(1.0F, 0.0F, 0.0F)},
            {5.0F, IronShadows::Vector3(9.0F, 0.0F, 0.0F), IronShadows::Vector3(2.0F, 0.0F, 0.0F)},
        };

        IronShadows::CutscenePlayer player;
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
        const std::filesystem::path path = std::filesystem::current_path() / "iron_shadows_bad_cutscene.json";
        std::string error;
        IronShadows::CutsceneSequence sequence;

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[]})JSON");
        Require(!IronShadows::LoadCutsceneSequence(path.string(), sequence, error),
                "an empty cameraKeyframes array must be rejected");

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[
            {"time":0.5,"position":[0,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronShadows::LoadCutsceneSequence(path.string(), sequence, error),
                "a first keyframe not at time 0 must be rejected");

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[
            {"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]},
            {"time":0.0,"position":[1,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronShadows::LoadCutsceneSequence(path.string(), sequence, error),
                "keyframes that are not strictly ascending in time must be rejected");

        WriteTempJson(path, R"JSON({"duration":1.0,"cameraKeyframes":[
            {"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]},
            {"time":2.0,"position":[1,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronShadows::LoadCutsceneSequence(path.string(), sequence, error),
                "a duration shorter than the last keyframe's time must be rejected");

        WriteTempJson(path, R"JSON({"cameraKeyframes":[{"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]}]})JSON");
        Require(!IronShadows::LoadCutsceneSequence(path.string(), sequence, error),
                "a missing \"duration\" must be rejected");

        Require(!IronShadows::LoadCutsceneSequence((path.string() + ".does-not-exist"), sequence, error),
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
        Require(IronShadows::LoadCutsceneSequence(path.string(), sequence, error),
                "a well-formed minimal cutscene must load successfully: " + error);
        Require(sequence.cameraKeyframes.size() == 2, "both keyframes must be parsed");
        Require(std::abs(sequence.cameraKeyframes[0].position.Y - 1.0F) < 1e-4F,
                "keyframe vector components must round-trip correctly");

        std::filesystem::remove(path);
    }

    void TestDialogueFallback()
    {
        IronShadows::DialogueSystem dialogue;
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

    // Gate M9 / plan_19 (IS-19-001/002): AdvanceAlongPath must move toward the current target at
    // the given speed, and only advance (wrapping, since loop=true here) to the next target once
    // it arrives within arrivalRadius -- checked against hand-computed positions/yaws. Values
    // below are hand-verified via a standalone diagnostic, not just derived on paper.
    void TestWaypointPathAdvancesAndWraps()
    {
        IronShadows::WaypointPath path;
        path.points = {IronShadows::Vector3(0.0F, 0.0F, 0.0F), IronShadows::Vector3(10.0F, 0.0F, 0.0F)};
        path.loop = true;

        // Starting exactly AT points[0] with targetIndex 0 already means "arrived" (distance 0),
        // so the very first call immediately advances to points[1] before moving -- matching how
        // TrafficVehicle::Reset()/Pedestrian::Reset() both start a mover exactly at its first
        // waypoint.
        IronShadows::Vector3 position(0.0F, 0.0F, 0.0F);
        std::size_t targetIndex = 0;
        float yaw = 0.0F;

        yaw = IronShadows::AdvanceAlongPath(path, position, targetIndex, 5.0F, 1.0F, 0.5F, yaw);
        Require(targetIndex == 1, "starting exactly at the current target must advance to the next one immediately");
        Require(std::abs(position.X - 5.0F) < 1e-4F, "first step must then move halfway toward the new target");
        Require(std::abs(yaw - std::numbers::pi_v<float> / 2.0F) < 1e-4F,
                "yaw facing +X must be +90 degrees under ForwardFromYaw's convention");

        yaw = IronShadows::AdvanceAlongPath(path, position, targetIndex, 5.0F, 1.0F, 0.5F, yaw);
        Require(std::abs(position.X - 10.0F) < 1e-4F, "second step must land exactly on the target");
        Require(targetIndex == 1, "the arrival check only runs at the START of a call, one frame later");

        // Third call: now within arrivalRadius (distance 0) at the START of the call, and index 1
        // is the path's last point, so a looping path must wrap back to index 0.
        yaw = IronShadows::AdvanceAlongPath(path, position, targetIndex, 5.0F, 1.0F, 0.5F, yaw);
        Require(targetIndex == 0, "reaching the last point of a looping path must wrap back to index 0");
        Require(std::abs(position.X - 5.0F) < 1e-4F, "third step must move back toward the wrapped target");
        Require(std::abs(yaw - (-std::numbers::pi_v<float> / 2.0F)) < 1e-4F,
                "yaw facing -X must be -90 degrees under ForwardFromYaw's convention");
    }

    // Gate M9 / plan_21 (IS-21-001/002): a TrafficVehicle must accelerate toward its cruise speed
    // when clear ahead, and brake to a stop when something is within the minimum gap.
    void TestTrafficVehicleAcceleratesAndBrakes()
    {
        IronShadows::WaypointPath path;
        path.points = {IronShadows::Vector3(0.0F, 0.0F, 0.0F), IronShadows::Vector3(1000.0F, 0.0F, 0.0F)};
        path.loop = false;

        IronShadows::TrafficVehicle vehicle;
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

    // Gate M9 / plan_20 (IS-20-001/002/003): a Pedestrian must flee directly away from a threat
    // for a fixed duration (even after the threat itself is no longer reported present), then
    // resume its normal path once that duration elapses.
    void TestPedestrianFleesAndResumesPath()
    {
        IronShadows::WaypointPath path;
        path.points = {IronShadows::Vector3(0.0F, 0.9F, 0.0F), IronShadows::Vector3(0.0F, 0.9F, 50.0F)};
        path.loop = true;

        IronShadows::Pedestrian pedestrian;
        pedestrian.Reset(path, 0, 1.6F);

        const IronShadows::Vector3 threatPosition(0.0F, 0.9F, -5.0F);
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
            pedestrian.Update(1.0F, false, IronShadows::Vector3());
            Require(pedestrian.IsFleeing(), "the flee state must persist for its full duration on its own timer");
        }
        Require(std::abs(pedestrian.GetPosition().Z - 16.0F) < 1e-4F,
                "each subsequent flee second must add another 4.0 units of distance");

        // The flee timer (4s total) expires on this 5th call (1 trigger + 4 no-threat calls);
        // the pedestrian must resume following its own WaypointPath instead of still fleeing.
        pedestrian.Update(1.0F, false, IronShadows::Vector3());
        Require(!pedestrian.IsFleeing(), "the flee state must end once its fixed duration elapses");
    }

    // Gate M9 / plan_22 (IS-22-001/002/003/004): the full witnessed-offense -> dispatch -> chase
    // -> escalate -> resolve cycle, with every timer/threshold exercised at its exact boundary.
    void TestPoliceSystemFullCycle()
    {
        IronShadows::PoliceSystem police;
        police.Reset();
        Require(police.GetState() == IronShadows::PoliceState::Clear, "a fresh PoliceSystem must start Clear");

        const IronShadows::Vector3 origin(0.0F, 0.0F, 0.0F);
        const IronShadows::Vector3 spawnPosition(20.0F, 0.0F, 0.0F);

        // Not driving: even a very close, very fast "witness" must never trigger a chase.
        police.Update(1.0F, false, origin, 120.0F, {IronShadows::Vector3(1.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Clear,
                "an offense while not driving must never be witnessed");

        // Driving fast, but the only witness is far outside the witness radius (15 units).
        police.Update(1.0F, true, origin, 120.0F, {IronShadows::Vector3(1000.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Clear,
                "a witness outside the witness radius must not trigger a chase");

        // Driving over the speed threshold with a witness inside the radius: must dispatch.
        police.Update(1.0F, true, origin, 100.0F, {IronShadows::Vector3(5.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Dispatched,
                "speeding witnessed within radius must dispatch a patrol car");
        Require(police.GetActivePatrolCount() == 1, "dispatch must spawn exactly one patrol car");
        Require(std::abs(police.GetPatrolPosition(0).X - spawnPosition.X) < 1e-4F,
                "the dispatched patrol car must appear at the given spawn position");

        // Dispatched has a fixed delay (2s) before patrol cars actually start moving/chasing.
        police.Update(1.0F, true, origin, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Dispatched,
                "the dispatch delay must not elapse after only 1 of its 2 seconds");
        police.Update(1.5F, true, origin, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Chasing,
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
        Require(police.GetState() == IronShadows::PoliceState::Chasing, "escalating must not end the chase");

        // The player "escapes" to somewhere far away; patrol cars can only close 9 units/s, so
        // they stay far behind (closestDistance stays well over the 40-unit resolve distance) for
        // 3 full seconds -- the chase must then resolve back to Clear.
        const IronShadows::Vector3 farAway(1000.0F, 0.0F, 0.0F);
        police.Update(1.0F, true, farAway, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Chasing,
                "the resolve distance must be sustained, not trigger instantly");
        police.Update(1.0F, true, farAway, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Chasing,
                "resolve must require the full sustain duration (2 of 3 seconds so far)");
        police.Update(1.0F, true, farAway, 0.0F, {}, spawnPosition);
        Require(police.GetState() == IronShadows::PoliceState::Clear,
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
        const auto pixels = IronShadows::BuildFont8x8AtlasPixels();
        Require(pixels.size() ==
                    static_cast<std::size_t>(IronShadows::kFont8x8AtlasWidth) *
                        static_cast<std::size_t>(IronShadows::kFont8x8AtlasHeight),
                "the atlas pixel buffer must be exactly atlasWidth * atlasHeight pixels");

        auto pixelAt = [&](int x, int y) -> const IronShadows::Color&
        {
            return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(IronShadows::kFont8x8AtlasWidth) +
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
        const IronShadows::Color& litPixel = pixelAt(originX + 2, originY + 0);
        Require(litPixel.getRProperty() == 255 && litPixel.getGProperty() == 255 && litPixel.getBProperty() == 255,
                "an opaque glyph pixel must be pure white");
    }

    void TestSaveRoundTrip()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_shadows_core_test.save";
        IronShadows::SaveSnapshot source;
        source.missionState = IronShadows::PrototypeMissionState::DriveToWarehouse;
        source.playerPosition = {1.0F, 1.7F, 2.0F};
        source.playerYaw = 0.25F;
        source.vehiclePosition = {3.0F, 0.65F, 4.0F};
        source.vehicleYaw = -0.5F;
        source.vehicleSpeed = 8.0F;
        source.playerDriving = true;
        source.districtId = IronShadows::DistrictId::Countryside;

        std::string error;
        Require(IronShadows::SaveGame::Write(path.string(), source, error), "save write failed: " + error);
        const auto loaded = IronShadows::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "save read failed: " + error);
        Require(loaded->missionState == source.missionState, "mission state round-trip failed");
        Require(std::abs(loaded->vehicleSpeed - source.vehicleSpeed) < 0.001F,
                "vehicle speed round-trip failed");
        Require(loaded->playerDriving, "driving flag round-trip failed");
        Require(loaded->districtId == source.districtId, "district id round-trip failed");
        std::filesystem::remove(path);
    }
}

int main()
{
    try
    {
        TestWorldCollision();
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
        TestSaveRoundTrip();
        std::cout << "Iron Shadows core tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Iron Shadows core test failure: " << exception.what() << '\n';
        return 1;
    }
}
