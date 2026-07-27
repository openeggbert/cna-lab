#include "IronShadows/Cutscenes/CutscenePlayer.hpp"
#include "IronShadows/Cutscenes/CutsceneSequence.hpp"
#include "IronShadows/Dialogue/DialogueSystem.hpp"
#include "IronShadows/Gameplay/PlayerController.hpp"
#include "IronShadows/Gameplay/VehicleController.hpp"
#include "IronShadows/Missions/MissionDefinition.hpp"
#include "IronShadows/Missions/PrototypeMission.hpp"
#include "IronShadows/Persistence/SaveGame.hpp"
#include "IronShadows/Physics/PhysicsWorld.hpp"
#include "IronShadows/World/DistrictManager.hpp"
#include "IronShadows/World/PrototypeWorld.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
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
