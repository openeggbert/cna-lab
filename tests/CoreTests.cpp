#include "IronGang/Cutscenes/CutscenePlayer.hpp"
#include "IronGang/Cutscenes/CutsceneSequence.hpp"
#include "IronGang/Core/GameConfig.hpp"
#include "IronGang/Core/JsonDataFile.hpp"
#include "IronGang/Core/Log.hpp"
#include "IronGang/Core/PerformanceProfiler.hpp"
#include "IronGang/Core/RandomSource.hpp"
#include "IronGang/Core/SimulationClock.hpp"
#include "IronGang/Dialogue/DialogueSystem.hpp"
#include "IronGang/Gameplay/LaneClearance.hpp"
#include "IronGang/Gameplay/Locomotion.hpp"
#include "IronGang/Gameplay/Pedestrian.hpp"
#include "IronGang/Gameplay/PlayerController.hpp"
#include "IronGang/Gameplay/PoliceSystem.hpp"
#include "IronGang/Gameplay/TrafficVehicle.hpp"
#include "IronGang/Gameplay/VehicleConfig.hpp"
#include "IronGang/Gameplay/VehicleDamage.hpp"
#include "IronGang/Gameplay/VehicleController.hpp"
#include "IronGang/Missions/MissionDefinition.hpp"
#include "IronGang/Missions/MissionExpression.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"
#include "IronGang/Persistence/AutosavePolicy.hpp"
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

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <numbers>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
        Require(mission.IsInState("reach_vehicle"),
                "dialogue completion must start reach-vehicle objective");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.IsInState("enter_vehicle"),
                "reaching the car must request entry");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.IsInState("drive_to_warehouse"),
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
        Require(mission.IsInState("reach_vehicle"),
                "loaded mission: dialogue completion must start reach-vehicle objective");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.IsInState("enter_vehicle"),
                "loaded mission: reaching the car must request entry");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.IsInState("drive_to_warehouse"),
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

    // Gate M7 / plan_24 (IG-24-003/027/033): LoadMissionDefinition must reject malformed mission
    // data -- graph errors, schema-version errors, bad variable declarations, non-bool or
    // unparseable conditions, and malformed entry actions -- with an actionable error rather than
    // silently accepting it.
    void TestMissionValidationRejectsMalformedData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_bad_mission.json";
        const IronGang::MissionContext facts = IronGang::CreatePrototypeMissionFacts();
        std::string error;
        IronGang::MissionDefinition definition;
        const auto load = [&]() {
            return IronGang::LoadMissionDefinition(path.string(), facts, definition, error);
        };

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a","next":"missing"}]})JSON");
        Require(!load(), "a \"next\" referencing an unknown state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"nowhere","states":[{"id":"a"}]})JSON");
        Require(!load(), "an initialState referencing an unknown state id must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[{"id":"a"},{"id":"a"}]})JSON");
        Require(!load(), "a duplicate state id must be rejected");

        WriteTempJson(path,
                      R"JSON({"initialState":"a","states":[{"id":"a","condition":"not_a_real_condition","next":"b"},{"id":"b"}]})JSON");
        Require(!load(), "a condition naming an undeclared fact must be rejected");

        WriteTempJson(path, R"JSON({"initialState":"a","states":[]})JSON");
        Require(!load(), "a mission with no states must be rejected");

        WriteTempJson(path,
                      R"JSON({"initialState":"a","states":[{"id":"a","when":"player_driving"}]})JSON");
        Require(!load(), "a condition on a state with no \"next\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"initialState":"a","states":[{"id":"a","when":"player_driving","condition":"player_driving","next":"b"},{"id":"b"}]})JSON");
        Require(!load(), "declaring both \"when\" and \"condition\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"initialState":"a","states":[{"id":"a","when":"player_vehicle_distance","next":"b"},{"id":"b"}]})JSON");
        Require(!load(), "a non-bool condition must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":3,"initialState":"a","states":[{"id":"a"}]})JSON");
        Require(!load(), "an unsupported schema version must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":1,"initialState":"a","variables":[{"id":"v","type":"int"}],"states":[{"id":"a"}]})JSON");
        Require(!load(), "declaring variables in a version-1 file must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","variables":[{"id":"v","type":"decimal"}],"states":[{"id":"a"}]})JSON");
        Require(!load(), "an unknown variable type must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","variables":[{"id":"v","type":"int","value":"nope"}],"states":[{"id":"a"}]})JSON");
        Require(!load(), "a variable value that is not of its declared type must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","variables":[{"id":"player_driving","type":"bool"}],"states":[{"id":"a"}]})JSON");
        Require(!load(), "a variable shadowing an engine fact must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","onEnter":[{"action":"set","variable":"missing","value":"1"}]}]})JSON");
        Require(!load(), "setting an undeclared variable must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","variables":[{"id":"v","type":"int"}],"states":[{"id":"a","onEnter":[{"action":"set","variable":"v","value":"true"}]}]})JSON");
        Require(!load(), "assigning a bool expression to an int variable must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","onEnter":[{"action":"explode"}]}]})JSON");
        Require(!load(), "an unknown action must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","onEnter":[{"action":"log"}]}]})JSON");
        Require(!load(), "a log action with no message must be rejected");

        Require(!IronGang::LoadMissionDefinition((path.string() + ".does-not-exist"), facts, definition, error),
                "a missing file must be rejected, not crash");

        // One well-formed, minimal two-state mission must still succeed after all the rejections
        // above -- proves failures didn't corrupt LoadMissionDefinition's own state.
        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","outcome":"cancelled"}]})JSON");
        Require(!load(), "an unknown outcome must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","outcome":"completed","next":"b"},{"id":"b"}]})JSON");
        Require(!load(), "an outcome state with a \"next\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":1,"initialState":"a","states":[{"id":"a","outcome":"completed"}]})JSON");
        Require(!load(), "declaring an outcome in a version-1 file must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","objective":"Nowhere"}]})JSON");
        Require(!load(), "a mission no state can end must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","when":"player_driving","transitions":[{"when":"player_driving","next":"b"}],"next":"b"},{"id":"b","outcome":"completed"}]})JSON");
        Require(!load(), "mixing \"transitions\" with \"when\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","transitions":[]},{"id":"b","outcome":"completed"}]})JSON");
        Require(!load(), "an empty \"transitions\" array must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","transitions":[{"when":"player_driving"}]},{"id":"b","outcome":"completed"}]})JSON");
        Require(!load(), "a transition with no \"next\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","transitions":[{"next":"b"}]},{"id":"b","outcome":"completed"}]})JSON");
        Require(!load(), "a transition with no \"when\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":1,"initialState":"a","states":[{"id":"a","transitions":[{"when":"player_driving","next":"completed"}]},{"id":"completed"}]})JSON");
        Require(!load(), "declaring \"transitions\" in a version-1 file must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","next":"b"},{"id":"b","outcome":"completed"}]})JSON");
        Require(!load(), "a \"next\" with no condition must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","when":"player_driving"},{"id":"b","outcome":"completed"}]})JSON");
        Require(!load(), "a condition with no \"next\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","retry":"from_orbit","states":[{"id":"a","outcome":"completed"}]})JSON");
        Require(!load(), "an unknown retry policy must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":1,"initialState":"a","retry":"checkpoint","states":[{"id":"completed"},{"id":"a","next":"completed"}]})JSON");
        Require(!load(), "declaring a retry policy in a version-1 file must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","outcome":"completed","reason":"why"}]})JSON");
        Require(!load(), "a failure reason on a non-failing state must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","outcome":"failed","checkpoint":true}]})JSON");
        Require(!load(), "a state that is both an outcome and a checkpoint must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":2,"initialState":"a","states":[{"id":"a","checkpoint":"yes","outcome":"completed"}]})JSON");
        Require(!load(), "a non-boolean \"checkpoint\" must be rejected");

        WriteTempJson(path,
                      R"JSON({"version":1,"initialState":"a","states":[{"id":"a","checkpoint":true,"next":"completed"},{"id":"completed"}]})JSON");
        Require(!load(), "declaring a checkpoint in a version-1 file must be rejected");

        // One well-formed, minimal two-state mission must still succeed after all the rejections
        // above -- proves failures didn't corrupt LoadMissionDefinition's own state. Neither state
        // is called "completed", so this also proves state ids are no longer a fixed set and that
        // "outcome" is what ends a mission (IG-24-002/018).
        WriteTempJson(path, R"JSON({
            "id": "test_mission",
            "version": 2,
            "initialState": "start",
            "variables": [ { "id": "crates", "type": "int", "value": 2 } ],
            "states": [
                { "id": "start", "objective": "Go", "when": "player_driving && crates > 0", "next": "done",
                  "onEnter": [ { "action": "set", "variable": "crates", "value": "crates + 1" } ] },
                { "id": "done", "objective": "Done", "outcome": "completed" }
            ]
        })JSON");
        const bool loaded = load();
        Require(loaded, "a well-formed minimal mission must load successfully: " + error);
        Require(definition.initialState == "start", "initialState must round-trip correctly");
        Require(definition.version == 2, "version must round-trip correctly");
        Require(definition.states.size() == 2, "both states must be parsed");
        const IronGang::MissionStateDefinition* start = definition.FindState("start");
        Require(start != nullptr && start->transitions.size() == 1 &&
                    start->transitions.front().condition.GetSource() == "player_driving && crates > 0",
                "the condition expression's source must round-trip correctly");
        Require(start != nullptr &&
                    start->transitions.front().condition.GetResultType() == IronGang::MissionValueType::Bool,
                "a condition must type-check as bool");
        Require(start != nullptr && start->onEnter.size() == 1 &&
                    start->onEnter.front().kind == IronGang::MissionAction::Kind::Set &&
                    start->onEnter.front().variable == "crates",
                "the entry action must round-trip correctly");
        Require(start != nullptr && start->transitions.front().next == "done",
                "next must round-trip correctly");
        const IronGang::MissionStateDefinition* done = definition.FindState("done");
        Require(done != nullptr && done->transitions.empty(),
                "a state with no \"next\"/\"when\" fields must default to terminal");
        Require(done != nullptr && done->outcome == IronGang::MissionOutcome::Completed,
                "a declared outcome must round-trip correctly");
        Require(start != nullptr && start->outcome == IronGang::MissionOutcome::None,
                "a state with no outcome must not end the mission");
        IronGang::MissionValue crates;
        Require(definition.declaredContext.TryGetValue("crates", crates) &&
                    crates.GetType() == IronGang::MissionValueType::Int && crates.AsInt() == 2,
                "a declared variable must keep its declared type and initial value");

        std::filesystem::remove(path);
    }

    // plan_24 IG-24-043's integration scenario: the **committed** prologue mission, the real
    // PoliceSystem, and the real retry path, driven frame by frame. A sustained chase must fail
    // the delivery, the retry must land back on the checkpoint, and -- the failure mode the design
    // exists to prevent -- clearing the chase must not let the mission fail again immediately.
    void TestPrologueFailsAndRetriesUnderPoliceChase()
    {
        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        IronGang::PoliceSystem police;
        std::string error;
        Require(mission.LoadMission(std::string(IRON_GANG_SOURCE_ASSET_DIR) + "/missions/prologue.mission.json",
                                    error),
                "the committed prologue mission must load: " + error);
        mission.Reset();

        const IronGang::TriggerZone& goal = world.GetWarehouseGoal();
        const IronGang::Vector3 vehicleSpawn = world.GetVehicleSpawn();
        constexpr float kDeltaSeconds = 0.1F;
        constexpr float kSpeedingKph = 80.0F; // above PoliceSystem's 70 kph offense threshold

        // Publishes this frame's police state the same way IronGangGame::Update() does.
        const auto publishPoliceFacts = [&]() {
            const IronGang::PoliceState state = police.GetState();
            std::string factError;
            Require(mission.SetFact("police_alerted",
                                    IronGang::MissionValue::Bool(state != IronGang::PoliceState::Clear),
                                    factError) &&
                        mission.SetFact("police_chasing",
                                        IronGang::MissionValue::Bool(state == IronGang::PoliceState::Chasing),
                                        factError) &&
                        mission.SetFact("police_chase_seconds",
                                        IronGang::MissionValue::Float(police.GetChaseSeconds()), factError),
                    "publishing the police facts must succeed: " + factError);
        };

        // Reach the checkpoint the ordinary way: dialogue, walk to the sedan, drive off.
        mission.Update(true, world.GetPlayerSpawn(), vehicleSpawn, false, goal);
        mission.Update(true, vehicleSpawn, vehicleSpawn, false, goal);
        mission.Update(true, vehicleSpawn, vehicleSpawn, true, goal);
        Require(mission.IsInState("drive_to_warehouse"), "the mission must reach the driving state");
        Require(mission.HasCheckpoint() && mission.GetCheckpoint().stateId == "drive_to_warehouse",
                "the driving state must record a checkpoint");
        IronGang::MissionValue cargo;
        Require(mission.TryGetVariable("cargo_secured", cargo) && cargo.AsBool(),
                "the checkpoint must be recorded with the entry action already applied");

        // Speed past a witness standing next to the sedan: PoliceSystem dispatches, then chases.
        const std::vector<IronGang::Vector3> witnesses{vehicleSpawn};
        const IronGang::Vector3 patrolSpawn{vehicleSpawn.X + 20.0F, vehicleSpawn.Y, vehicleSpawn.Z};
        int frames = 0;
        while (!mission.IsFailed() && frames < 1000)
        {
            police.Update(kDeltaSeconds, true, vehicleSpawn, kSpeedingKph, witnesses, patrolSpawn);
            publishPoliceFacts();
            mission.Update(true, vehicleSpawn, vehicleSpawn, true, goal);
            ++frames;
        }
        Require(mission.IsFailed(), "a sustained chase must fail the delivery");
        Require(mission.IsInState("busted"), "the failure branch must be the one that fired");
        Require(mission.GetFailureReason() == "The police stayed on you too long to make the drop",
                "the committed mission's own failure reason must be reported: " + mission.GetFailureReason());
        Require(police.GetChaseSeconds() > 25.0F, "the chase must have run past the mission's threshold");

        // Retry: the mission returns to the checkpoint, and the game clears the police response.
        mission.Retry();
        police.Reset();
        Require(mission.IsInState("drive_to_warehouse"), "the retry must return to the checkpoint state");
        Require(!mission.IsFailed(), "the retry must clear the failure");
        Require(mission.TryGetVariable("cargo_secured", cargo) && cargo.AsBool(),
                "the retry must restore the checkpoint's variable values");

        // With the chase cleared, several frames must pass without failing again -- this is what
        // resetting the police response on retry is for.
        for (int frame = 0; frame < 30; ++frame)
        {
            police.Update(kDeltaSeconds, true, vehicleSpawn, 0.0F, {}, patrolSpawn);
            publishPoliceFacts();
            mission.Update(true, vehicleSpawn, vehicleSpawn, true, goal);
        }
        Require(mission.IsInState("drive_to_warehouse"),
                "a cleared chase must not immediately re-fail the retried mission");

        // And the delivery can still be completed after the retry.
        publishPoliceFacts();
        mission.Update(true, goal.bounds.center, goal.bounds.center, true, goal);
        Require(mission.IsCompleted(), "the retried mission must still be completable");

        IronGang::MissionValue deliveries;
        Require(mission.TryGetVariable("deliveries_made", deliveries) && deliveries.AsInt() == 1,
                "completing after a retry must count exactly one delivery");
    }

    // plan_24 IG-24-024/006: a state may declare several ways out, evaluated in file order, so a
    // mission can branch on a wanted-state fact the game pushes in rather than only ever running
    // in a straight line.
    void TestMissionBranchesOnFirstMatchingTransition()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_branch.json";
        WriteTempJson(path, R"JSON({
            "id": "branching_mission",
            "version": 2,
            "initialState": "driving",
            "states": [
                { "id": "driving", "objective": "Make the drop", "checkpoint": true,
                  "transitions": [
                    { "when": "police_chase_seconds > 25", "next": "busted" },
                    { "when": "player_driving && vehicle_in_warehouse_goal", "next": "delivered" }
                  ] },
                { "id": "delivered", "objective": "Delivered", "outcome": "completed" },
                { "id": "busted", "objective": "Busted", "outcome": "failed",
                  "reason": "The police stayed on you" }
            ]
        })JSON");

        IronGang::PrototypeWorld world;
        const IronGang::TriggerZone& goal = world.GetWarehouseGoal();
        IronGang::PrototypeMission mission;
        std::string error;
        Require(mission.LoadMission(path.string(), error), "the branching mission must load: " + error);

        const IronGang::MissionStateDefinition* driving = mission.GetDefinition().FindState("driving");
        Require(driving != nullptr && driving->transitions.size() == 2,
                "both declared transitions must be parsed");
        Require(driving != nullptr && driving->transitions.front().next == "busted" &&
                    driving->transitions.back().next == "delivered",
                "transitions must keep their file order");

        // Neither branch: the mission stays put.
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), true, goal);
        Require(mission.IsInState("driving"), "no matching transition must leave the mission put");

        // Second branch: delivering while no chase is running completes the mission.
        mission.Update(true, goal.bounds.center, goal.bounds.center, true, goal);
        Require(mission.IsCompleted(), "the delivery branch must complete the mission");

        // First branch wins when both hold: the same delivering frame, but with a long chase.
        mission.Reset();
        Require(mission.SetFact("police_chase_seconds", IronGang::MissionValue::Float(26.0F), error),
                "pushing a police fact must succeed: " + error);
        mission.Update(true, goal.bounds.center, goal.bounds.center, true, goal);
        Require(mission.IsFailed() && mission.IsInState("busted"),
                "the earlier transition must win when both conditions hold");
        Require(mission.GetFailureReason() == "The police stayed on you",
                "the failing branch's reason must be reported");

        // The checkpoint recorded on entering "driving" is what a retry returns to.
        mission.Retry();
        Require(mission.IsInState("driving"), "a retry must return to the checkpoint state");

        // A fact is engine-owned: a mission cannot be handed one of the wrong type, and an
        // undeclared fact is refused outright.
        Require(!mission.SetFact("police_chase_seconds", IronGang::MissionValue::Int(1), error),
                "a fact must reject a value of the wrong type");
        Require(!mission.SetFact("not_a_fact", IronGang::MissionValue::Bool(true), error),
                "an undeclared fact must be refused");

        std::filesystem::remove(path);
    }

    // plan_24 IG-24-009/010/042/043: a failure ends the mission with its own reason, and a retry
    // returns to the last checkpoint -- or to the mission start, depending on the declared policy.
    void TestMissionCheckpointRetryAndFailureReason()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_checkpoint.json";
        const std::string missionText = R"JSON({
            "id": "checkpoint_mission",
            "version": 2,
            "initialState": "briefing",
            "retry": "POLICY",
            "variables": [
                { "id": "crates", "type": "int", "value": 0 },
                { "id": "briefed", "type": "bool", "value": false }
            ],
            "states": [
                { "id": "briefing", "objective": "Hear the plan",
                  "onEnter": [ { "action": "set", "variable": "briefed", "value": "true" } ],
                  "when": "dialogue_finished", "next": "loading" },
                { "id": "loading", "objective": "Load the crates", "checkpoint": true,
                  "onEnter": [ { "action": "set", "variable": "crates", "value": "crates + 3" } ],
                  "when": "player_driving", "next": "delivered" },
                { "id": "delivered", "objective": "Delivered", "outcome": "completed" },
                { "id": "busted", "objective": "Busted", "outcome": "failed",
                  "reason": "The police took the shipment" }
            ]
        })JSON";

        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        std::string error;

        // Default policy: retry returns to the checkpoint with the variables it recorded.
        WriteTempJson(path, std::regex_replace(missionText, std::regex("POLICY"), "checkpoint"));
        Require(mission.LoadMission(path.string(), error), "the checkpoint mission must load: " + error);
        Require(mission.GetRetryPolicy() == IronGang::MissionRetryPolicy::Checkpoint,
                "the declared retry policy must round-trip");
        mission.Reset();
        Require(!mission.HasCheckpoint(), "no checkpoint exists before one has been entered");

        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.IsInState("loading"), "the mission must reach the checkpoint state");
        Require(mission.HasCheckpoint() && mission.GetCheckpoint().stateId == "loading",
                "entering a checkpoint state must record a checkpoint");
        IronGang::MissionValue crates;
        Require(mission.TryGetVariable("crates", crates) && crates.AsInt() == 3,
                "the checkpoint state's entry action must have run");

        // Move on, change a variable, then fail.
        Require(mission.SetStateId("busted"), "moving to the failure state must succeed");
        Require(mission.IsFailed() && mission.IsFinished() && !mission.IsCompleted(),
                "a failed state must report failure");
        Require(mission.GetFailureReason() == "The police took the shipment",
                "the failing state's reason must be reported verbatim");
        std::string ignored;
        Require(mission.GetContext().IsVariable("crates"), "crates must still be a declared variable");

        mission.Retry();
        Require(mission.IsInState("loading"), "a checkpoint retry must return to the checkpoint state");
        Require(!mission.IsFailed(), "a retry must clear the failed state");
        Require(mission.GetFailureReason().empty(), "a non-failed mission must report no reason");
        Require(mission.TryGetVariable("crates", crates) && crates.AsInt() == 3,
                "a checkpoint retry must restore the recorded values, not re-run the entry action");
        IronGang::MissionValue briefed;
        Require(mission.TryGetVariable("briefed", briefed) && briefed.AsBool(),
                "a checkpoint retry must keep variables set before the checkpoint");
        Require(mission.HasCheckpoint(), "a retry must not consume the checkpoint");

        // The same mission under the mission_start policy restarts from the beginning instead.
        WriteTempJson(path, std::regex_replace(missionText, std::regex("POLICY"), "mission_start"));
        IronGang::PrototypeMission restarting;
        Require(restarting.LoadMission(path.string(), error), "the restart mission must load: " + error);
        Require(restarting.GetRetryPolicy() == IronGang::MissionRetryPolicy::MissionStart,
                "the mission_start policy must round-trip");
        restarting.Reset();
        restarting.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false,
                          world.GetWarehouseGoal());
        Require(restarting.HasCheckpoint(), "a checkpoint is still recorded under either policy");
        Require(restarting.SetStateId("busted"), "moving to the failure state must succeed");
        restarting.Retry();
        Require(restarting.IsInState("briefing"), "a mission_start retry must go back to the beginning");
        Require(restarting.TryGetVariable("crates", crates) && crates.AsInt() == 0,
                "a mission_start retry must restore declared values");
        Require(!restarting.HasCheckpoint(), "restarting must discard the checkpoint");

        // Before any checkpoint has been reached, a checkpoint retry is a plain restart.
        IronGang::PrototypeMission fresh;
        WriteTempJson(path, std::regex_replace(missionText, std::regex("POLICY"), "checkpoint"));
        Require(fresh.LoadMission(path.string(), error), "the checkpoint mission must load again: " + error);
        fresh.Reset();
        Require(fresh.SetStateId("busted"), "moving to the failure state must succeed");
        fresh.Retry();
        Require(fresh.IsInState("briefing"),
                "a checkpoint retry with no checkpoint reached must restart the mission");

        std::filesystem::remove(path);
    }

    // plan_24 IG-24-044: the checkpoint survives a save/load round trip, and a checkpoint the
    // loaded mission can no longer honour is dropped rather than sending a retry nowhere.
    void TestMissionCheckpointSurvivesSaveLoad()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_checkpoint_save.json";
        WriteTempJson(path, R"JSON({
            "id": "checkpoint_save_mission",
            "version": 2,
            "initialState": "start",
            "variables": [ { "id": "crates", "type": "int", "value": 0 } ],
            "states": [
                { "id": "start", "objective": "Go", "checkpoint": true,
                  "onEnter": [ { "action": "set", "variable": "crates", "value": "crates + 2" } ],
                  "when": "player_driving", "next": "done" },
                { "id": "done", "objective": "Done", "outcome": "completed" }
            ]
        })JSON");

        IronGang::PrototypeMission mission;
        std::string error;
        Require(mission.LoadMission(path.string(), error), "the mission must load: " + error);
        mission.Reset();
        Require(mission.HasCheckpoint() && mission.GetCheckpoint().stateId == "start",
                "an initial checkpoint state must record a checkpoint on Reset");

        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = mission.GetStateId();
        snapshot.missionVariables = mission.CaptureVariables();
        snapshot.missionCheckpoint = mission.GetCheckpoint();

        const std::filesystem::path savePath =
            std::filesystem::current_path() / "iron_gang_checkpoint.save";
        Require(IronGang::SaveGame::Write(savePath.string(), snapshot, error),
                "writing must succeed: " + error);
        const std::optional<IronGang::SaveSnapshot> restored =
            IronGang::SaveGame::Read(savePath.string(), error);
        Require(restored.has_value(), "reading must succeed: " + error);
        Require(restored->missionCheckpoint.stateId == "start",
                "the checkpoint state id must survive the round trip");
        Require(restored->missionCheckpoint.variables.size() == 1 &&
                    restored->missionCheckpoint.variables.front().name == "crates" &&
                    restored->missionCheckpoint.variables.front().value.AsInt() == 2,
                "the checkpoint's variables must survive the round trip");

        IronGang::PrototypeMission reloaded;
        Require(reloaded.LoadMission(path.string(), error), "the mission must load again: " + error);
        reloaded.Reset();
        std::vector<std::string> warnings;
        reloaded.ApplyCheckpoint(restored->missionCheckpoint, &warnings);
        Require(warnings.empty(), "restoring a matching checkpoint must not warn");
        Require(reloaded.HasCheckpoint() && reloaded.GetCheckpoint().stateId == "start",
                "the restored checkpoint must be usable");

        // A checkpoint into a state (or with a variable) this mission no longer has is dropped.
        IronGang::MissionCheckpointSnapshot stale;
        stale.stateId = "no_such_state";
        warnings.clear();
        reloaded.ApplyCheckpoint(stale, &warnings);
        Require(!reloaded.HasCheckpoint() && warnings.size() == 1,
                "a checkpoint naming an undefined state must be dropped with a warning");

        IronGang::MissionCheckpointSnapshot mismatched;
        mismatched.stateId = "start";
        mismatched.variables.push_back(
            IronGang::MissionVariableSnapshot{"crates", IronGang::MissionValue::String("two")});
        warnings.clear();
        reloaded.ApplyCheckpoint(mismatched, &warnings);
        Require(reloaded.HasCheckpoint() && reloaded.GetCheckpoint().variables.empty() &&
                    warnings.size() == 1,
                "a checkpoint variable whose type changed must be dropped with a warning");

        // A save from a mission with no checkpoint round-trips as "no checkpoint".
        IronGang::SaveSnapshot plain;
        plain.missionStateId = "start";
        Require(IronGang::SaveGame::Write(savePath.string(), plain, error),
                "writing a checkpoint-free save must succeed: " + error);
        const std::optional<IronGang::SaveSnapshot> plainRestored =
            IronGang::SaveGame::Read(savePath.string(), error);
        Require(plainRestored.has_value() && plainRestored->missionCheckpoint.stateId.empty(),
                "a save with no checkpoint must load as having none");

        std::filesystem::remove(savePath);
        std::filesystem::remove(path);
    }

    // plan_24 IG-24-018: mission state ids are no longer a fixed five-value enum. A mission may
    // name its states anything, declare which one ends the run and how, and those ids -- not an
    // int index -- are what the save file stores.
    void TestMissionStateIdsAreNotAFixedSet()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_free_states.json";
        WriteTempJson(path, R"JSON({
            "id": "night_stakeout",
            "version": 2,
            "initialState": "briefing",
            "states": [
                { "id": "briefing", "objective": "Hear the plan", "when": "dialogue_finished",
                  "next": "stakeout" },
                { "id": "stakeout", "objective": "Sit on the car", "when": "player_driving",
                  "next": "escaped" },
                { "id": "escaped", "objective": "You made it", "outcome": "completed" },
                { "id": "caught", "objective": "They got you", "outcome": "failed" }
            ]
        })JSON");

        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        std::string error;
        Require(mission.LoadMission(path.string(), error),
                "a mission using ids outside the old fixed set must load: " + error);
        mission.Reset();
        Require(mission.IsInState("briefing"), "the mission must start in its own initial state");
        Require(!mission.IsFinished(), "a mission in a non-outcome state must not be finished");

        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.IsInState("stakeout"), "an arbitrary state id must still transition normally");
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.IsInState("escaped"), "the mission must reach its own terminal state");
        Require(mission.IsCompleted() && mission.IsFinished() && !mission.IsFailed(),
                "an \"outcome\": \"completed\" state must report completion");

        // A failure outcome is what ends the run unsuccessfully (IG-24-002/009's groundwork).
        Require(mission.SetStateId("caught"), "restoring a declared state must succeed");
        Require(mission.IsFailed() && mission.IsFinished() && !mission.IsCompleted(),
                "an \"outcome\": \"failed\" state must report failure, not completion");

        // A state the loaded mission does not define must be refused, leaving the mission put.
        Require(!mission.SetStateId("drive_to_warehouse"),
                "restoring an undefined state id must fail rather than strand the mission");
        Require(mission.IsInState("caught"), "a refused restore must leave the state unchanged");

        // The id, not an index, is what the save file carries.
        const std::filesystem::path savePath =
            std::filesystem::current_path() / "iron_gang_free_states.save";
        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = mission.GetStateId();
        Require(IronGang::SaveGame::Write(savePath.string(), snapshot, error),
                "writing a free-form state id must succeed: " + error);
        const std::optional<IronGang::SaveSnapshot> restored =
            IronGang::SaveGame::Read(savePath.string(), error);
        Require(restored.has_value() && restored->missionStateId == "caught",
                "a free-form state id must survive the save round trip");

        std::filesystem::remove(savePath);
        std::filesystem::remove(path);
    }

    // plan_04 IG-04-011/012: the same seed must produce the same sequence -- on any platform and
    // any standard library, which is why this generator is hand-written rather than <random>.
    void TestRandomSourceIsDeterministicAndUniform()
    {
        // Golden values: if these change, every seeded thing in the game changes with them, and
        // that must be a deliberate decision rather than a silent consequence of an edit here.
        IronGang::RandomSource golden(1);
        const std::uint64_t expected[] = {
            golden.NextUInt64(), golden.NextUInt64(), golden.NextUInt64(), golden.NextUInt64()};
        IronGang::RandomSource replay(1);
        for (const std::uint64_t value : expected)
        {
            Require(replay.NextUInt64() == value, "the same seed must replay the same sequence");
        }
        Require(expected[0] != expected[1] && expected[1] != expected[2],
                "consecutive draws must differ");

        IronGang::RandomSource other(2);
        Require(other.NextUInt64() != expected[0], "a different seed must produce a different sequence");

        // Derive() gives an independent stream without disturbing the parent's own sequence.
        IronGang::RandomSource parent(7);
        IronGang::RandomSource derivedA = parent.Derive(1);
        IronGang::RandomSource derivedB = parent.Derive(2);
        IronGang::RandomSource untouched(7);
        Require(parent.NextUInt64() == untouched.NextUInt64(),
                "deriving must not consume from the parent");
        Require(derivedA.NextUInt64() != derivedB.NextUInt64(),
                "two labels must give two different streams");
        IronGang::RandomSource sameLabelA = IronGang::RandomSource(7).Derive(1);
        IronGang::RandomSource sameLabelB = IronGang::RandomSource(7).Derive(1);
        Require(sameLabelA.NextUInt64() == sameLabelB.NextUInt64(),
                "the same parent seed and label must derive the same stream");

        // Ranges hold, including the degenerate ones.
        IronGang::RandomSource source(12345);
        for (int draw = 0; draw < 2000; ++draw)
        {
            const float unit = source.NextUnitFloat();
            Require(unit >= 0.0F && unit < 1.0F, "NextUnitFloat must stay in [0, 1)");
            const float ranged = source.NextFloatInRange(-2.5F, 4.0F);
            Require(ranged >= -2.5F && ranged <= 4.0F, "NextFloatInRange must stay within its bounds");
            const std::uint32_t index = source.NextIndex(7);
            Require(index < 7, "NextIndex must stay below its bound");
        }
        Require(source.NextIndex(0) == 0, "a zero bound must yield 0 rather than dividing by it");
        Require(source.NextIndex(1) == 0, "a bound of 1 must always yield 0");
        Require(std::fabs(source.NextFloatInRange(3.0F, 3.0F) - 3.0F) < 1e-6F,
                "an empty range must yield its single value");
        Require(std::fabs(source.NextFloatInRange(5.0F, 1.0F) - 5.0F) < 1e-6F,
                "a reversed range must yield the minimum rather than a value outside it");

        // Distribution sanity: uniform enough that no bucket is starved. This is a smoke test for
        // gross bias (a broken modulo, a stuck bit), not a statistical proof.
        IronGang::RandomSource spread(999);
        std::array<int, 8> buckets{};
        constexpr int kDraws = 8000;
        for (int draw = 0; draw < kDraws; ++draw)
        {
            ++buckets[spread.NextIndex(static_cast<std::uint32_t>(buckets.size()))];
        }
        for (const int count : buckets)
        {
            Require(count > kDraws / 16 && count < kDraws / 4,
                    "no bucket may be starved or flooded: " + std::to_string(count));
        }
        int heads = 0;
        for (int draw = 0; draw < kDraws; ++draw)
        {
            heads += spread.NextBool() ? 1 : 0;
        }
        Require(heads > kDraws / 3 && heads < (2 * kDraws) / 3, "NextBool must not be stuck");
    }

    // plan_16 IG-16-005: on-foot movement builds up and dies away instead of switching on and
    // off. The model is pure arithmetic, so the feel is testable without a physics world.
    void TestLocomotionAcceleratesAndDecelerates()
    {
        constexpr float kFrame = 1.0F / 60.0F;
        IronGang::Locomotion locomotion;
        const IronGang::LocomotionSettings settings = locomotion.GetSettings();

        Require(locomotion.GetSpeed() == 0.0F && !locomotion.IsMoving(),
                "a character starts at rest");

        // Full forward input does not reach walking speed in one frame -- that was the old
        // behaviour, and it is what made movement read as a cursor rather than a person.
        locomotion.Update(kFrame, 1.0F, 0.0F, 0.0F, false);
        Require(locomotion.GetForwardVelocity() > 0.0F, "the character must start moving");
        Require(locomotion.GetForwardVelocity() < settings.walkSpeed * 0.5F,
                "one frame must not reach walking speed");

        // It reaches walking speed within a fraction of a second, and never exceeds it.
        int framesToWalk = 1;
        while (locomotion.GetSpeed() < settings.walkSpeed - 1e-4F && framesToWalk < 600)
        {
            locomotion.Update(kFrame, 1.0F, 0.0F, 0.0F, false);
            ++framesToWalk;
        }
        Require(framesToWalk < 30, "walking pace must be reached in well under half a second");
        Require(std::fabs(locomotion.GetSpeed() - settings.walkSpeed) < 1e-3F,
                "speed must settle exactly at walking pace, not overshoot it");
        locomotion.Update(kFrame, 1.0F, 0.0F, 0.0F, false);
        Require(locomotion.GetSpeed() <= settings.walkSpeed + 1e-4F, "speed must not creep past its target");

        // Sprinting raises the target; releasing it eases back down rather than snapping.
        locomotion.Update(kFrame, 1.0F, 0.0F, 0.0F, true);
        Require(locomotion.GetSpeed() > settings.walkSpeed, "sprinting must go faster than walking");
        int framesToSprint = 1;
        while (locomotion.GetSpeed() < settings.walkSpeed * settings.sprintMultiplier - 1e-4F &&
               framesToSprint < 600)
        {
            locomotion.Update(kFrame, 1.0F, 0.0F, 0.0F, true);
            ++framesToSprint;
        }
        Require(std::fabs(locomotion.GetSpeed() - settings.walkSpeed * settings.sprintMultiplier) < 1e-3F,
                "sprint speed must settle at walk speed times the multiplier");
        locomotion.Update(kFrame, 1.0F, 0.0F, 0.0F, false);
        Require(locomotion.GetSpeed() < settings.walkSpeed * settings.sprintMultiplier,
                "releasing sprint must slow down");
        Require(locomotion.GetSpeed() > settings.walkSpeed,
                "releasing sprint must not snap straight to walking pace");

        // Releasing input stops the character -- and stopping is quicker than starting, which is
        // what makes momentum feel responsive rather than sluggish. Measured from a clean walk,
        // not from wherever the sprint checks above left the character.
        IronGang::Locomotion stopping;
        int framesFromRestToWalk = 0;
        while (stopping.GetSpeed() < settings.walkSpeed - 1e-4F && framesFromRestToWalk < 600)
        {
            stopping.Update(kFrame, 1.0F, 0.0F, 0.0F, false);
            ++framesFromRestToWalk;
        }
        int framesToStop = 0;
        while (stopping.IsMoving() && framesToStop < 600)
        {
            stopping.Update(kFrame, 0.0F, 0.0F, 0.0F, false);
            ++framesToStop;
        }
        Require(!stopping.IsMoving(), "releasing input must bring the character to a stop");
        Require(framesToStop < framesFromRestToWalk,
                "stopping must be quicker than starting: " + std::to_string(framesToStop) + " vs " +
                    std::to_string(framesFromRestToWalk));
        Require(stopping.GetSpeed() == 0.0F, "the character must settle at exactly zero, not drift");

        // Diagonal input must not be faster than straight input.
        IronGang::Locomotion diagonal;
        for (int frame = 0; frame < 120; ++frame)
        {
            diagonal.Update(kFrame, 1.0F, 1.0F, 0.0F, false);
        }
        Require(diagonal.GetSpeed() <= settings.walkSpeed + 1e-3F,
                "diagonal movement must be clamped to walking speed: " +
                    std::to_string(diagonal.GetSpeed()));
        Require(diagonal.GetForwardVelocity() > 0.0F && diagonal.GetStrafeVelocity() > 0.0F,
                "diagonal movement must still use both axes");

        // Releasing forward while still strafing must not brake the strafe.
        IronGang::Locomotion mixed;
        for (int frame = 0; frame < 120; ++frame)
        {
            mixed.Update(kFrame, 1.0F, 1.0F, 0.0F, false);
        }
        const float strafeBefore = mixed.GetStrafeVelocity();
        mixed.Update(kFrame, 0.0F, 1.0F, 0.0F, false);
        Require(mixed.GetForwardVelocity() < settings.walkSpeed, "the forward axis must slow");
        Require(mixed.GetStrafeVelocity() >= strafeBefore - 1e-4F,
                "the strafe axis must not be braked by releasing forward");

        // Turning has inertia too, in both directions, and settles exactly.
        IronGang::Locomotion turning;
        turning.Update(kFrame, 0.0F, 0.0F, 1.0F, false);
        Require(turning.GetTurnRate() > 0.0F && turning.GetTurnRate() < settings.turnSpeed,
                "the turn rate must ease in rather than snapping to full");
        for (int frame = 0; frame < 120; ++frame)
        {
            turning.Update(kFrame, 0.0F, 0.0F, 1.0F, false);
        }
        Require(std::fabs(turning.GetTurnRate() - settings.turnSpeed) < 1e-3F,
                "the turn rate must settle at the configured speed");
        turning.Update(kFrame, 0.0F, 0.0F, -1.0F, false);
        Require(turning.GetTurnRate() < settings.turnSpeed,
                "reversing the turn must go through the intervening rates, not flip instantly");

        // Reversing movement input passes through zero rather than teleporting the velocity.
        IronGang::Locomotion reversing;
        for (int frame = 0; frame < 120; ++frame)
        {
            reversing.Update(kFrame, 1.0F, 0.0F, 0.0F, false);
        }
        reversing.Update(kFrame, -1.0F, 0.0F, 0.0F, false);
        Require(reversing.GetForwardVelocity() > 0.0F,
                "one frame of reverse input must not already be moving backwards");

        // A teleport drops momentum, and a zero-length frame changes nothing.
        reversing.Stop();
        Require(reversing.GetSpeed() == 0.0F && reversing.GetTurnRate() == 0.0F,
                "Stop must drop all momentum");
        reversing.Update(0.0F, 1.0F, 0.0F, 1.0F, false);
        Require(reversing.GetSpeed() == 0.0F && reversing.GetTurnRate() == 0.0F,
                "a zero-length frame must change nothing");

        // Tuning is honoured.
        IronGang::Locomotion tuned;
        IronGang::LocomotionSettings fast;
        fast.walkSpeed = 10.0F;
        fast.acceleration = 1000.0F;
        tuned.Configure(fast);
        tuned.Update(kFrame, 1.0F, 0.0F, 0.0F, false);
        Require(std::fabs(tuned.GetSpeed() - 10.0F) < 1e-3F,
                "a high acceleration must reach the configured speed at once");
    }

    // plan_17 IG-17-015: a sedan takes damage from impacts, and the thing the model has to get
    // right is telling a crash apart from braking -- otherwise every red light wrecks the car.
    void TestVehicleDamageDistinguishesCrashesFromBraking()
    {
        constexpr float kFrame = 1.0F / 60.0F;
        IronGang::VehicleDamage damage;
        Require(std::fabs(damage.GetIntegrity() - 1.0F) < 1e-6F, "a new vehicle must be undamaged");
        Require(!damage.IsDisabled(), "a new vehicle must not be disabled");
        Require(std::fabs(damage.GetSpeedFactor() - 1.0F) < 1e-6F,
                "an undamaged vehicle must reach its full speed");

        // Hard braking from 22 m/s to a stop over two seconds: about 1.1 g, and not a scratch.
        float speed = 22.0F;
        for (int frame = 0; frame < 120 && speed > 0.0F; ++frame)
        {
            const float next = std::max(0.0F, speed - 11.0F * kFrame);
            Require(damage.RegisterFrame(speed, next, kFrame) == 0.0F,
                    "braking must never damage the car");
            speed = next;
        }
        Require(std::fabs(damage.GetIntegrity() - 1.0F) < 1e-6F,
                "a full braking stop must leave the car undamaged");

        // Accelerating, coasting, and holding speed do nothing either.
        Require(damage.RegisterFrame(10.0F, 12.0F, kFrame) == 0.0F, "accelerating must not damage");
        Require(damage.RegisterFrame(10.0F, 10.0F, kFrame) == 0.0F, "holding speed must not damage");
        Require(damage.RegisterFrame(10.0F, 9.99F, kFrame) == 0.0F, "coasting must not damage");
        Require(damage.RegisterFrame(10.0F, 5.0F, 0.0F) == 0.0F, "a zero-length frame must be ignored");

        // A crash: 18 m/s into a wall, stopped within one frame.
        const float firstImpact = damage.RegisterFrame(18.0F, 2.0F, kFrame);
        Require(firstImpact > 0.0F, "a wall at speed must damage the car");
        Require(damage.GetIntegrity() < 1.0F, "the damage must be recorded");
        const float afterFirst = damage.GetIntegrity();

        // A harder crash costs more than a softer one, from the same starting integrity.
        IronGang::VehicleDamage gentle;
        IronGang::VehicleDamage harsh;
        const float gentleLoss = gentle.RegisterFrame(8.0F, 4.0F, kFrame);
        const float harshLoss = harsh.RegisterFrame(22.0F, 0.0F, kFrame);
        Require(harshLoss > gentleLoss, "a faster impact must cost more integrity");
        Require(gentleLoss >= 0.0F, "a light knock must never restore integrity");

        // Damage accumulates, and the car eventually wrecks -- but never past zero.
        for (int impact = 0; impact < 20; ++impact)
        {
            damage.RegisterFrame(20.0F, 0.0F, kFrame);
        }
        Require(damage.GetIntegrity() < afterFirst, "repeated impacts must accumulate");
        Require(damage.IsDisabled() && damage.GetIntegrity() == 0.0F,
                "enough impacts must wreck the car, and integrity must floor at 0");
        Require(std::fabs(damage.GetSpeedFactor() - damage.GetSettings().minimumSpeedFactor) < 1e-6F,
                "a wrecked car must keep exactly its minimum speed factor");
        Require(damage.GetSpeedFactor() > 0.0F,
                "a wrecked car must still roll -- stranded is a situation, immobile is a trap");

        // Reversing into something is a crash too: magnitudes are what count.
        IronGang::VehicleDamage reversing;
        Require(reversing.RegisterFrame(-6.0F, 0.0F, kFrame) > 0.0F,
                "reversing into a wall must damage the car");
        IronGang::VehicleDamage throughZero;
        Require(throughZero.RegisterFrame(-0.2F, 0.2F, kFrame) == 0.0F,
                "changing direction through zero must not read as an impact");

        // Repair and restore.
        damage.Reset();
        Require(std::fabs(damage.GetIntegrity() - 1.0F) < 1e-6F, "Reset must repair the car");
        damage.SetIntegrity(0.4F);
        Require(std::fabs(damage.GetIntegrity() - 0.4F) < 1e-6F, "a saved integrity must be restored");
        damage.SetIntegrity(5.0F);
        Require(std::fabs(damage.GetIntegrity() - 1.0F) < 1e-6F, "an out-of-range integrity must clamp");
        damage.SetIntegrity(-1.0F);
        Require(damage.GetIntegrity() == 0.0F, "a negative integrity must clamp to a wreck");
        damage.SetIntegrity(std::numeric_limits<float>::quiet_NaN());
        Require(std::fabs(damage.GetIntegrity() - 1.0F) < 1e-6F,
                "a NaN integrity must fall back to undamaged rather than poisoning the model");

        // Tuning changes what counts as a crash.
        IronGang::VehicleDamage tuned;
        IronGang::VehicleDamageSettings settings;
        // 18 -> 2 m/s in one 60 Hz frame is about 960 m/s^2; a threshold above that must ignore it,
        // while a harder crash (25 -> 0, about 1500 m/s^2) must still register.
        settings.impactDecelerationThreshold = 1200.0F;
        tuned.Configure(settings);
        Require(tuned.RegisterFrame(18.0F, 2.0F, kFrame) == 0.0F,
                "raising the threshold must stop counting the same impact");
        Require(tuned.RegisterFrame(25.0F, 0.0F, kFrame) > 0.0F,
                "a crash past the raised threshold must still register");

        // The saved integrity survives a save/load round trip.
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_damage.save";
        std::string error;
        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = "drive_to_warehouse";
        snapshot.vehicleIntegrity = 0.42F;
        Require(IronGang::SaveGame::Write(path.string(), snapshot, error), "writing must succeed: " + error);
        const std::optional<IronGang::SaveSnapshot> loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value() && std::fabs(loaded->vehicleIntegrity - 0.42F) < 1e-4F,
                "vehicle integrity must survive the save round trip: " + error);

        // A save from before the field existed loads an intact car, not a wreck.
        WriteTempJson(path,
                      "format=iron-gang-save-v1\n"
                      "mission_state_id=reach_vehicle\n"
                      "player_position=1,1.7,2\n"
                      "player_yaw=0\n"
                      "vehicle_position=3,0.65,4\n"
                      "vehicle_yaw=0\n"
                      "vehicle_speed=0\n"
                      "player_driving=0\n");
        const std::optional<IronGang::SaveSnapshot> old = IronGang::SaveGame::Read(path.string(), error);
        Require(old.has_value() && std::fabs(old->vehicleIntegrity - 1.0F) < 1e-6F,
                "an older save must load an undamaged car -- the friendlier of the two defaults");

        std::filesystem::remove(path);
    }

    // plan_36 IG-36-002/006/009: every JSON data file is bounded before a parser touches it.
    // These are the checks that stand between a generated or downloaded file and the game.
    void TestJsonDataFileIsBoundedBeforeParsing()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_bounded.json";
        std::string contents;
        std::string error;

        WriteTempJson(path, R"JSON({"id":"fine","nested":{"deep":[1,2,3]}})JSON");
        Require(IronGang::ReadBoundedJsonText(path.string(), contents, error),
                "an ordinary data file must be readable: " + error);
        Require(contents.find("fine") != std::string::npos, "the text must come back intact");

        Require(!IronGang::ReadBoundedJsonText((path.string() + ".missing"), contents, error),
                "a missing file must be refused");

        // A non-object root is refused by the loaders that require one; the bounded read itself
        // does not care what shape the JSON is, only that it is small, valid, and shallow.
        WriteTempJson(path, R"JSON(["not","an","object"])JSON");
        IronGang::GameConfig rootCheck;
        Require(!IronGang::LoadGameConfig(path.string(), rootCheck, error, nullptr),
                "a non-object root must be refused by a loader that needs an object");
        Require(error.find("object") != std::string::npos, "the refusal must say why: " + error);

        // Oversized: refused on the filesystem's size, without reading the bytes in.
        {
            std::ofstream oversized(path, std::ios::binary | std::ios::trunc);
            oversized << "{\"padding\":\"";
            const std::string chunk(4096, 'x');
            for (std::size_t written = 0; written <= IronGang::kMaxJsonDataFileBytes; written += chunk.size())
            {
                oversized << chunk;
            }
            oversized << "\"}";
        }
        Require(!IronGang::ReadBoundedJsonText(path.string(), contents, error),
                "a file past the size limit must be refused");
        Require(error.find("larger than") != std::string::npos,
                "the size refusal must name the limit: " + error);

        // Deeply nested: refused before a recursive parser can exhaust the stack.
        {
            std::string deep = "{\"a\":";
            const int levels = IronGang::kMaxJsonDataFileDepth + 20;
            for (int level = 0; level < levels; ++level)
            {
                deep += "[";
            }
            deep += "1";
            for (int level = 0; level < levels; ++level)
            {
                deep += "]";
            }
            deep += "}";
            WriteTempJson(path, deep);
        }
        Require(!IronGang::ReadBoundedJsonText(path.string(), contents, error),
                "a file nested past the depth limit must be refused");
        Require(error.find("nests") != std::string::npos, "the depth refusal must say so: " + error);

        // Invalid UTF-8: refused before any of it reaches a string comparison.
        {
            std::ofstream bad(path, std::ios::binary | std::ios::trunc);
            bad << "{\"id\":\"";
            bad.put(static_cast<char>(0xC3)); // lead byte with no continuation
            bad.put(static_cast<char>(0x28));
            bad << "\"}";
        }
        Require(!IronGang::ReadBoundedJsonText(path.string(), contents, error),
                "invalid UTF-8 must be refused");
        Require(error.find("UTF-8") != std::string::npos, "the encoding refusal must say so: " + error);

        // The two checks, on their own.
        Require(IronGang::IsValidUtf8("plain ascii"), "ASCII must validate");
        Require(IronGang::IsValidUtf8("k\xC5\x99\xC3\xADzek"), "well-formed multi-byte UTF-8 must validate");
        Require(IronGang::IsValidUtf8(std::string("emoji \xF0\x9F\x9A\x97")), "a 4-byte form must validate");
        Require(!IronGang::IsValidUtf8(std::string("\x80")), "a stray continuation byte must be refused");
        Require(!IronGang::IsValidUtf8(std::string("\xC0\xAF")),
                "an overlong encoding must be refused -- it is how one character gets two spellings");
        Require(!IronGang::IsValidUtf8(std::string("\xED\xA0\x80")), "a surrogate must be refused");
        Require(!IronGang::IsValidUtf8(std::string("\xF5\x80\x80\x80")), "past U+10FFFF must be refused");
        Require(!IronGang::IsValidUtf8(std::string("\xE2\x82")), "a truncated sequence must be refused");

        Require(IronGang::MeasureJsonNestingDepth("{}") == 1, "one object is one level");
        Require(IronGang::MeasureJsonNestingDepth(R"({"a":{"b":[1]}})") == 3, "nesting must be counted");
        Require(IronGang::MeasureJsonNestingDepth(R"({"a":"{{{{{{"})") == 1,
                "brackets inside a string must not count");
        Require(IronGang::MeasureJsonNestingDepth(R"({"a":"\""})") == 1,
                "an escaped quote must not end the string");
        Require(IronGang::MeasureJsonNestingDepth("{") == -1, "an unclosed bracket must be reported");
        Require(IronGang::MeasureJsonNestingDepth("}{") == -1, "a closer before an opener must be reported");
        Require(IronGang::MeasureJsonNestingDepth(R"({"a":"unterminated})") == -1,
                "an unterminated string must be reported");

        // The bound applies through the loaders that use it, not just directly.
        {
            std::string deep = "{\"states\":";
            const int levels = IronGang::kMaxJsonDataFileDepth + 5;
            for (int level = 0; level < levels; ++level)
            {
                deep += "[";
            }
            for (int level = 0; level < levels; ++level)
            {
                deep += "]";
            }
            deep += "}";
            WriteTempJson(path, deep);
        }
        IronGang::MissionDefinition definition;
        Require(!IronGang::LoadMissionDefinition(path.string(), IronGang::CreatePrototypeMissionFacts(),
                                                 definition, error),
                "the mission loader must inherit the depth bound");
        IronGang::GameConfig config;
        Require(!IronGang::LoadGameConfig(path.string(), config, error, nullptr),
                "the configuration loader must inherit the depth bound");
        IronGang::VehicleConfig vehicle;
        Require(!IronGang::LoadVehicleConfig(path.string(), vehicle, error, nullptr),
                "the vehicle loader must inherit the depth bound");

        std::filesystem::remove(path);
    }

    // plan_17 IG-17-003: the sedan's numbers are data now. What the tests protect is that a
    // broken or partial vehicle file leaves the car exactly as it was, rather than putting a
    // massless chassis or a zero-radius wheel into a physics engine.
    void TestVehicleConfigLoadsValidatesAndFallsBack()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_vehicle.json";
        const IronGang::VehicleConfig defaults;
        IronGang::VehicleConfig config;
        std::string error;
        std::vector<std::string> warnings;

        Require(IronGang::LoadVehicleConfig((path.string() + ".missing"), config, error, &warnings),
                "a missing vehicle file must not be an error");
        Require(std::fabs(config.chassisMass - defaults.chassisMass) < 1e-4F &&
                    std::fabs(config.maxForwardSpeed - defaults.maxForwardSpeed) < 1e-4F,
                "a missing file must leave the built-in sedan in place");
        Require(warnings.size() == 1, "a missing file must be reported once, as a warning");

        // Everything round-trips.
        warnings.clear();
        WriteTempJson(path, R"JSON({
            "id": "coupe",
            "version": 1,
            "chassis": { "mass": 1200, "halfExtents": [1.0, 0.3, 2.0] },
            "wheels": { "radius": 0.30, "width": 0.25,
                        "positions": [[-1,-0.2,-1.3],[1,-0.2,-1.3],[-1,-0.2,1.3],[1,-0.2,1.3]] },
            "performance": { "maxForwardSpeed": 30.0, "maxReverseSpeed": 8.0 }
        })JSON");
        Require(IronGang::LoadVehicleConfig(path.string(), config, error, &warnings),
                "a well-formed vehicle file must load: " + error);
        Require(warnings.empty(), "a well-formed vehicle file must produce no warnings");
        Require(config.id == "coupe" && std::fabs(config.chassisMass - 1200.0F) < 1e-4F,
                "identity and mass must round-trip");
        Require(std::fabs(config.wheelRadius - 0.30F) < 1e-4F &&
                    std::fabs(config.chassisHalfExtents.Z - 2.0F) < 1e-4F,
                "geometry must round-trip");
        Require(std::fabs(config.wheelPositions[3].X - 1.0F) < 1e-4F &&
                    std::fabs(config.wheelPositions[0].Z + 1.3F) < 1e-4F,
                "every wheel position must round-trip in order");
        Require(std::fabs(config.maxForwardSpeed - 30.0F) < 1e-4F, "performance must round-trip");

        // Values a physics engine cannot survive keep their defaults instead.
        warnings.clear();
        WriteTempJson(path, R"JSON({"chassis": {"mass": 0}, "wheels": {"radius": 0, "width": -1}})JSON");
        Require(IronGang::LoadVehicleConfig(path.string(), config, error, &warnings),
                "unusable numbers must not fail the load: " + error);
        Require(warnings.size() == 3, "each unusable number must be reported");
        Require(std::fabs(config.chassisMass - defaults.chassisMass) < 1e-4F &&
                    std::fabs(config.wheelRadius - defaults.wheelRadius) < 1e-4F,
                "a zero mass or zero-radius wheel must never reach the physics body");

        // The wheel list is exactly four: the physics layer builds a four-wheel vehicle.
        warnings.clear();
        WriteTempJson(path, R"JSON({"wheels": {"positions": [[0,0,0],[1,0,0],[2,0,0]]}})JSON");
        Require(IronGang::LoadVehicleConfig(path.string(), config, error, &warnings),
                "a short wheel list must not fail the load: " + error);
        Require(warnings.size() == 1 && warnings.front().find("exactly 4") != std::string::npos,
                "the wheel count must be named in the warning: " +
                    (warnings.empty() ? std::string() : warnings.front()));
        Require(std::fabs(config.wheelPositions[0].X - defaults.wheelPositions[0].X) < 1e-4F,
                "a bad wheel list must leave every default position");

        // A malformed wheel entry is refused as a set rather than half-applied.
        warnings.clear();
        WriteTempJson(path,
                      R"JSON({"wheels": {"positions": [[0,0,0],[1,0],[2,0,0],[3,0,0]]}})JSON");
        Require(IronGang::LoadVehicleConfig(path.string(), config, error, &warnings),
                "a malformed wheel entry must not fail the load: " + error);
        Require(std::fabs(config.wheelPositions[0].X - defaults.wheelPositions[0].X) < 1e-4F,
                "one bad wheel must not leave three applied and one defaulted");

        // Typos are named, and reversing faster than driving forward is called out.
        warnings.clear();
        WriteTempJson(path, R"JSON({"chassis": {"masss": 900}, "wheels": {"radius": 0.4}})JSON");
        Require(IronGang::LoadVehicleConfig(path.string(), config, error, &warnings),
                "an unknown key must not fail the load: " + error);
        Require(warnings.size() == 1 && warnings.front().find("chassis.masss") != std::string::npos,
                "the unknown key must be named with its section");
        warnings.clear();
        WriteTempJson(path,
                      R"JSON({"performance": {"maxForwardSpeed": 10, "maxReverseSpeed": 20}})JSON");
        Require(IronGang::LoadVehicleConfig(path.string(), config, error, &warnings) &&
                    warnings.size() == 1,
                "reversing faster than driving forward must warn");

        // Only an unreadable file or an unsupported version is a failure, and neither touches the
        // caller's own configuration.
        IronGang::VehicleConfig untouched;
        untouched.id = "caller's sedan";
        WriteTempJson(path, "{ not json");
        Require(!IronGang::LoadVehicleConfig(path.string(), untouched, error, &warnings),
                "malformed JSON must be an error");
        WriteTempJson(path, R"JSON({"version": 99})JSON");
        Require(!IronGang::LoadVehicleConfig(path.string(), untouched, error, &warnings),
                "an unsupported version must be an error");
        Require(error.find("99") != std::string::npos, "the refused version must be named: " + error);
        Require(untouched.id == "caller's sedan", "a failed load must leave the caller's vehicle alone");

        // The committed sedan must load cleanly **and reproduce the numbers the code used to
        // hard-code** -- that is what proves moving them into data changed nothing about driving.
        warnings.clear();
        Require(IronGang::LoadVehicleConfig(std::string(IRON_GANG_SOURCE_ASSET_DIR) +
                                                "/vehicles/sedan.vehicle.json",
                                            config, error, &warnings),
                "the committed sedan must load: " + error);
        Require(warnings.empty(), "the committed sedan must produce no warnings: " +
                                      (warnings.empty() ? std::string() : warnings.front()));
        Require(config.id == "sedan" && std::fabs(config.chassisMass - 1400.0F) < 1e-4F &&
                    std::fabs(config.wheelRadius - 0.33F) < 1e-4F &&
                    std::fabs(config.wheelWidth - 0.30F) < 1e-4F &&
                    std::fabs(config.maxForwardSpeed - 22.0F) < 1e-4F &&
                    std::fabs(config.maxReverseSpeed - 6.0F) < 1e-4F,
                "the committed sedan must carry exactly the previously hard-coded values");
        Require(std::fabs(config.chassisHalfExtents.X - 1.05F) < 1e-4F &&
                    std::fabs(config.chassisHalfExtents.Y - 0.325F) < 1e-4F &&
                    std::fabs(config.chassisHalfExtents.Z - 2.1F) < 1e-4F,
                "the committed chassis must still match PrototypeRenderer's body box");
        for (std::size_t index = 0; index < config.wheelPositions.size(); ++index)
        {
            Require((config.wheelPositions[index] - defaults.wheelPositions[index]).Length() < 1e-4F,
                    "the committed wheel positions must match the renderer's wheel offsets");
        }

        std::filesystem::remove(path);
    }

    // plan_20 IG-20-010 / plan_21 IG-21-002: the shared lane-clearance test both movers rely on.
    void TestLaneClearanceSeesOnlyWhatIsAhead()
    {
        const IronGang::Vector3 origin{0.0F, 0.0F, 0.0F};
        constexpr float kFacingNegativeZ = 0.0F; // ForwardFromYaw(0) points down -Z

        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {0.0F, 0.0F, -5.0F}, 2.0F) > 4.9F &&
                    IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {0.0F, 0.0F, -5.0F}, 2.0F) < 5.1F,
                "something straight ahead must report its distance");
        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {0.0F, 0.0F, 5.0F}, 2.0F) ==
                    IronGang::kNoObstacleAhead,
                "something behind must not count as an obstacle");
        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, origin, 2.0F) ==
                    IronGang::kNoObstacleAhead,
                "something in exactly the same place must not count -- it is not ahead");
        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {1.5F, 0.0F, -5.0F}, 2.0F) <
                    IronGang::kNoObstacleAhead,
                "something inside the lane must count");
        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {2.5F, 0.0F, -5.0F}, 2.0F) ==
                    IronGang::kNoObstacleAhead,
                "something beyond the lane's half-width must not count");
        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {0.0F, 9.0F, -5.0F}, 2.0F) <
                    IronGang::kNoObstacleAhead,
                "height must be ignored: roads and sidewalks are flat here");

        // The narrow walking lane is what lets two people pass shoulder to shoulder.
        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {0.9F, 0.0F, -2.0F},
                                              IronGang::kWalkingLaneHalfWidth) ==
                    IronGang::kNoObstacleAhead,
                "a pedestrian in the next walking lane must not block this one");
        Require(IronGang::DistanceAheadInLane(origin, kFacingNegativeZ, {0.9F, 0.0F, -2.0F},
                                              IronGang::kTrafficLaneHalfWidth) < IronGang::kNoObstacleAhead,
                "the same offset is inside a traffic lane, which is why the widths differ");
    }

    // plan_20 IG-20-010: twelve pedestrians on two two-point sidewalks used to walk through each
    // other. Lanes separate the two directions of travel; yielding stops a follower rather than
    // letting it pass through the pedestrian ahead.
    void TestPedestriansDoNotWalkThroughEachOther()
    {
        const IronGang::WaypointPath sidewalk{{{0.0F, 0.9F, -20.0F}, {0.0F, 0.9F, 20.0F}}, true};

        // Head-on: two pedestrians on the same line, walking opposite ways, in opposite lanes.
        IronGang::Pedestrian northbound;
        northbound.Reset(sidewalk, 0, 1.5F, 15.0F);
        northbound.SetLaneOffset(0.45F);
        IronGang::Pedestrian southbound;
        southbound.Reset(sidewalk, 1, 1.5F, 15.0F);
        southbound.SetLaneOffset(0.45F);

        float closestApproach = 1e9F;
        for (int frame = 0; frame < 400; ++frame)
        {
            northbound.Update(0.05F, false, IronGang::Vector3{}, IronGang::kNoObstacleAhead);
            southbound.Update(0.05F, false, IronGang::Vector3{}, IronGang::kNoObstacleAhead);
            const IronGang::Vector3 separation = northbound.GetPosition() - southbound.GetPosition();
            closestApproach = std::min(closestApproach, separation.Length());
        }
        Require(closestApproach > 0.5F,
                "pedestrians walking opposite ways must pass beside each other, not through: closest " +
                    std::to_string(closestApproach));

        // Their lane offsets put them on opposite sides of the centreline.
        Require(std::fabs(northbound.GetPosition().X - northbound.GetPathPosition().X) > 0.1F,
                "a lane offset must actually move where the pedestrian stands");
        Require((northbound.GetPosition().X - northbound.GetPathPosition().X) *
                        (southbound.GetPosition().X - southbound.GetPathPosition().X) <
                    0.0F,
                "opposite directions of travel must end up on opposite sides of the path");

        // Following: a fast walker behind a stopped one must stop rather than pass through it.
        IronGang::Pedestrian leader;
        leader.Reset(sidewalk, 0, 1.4F, 10.0F);
        IronGang::Pedestrian follower;
        follower.Reset(sidewalk, 0, 1.4F, 8.0F);
        const float leaderZ = leader.GetPosition().Z;
        for (int frame = 0; frame < 200; ++frame)
        {
            // The leader is standing still (blocked by something of its own); the follower sees it.
            const float clearance = IronGang::DistanceAheadInLane(follower.GetPosition(), follower.GetYaw(),
                                                                  leader.GetPosition(),
                                                                  IronGang::kWalkingLaneHalfWidth);
            follower.Update(0.05F, false, IronGang::Vector3{}, clearance);
        }
        const float gap = leaderZ - follower.GetPosition().Z;
        Require(gap > 0.2F, "the follower must stop short of the pedestrian ahead, not overlap it: gap " +
                                std::to_string(gap));
        Require(gap < 2.5F, "the follower must still close up to a queueing distance: gap " +
                                std::to_string(gap));

        // Once the way is clear it walks on again -- yielding must not deadlock a pedestrian.
        const float stoppedZ = follower.GetPosition().Z;
        for (int frame = 0; frame < 40; ++frame)
        {
            follower.Update(0.05F, false, IronGang::Vector3{}, IronGang::kNoObstacleAhead);
        }
        Require(follower.GetPosition().Z > stoppedZ + 1.0F,
                "a pedestrian must resume walking once the way ahead clears");

        // Fleeing ignores congestion on purpose: someone running from a car does not queue.
        IronGang::Pedestrian fleeing;
        fleeing.Reset(sidewalk, 0, 1.4F, 10.0F);
        const IronGang::Vector3 threat = fleeing.GetPosition() + IronGang::Vector3(0.0F, 0.0F, -2.0F);
        const IronGang::Vector3 beforeFlee = fleeing.GetPosition();
        fleeing.Update(0.1F, true, threat, 0.0F); // zero clearance: blocked in every sense
        Require(fleeing.IsFleeing(), "a threat must start the flee state");
        Require((fleeing.GetPosition() - beforeFlee).Length() > 0.01F,
                "a fleeing pedestrian must keep moving even with no clearance ahead");
    }

    // plan_20 IG-20-001 / plan_21 IG-21-001: several pedestrians share one sidewalk path without
    // stacking on its endpoint, and a spawn offset leaves them walking the way they were headed.
    void TestPedestrianSpawnOffsetSpreadsAlongPath()
    {
        const IronGang::WaypointPath sidewalk{{{-7.5F, 0.9F, -38.0F}, {-7.5F, 0.9F, 38.0F}}, true};
        const float length = (sidewalk.points[1] - sidewalk.points[0]).Length();

        // No offset keeps gate M9's exact behaviour: standing on the chosen waypoint.
        IronGang::Pedestrian atStart;
        atStart.Reset(sidewalk, 0, 1.6F);
        Require(std::fabs(atStart.GetPosition().Z - sidewalk.points[0].Z) < 1e-4F,
                "a zero offset must leave the pedestrian on its start waypoint");

        // An offset walks it along the segment, and several offsets give several distinct places.
        std::vector<float> spawnedZ;
        for (int i = 0; i < 6; ++i)
        {
            const float offset = length * (static_cast<float>(i) + 0.5F) / 6.0F;
            IronGang::Pedestrian pedestrian;
            pedestrian.Reset(sidewalk, 0, 1.6F, offset);
            Require(std::fabs(pedestrian.GetPosition().Z - (sidewalk.points[0].Z + offset)) < 1e-3F,
                    "the pedestrian must stand exactly its offset along the segment");
            for (const float existing : spawnedZ)
            {
                Require(std::fabs(existing - pedestrian.GetPosition().Z) > 1.0F,
                        "spawned pedestrians must not stack on each other");
            }
            spawnedZ.push_back(pedestrian.GetPosition().Z);
        }

        // Offsetting past the end of the segment clamps to it rather than overshooting the path.
        IronGang::Pedestrian clamped;
        clamped.Reset(sidewalk, 0, 1.6F, length * 10.0F);
        Require(std::fabs(clamped.GetPosition().Z - sidewalk.points[1].Z) < 1e-3F,
                "an offset past the segment must clamp to its far end");

        // A pedestrian spawned mid-segment keeps walking toward the far end, not back to the one
        // it started from.
        IronGang::Pedestrian walking;
        walking.Reset(sidewalk, 0, 4.0F, length * 0.5F);
        const float before = walking.GetPosition().Z;
        walking.Update(1.0F, false, IronGang::Vector3{});
        Require(walking.GetPosition().Z > before,
                "a mid-segment spawn must continue toward the next waypoint, not turn round");

        // The other endpoint works the same way, in the other direction.
        IronGang::Pedestrian reversed;
        reversed.Reset(sidewalk, 1, 4.0F, length * 0.25F);
        Require(reversed.GetPosition().Z < sidewalk.points[1].Z && reversed.GetPosition().Z > sidewalk.points[0].Z,
                "an offset from the far waypoint must land inside the segment");
        const float reversedBefore = reversed.GetPosition().Z;
        reversed.Update(1.0F, false, IronGang::Vector3{});
        Require(reversed.GetPosition().Z < reversedBefore,
                "a pedestrian started from the far end must walk back toward the near one");
    }

    // plan_04 IG-04-003/004/007: the simulation clock clamps a stall instead of letting it
    // teleport the world, refuses to run backwards on a broken timer, and keeps monotonic
    // simulation time that owes nothing to the wall clock.
    void TestSimulationClockClampsStallsAndStaysMonotonic()
    {
        IronGang::SimulationClock clock;
        Require(std::fabs(clock.GetMaximumStepSeconds() -
                          IronGang::SimulationClock::kDefaultMaximumStepSeconds) < 1e-6F,
                "the clock must start at its documented maximum step");

        // An ordinary frame passes through untouched.
        const float ordinary = clock.Advance(1.0F / 60.0F);
        Require(std::fabs(ordinary - 1.0F / 60.0F) < 1e-6F, "an ordinary delta must pass through unchanged");
        Require(clock.GetClampedStepCount() == 0 && clock.GetDroppedSeconds() == 0.0,
                "an ordinary delta must not count as a stall");

        // A stall is clamped, and the time the world refused to take is accounted for rather than
        // quietly vanishing.
        const float stalled = clock.Advance(2.5F);
        Require(std::fabs(stalled - IronGang::SimulationClock::kDefaultMaximumStepSeconds) < 1e-6F,
                "a stall must be clamped to the maximum step");
        Require(clock.GetClampedStepCount() == 1, "the clamp must be counted");
        Require(std::fabs(clock.GetDroppedSeconds() -
                          (2.5 - IronGang::SimulationClock::kDefaultMaximumStepSeconds)) < 1e-4,
                "the wall time the simulation refused must be reported, not discarded silently");

        // Elapsed simulation time is the sum of what actually ran -- not of what was asked for.
        Require(std::fabs(clock.GetElapsedSeconds() -
                          (1.0 / 60.0 + IronGang::SimulationClock::kDefaultMaximumStepSeconds)) < 1e-4,
                "elapsed time must be the sum of the deltas the simulation took");
        Require(clock.GetFrameCount() == 2, "every advanced frame must be counted");

        // A broken timer must not run the world backwards or poison the totals.
        const double elapsedBefore = clock.GetElapsedSeconds();
        Require(clock.Advance(-1.0F) == 0.0F, "a negative delta must yield no movement");
        Require(clock.Advance(std::numeric_limits<float>::quiet_NaN()) == 0.0F,
                "a NaN delta must yield no movement");
        Require(clock.Advance(std::numeric_limits<float>::infinity()) == 0.0F,
                "an infinite delta must yield no movement");
        Require(clock.Advance(0.0F) == 0.0F, "a zero delta must yield no movement");
        Require(std::fabs(clock.GetElapsedSeconds() - elapsedBefore) < 1e-9,
                "a broken delta must leave elapsed time exactly where it was");
        Require(clock.GetFrameCount() == 6, "a refused frame still happened and must be counted");
        Require(clock.GetClampedStepCount() == 1, "a refused frame is not a clamped one");

        // Monotonicity across a long run of mixed good and broken deltas.
        double previous = clock.GetElapsedSeconds();
        for (int frame = 0; frame < 200; ++frame)
        {
            const float delta = (frame % 17 == 0) ? 3.0F : (frame % 5 == 0) ? -0.5F : 1.0F / 60.0F;
            const float taken = clock.Advance(delta);
            Require(taken >= 0.0F, "the clock must never hand back a negative step");
            Require(taken <= clock.GetMaximumStepSeconds() + 1e-6F,
                    "the clock must never hand back more than the maximum step");
            const double now = clock.GetElapsedSeconds();
            Require(now >= previous, "simulation time must never go backwards");
            previous = now;
        }

        // The maximum is configurable, and a nonsensical one is ignored rather than freezing time.
        clock.Configure(0.25F);
        Require(std::fabs(clock.GetMaximumStepSeconds() - 0.25F) < 1e-6F,
                "a sensible maximum must apply");
        Require(std::fabs(clock.Advance(10.0F) - 0.25F) < 1e-6F, "the configured maximum must be used");
        clock.Configure(0.0F);
        clock.Configure(-1.0F);
        clock.Configure(std::numeric_limits<float>::quiet_NaN());
        Require(std::fabs(clock.GetMaximumStepSeconds() - 0.25F) < 1e-6F,
                "a maximum of zero, negative, or NaN must be ignored -- a clock that cannot advance "
                "is not a configuration anyone means to ask for");

        clock.Reset();
        Require(clock.GetElapsedSeconds() == 0.0 && clock.GetDroppedSeconds() == 0.0 &&
                    clock.GetFrameCount() == 0 && clock.GetClampedStepCount() == 0,
                "Reset must clear every total");
        Require(std::fabs(clock.GetMaximumStepSeconds() - 0.25F) < 1e-6F,
                "Reset must keep the configured maximum -- it is a setting, not a total");
    }

    // plan_04 IG-04-002: severity ordering, category filtering, the exact line format, and the
    // sink that lets a test (or a future in-game console) take the output.
    void TestLogSeverityAndCategoryFiltering()
    {
        struct Captured
        {
            IronGang::LogCategory category;
            IronGang::LogSeverity severity;
            std::string message;
        };
        std::vector<Captured> captured;
        IronGang::Log::Reset();
        IronGang::Log::SetSink([&captured](IronGang::LogCategory category, IronGang::LogSeverity severity,
                                           const std::string& message) {
            captured.push_back(Captured{category, severity, message});
        });

        // The default minimum is Info: debug detail stays out of an ordinary run.
        Require(IronGang::Log::GetMinimumSeverity() == IronGang::LogSeverity::Info,
                "the default minimum severity must be info");
        IronGang::Log::Debug(IronGang::LogCategory::Mission, "not shown");
        IronGang::Log::Info(IronGang::LogCategory::Mission, "shown");
        IronGang::Log::Warning(IronGang::LogCategory::Save, "also shown");
        IronGang::Log::Error(IronGang::LogCategory::Assets, "definitely shown");
        Require(captured.size() == 3, "debug must be filtered out at the default minimum");
        Require(captured.front().message == "shown" && captured.front().category == IronGang::LogCategory::Mission,
                "the category and message must reach the sink unchanged");

        // Raising the minimum drops everything below it.
        captured.clear();
        IronGang::Log::SetMinimumSeverity(IronGang::LogSeverity::Error);
        IronGang::Log::Info(IronGang::LogCategory::Mission, "dropped");
        IronGang::Log::Warning(IronGang::LogCategory::Mission, "dropped");
        IronGang::Log::Error(IronGang::LogCategory::Mission, "kept");
        Require(captured.size() == 1 && captured.front().message == "kept",
                "only messages at or above the minimum severity may pass");

        // Lowering it to debug lets everything through.
        captured.clear();
        IronGang::Log::SetMinimumSeverity(IronGang::LogSeverity::Debug);
        IronGang::Log::Debug(IronGang::LogCategory::Audio, "detail");
        Require(captured.size() == 1, "debug must pass once the minimum allows it");

        // A disabled category is silent at every severity -- errors included, by design.
        captured.clear();
        IronGang::Log::SetCategoryEnabled(IronGang::LogCategory::Audio, false);
        Require(!IronGang::Log::IsCategoryEnabled(IronGang::LogCategory::Audio),
                "the category must report itself disabled");
        IronGang::Log::Error(IronGang::LogCategory::Audio, "silenced");
        IronGang::Log::Error(IronGang::LogCategory::Mission, "heard");
        Require(captured.size() == 1 && captured.front().category == IronGang::LogCategory::Mission,
                "a disabled category must be silent even at error severity");
        Require(!IronGang::Log::IsEnabled(IronGang::LogCategory::Audio, IronGang::LogSeverity::Error),
                "IsEnabled must agree with what Write actually does");

        // The formatted line is what the default sink writes, and what a log grep sees.
        Require(IronGang::Log::FormatLine(IronGang::LogCategory::Mission, IronGang::LogSeverity::Warning,
                                          "something") == "[IronGang][mission][warning] something",
                "the log line format must be [IronGang][category][severity] message");

        // Names and parsing round-trip for every value, which is what the config file and
        // --log-level rely on.
        for (const IronGang::LogSeverity severity :
             {IronGang::LogSeverity::Debug, IronGang::LogSeverity::Info, IronGang::LogSeverity::Warning,
              IronGang::LogSeverity::Error})
        {
            IronGang::LogSeverity parsed{};
            Require(IronGang::ParseLogSeverity(IronGang::LogSeverityName(severity), parsed) &&
                        parsed == severity,
                    "every severity name must parse back to itself");
        }
        for (const IronGang::LogCategory category :
             {IronGang::LogCategory::Application, IronGang::LogCategory::Assets, IronGang::LogCategory::Audio,
              IronGang::LogCategory::Config, IronGang::LogCategory::Cutscene, IronGang::LogCategory::Dialogue,
              IronGang::LogCategory::Mission, IronGang::LogCategory::Save})
        {
            IronGang::LogCategory parsed{};
            Require(IronGang::ParseLogCategory(IronGang::LogCategoryName(category), parsed) &&
                        parsed == category,
                    "every category name must parse back to itself");
        }
        IronGang::LogSeverity ignoredSeverity{};
        IronGang::LogCategory ignoredCategory{};
        Require(!IronGang::ParseLogSeverity("verbose", ignoredSeverity),
                "an unknown severity name must be rejected");
        Require(!IronGang::ParseLogCategory("physics", ignoredCategory),
                "an unknown category name must be rejected");

        // Reset puts the log back to stderr at info with every category on, so one test cannot
        // leave the next one deaf.
        IronGang::Log::Reset();
        Require(IronGang::Log::GetMinimumSeverity() == IronGang::LogSeverity::Info &&
                    IronGang::Log::IsCategoryEnabled(IronGang::LogCategory::Audio),
                "Reset must restore the defaults");
    }

    // plan_04 IG-04-001/006: the configuration loader's defaults, validation, and round trip. The
    // rule the tests protect is that a broken or partial config costs the tuning, never the run --
    // only a file that cannot be understood at all is a failure.
    void TestGameConfigLoadsValidatesAndFallsBack()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_config.json";
        const IronGang::GameConfig defaults;
        IronGang::GameConfig config;
        std::string error;
        std::vector<std::string> warnings;

        // A missing file is not a failure: the defaults are a complete configuration.
        Require(IronGang::LoadGameConfig((path.string() + ".missing"), config, error, &warnings),
                "a missing configuration file must not be an error");
        Require(config.projectName == defaults.projectName &&
                    std::fabs(config.autosaveIntervalSeconds - defaults.autosaveIntervalSeconds) < 1e-4F,
                "a missing file must leave every default in place");
        Require(warnings.size() == 1, "a missing file must be reported once, as a warning");

        // Every field round-trips.
        warnings.clear();
        WriteTempJson(path, R"JSON({
            "projectName": "Test Title",
            "cityName": "Test City",
            "prototypeYear": 1948,
            "autosaveIntervalSeconds": 45.5,
            "autosaveMinimumSpacingSeconds": 5,
            "logSeverity": "warning"
        })JSON");
        Require(IronGang::LoadGameConfig(path.string(), config, error, &warnings),
                "a well-formed configuration must load: " + error);
        Require(warnings.empty(), "a well-formed configuration must produce no warnings");
        Require(config.projectName == "Test Title" && config.cityName == "Test City" &&
                    config.prototypeYear == 1948,
                "string and integer values must round-trip");
        Require(config.logSeverity == IronGang::LogSeverity::Warning, "logSeverity must round-trip");
        Require(std::fabs(config.autosaveIntervalSeconds - 45.5F) < 1e-4F &&
                    std::fabs(config.autosaveMinimumSpacingSeconds - 5.0F) < 1e-4F,
                "second values must round-trip");

        // An unknown key is the most common configuration mistake, so it must be reported.
        warnings.clear();
        WriteTempJson(path, R"JSON({"projectNmae": "typo", "cityName": "Kept"})JSON");
        Require(IronGang::LoadGameConfig(path.string(), config, error, &warnings),
                "an unknown key must not fail the load: " + error);
        Require(warnings.size() == 1 && warnings.front().find("projectNmae") != std::string::npos,
                "the unknown key must be named in the warning");
        Require(config.projectName == defaults.projectName, "the mistyped key must leave the default");
        Require(config.cityName == "Kept", "the keys that were right must still apply");

        // A value of the wrong type keeps the default and says so.
        warnings.clear();
        WriteTempJson(path,
                      R"JSON({"projectName": 7, "prototypeYear": "soon", "autosaveIntervalSeconds": "later"})JSON");
        Require(IronGang::LoadGameConfig(path.string(), config, error, &warnings),
                "wrong-typed values must not fail the load: " + error);
        Require(warnings.size() == 3, "each wrong-typed value must be reported once");
        Require(config.projectName == defaults.projectName && config.prototypeYear == defaults.prototypeYear &&
                    std::fabs(config.autosaveIntervalSeconds - defaults.autosaveIntervalSeconds) < 1e-4F,
                "wrong-typed values must leave their defaults");

        // Out-of-range values: a year is rejected outright, negative seconds mean "off".
        warnings.clear();
        WriteTempJson(path,
                      R"JSON({"prototypeYear": 12000, "autosaveIntervalSeconds": -5, "projectName": ""})JSON");
        Require(IronGang::LoadGameConfig(path.string(), config, error, &warnings),
                "out-of-range values must not fail the load: " + error);
        Require(warnings.size() == 3, "each out-of-range value must be reported");
        Require(config.prototypeYear == defaults.prototypeYear, "an absurd year must keep the default");
        Require(std::fabs(config.autosaveIntervalSeconds) < 1e-4F,
                "negative seconds must clamp to 0 -- the author meant \"off\"");
        Require(config.projectName == defaults.projectName, "an empty string must keep the default");

        // An unrecognized severity name keeps the default and says what the choices are.
        warnings.clear();
        WriteTempJson(path, R"JSON({"logSeverity": "verbose"})JSON");
        Require(IronGang::LoadGameConfig(path.string(), config, error, &warnings),
                "an unknown severity name must not fail the load: " + error);
        Require(warnings.size() == 1 && warnings.front().find("debug/info/warning/error") != std::string::npos,
                "the warning must list the accepted severity names");
        Require(config.logSeverity == defaults.logSeverity, "an unknown severity must keep the default");

        // A spacing longer than the interval is legal but almost always a mistake.
        warnings.clear();
        WriteTempJson(path,
                      R"JSON({"autosaveIntervalSeconds": 10, "autosaveMinimumSpacingSeconds": 60})JSON");
        Require(IronGang::LoadGameConfig(path.string(), config, error, &warnings),
                "a spacing longer than the interval must still load: " + error);
        Require(warnings.size() == 1 && warnings.front().find("spacing") != std::string::npos,
                "the spacing/interval combination must be reported");

        // Only an unreadable file is a failure, and it leaves the caller's defaults alone.
        warnings.clear();
        WriteTempJson(path, "{ this is not json");
        IronGang::GameConfig untouched;
        untouched.projectName = "Caller's value";
        Require(!IronGang::LoadGameConfig(path.string(), untouched, error, &warnings),
                "malformed JSON must be an error");
        Require(untouched.projectName == "Caller's value",
                "a failed load must leave the caller's configuration untouched");
        WriteTempJson(path, R"JSON(["not", "an", "object"])JSON");
        Require(!IronGang::LoadGameConfig(path.string(), untouched, error, &warnings),
                "a non-object root must be an error");
        Require(error.find("object") != std::string::npos, "the error must say what was wrong: " + error);

        // The committed configuration must load cleanly -- no typos, no stale keys, no warnings.
        warnings.clear();
        Require(IronGang::LoadGameConfig(std::string(IRON_GANG_SOURCE_ASSET_DIR) + "/config/game.json",
                                         config, error, &warnings),
                "the committed configuration must load: " + error);
        Require(warnings.empty(),
                "the committed configuration must produce no warnings: " +
                    (warnings.empty() ? std::string() : warnings.front()));
        Require(config.projectName == "Iron Shadows" && config.cityName == "Iron City" &&
                    config.prototypeYear == 1932,
                "the committed configuration must carry the project's identity");

        std::filesystem::remove(path);
    }

    // plan_29 IG-29-010/011/037: when an autosave is allowed to happen. The two behaviours worth
    // protecting are that a request made at an unsafe moment is held rather than dropped, and that
    // two triggers landing together produce one save rather than two.
    void TestAutosaveSchedulingAvoidsUnsafeMoments()
    {
        IronGang::AutosaveScheduler scheduler;
        scheduler.Configure(100.0F, 10.0F);

        // The interval fires when it elapses, not before.
        Require(scheduler.Update(99.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::None,
                "the interval must not fire early");
        Require(scheduler.Update(1.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::Interval,
                "the interval must fire once it elapses");
        Require(scheduler.GetSecondsSinceLastSave() < 1e-4F, "saving must restart the interval");
        Require(scheduler.Update(1.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::None,
                "the interval must not fire again immediately");

        // A request during an unsafe moment is held, not dropped, and fires the instant it is safe.
        scheduler.Reset();
        scheduler.Request(IronGang::AutosaveTrigger::Checkpoint);
        for (int frame = 0; frame < 60; ++frame)
        {
            Require(scheduler.Update(1.0F, IronGang::SaveBlockReason::Cutscene) ==
                        IronGang::AutosaveTrigger::None,
                    "no autosave may happen during a cutscene");
        }
        Require(scheduler.GetPendingTrigger() == IronGang::AutosaveTrigger::Checkpoint,
                "the request must still be pending after the block");
        Require(scheduler.Update(0.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::Checkpoint,
                "the held request must fire the moment saving is safe again");

        // Minimum spacing collapses two triggers that land together into one save, and the second
        // request is still held until the spacing has passed rather than being thrown away.
        scheduler.Reset();
        scheduler.Request(IronGang::AutosaveTrigger::DistrictArrival);
        Require(scheduler.Update(11.0F, IronGang::SaveBlockReason::None) ==
                    IronGang::AutosaveTrigger::DistrictArrival,
                "the first request must be served once the spacing has passed");
        scheduler.Request(IronGang::AutosaveTrigger::Checkpoint);
        Require(scheduler.Update(1.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::None,
                "a second trigger inside the minimum spacing must not write again immediately");
        Require(scheduler.GetPendingTrigger() == IronGang::AutosaveTrigger::Checkpoint,
                "the deferred request must be kept");
        Require(scheduler.Update(9.5F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::Checkpoint,
                "the deferred request must fire once the spacing has passed");

        // A higher-priority trigger takes over the label; the file written is the same either way.
        scheduler.Reset();
        scheduler.Request(IronGang::AutosaveTrigger::Interval);
        scheduler.Request(IronGang::AutosaveTrigger::Checkpoint);
        scheduler.Request(IronGang::AutosaveTrigger::DistrictArrival);
        Require(scheduler.Update(20.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::Checkpoint,
                "the highest-priority pending trigger must be the one reported");

        // Requesting every frame while a condition holds must not queue up saves.
        scheduler.Reset();
        for (int frame = 0; frame < 5; ++frame)
        {
            scheduler.Request(IronGang::AutosaveTrigger::Checkpoint);
        }
        Require(scheduler.Update(20.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::Checkpoint,
                "a repeated request must be served once");
        Require(scheduler.Update(20.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::None,
                "a repeated request must not be served twice");

        // Reset abandons a pending request: whatever was worth saving is gone.
        scheduler.Request(IronGang::AutosaveTrigger::Checkpoint);
        scheduler.Reset();
        Require(scheduler.GetPendingTrigger() == IronGang::AutosaveTrigger::None,
                "Reset must abandon the pending request");
        Require(scheduler.Update(1000.0F, IronGang::SaveBlockReason::Dialogue) ==
                    IronGang::AutosaveTrigger::None,
                "a blocked interval must not fire either");

        // Periodic autosaves can be turned off without disabling event-driven ones.
        IronGang::AutosaveScheduler eventsOnly;
        eventsOnly.Configure(0.0F, 0.0F);
        Require(eventsOnly.Update(10000.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::None,
                "a zero interval must disable periodic autosaves");
        eventsOnly.Request(IronGang::AutosaveTrigger::Checkpoint);
        Require(eventsOnly.Update(0.0F, IronGang::SaveBlockReason::None) == IronGang::AutosaveTrigger::Checkpoint,
                "event triggers must still work with periodic autosaves off");

        // Every unsafe moment blocks, and the reported one follows the documented order.
        IronGang::SaveConditions conditions;
        Require(IronGang::FindSaveBlockReason(conditions) == IronGang::SaveBlockReason::None,
                "ordinary play must not block saving");
        conditions.vehicleTransitionActive = true;
        Require(IronGang::FindSaveBlockReason(conditions) == IronGang::SaveBlockReason::VehicleTransition,
                "entering or leaving the car must block saving");
        conditions.districtTransitioning = true;
        Require(IronGang::FindSaveBlockReason(conditions) == IronGang::SaveBlockReason::DistrictTransition,
                "a district load must outrank the vehicle transition");
        conditions.dialogueActive = true;
        Require(IronGang::FindSaveBlockReason(conditions) == IronGang::SaveBlockReason::Dialogue,
                "dialogue must outrank the district load");
        conditions.cutsceneActive = true;
        Require(IronGang::FindSaveBlockReason(conditions) == IronGang::SaveBlockReason::Cutscene,
                "a cutscene must outrank everything else");
        for (const IronGang::SaveBlockReason reason :
             {IronGang::SaveBlockReason::Cutscene, IronGang::SaveBlockReason::Dialogue,
              IronGang::SaveBlockReason::DistrictTransition, IronGang::SaveBlockReason::VehicleTransition})
        {
            Require(std::strlen(IronGang::DescribeSaveBlockReason(reason)) > 0,
                    "every blocking reason must have player-facing text");
        }
        for (const IronGang::AutosaveTrigger trigger :
             {IronGang::AutosaveTrigger::Interval, IronGang::AutosaveTrigger::DistrictArrival,
              IronGang::AutosaveTrigger::Checkpoint})
        {
            Require(std::strlen(IronGang::DescribeAutosaveTrigger(trigger)) > 0,
                    "every autosave trigger must have player-facing text");
        }
    }

    // plan_29 IG-29-010: "load" means "resume", so the newest save wins whether the player wrote
    // it or the autosave did.
    void TestLoadChoosesTheMostRecentSave()
    {
        const std::filesystem::path directory = std::filesystem::current_path() / "iron_gang_recent_saves";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const std::string manual = (directory / "prototype.save").string();
        const std::string automatic = (directory / "prototype.autosave").string();
        std::string error;

        Require(IronGang::SaveGame::ChooseMostRecent({manual, automatic}).empty(),
                "with no saves at all there is nothing to choose");

        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = "reach_vehicle";
        Require(IronGang::SaveGame::Write(manual, snapshot, error), "writing the manual save: " + error);
        Require(IronGang::SaveGame::ChooseMostRecent({manual, automatic}) == manual,
                "the only existing save must be chosen");

        // Give the autosave a strictly later timestamp rather than relying on filesystem
        // resolution, which is coarse enough on some systems to make two writes look simultaneous.
        snapshot.missionStateId = "drive_to_warehouse";
        Require(IronGang::SaveGame::Write(automatic, snapshot, error), "writing the autosave: " + error);
        std::filesystem::last_write_time(std::filesystem::path(automatic),
                                         std::filesystem::last_write_time(std::filesystem::path(manual)) +
                                             std::chrono::seconds(5));
        Require(IronGang::SaveGame::ChooseMostRecent({manual, automatic}) == automatic,
                "a newer autosave must win over an older manual save");

        std::filesystem::last_write_time(std::filesystem::path(manual),
                                         std::filesystem::last_write_time(std::filesystem::path(automatic)) +
                                             std::chrono::seconds(5));
        Require(IronGang::SaveGame::ChooseMostRecent({manual, automatic}) == manual,
                "a newer manual save must win over an older autosave");

        std::filesystem::remove(manual);
        Require(IronGang::SaveGame::ChooseMostRecent({manual, automatic}) == automatic,
                "a missing candidate must be skipped rather than chosen");

        std::filesystem::remove_all(directory);
    }

    // plan_29 IG-29-009/029/030: a checkpoint records the world it was reached in, not just the
    // mission's own state, and both halves survive a save/load -- so a retry after loading a save
    // puts the player back where the checkpoint was, instead of restarting the mission.
    void TestCheckpointWorldSurvivesSaveLoad()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_checkpoint_world.save";
        std::string error;

        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = "busted";
        snapshot.playerPosition = {40.0F, 1.7F, -12.0F}; // where the failure happened
        snapshot.playerYaw = 2.5F;
        snapshot.vehiclePosition = {41.0F, 0.65F, -12.0F};
        snapshot.vehicleSpeed = 0.0F;
        snapshot.playerDriving = true;
        snapshot.districtId = IronGang::DistrictId::WarehouseBlock;
        snapshot.missionCheckpoint.stateId = "drive_to_warehouse";
        snapshot.missionCheckpoint.variables.push_back(
            IronGang::MissionVariableSnapshot{"cargo_secured", IronGang::MissionValue::Bool(true)});

        IronGang::WorldStateSnapshot checkpointWorld;
        checkpointWorld.playerPosition = {2.0F, 1.7F, 9.0F}; // where the checkpoint was reached
        checkpointWorld.playerYaw = 0.75F;
        checkpointWorld.vehiclePosition = {2.0F, 0.65F, 9.5F};
        checkpointWorld.vehicleYaw = -1.25F;
        checkpointWorld.vehicleSpeed = 4.5F;
        checkpointWorld.playerDriving = true;
        checkpointWorld.districtId = IronGang::DistrictId::Countryside;
        snapshot.missionCheckpointWorld = checkpointWorld;

        Require(IronGang::SaveGame::Write(path.string(), snapshot, error), "writing must succeed: " + error);
        std::optional<IronGang::SaveSnapshot> loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "reading must succeed: " + error);
        Require(loaded->missionCheckpointWorld.has_value(), "the checkpoint's world half must survive");

        const IronGang::WorldStateSnapshot& restored = *loaded->missionCheckpointWorld;
        Require(std::fabs(restored.playerPosition.X - 2.0F) < 1e-4F &&
                    std::fabs(restored.playerPosition.Z - 9.0F) < 1e-4F,
                "the checkpoint's player position must survive exactly");
        Require(std::fabs(restored.playerYaw - 0.75F) < 1e-4F &&
                    std::fabs(restored.vehicleYaw + 1.25F) < 1e-4F,
                "the checkpoint's yaws must survive");
        Require(std::fabs(restored.vehicleSpeed - 4.5F) < 1e-4F, "the checkpoint's speed must survive");
        Require(restored.playerDriving, "the checkpoint's driving flag must survive");
        Require(restored.districtId == IronGang::DistrictId::Countryside,
                "the checkpoint's district must survive independently of the current one");
        Require(loaded->districtId == IronGang::DistrictId::WarehouseBlock,
                "the live district must not be overwritten by the checkpoint's");
        Require(std::fabs(loaded->playerPosition.X - 40.0F) < 1e-4F,
                "the live player position must stay distinct from the checkpoint's");

        // A save with no checkpoint world -- an older file, or a mission with no checkpoint --
        // loads with none rather than inventing one.
        IronGang::SaveSnapshot plain = snapshot;
        plain.missionCheckpointWorld.reset();
        Require(IronGang::SaveGame::Write(path.string(), plain, error),
                "writing without a checkpoint world must succeed: " + error);
        loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value() && !loaded->missionCheckpointWorld.has_value(),
                "a save with no checkpoint world must load with none");

        // The world half is all-or-nothing: a partial one would put the player somewhere and the
        // vehicle nowhere, so it is dropped rather than half-applied.
        WriteTempJson(path,
                      "format=iron-gang-save-v1\n"
                      "mission_state_id=busted\n"
                      "player_position=1,1.7,2\n"
                      "player_yaw=0\n"
                      "vehicle_position=3,0.65,4\n"
                      "vehicle_yaw=0\n"
                      "vehicle_speed=0\n"
                      "player_driving=0\n"
                      "mission_checkpoint_state_id=drive_to_warehouse\n"
                      "checkpoint_player_position=2,1.7,9\n"
                      "checkpoint_player_yaw=0.75\n");
        loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "a partial checkpoint world must not fail the load: " + error);
        Require(!loaded->missionCheckpointWorld.has_value(),
                "a partial checkpoint world must be dropped, not half-applied");
        Require(loaded->missionCheckpoint.stateId == "drive_to_warehouse",
                "dropping the world half must leave the mission half intact");

        std::filesystem::remove(path);
    }

    // plan_29 IG-29-001/002/003/004/023: the save format's integrity guarantees -- an atomic
    // write that cannot leave a half-written file, one rolling backup, a checksum that refuses a
    // damaged file, and a version check that refuses a file from a newer build.
    void TestSaveFormatRobustness()
    {
        const std::filesystem::path directory =
            std::filesystem::current_path() / "iron_gang_save_robustness";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const std::filesystem::path path = directory / "prototype.save";
        const std::string backupPath = IronGang::SaveGame::BackupPath(path.string());
        const std::string temporaryPath = IronGang::SaveGame::TemporaryPath(path.string());
        std::string error;

        IronGang::SaveSnapshot first;
        first.missionStateId = "reach_vehicle";
        first.playerPosition = {1.0F, 1.7F, 2.0F};
        first.vehiclePosition = {3.0F, 0.65F, 4.0F};
        Require(IronGang::SaveGame::Write(path.string(), first, error), "the first write must succeed: " + error);
        Require(!std::filesystem::exists(temporaryPath),
                "a completed write must leave no temporary file behind");
        Require(!std::filesystem::exists(backupPath), "the first write has no previous save to back up");

        const auto readDocument = [](const std::filesystem::path& file) {
            std::ifstream input(file, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        };
        const std::string document = readDocument(path);
        Require(document.rfind("format=iron-gang-save-v" + std::to_string(IronGang::kCurrentSaveFormatVersion),
                               0) == 0,
                "a save must declare the current format version on its first line");
        Require(document.find("\nchecksum=") != std::string::npos,
                "a save must carry a checksum line");

        IronGang::SaveReadDiagnostics diagnostics;
        std::optional<IronGang::SaveSnapshot> loaded =
            IronGang::SaveGame::Read(path.string(), error, &diagnostics);
        Require(loaded.has_value() && loaded->missionStateId == "reach_vehicle",
                "a freshly written save must read back: " + error);
        Require(!diagnostics.usedBackup && diagnostics.formatVersion == IronGang::kCurrentSaveFormatVersion,
                "a current-format primary save must report neither a backup nor a migration");

        // The second write rotates the first save into the backup (IG-29-003).
        IronGang::SaveSnapshot second = first;
        second.missionStateId = "drive_to_warehouse";
        Require(IronGang::SaveGame::Write(path.string(), second, error), "the second write must succeed: " + error);
        Require(std::filesystem::exists(backupPath), "the previous save must be kept as a backup");
        Require(!std::filesystem::exists(temporaryPath), "no temporary file may survive the write");
        loaded = IronGang::SaveGame::Read(backupPath, error, &diagnostics);
        Require(loaded.has_value() && loaded->missionStateId == "reach_vehicle",
                "the backup must hold the previous save: " + error);

        // A damaged primary file must be refused and the backup used instead (IG-29-003/004).
        {
            std::string damaged = readDocument(path);
            const std::size_t position = damaged.find("drive_to_warehouse");
            Require(position != std::string::npos, "the test needs a byte it can flip");
            damaged[position] = 'X';
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << damaged;
        }
        loaded = IronGang::SaveGame::Read(path.string(), error, &diagnostics);
        Require(loaded.has_value(), "a damaged primary save must fall back to the backup: " + error);
        Require(diagnostics.usedBackup, "falling back to the backup must be reported");
        Require(diagnostics.primaryError.find("corrupt") != std::string::npos,
                "the reported reason must name the corruption: " + diagnostics.primaryError);
        Require(loaded->missionStateId == "reach_vehicle", "the backup's contents must be what loads");

        // Truncation is the other half of the same guarantee.
        {
            const std::string truncated = readDocument(path).substr(0, 40);
            std::ofstream output(backupPath, std::ios::binary | std::ios::trunc);
            output << truncated;
        }
        Require(!IronGang::SaveGame::Read(path.string(), error, &diagnostics).has_value(),
                "a damaged save with a damaged backup must fail rather than load garbage");
        Require(!error.empty(), "the failure must be reported");

        // A file from a newer build must be refused, not half-read.
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << "format=iron-gang-save-v" << (IronGang::kCurrentSaveFormatVersion + 1) << "\n"
                   << "checksum=0000000000000000\n"
                   << "mission_state_id=reach_vehicle\n";
        }
        std::filesystem::remove(backupPath);
        Require(!IronGang::SaveGame::Read(path.string(), error, &diagnostics).has_value(),
                "a newer format version must be refused");
        Require(error.find("newer version") != std::string::npos,
                "the refusal must say the save is from a newer build: " + error);

        // A version-1 file still loads, and reports that it was migrated (IG-29-001).
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << "format=iron-gang-save-v1\n"
                   << "mission_state=3\n"
                   << "player_position=1,1.7,2\n"
                   << "player_yaw=0\n"
                   << "vehicle_position=3,0.65,4\n"
                   << "vehicle_yaw=0\n"
                   << "vehicle_speed=0\n"
                   << "player_driving=0\n";
        }
        loaded = IronGang::SaveGame::Read(path.string(), error, &diagnostics);
        Require(loaded.has_value() && loaded->missionStateId == "drive_to_warehouse",
                "a version-1 save must still load: " + error);
        Require(diagnostics.formatVersion == 1, "the file's own version must be reported");

        // A missing required field is reported by name rather than surfacing as an exception.
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << "format=iron-gang-save-v1\nmission_state_id=reach_vehicle\n";
        }
        Require(!IronGang::SaveGame::Read(path.string(), error, &diagnostics).has_value(),
                "a save missing required fields must be refused");
        Require(error.find("player_position") != std::string::npos,
                "the missing field must be named: " + error);

        // A temporary file left behind by an interrupted write is ignored, and the next write
        // replaces it rather than tripping over it.
        {
            std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
            output << "half a save, interrupted";
        }
        Require(IronGang::SaveGame::Write(path.string(), second, error),
                "a leftover temporary file must not block the next write: " + error);
        Require(!std::filesystem::exists(temporaryPath), "the leftover temporary file must be replaced");
        loaded = IronGang::SaveGame::Read(path.string(), error, &diagnostics);
        Require(loaded.has_value() && loaded->missionStateId == "drive_to_warehouse",
                "the save written over a leftover temporary must be sound: " + error);

        // A write that cannot complete must leave the existing save untouched (IG-29-023's
        // disk-full case, approximated with a read-only directory).
        const std::string beforeFailedWrite = readDocument(path);
        std::error_code permissionError;
        std::filesystem::permissions(directory,
                                     std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::remove,
                                     permissionError);
        if (!permissionError)
        {
            IronGang::SaveSnapshot third = second;
            third.missionStateId = "completed";
            const bool wrote = IronGang::SaveGame::Write(path.string(), third, error);
            std::filesystem::permissions(directory,
                                         std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::add,
                                         permissionError);
            // Running as a user who can write anyway (root) makes this case untestable; only
            // assert when the write really did fail.
            if (!wrote)
            {
                Require(readDocument(path) == beforeFailedWrite,
                        "a failed write must leave the existing save byte-for-byte intact");
                Require(!std::filesystem::exists(temporaryPath),
                        "a failed write must not leave a temporary file behind");
            }
        }

        std::filesystem::remove_all(directory);
    }

    // plan_24 IG-24-018: a save written before mission states became free-form stored a 0-4 index
    // into a fixed enum. Those saves must still load, mapped onto the ids they meant.
    void TestSaveMigratesLegacyMissionState()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_legacy.save";
        const std::string body =
            "player_position=1,1.7,2\n"
            "player_yaw=0\n"
            "vehicle_position=3,0.65,4\n"
            "vehicle_yaw=0\n"
            "vehicle_speed=0\n"
            "player_driving=0\n";

        WriteTempJson(path, "format=iron-gang-save-v1\nmission_state=3\n" + body);
        std::string error;
        std::optional<IronGang::SaveSnapshot> loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "a legacy save must still load: " + error);
        Require(loaded->missionStateId == "drive_to_warehouse",
                "a legacy mission_state index must map onto the id it meant");

        WriteTempJson(path, "format=iron-gang-save-v1\nmission_state=0\n" + body);
        loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value() && loaded->missionStateId == "introduction",
                "the first legacy state must map to introduction");

        WriteTempJson(path, "format=iron-gang-save-v1\nmission_state=9\n" + body);
        Require(!IronGang::SaveGame::Read(path.string(), error).has_value(),
                "an out-of-range legacy mission_state must be rejected, not silently clamped");

        WriteTempJson(path, "format=iron-gang-save-v1\n" + body);
        Require(!IronGang::SaveGame::Read(path.string(), error).has_value(),
                "a save with no mission state at all must be rejected");

        // A new-format save wins over a legacy field if both are somehow present.
        WriteTempJson(path,
                      "format=iron-gang-save-v1\nmission_state=0\nmission_state_id=enter_vehicle\n" + body);
        loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value() && loaded->missionStateId == "enter_vehicle",
                "mission_state_id must take precedence over the legacy index");

        std::filesystem::remove(path);
    }

    // plan_24 IG-24-013/032/034: the expression evaluator's operators, precedence, typing, and
    // short-circuiting, evaluated against a context of declared facts and variables.
    void TestMissionExpressionEvaluatesTypedOperations()
    {
        IronGang::MissionContext context;
        std::string error;
        Require(context.DeclareFact("driving", IronGang::MissionValue::Bool(true), error) &&
                    context.DeclareFact("distance", IronGang::MissionValue::Float(4.5F), error) &&
                    context.DeclareVariable("crates", IronGang::MissionValue::Int(3), error) &&
                    context.DeclareVariable("contact", IronGang::MissionValue::String("Mara"), error),
                "declaring the test symbols must succeed: " + error);

        const auto evaluate = [&](const std::string& source) {
            IronGang::MissionExpression expression;
            std::string compileError;
            Require(IronGang::MissionExpression::Compile(source, context, expression, compileError),
                    "expression \"" + source + "\" must compile: " + compileError);
            IronGang::MissionValue value;
            std::string evaluateError;
            Require(expression.Evaluate(context, value, evaluateError),
                    "expression \"" + source + "\" must evaluate: " + evaluateError);
            return value;
        };

        Require(evaluate("true").AsBool(), "a bool literal must evaluate to itself");
        Require(evaluate("driving").AsBool(), "a bool fact must evaluate to its current value");
        Require(!evaluate("!driving").AsBool(), "'!' must negate a bool");
        Require(evaluate("crates").AsInt() == 3, "an int variable must evaluate to its current value");
        Require(evaluate("crates + 2 * 3").AsInt() == 9, "'*' must bind tighter than '+'");
        Require(evaluate("(crates + 2) * 3").AsInt() == 15, "parentheses must override precedence");
        Require(evaluate("crates - 5").AsInt() == -2, "int subtraction must produce a negative value");
        Require(evaluate("-crates").AsInt() == -3, "unary '-' must negate an int");
        Require(evaluate("crates / 2").AsInt() == 1, "int division must truncate");
        Require(evaluate("7 / 2.0").GetType() == IronGang::MissionValueType::Float,
                "mixing an int and a float must promote the result to float");
        Require(std::fabs(evaluate("7 / 2.0").AsFloat() - 3.5F) < 1e-6F,
                "float division must not truncate");
        Require(evaluate("distance < 5").AsBool(), "'<' must compare a float fact against an int literal");
        Require(evaluate("distance >= 4.5").AsBool(), "'>=' must be inclusive");
        Require(!evaluate("distance > 5").AsBool(), "'>' must compare in the right direction");
        Require(evaluate("crates == 3 && driving").AsBool(), "'&&' must combine two true operands");
        Require(!evaluate("crates != 3 || !driving").AsBool(), "'||' must combine two false operands");
        Require(evaluate("contact == 'Mara'").AsBool(), "strings must compare by value");
        Require(evaluate("contact != 'Salieri'").AsBool(), "'!=' must work on strings");
        Require(evaluate("driving && distance <= 4.5 && crates > 0").AsBool(),
                "comparison must bind tighter than '&&'");

        // Short-circuiting is observable through the step limit: the right operand of a decided
        // '&&'/'||' is never evaluated, so a division by zero hidden behind one cannot fail.
        Require(!evaluate("false && 1 / 0 == 0").AsBool(),
                "'&&' must not evaluate its right operand when the left one is false");
        Require(evaluate("true || 1 / 0 == 0").AsBool(),
                "'||' must not evaluate its right operand when the left one is true");

        // A compiled expression reads the context's current values, not a snapshot from compile time.
        IronGang::MissionExpression expression;
        Require(IronGang::MissionExpression::Compile("crates > 3", context, expression, error),
                "compiling against live values must succeed: " + error);
        bool result = true;
        Require(expression.EvaluateBool(context, result, error) && !result,
                "the expression must see crates == 3 before the change");
        Require(context.SetVariable("crates", IronGang::MissionValue::Int(4), error),
                "updating the variable must succeed: " + error);
        Require(expression.EvaluateBool(context, result, error) && result,
                "the same compiled expression must see the updated value");

        // ResetVariables restores declared values, and leaves facts alone.
        context.ResetVariables();
        IronGang::MissionValue crates;
        Require(context.TryGetValue("crates", crates) && crates.AsInt() == 3,
                "ResetVariables must restore a variable's declared initial value");
        IronGang::MissionValue driving;
        Require(context.TryGetValue("driving", driving) && driving.AsBool(),
                "ResetVariables must leave engine facts untouched");
    }

    // plan_24 IG-24-014/033: every malformed expression must be a compile-time error with a
    // column, and the depth/length/step limits must actually bound what a mission file can do.
    void TestMissionExpressionRejectsMalformedInput()
    {
        IronGang::MissionContext context;
        std::string error;
        Require(context.DeclareFact("driving", IronGang::MissionValue::Bool(false), error) &&
                    context.DeclareVariable("crates", IronGang::MissionValue::Int(1), error),
                "declaring the test symbols must succeed: " + error);

        const auto rejects = [&](const std::string& source, const std::string& why) {
            IronGang::MissionExpression expression;
            std::string compileError;
            Require(!IronGang::MissionExpression::Compile(source, context, expression, compileError), why);
            Require(!compileError.empty(), why + " (with a non-empty error message)");
            Require(expression.IsEmpty(), why + " (leaving the output expression empty)");
        };

        rejects("", "an empty expression must be rejected");
        rejects("   ", "a whitespace-only expression must be rejected");
        rejects("missing_symbol", "an unknown identifier must be rejected");
        rejects("crates +", "a missing right operand must be rejected");
        rejects("+ crates", "a missing left operand must be rejected");
        rejects("(crates", "an unbalanced '(' must be rejected");
        rejects("crates)", "an unbalanced ')' must be rejected");
        rejects("crates crates", "trailing input must be rejected");
        rejects("crates + true", "adding a bool to an int must be rejected");
        rejects("!crates", "negating an int must be rejected");
        rejects("-driving", "arithmetically negating a bool must be rejected");
        rejects("driving && crates", "using an int as a '&&' operand must be rejected");
        rejects("crates < 'two'", "ordering an int against a string must be rejected");
        rejects("driving == 1", "comparing a bool against an int must be rejected");
        rejects("1 < 2 < 3", "a chained comparison must be rejected");
        rejects("crates & 1", "a single '&' must be rejected");
        rejects("crates $ 1", "an unexpected character must be rejected");
        rejects("'unterminated", "an unterminated string literal must be rejected");
        rejects("1.", "a float literal with no fractional digits must be rejected");
        rejects("99999999999", "an out-of-range int literal must be rejected");

        rejects(std::string(IronGang::kMissionExpressionMaxLength + 1, 'a'),
                "an over-long expression must be rejected before parsing");
        std::string deep;
        for (std::size_t index = 0; index <= IronGang::kMissionExpressionMaxDepth; ++index)
        {
            deep += "(";
        }
        deep += "driving";
        for (std::size_t index = 0; index <= IronGang::kMissionExpressionMaxDepth; ++index)
        {
            deep += ")";
        }
        rejects(deep, "an expression nested past the depth limit must be rejected");

        std::string wide = "crates";
        for (std::size_t index = 0; index < IronGang::kMissionExpressionMaxTokens; ++index)
        {
            wide += " + 1";
        }
        rejects(wide, "an expression past the token limit must be rejected");

        // Division by zero is the one fault the type check cannot catch, so it must be a clean
        // evaluation error rather than a crash or a silent infinity.
        IronGang::MissionExpression expression;
        Require(IronGang::MissionExpression::Compile("crates / 0", context, expression, error),
                "a divide-by-zero expression must still compile: " + error);
        IronGang::MissionValue value;
        std::string evaluateError;
        Require(!expression.Evaluate(context, value, evaluateError),
                "dividing by zero must fail evaluation instead of producing a value");
        Require(!evaluateError.empty(), "a divide-by-zero must report an actionable error");

        // EvaluateBool must refuse a non-bool expression rather than coerce it.
        Require(IronGang::MissionExpression::Compile("crates + 1", context, expression, error),
                "an int expression must compile: " + error);
        bool ignored = false;
        Require(!expression.EvaluateBool(context, ignored, evaluateError),
                "EvaluateBool must refuse a non-bool expression");

        // A default-constructed expression is the "terminal state, no condition" case.
        const IronGang::MissionExpression empty;
        bool emptyResult = true;
        Require(empty.IsEmpty(), "a default-constructed expression must be empty");
        Require(empty.EvaluateBool(context, emptyResult, evaluateError) && !emptyResult,
                "an empty condition must evaluate to false without an error");
    }

    // plan_24 IG-24-005: the typed variable store's declaration, typed assignment, and reset
    // rules, including the ones that protect a mission from a save file or a bad action.
    void TestMissionVariablesEnforceTypes()
    {
        IronGang::MissionContext context;
        std::string error;
        Require(context.DeclareVariable("crates", IronGang::MissionValue::Int(2), error),
                "declaring a variable must succeed: " + error);
        Require(context.DeclareFact("driving", IronGang::MissionValue::Bool(false), error),
                "declaring a fact must succeed: " + error);

        Require(!context.DeclareVariable("crates", IronGang::MissionValue::Int(0), error),
                "declaring the same variable twice must be rejected");
        Require(!context.DeclareVariable("driving", IronGang::MissionValue::Bool(false), error),
                "a variable must not shadow a fact");
        Require(!context.DeclareVariable("", IronGang::MissionValue::Int(0), error),
                "an empty variable name must be rejected");

        Require(!context.SetVariable("crates", IronGang::MissionValue::String("two"), error),
                "assigning a string to an int variable must be rejected");
        Require(!context.SetVariable("driving", IronGang::MissionValue::Bool(true), error),
                "a fact must be read-only to mission actions");
        Require(!context.SetVariable("nothing", IronGang::MissionValue::Int(1), error),
                "assigning an undeclared variable must be rejected");
        Require(!context.SetFact("crates", IronGang::MissionValue::Int(1), error),
                "SetFact must refuse a variable");
        Require(!context.SetFact("driving", IronGang::MissionValue::Int(1), error),
                "SetFact must refuse a value of the wrong type");

        Require(context.SetVariable("crates", IronGang::MissionValue::Int(7), error),
                "a correctly typed assignment must succeed: " + error);
        const std::vector<IronGang::MissionVariableSnapshot> captured = context.CaptureVariables();
        Require(captured.size() == 1 && captured.front().name == "crates" &&
                    captured.front().value.AsInt() == 7,
                "CaptureVariables must return the variables only, at their current values");

        // Every type must survive ToText()/Parse() exactly -- this is what the save file relies on.
        const IronGang::MissionValue values[] = {
            IronGang::MissionValue::Bool(true),
            IronGang::MissionValue::Int(-1234),
            IronGang::MissionValue::Float(0.1F),
            IronGang::MissionValue::String("Mara: 3 crates"),
        };
        for (const IronGang::MissionValue& value : values)
        {
            IronGang::MissionValue parsed;
            Require(IronGang::MissionValue::Parse(value.GetType(), value.ToText(), parsed),
                    "MissionValue::Parse must accept its own ToText() output for " +
                        std::string(IronGang::MissionValueTypeName(value.GetType())));
            Require(parsed.GetType() == value.GetType() && parsed.ToText() == value.ToText(),
                    "a MissionValue must round-trip through text exactly");
        }
        IronGang::MissionValue rejected;
        Require(!IronGang::MissionValue::Parse(IronGang::MissionValueType::Int, "12x", rejected),
                "trailing garbage must not parse as an int");
        Require(!IronGang::MissionValue::Parse(IronGang::MissionValueType::Bool, "yes", rejected),
                "only true/false must parse as a bool");
    }

    // plan_24 IG-24-007/016: entry actions run exactly once per state entry, in file order, and a
    // retry (Reset) restores the declared values and runs the initial state's actions again.
    void TestMissionEntryActionsRunOncePerEntry()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_action_mission.json";
        WriteTempJson(path, R"JSON({
            "id": "action_mission",
            "version": 2,
            "initialState": "introduction",
            "variables": [
                { "id": "entries", "type": "int", "value": 0 },
                { "id": "started", "type": "bool", "value": false },
                { "id": "radius", "type": "float", "value": 3.0 }
            ],
            "states": [
                { "id": "introduction", "objective": "Wait",
                  "onEnter": [ { "action": "set", "variable": "started", "value": "true" },
                               { "action": "set", "variable": "entries", "value": "entries + 1" } ],
                  "when": "dialogue_finished", "next": "reach_vehicle" },
                { "id": "reach_vehicle", "objective": "Walk",
                  "onEnter": [ { "action": "set", "variable": "entries", "value": "entries + 10" } ],
                  "when": "player_vehicle_distance <= radius", "next": "completed" },
                { "id": "completed", "objective": "Done" }
            ]
        })JSON");

        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        std::string error;
        Require(mission.LoadMission(path.string(), error), "the action mission must load: " + error);
        mission.Reset();

        IronGang::MissionValue value;
        Require(mission.TryGetVariable("started", value) && value.AsBool(),
                "Reset must run the initial state's entry actions");
        Require(mission.TryGetVariable("entries", value) && value.AsInt() == 1,
                "the initial state's counter action must run exactly once");

        // Staying in the same state for several frames must not re-run its entry actions.
        for (int frame = 0; frame < 3; ++frame)
        {
            mission.Update(false, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false,
                           world.GetWarehouseGoal());
        }
        Require(mission.TryGetVariable("entries", value) && value.AsInt() == 1,
                "entry actions must not re-run while the state is unchanged");

        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.IsInState("reach_vehicle"),
                "the expression condition must advance the mission");
        Require(mission.TryGetVariable("entries", value) && value.AsInt() == 11,
                "the entered state's action must run exactly once, after the transition");

        // A float variable read by a condition: standing on the sedan is within radius.
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        Require(mission.IsCompleted(), "a condition comparing a fact against a variable must fire");

        // Reset is a retry: declared values come back and the initial actions run again.
        mission.Reset();
        Require(mission.IsInState("introduction"),
                "Reset must return to the initial state");
        Require(mission.TryGetVariable("entries", value) && value.AsInt() == 1,
                "Reset must restore declared values before re-running the initial actions");

        std::filesystem::remove(path);
    }

    // plan_24 IG-24-029/039 and IG-24-019: mission variables survive a save/load round trip, and a
    // save naming a variable the mission no longer declares loads with a warning instead of failing.
    void TestMissionVariablesSurviveSaveLoad()
    {
        IronGang::PrototypeWorld world;
        IronGang::PrototypeMission mission;
        std::string error;
        Require(mission.LoadMission(std::string(IRON_GANG_SOURCE_ASSET_DIR) + "/missions/prologue.mission.json",
                                    error),
                "the committed mission must load: " + error);
        mission.Reset();

        // Advance far enough that the committed mission has actually written a variable.
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        IronGang::MissionValue briefingRead;
        Require(mission.TryGetVariable("briefing_read", briefingRead) && briefingRead.AsBool(),
                "reaching reach_vehicle must set briefing_read");

        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = mission.GetStateId();
        snapshot.missionVariables = mission.CaptureVariables();
        Require(snapshot.missionVariables.size() == 4,
                "every declared variable must be captured for the save file");

        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_mission_variables.save";
        Require(IronGang::SaveGame::Write(path.string(), snapshot, error), "writing must succeed: " + error);
        const std::optional<IronGang::SaveSnapshot> restored = IronGang::SaveGame::Read(path.string(), error);
        Require(restored.has_value(), "reading must succeed: " + error);
        Require(restored->missionVariables.size() == snapshot.missionVariables.size(),
                "every mission variable must survive the round trip");
        for (std::size_t index = 0; index < snapshot.missionVariables.size(); ++index)
        {
            Require(restored->missionVariables[index].name == snapshot.missionVariables[index].name,
                    "mission variables must keep their file order");
            Require(restored->missionVariables[index].value.GetType() ==
                            snapshot.missionVariables[index].value.GetType() &&
                        restored->missionVariables[index].value.ToText() ==
                            snapshot.missionVariables[index].value.ToText(),
                    "a mission variable's type and value must survive the round trip");
        }

        IronGang::PrototypeMission reloaded;
        Require(reloaded.LoadMission(std::string(IRON_GANG_SOURCE_ASSET_DIR) + "/missions/prologue.mission.json",
                                     error),
                "the committed mission must load again: " + error);
        reloaded.Reset();
        Require(reloaded.SetStateId(restored->missionStateId), "restoring the saved mission state must succeed");
        std::vector<std::string> warnings;
        reloaded.ApplyVariables(restored->missionVariables, &warnings);
        Require(warnings.empty(), "restoring a matching save must not warn");
        IronGang::MissionValue value;
        Require(reloaded.TryGetVariable("briefing_read", value) && value.AsBool(),
                "the restored mission must resume with the saved variable values");

        // IG-24-019: a save written by an older/edited mission file must not fail the load.
        std::vector<IronGang::MissionVariableSnapshot> stale = restored->missionVariables;
        stale.push_back(IronGang::MissionVariableSnapshot{"removed_variable", IronGang::MissionValue::Int(1)});
        stale.push_back(
            IronGang::MissionVariableSnapshot{"briefing_read", IronGang::MissionValue::String("yes")});
        warnings.clear();
        reloaded.ApplyVariables(stale, &warnings);
        Require(warnings.size() == 2, "an unknown name and a changed type must each warn");
        Require(reloaded.TryGetVariable("briefing_read", value) && value.AsBool(),
                "a rejected value must leave the existing variable untouched");

        // A malformed variable line must be skipped without failing the rest of the save. Written
        // as a version-1 document, which has no checksum -- appending to a version-2 save would
        // (correctly) be rejected as corrupt instead, which is what TestSaveFormatRobustness covers.
        WriteTempJson(path,
                      "format=iron-gang-save-v1\n"
                      "mission_state_id=reach_vehicle\n"
                      "player_position=1,1.7,2\n"
                      "player_yaw=0\n"
                      "vehicle_position=3,0.65,4\n"
                      "vehicle_yaw=0\n"
                      "vehicle_speed=0\n"
                      "player_driving=0\n"
                      "mission_var.briefing_read=bool:true\n"
                      "mission_var.broken=int:not-a-number\n"
                      "mission_var.=int:1\n");
        const std::optional<IronGang::SaveSnapshot> tolerant = IronGang::SaveGame::Read(path.string(), error);
        Require(tolerant.has_value(), "a malformed mission variable must not fail the whole load: " + error);
        Require(tolerant->missionVariables.size() == 1 &&
                    tolerant->missionVariables.front().name == "briefing_read",
                "a malformed mission variable must be skipped and the sound ones kept");

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
        const IronGang::PoliceUpdateWorkload onFootWorkload = police.Update(
            1.0F, false, origin, 120.0F, {IronGang::Vector3(1.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(onFootWorkload.witnessChecks == 0 && onFootWorkload.patrolUpdates == 0,
                "police workload must count only loops that actually execute");
        Require(police.GetState() == IronGang::PoliceState::Clear,
                "an offense while not driving must never be witnessed");

        // Driving fast, but the only witness is far outside the witness radius (15 units).
        const IronGang::PoliceUpdateWorkload farWitnessWorkload = police.Update(
            1.0F, true, origin, 120.0F, {IronGang::Vector3(1000.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(farWitnessWorkload.witnessChecks == 1 && farWitnessWorkload.patrolUpdates == 0,
                "police workload must count each tested witness even when no offense is seen");
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
        const IronGang::PoliceUpdateWorkload escalationWorkload =
            police.Update(19.0F, true, origin, 0.0F, {}, spawnPosition);
        Require(escalationWorkload.patrolUpdates == 2,
                "police workload must count both patrol updates on the escalation tick");
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
        Require(mission.IsInState("drive_to_warehouse"),
                "setup: mission must reach DriveToWarehouse before saving");

        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_playthrough_test.save";
        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = mission.GetStateId();
        snapshot.playerPosition = world.GetVehicleSpawn();
        snapshot.vehiclePosition = world.GetVehicleSpawn();
        snapshot.playerDriving = true;
        snapshot.districtId = world.GetId();

        std::string error;
        Require(IronGang::SaveGame::Write(path.string(), snapshot, error), "save write failed: " + error);
        const auto loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "save read failed: " + error);

        IronGang::PrototypeMission resumedMission;
        Require(resumedMission.SetStateId(loaded->missionStateId), "restoring the saved mission state must succeed");
        Require(resumedMission.IsInState("drive_to_warehouse"),
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
        Require(mission.IsInState("reach_vehicle"),
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
        Require(mission.IsInState("drive_to_warehouse"),
                "setup: mission must be mid-flight before retrying");

        mission.Reset();
        Require(mission.IsInState("introduction"),
                "Reset() must return a mid-mission run to the mission's own initial state");

        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        mission.Update(true, world.GetWarehouseGoal().bounds.center, world.GetWarehouseGoal().bounds.center, true,
                      world.GetWarehouseGoal());
        Require(mission.IsCompleted(), "a retried mission must be able to complete again");
    }

    // Gate M11 / plan_39 IG-39-064 (vehicle-loss recovery): the sedan can now be wrecked by
    // impacts (plan_17 IG-17-015) but never destroyed or removed -- a wreck still steers and rolls
    // -- so "losing" a vehicle still means being separated from it. Recovery is therefore proven
    // at the level that actually exists: if the player saves while separated from their own
    // vehicle (on foot, vehicle parked somewhere else), loading must restore BOTH independently
    // rather than collapsing one onto the other -- the vehicle is never actually "lost" as long as
    // its own saved position survives the round trip.
    void TestVehicleStatePersistsIndependentlyOfPlayer()
    {
        const std::filesystem::path path =
            std::filesystem::current_path() / "iron_gang_vehicle_recovery_test.save";
        IronGang::SaveSnapshot snapshot;
        snapshot.missionStateId = "reach_vehicle";
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
        Require(mission.IsInState("enter_vehicle"),
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
        Require(mission.IsInState("enter_vehicle"),
                "leaving the district must not disturb the mission's state");

        districts.RequestTransition(physics); // Countryside -> WarehouseBlock
        for (int i = 0; i < 60; ++i)
        {
            districts.Update(1.0F / 60.0F);
        }
        Require(districts.ConsumeArrival(), "arrival must be reported once the return trip's loading screen finishes");
        Require(mission.IsInState("enter_vehicle"),
                "returning to the original district must not disturb the mission's state either");
    }

    void TestSaveRoundTrip()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_core_test.save";
        IronGang::SaveSnapshot source;
        source.missionStateId = "drive_to_warehouse";
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
        Require(loaded->missionStateId == source.missionStateId, "mission state round-trip failed");
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
        const auto missionScenario = IronGang::ParsePerformanceScenario("mission");
        Require(introScenario == IronGang::PerformanceScenario::Intro &&
                    idleScenario == IronGang::PerformanceScenario::Idle &&
                    walkScenario == IronGang::PerformanceScenario::Walk &&
                    driveScenario == IronGang::PerformanceScenario::Drive &&
                    mixedScenario == IronGang::PerformanceScenario::Mixed &&
                    missionScenario == IronGang::PerformanceScenario::Mission,
                "every documented performance scenario must parse to its distinct enum value");
        Require(!IronGang::ParsePerformanceScenario("unknown"),
                "an unknown performance scenario must be rejected");
        Require(std::string(IronGang::PerformanceScenarioName(*driveScenario)) == "drive",
                "performance scenario report names must round-trip through the parser");
        Require(std::string(IronGang::PerformanceScenarioName(*missionScenario)) == "mission",
                "mission performance scenario must round-trip through the parser");

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
        IronGang::AiWorkloadSample aiWorkload;
        aiWorkload.trafficVehicles = 2;
        aiWorkload.pedestrians = 2;
        aiWorkload.fleeingPedestrians = 1;
        aiWorkload.policePatrols = 1;
        aiWorkload.trafficUpdates = 2;
        aiWorkload.trafficObstacleChecks = 4;
        aiWorkload.pedestrianUpdates = 2;
        aiWorkload.pedestrianThreatChecks = 2;
        aiWorkload.policeWitnessChecks = 4;
        aiWorkload.policePatrolUpdates = 1;
        profiler.RecordAiWorkload(aiWorkload);
        aiWorkload.trafficObstacleChecks = 2;
        profiler.RecordAiWorkload(aiWorkload);
        IronGang::AudioWorkloadSample audioWorkload;
        audioWorkload.loadedSoundAssets = 3;
        audioWorkload.trackedLoopInstances = 1;
        audioWorkload.trackedPlayingLoopVoices = 1;
        audioWorkload.oneShotPlayRequests = 2;
        audioWorkload.oneShotPlaySuccesses = 1;
        audioWorkload.loopPlayCommands = 1;
        audioWorkload.loopParameterUpdates = 2;
        profiler.RecordAudioWorkload(audioWorkload);
        audioWorkload.oneShotPlayRequests = 0;
        audioWorkload.oneShotPlaySuccesses = 0;
        audioWorkload.loopPlayCommands = 0;
        profiler.RecordAudioWorkload(audioWorkload);

        const IronGang::PerformanceStatistics frame =
            profiler.GetStatistics(IronGang::PerformanceMetric::FrameInterval);
        Require(frame.sampleCount == 20, "performance profiler must retain every recorded sample");
        Require(std::abs(frame.averageMilliseconds - 10.5) < 1e-9,
                "performance profiler average must match the hand-computed value");
        Require(std::abs(frame.p95Milliseconds - 19.0) < 1e-9,
                "performance profiler p95 must use the nearest-rank definition");
        Require(std::abs(frame.maximumMilliseconds - 20.0) < 1e-9,
                "performance profiler maximum must match the largest sample");
        const std::vector<IronGang::WorstFrameInterval>& worstFrames =
            profiler.GetWorstFrameIntervals();
        Require(worstFrames.size() == IronGang::kWorstFrameIntervalRetentionCount &&
                    worstFrames.front().sampleIndex == 19 &&
                    std::abs(worstFrames.front().milliseconds - 20.0) < 1e-9 &&
                    worstFrames.back().sampleIndex == 12 &&
                    std::abs(worstFrames.back().milliseconds - 13.0) < 1e-9,
                "worst-frame retention must keep the bounded largest intervals in descending order");

        IronGang::PerformanceProfiler phaseProfiler;
        phaseProfiler.SetEnabled(true);
        phaseProfiler.RecordFrameInterval(18.0, "mixed_walk", 119);
        phaseProfiler.RecordFrameInterval(75.0, "mixed_drive", 121);
        phaseProfiler.RecordFrameInterval(75.0, "mixed_walk", 80);
        const std::vector<IronGang::WorstFrameInterval>& phasedFrames =
            phaseProfiler.GetWorstFrameIntervals();
        Require(phasedFrames.size() == 3 && phasedFrames[0].sampleIndex == 1 &&
                    phasedFrames[0].phase == "mixed_drive" &&
                    phasedFrames[0].scenarioUpdate == 121 &&
                    phasedFrames[1].sampleIndex == 2 && phasedFrames[1].phase == "mixed_walk",
                "worst-frame ties must preserve sample order and retain phase/update correlation");
        const IronGang::FramePacingStatistics framePacing = profiler.GetFramePacingStatistics();
        Require(framePacing.sampleCount == 20 &&
                    framePacing.atOrBelowRecommendedBudgetCount == 16 &&
                    framePacing.aboveRecommendedAtOrBelowMinimumBudgetCount == 4 &&
                    framePacing.minimumBudgetMissCount == 0 && framePacing.hitchCount == 0,
                "frame-pacing histogram must place every interval into exact budget buckets");
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
        const IronGang::AiWorkloadStatistics obstacleChecks =
            profiler.GetAiWorkloadStatistics(IronGang::AiWorkloadMetric::TrafficObstacleChecks);
        Require(obstacleChecks.sampleCount == 2 && std::abs(obstacleChecks.average - 3.0) < 1e-9 &&
                    std::abs(obstacleChecks.p95 - 4.0) < 1e-9 &&
                    std::abs(obstacleChecks.maximum - 4.0) < 1e-9,
                "AI workload statistics must retain exact loop counts and use nearest-rank p95");
        const IronGang::AudioWorkloadStatistics oneShotRequests =
            profiler.GetAudioWorkloadStatistics(IronGang::AudioWorkloadMetric::OneShotPlayRequests);
        Require(oneShotRequests.sampleCount == 2 && std::abs(oneShotRequests.average - 1.0) < 1e-9 &&
                    std::abs(oneShotRequests.p95 - 2.0) < 1e-9 &&
                    std::abs(oneShotRequests.maximum - 2.0) < 1e-9,
                "audio workload statistics must retain exact command counts and use nearest-rank p95");

        IronGang::PerformanceProfiler hitchProfiler;
        hitchProfiler.SetEnabled(true);
        for (const double interval : {16.0, 20.0, 33.0, 40.0})
        {
            hitchProfiler.Record(IronGang::PerformanceMetric::FrameInterval, interval);
        }
        hitchProfiler.RecordDistrictLoad({});
        hitchProfiler.Record(IronGang::PerformanceMetric::FrameInterval, 50.0);
        hitchProfiler.RecordDistrictLoad({});
        for (const double interval : {50.001, 100.0, 100.001})
        {
            hitchProfiler.Record(IronGang::PerformanceMetric::FrameInterval, interval);
        }
        const IronGang::FramePacingStatistics hitches = hitchProfiler.GetFramePacingStatistics();
        Require(hitches.sampleCount == 8 && hitches.atOrBelowRecommendedBudgetCount == 1 &&
                    hitches.aboveRecommendedAtOrBelowMinimumBudgetCount == 2 &&
                    hitches.aboveMinimumAtOrBelowHitchCount == 2 &&
                    hitches.aboveHitchAtOrBelowSevereHitchCount == 2 &&
                    hitches.aboveSevereHitchCount == 1 && hitches.minimumBudgetMissCount == 5 &&
                    hitches.hitchCount == 3 && hitches.severeHitchCount == 1,
                "frame-pacing detector must use strict hitch thresholds and exclusive buckets");
        Require(hitches.districtTransitionCount == 2 &&
                    hitches.measuredDistrictTransitionCount == 2 &&
                    hitches.districtTransitionHitchCount == 1 &&
                    hitches.maximumDistrictTransitionMilliseconds &&
                    std::abs(*hitches.maximumDistrictTransitionMilliseconds - 50.001) < 1e-9,
                "district boundaries must select the first following frame interval and flag its hitches");

        IronGang::PerformanceReportContext context;
        context.backend = "TEST";
        context.buildConfiguration = "Debug";
        context.scenario = "unit_test";
        context.width = 1280;
        context.height = 720;
        context.verticalSyncRequested = false;
        context.requestedSwapInterval = 0;
        context.nativeWindowSystem = "X11";
        context.nativeWindowAvailable = true;
        context.graphicsRuntimeIdentityKnown = true;
        context.graphicsRuntimeVendor = "Mesa/X.org";
        context.graphicsRuntimeRenderer = "AMD Radeon test GPU";
        context.graphicsRuntimeVersion = "OpenGL ES 3.2 test";
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
        Require(report.find("\"schema_version\": 8") != std::string::npos &&
                    report.find("\"capture_session\": {\"process\": {\"executable\": \"iron_gang\", \"pid_known\": true, \"pid\": ") !=
                        std::string::npos &&
                    report.find("\"started_utc\": \"") != std::string::npos &&
                    report.find("\"ended_utc\": \"") != std::string::npos &&
                    report.find("\"draw_calls\": {\"samples\": 2, \"average\": 11.000, \"p95\": 12.000") !=
                        std::string::npos &&
                    report.find("\"state_change_calls\": {\"samples\": 1, \"average\": 31.000") !=
                        std::string::npos &&
                    report.find("\"visible_objects\": {\"samples\": 1, \"average\": 42.000") !=
                        std::string::npos &&
                    report.find("excludes Clear, HUD SpriteBatch internal batching") != std::string::npos,
                "performance report must expose scoped 3D workload counts without claiming backend counters");
        Require(report.find("\"frame_pacing\": {") != std::string::npos &&
                    report.find("\"at_or_below_recommended_budget\": {\"upper_bound_ms\": 16.667, \"count\": 16}") !=
                        std::string::npos &&
                    report.find("\"above_recommended_at_or_below_minimum_budget\": {\"lower_bound_exclusive_ms\": 16.667, \"upper_bound_ms\": 33.333, \"count\": 4}") !=
                        std::string::npos &&
                    report.find("\"hitches\": {\"threshold_ms\": 50.000, \"comparison\": \"greater_than\", \"count\": 0") !=
                        std::string::npos &&
                    report.find("\"district_transition_boundaries\": {\"transitions\": 1, \"measured_samples\": 0, \"hitch_count\": 0, \"maximum_ms\": null}") !=
                        std::string::npos,
                "performance report must expose stable pacing buckets, strict hitch policy, and boundary coverage");
        Require(report.find("\"worst_frame_intervals\": {") != std::string::npos &&
                    report.find("\"retention_limit\": 8") != std::string::npos &&
                    report.find("{\"sample_index\": 19, \"interval_ms\": 20.000, \"phase\": \"unspecified\", \"scenario_update\": null}") !=
                        std::string::npos,
                "performance report must retain bounded worst-frame index/phase correlation");
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
        Require(report.find("\"traffic_obstacle_checks\": {\"samples\": 2, \"average\": 3.000, \"p95\": 4.000") !=
                        std::string::npos &&
                    report.find("\"fleeing_pedestrians\": {\"samples\": 2, \"average\": 1.000") !=
                        std::string::npos &&
                    report.find("mission state progression is excluded") != std::string::npos &&
                    report.find("no road graph or path-request queue exists yet") != std::string::npos,
                "performance report must expose exact ambient-AI state and loop work without inventing path requests");
        Require(report.find("\"loaded_sound_assets\": {\"samples\": 2, \"average\": 3.000") !=
                        std::string::npos &&
                    report.find("\"one_shot_play_requests\": {\"samples\": 2, \"average\": 1.000, \"p95\": 2.000") !=
                        std::string::npos &&
                    report.find("tracked_playing_loop_voices covers only retained SoundEffectInstances") !=
                        std::string::npos &&
                    report.find("bus cost are unavailable through CNA and are not reported as zero") !=
                        std::string::npos,
                "performance report must limit audio workload to observable assets, loop state, and commands");
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
        Require(report.find("\"native_window\": {\"system\": \"X11\", \"available\": true") !=
                        std::string::npos &&
                    report.find("CNA native-window handle classification; not physical display, vblank, or compositor proof") !=
                        std::string::npos,
                "performance report must expose machine-readable native-window availability without claiming a physical display");
        Require(report.find("\"graphics_runtime\": {\"identity_known\": true, \"vendor\": \"Mesa/X.org\"") !=
                        std::string::npos &&
                    report.find("\"renderer\": \"AMD Radeon test GPU\"") != std::string::npos &&
                    report.find("GL_VENDOR/GL_RENDERER/GL_VERSION strings; not physical display proof") !=
                        std::string::npos,
                "performance report must expose the current graphics context identity without claiming a physical display");
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
        TestMissionStateIdsAreNotAFixedSet();
        TestMissionBranchesOnFirstMatchingTransition();
        TestPrologueFailsAndRetriesUnderPoliceChase();
        TestMissionCheckpointRetryAndFailureReason();
        TestMissionCheckpointSurvivesSaveLoad();
        TestSaveMigratesLegacyMissionState();
        TestSaveFormatRobustness();
        TestCheckpointWorldSurvivesSaveLoad();
        TestRandomSourceIsDeterministicAndUniform();
        TestLocomotionAcceleratesAndDecelerates();
        TestVehicleDamageDistinguishesCrashesFromBraking();
        TestJsonDataFileIsBoundedBeforeParsing();
        TestVehicleConfigLoadsValidatesAndFallsBack();
        TestLaneClearanceSeesOnlyWhatIsAhead();
        TestPedestriansDoNotWalkThroughEachOther();
        TestPedestrianSpawnOffsetSpreadsAlongPath();
        TestSimulationClockClampsStallsAndStaysMonotonic();
        TestLogSeverityAndCategoryFiltering();
        TestGameConfigLoadsValidatesAndFallsBack();
        TestAutosaveSchedulingAvoidsUnsafeMoments();
        TestLoadChoosesTheMostRecentSave();
        TestMissionExpressionEvaluatesTypedOperations();
        TestMissionExpressionRejectsMalformedInput();
        TestMissionVariablesEnforceTypes();
        TestMissionEntryActionsRunOncePerEntry();
        TestMissionVariablesSurviveSaveLoad();
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
