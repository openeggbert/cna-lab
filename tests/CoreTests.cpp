#include "IronGang/Cutscenes/CutscenePlayer.hpp"
#include "IronGang/Cutscenes/CutsceneSequence.hpp"
#include "IronGang/Audio/AudioBuses.hpp"
#include "IronGang/Audio/AudioListener.hpp"
#include "IronGang/Core/GameConfig.hpp"
#include "IronGang/Core/JsonDataFile.hpp"
#include "IronGang/Core/Log.hpp"
#include "IronGang/Core/PerformanceProfiler.hpp"
#include "IronGang/Core/RandomSource.hpp"
#include "IronGang/Core/SimulationClock.hpp"
#include "IronGang/Dialogue/DialogueSystem.hpp"
#include "IronGang/Gameplay/InputContext.hpp"
#include "IronGang/Input/InputBindings.hpp"
#include "IronGang/Gameplay/LaneClearance.hpp"
#include "IronGang/Gameplay/Locomotion.hpp"
#include "IronGang/Gameplay/Pedestrian.hpp"
#include "IronGang/Gameplay/PedestrianCrossing.hpp"
#include "IronGang/Gameplay/PedestrianAnimation.hpp"
#include "IronGang/Gameplay/PlayerController.hpp"
#include "IronGang/Gameplay/PoliceSystem.hpp"
#include "IronGang/Gameplay/TrafficSignal.hpp"
#include "IronGang/Gameplay/CameraCollision.hpp"
#include "IronGang/Gameplay/Visibility.hpp"
#include "IronGang/Gameplay/TrafficVehicle.hpp"
#include "IronGang/Gameplay/VehicleConfig.hpp"
#include "IronGang/Gameplay/VehicleDamage.hpp"
#include "IronGang/Gameplay/VehicleController.hpp"
#include "IronGang/Missions/CampaignDefinition.hpp"
#include "IronGang/Missions/MissionDefinition.hpp"
#include "IronGang/Missions/MissionExpression.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"
#include "IronGang/Persistence/AutosavePolicy.hpp"
#include "IronGang/Core/AtomicFile.hpp"
#include "IronGang/Persistence/SaveGame.hpp"
#include "IronGang/Persistence/UserSettings.hpp"
#include "IronGang/Physics/PhysicsWorld.hpp"
#include "IronGang/Graphics/LightmapMesh.hpp"
#include "IronGang/Graphics/SunLight.hpp"
#include "IronGang/Graphics/ModelMaterials.hpp"
#include "IronGang/Graphics/ScreenshotSummary.hpp"
#include "IronGang/Input/InputScript.hpp"
#include "IronGang/Graphics/VideoMemoryAccounting.hpp"
#include "IronGang/UI/BitmapFont.hpp"
#include "IronGang/UI/DistrictMap.hpp"
#include "IronGang/UI/MenuModel.hpp"
#include "IronGang/UI/InteractionPrompt.hpp"
#include "IronGang/UI/Subtitle.hpp"
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
            police.Update(kDeltaSeconds, IronGang::PoliceObservation{true, kSpeedingKph, false}, vehicleSpawn,
                          witnesses, patrolSpawn);
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
            police.Update(kDeltaSeconds, IronGang::PoliceObservation{true, 0.0F, false}, vehicleSpawn, {},
                          patrolSpawn);
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

    // plan_24 IG-24-020/021/046/047/048: the campaign graph -- which missions exist, what unlocks
    // them, and the ways a campaign file can describe something unfinishable.
    void TestCampaignGraphUnlocksAndRejectsCycles()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_campaign.json";
        IronGang::CampaignDefinition campaign;
        std::string error;

        // The committed campaign must load, and must actually gate the second mission behind the
        // first -- that is the only thing a two-mission campaign can get wrong.
        Require(IronGang::LoadCampaignDefinition(std::string(IRON_GANG_SOURCE_ASSET_DIR) +
                                                     "/missions/campaign.json",
                                                 campaign, error),
                "the committed campaign must load: " + error);
        Require(campaign.missions.size() == 2, "the committed campaign must list both missions");
        Require(campaign.Find("prototype_delivery") != nullptr &&
                    campaign.Find("countryside_run") != nullptr,
                "both mission ids must be present");
        Require(campaign.Find("countryside_run")->requires_.size() == 1 &&
                    campaign.Find("countryside_run")->requires_.front() == "prototype_delivery",
                "the countryside run must require the prologue");

        IronGang::CampaignState state;
        Require(state.NextAvailable(campaign) == "prototype_delivery",
                "the mission with no prerequisites must be the one available first");
        Require(!state.IsAvailable(campaign, "countryside_run"),
                "a mission with an unmet prerequisite must not be available");
        Require(!state.IsFinished(campaign), "a campaign with nothing completed is not finished");

        state.MarkCompleted(campaign, "prototype_delivery");
        Require(state.IsCompleted("prototype_delivery"), "completion must be recorded");
        Require(!state.IsAvailable(campaign, "prototype_delivery"),
                "a completed mission must not be offered again");
        Require(state.NextAvailable(campaign) == "countryside_run",
                "completing the prerequisite must unlock the next mission");

        state.MarkCompleted(campaign, "countryside_run");
        Require(state.IsFinished(campaign) && state.NextAvailable(campaign).empty(),
                "a finished campaign must offer nothing further");

        // Progress from a save cannot invent missions, and completing twice cannot duplicate.
        state.SetCompleted(campaign, {"prototype_delivery", "prototype_delivery", "a_mission_we_cut"});
        Require(state.GetCompleted().size() == 1 && state.IsCompleted("prototype_delivery"),
                "restored progress must ignore duplicates and missions the campaign no longer has");
        state.Reset();
        Require(state.GetCompleted().empty() && state.NextAvailable(campaign) == "prototype_delivery",
                "Reset must start the campaign over");

        // Every way a campaign file can describe something that cannot be finished.
        const auto rejects = [&](const std::string& json, const std::string& why) {
            WriteTempJson(path, json);
            IronGang::CampaignDefinition rejected;
            Require(!IronGang::LoadCampaignDefinition(path.string(), rejected, error), why);
            Require(!error.empty(), why + " (with a reason)");
        };
        rejects(R"JSON({"version":99,"missions":[{"id":"a","path":"a.json"}]})JSON",
                "an unsupported version must be refused");
        rejects(R"JSON({"version":1})JSON", "a campaign with no missions array must be refused");
        rejects(R"JSON({"version":1,"missions":[]})JSON", "a campaign with no missions must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"","path":"a.json"}]})JSON",
                "a mission with no id must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"a"}]})JSON",
                "a mission with no path must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"a","path":"a.json"},{"id":"a","path":"b.json"}]})JSON",
                "a duplicate mission id must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"a","path":"a.json","requires":["ghost"]}]})JSON",
                "a prerequisite that is not in the campaign must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"a","path":"a.json","requires":["a"]}]})JSON",
                "a mission requiring itself must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"a","path":"a.json","requires":["b"]},
                        {"id":"b","path":"b.json","requires":["a"]}]})JSON",
                "a dependency cycle must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"a","path":"a.json","requires":["c"]},
                        {"id":"b","path":"b.json","requires":["a"]},
                        {"id":"c","path":"c.json","requires":["b"]}]})JSON",
                "a longer cycle must be refused too");
        rejects(R"JSON({"version":1,"missions":[{"id":"a","path":"a.json","requires":["b"]},
                        {"id":"b","path":"b.json","requires":["a"]}]})JSON",
                "a two-mission cycle must be refused");
        rejects(R"JSON({"version":1,"missions":[{"id":"a","path":"a.json","requires":"b"}]})JSON",
                "a non-array \"requires\" must be refused");

        // The cycle message names the cycle, not merely its existence.
        WriteTempJson(path, R"JSON({"version":1,"missions":[{"id":"a","path":"a.json","requires":["b"]},
                        {"id":"b","path":"b.json","requires":["a"]}]})JSON");
        IronGang::CampaignDefinition cyclic;
        Require(!IronGang::LoadCampaignDefinition(path.string(), cyclic, error), "the cycle must be refused");
        Require(error.find("->") != std::string::npos,
                "the cycle error must show the path that loops: " + error);

        // A valid chain of three loads and unlocks in order.
        WriteTempJson(path, R"JSON({"version":1,"missions":[
            {"id":"first","path":"1.json"},
            {"id":"second","path":"2.json","requires":["first"]},
            {"id":"third","path":"3.json","requires":["second","first"]}]})JSON");
        IronGang::CampaignDefinition chain;
        Require(IronGang::LoadCampaignDefinition(path.string(), chain, error),
                "a valid chain must load: " + error);
        IronGang::CampaignState progress;
        Require(progress.NextAvailable(chain) == "first", "the chain must start at its first mission");
        progress.MarkCompleted(chain, "first");
        Require(progress.NextAvailable(chain) == "second", "the chain must advance");
        Require(!progress.IsAvailable(chain, "third"), "a mission with two prerequisites needs both");
        progress.MarkCompleted(chain, "second");
        Require(progress.NextAvailable(chain) == "third", "both prerequisites met must unlock it");

        std::filesystem::remove(path);
    }

    // plan_24 IG-24-049: campaign progress survives a save. Without this, finishing the prologue
    // and reloading silently loses the unlock -- the save would restore a mission state against
    // whichever mission the campaign happened to start with.
    void TestCampaignProgressSurvivesSaveLoad()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_campaign.save";
        std::string error;
        IronGang::CampaignDefinition campaign;
        Require(IronGang::LoadCampaignDefinition(std::string(IRON_GANG_SOURCE_ASSET_DIR) +
                                                     "/missions/campaign.json",
                                                 campaign, error),
                "the committed campaign must load: " + error);

        IronGang::SaveSnapshot snapshot;
        snapshot.missionId = "countryside_run";
        snapshot.missionStateId = "reach_farmhouse";
        snapshot.completedMissions = {"prototype_delivery"};
        Require(IronGang::SaveGame::Write(path.string(), snapshot, error), "writing must succeed: " + error);

        const std::optional<IronGang::SaveSnapshot> loaded = IronGang::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "reading must succeed: " + error);
        Require(loaded->missionId == "countryside_run", "the mission being played must survive");
        Require(loaded->completedMissions.size() == 1 &&
                    loaded->completedMissions.front() == "prototype_delivery",
                "campaign progress must survive");

        // Restored progress drives the graph: the finished mission stays finished, and the one it
        // unlocked is the one on offer.
        IronGang::CampaignState state;
        state.SetCompleted(campaign, loaded->completedMissions);
        Require(state.IsCompleted("prototype_delivery"), "the completed mission must stay completed");
        Require(state.NextAvailable(campaign) == "countryside_run",
                "the unlocked mission must still be the one available");

        // Progress naming a mission this campaign no longer has is dropped rather than trusted --
        // a save from an edited campaign must not inject a mission that does not exist.
        IronGang::SaveSnapshot stale = snapshot;
        stale.completedMissions = {"prototype_delivery", "a_mission_we_cut", "prototype_delivery"};
        Require(IronGang::SaveGame::Write(path.string(), stale, error), "writing must succeed: " + error);
        const std::optional<IronGang::SaveSnapshot> staleLoaded =
            IronGang::SaveGame::Read(path.string(), error);
        Require(staleLoaded.has_value(), "reading must succeed: " + error);
        Require(staleLoaded->completedMissions.size() == 3,
                "the save file must round-trip exactly what it was given");
        IronGang::CampaignState filtered;
        filtered.SetCompleted(campaign, staleLoaded->completedMissions);
        Require(filtered.GetCompleted().size() == 1,
                "restoring must drop the unknown mission and the duplicate, not the file");

        // A save from before campaigns existed carries neither field and must not claim progress.
        WriteTempJson(path,
                      "format=iron-gang-save-v1\n"
                      "mission_state_id=drive_to_warehouse\n"
                      "player_position=1,1.7,2\n"
                      "player_yaw=0\n"
                      "vehicle_position=3,0.65,4\n"
                      "vehicle_yaw=0\n"
                      "vehicle_speed=0\n"
                      "player_driving=0\n");
        const std::optional<IronGang::SaveSnapshot> older = IronGang::SaveGame::Read(path.string(), error);
        Require(older.has_value(), "an older save must still load: " + error);
        Require(older->missionId.empty() && older->completedMissions.empty(),
                "an older save must claim no campaign progress rather than inventing some");
        Require(older->missionStateId == "drive_to_warehouse",
                "the rest of an older save must still restore");

        std::filesystem::remove(path);
    }

    // plan_24 IG-24-021: the second committed mission runs, is gated behind the prologue, and
    // fails on a wrecked sedan -- the failure fact that had nothing using it until now.
    void TestCountrysideMissionRunsAndFailsOnAWreck()
    {
        IronGang::PrototypeWorld countryside(IronGang::DistrictId::Countryside);
        IronGang::PrototypeMission mission;
        std::string error;
        const std::string missionPath =
            std::string(IRON_GANG_SOURCE_ASSET_DIR) + "/missions/countryside_run.mission.json";
        Require(mission.LoadMission(missionPath, error), "the countryside mission must load: " + error);
        mission.Reset();
        Require(mission.IsInState("briefing"), "it must start at its briefing");

        const IronGang::TriggerZone& goal = countryside.GetWarehouseGoal();
        Require(goal.id == "farmhouse_delivery",
                "the countryside must have a real delivery target for the mission to use");

        // Driving off starts the run; the district fact is what carries the player across.
        mission.Update(true, countryside.GetPlayerSpawn(), countryside.GetVehicleSpawn(), true, goal,
                       "warehouse_block");
        Require(mission.IsInState("drive_to_countryside"), "driving must start the run");
        IronGang::MissionValue cargo;
        Require(mission.TryGetVariable("cargo_loaded", cargo) && cargo.AsBool(),
                "the entry action must load the cargo");

        mission.Update(true, countryside.GetPlayerSpawn(), countryside.GetVehicleSpawn(), true, goal,
                       "countryside");
        Require(mission.IsInState("reach_farmhouse"),
                "arriving in the countryside must advance the mission");

        mission.Update(true, goal.bounds.center, goal.bounds.center, true, goal, "countryside");
        Require(mission.IsCompleted(), "reaching the farmhouse yard must complete the run");
        IronGang::MissionValue runs;
        Require(mission.TryGetVariable("runs_made", runs) && runs.AsInt() == 1, "the run must be counted");

        // The failure branch: a wrecked sedan ends the run, and a retry returns to the checkpoint.
        mission.Reset();
        mission.Update(true, countryside.GetPlayerSpawn(), countryside.GetVehicleSpawn(), true, goal,
                       "warehouse_block");
        Require(mission.IsInState("drive_to_countryside") && mission.HasCheckpoint(),
                "the driving state must be a checkpoint");
        Require(mission.SetFact("vehicle_disabled", IronGang::MissionValue::Bool(true), error),
                "publishing the wreck must succeed: " + error);
        mission.Update(true, countryside.GetPlayerSpawn(), countryside.GetVehicleSpawn(), true, goal,
                       "warehouse_block");
        Require(mission.IsFailed() && mission.IsInState("wrecked"),
                "a wrecked sedan must fail the run -- the branch the vehicle_disabled fact existed for");
        Require(mission.GetFailureReason() == "The sedan was wrecked before the cargo arrived",
                "the failure must explain itself");

        mission.Retry();
        Require(mission.IsInState("drive_to_countryside"), "a retry must return to the checkpoint");
        Require(mission.TryGetVariable("cargo_loaded", cargo) && cargo.AsBool(),
                "the retry must restore the checkpoint's variables");
    }

    // plan_28 IG-28-007: rebinding, whose whole difficulty is conflicts -- and whose whole
    // usefulness depends on not calling two keys a conflict when the game never listens for both
    // at once.
    void TestInputBindingsDetectConflictsWithinContexts()
    {
        using Keys = Microsoft::Xna::Framework::Input::Keys;
        IronGang::InputBindings bindings;

        Require(bindings.Get(IronGang::GameAction::MoveForward).primary == Keys::W,
                "the defaults must be the keys the game shipped with");
        Require(bindings.Get(IronGang::GameAction::MoveForward).secondary == Keys::Up,
                "an action may have two keys");
        Require(bindings.Matches(IronGang::GameAction::MoveForward, Keys::Up),
                "either binding must match");
        Require(!bindings.Matches(IronGang::GameAction::MoveForward, Keys::None),
                "an unbound key must match nothing");

        // Space is the handbrake while driving and confirms in a menu. Those are different
        // contexts, so this is **not** a conflict -- calling it one would force the player to
        // give up a perfectly good binding.
        Require(bindings.Get(IronGang::GameAction::Handbrake).primary == Keys::Space &&
                    bindings.Get(IronGang::GameAction::Confirm).secondary == Keys::Space,
                "the shipped bindings must actually share Space across two contexts");
        Require(!bindings.FindConflict(IronGang::GameAction::Handbrake, Keys::Enter).has_value(),
                "a vehicle action must not conflict with a menu action");

        // Within one context, it is a conflict.
        const std::optional<IronGang::GameAction> conflict =
            bindings.FindConflict(IronGang::GameAction::StrafeLeft, Keys::W);
        Require(conflict.has_value() && *conflict == IronGang::GameAction::MoveForward,
                "two on-foot actions must not share a key");
        Require(bindings.Get(IronGang::GameAction::MoveForward).primary == Keys::W,
                "asking about a conflict must not change anything");

        // A global action conflicts with everything, because it is read in every context.
        Require(bindings.FindConflict(IronGang::GameAction::Interact, Keys::Space).has_value(),
                "a global action must conflict with a vehicle binding");
        Require(bindings.FindConflict(IronGang::GameAction::Handbrake, Keys::E).has_value(),
                "a vehicle action must conflict with a global binding");

        // Rebinding displaces the loser, and reports it so the player can be told.
        const std::optional<IronGang::GameAction> displaced =
            bindings.Rebind(IronGang::GameAction::StrafeLeft, Keys::W);
        Require(displaced.has_value() && *displaced == IronGang::GameAction::MoveForward,
                "rebinding onto a taken key must report who lost it");
        Require(bindings.Get(IronGang::GameAction::StrafeLeft).primary == Keys::W,
                "the new binding must apply");
        Require(bindings.Get(IronGang::GameAction::MoveForward).primary == Keys::Up,
                "the displaced action must keep its other key, promoted to primary");
        Require(bindings.Get(IronGang::GameAction::MoveForward).secondary == Keys::None,
                "the displaced action must not keep a duplicate of its promoted key");
        Require(!bindings.Matches(IronGang::GameAction::MoveForward, Keys::W),
                "the displaced action must really have lost the key");

        // Displacing an action's only key leaves it unbound rather than silently keeping it.
        const std::optional<IronGang::GameAction> second =
            bindings.Rebind(IronGang::GameAction::StrafeRight, Keys::Up);
        Require(second.has_value() && *second == IronGang::GameAction::MoveForward,
                "the promoted key must itself be displaceable");
        Require(bindings.Get(IronGang::GameAction::MoveForward).primary == Keys::None,
                "an action can end up unbound, which is honest rather than surprising");

        // Rebinding without a conflict reports none.
        Require(!bindings.Rebind(IronGang::GameAction::MoveForward, Keys::I).has_value(),
                "rebinding to a free key must report no conflict");
        Require(bindings.Matches(IronGang::GameAction::MoveForward, Keys::I), "the free key must apply");

        bindings.ResetToDefaults();
        Require(bindings.Get(IronGang::GameAction::MoveForward).primary == Keys::W &&
                    bindings.Get(IronGang::GameAction::StrafeLeft).primary == Keys::A,
                "resetting must restore every shipped binding");

        // Identifiers and key names round-trip -- this is what the settings file stores, so a
        // gap here silently loses a player's bindings.
        for (std::size_t index = 0; index < static_cast<std::size_t>(IronGang::GameAction::Count); ++index)
        {
            const auto action = static_cast<IronGang::GameAction>(index);
            const std::string id = IronGang::GameActionId(action);
            Require(!id.empty(), "every action must have an identifier");
            IronGang::GameAction parsed{};
            Require(IronGang::ParseGameActionId(id, parsed) && parsed == action,
                    "every action identifier must parse back to itself: " + id);

            const IronGang::ActionBinding& binding = bindings.Get(action);
            for (const Keys key : {binding.primary, binding.secondary})
            {
                if (key == Keys::None)
                {
                    continue;
                }
                const std::string name = IronGang::KeyName(key);
                Require(!name.empty(),
                        "every shipped binding must have a storable key name (" + id + ")");
                Keys parsedKey{};
                Require(IronGang::ParseKeyName(name, parsedKey) && parsedKey == key,
                        "every key name must parse back to itself: " + name);
            }
        }
        IronGang::GameAction ignoredAction{};
        Keys ignoredKey{};
        Require(!IronGang::ParseGameActionId("fly", ignoredAction), "an unknown action id must be refused");
        Require(!IronGang::ParseKeyName("Joystick", ignoredKey), "an unknown key name must be refused");
        Require(IronGang::KeyName(Keys::None).empty(), "an unbound key must have no name to store");
    }

    // plan_29 IG-29-005 / plan_36 IG-36-005: player preferences are their own file with their own
    // lifetime, written atomically so a crash while saving one cannot cost the rest.
    void TestUserSettingsRoundTripAndFallBack()
    {
        const std::filesystem::path directory = std::filesystem::current_path() / "iron_gang_settings";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const std::filesystem::path path = directory / "settings.json";
        const IronGang::UserSettings defaults;
        IronGang::UserSettings settings;
        std::string error;
        std::vector<std::string> warnings;

        // No file at all is the normal state for a player who has never changed a setting: the
        // defaults apply, silently.
        Require(IronGang::LoadUserSettings(path.string(), settings, error, &warnings),
                "a missing settings file must not be an error");
        Require(warnings.empty(), "a missing settings file must not even warn -- it is the normal state");
        Require(std::fabs(settings.masterVolume - defaults.masterVolume) < 1e-6F && settings.showHud,
                "a missing file must leave the defaults");

        // Round trip.
        settings.masterVolume = 0.25F;
        settings.showHud = false;
        Require(IronGang::SaveUserSettings(path.string(), settings, error), "writing must succeed: " + error);
        Require(!std::filesystem::exists(IronGang::TemporaryFilePath(path.string())),
                "a completed write must leave no temporary file");
        IronGang::UserSettings loaded;
        Require(IronGang::LoadUserSettings(path.string(), loaded, error, &warnings),
                "reading must succeed: " + error);
        Require(std::fabs(loaded.masterVolume - 0.25F) < 1e-4F && !loaded.showHud,
                "both settings must survive the round trip");

        // A second write keeps the previous file as a backup, like the save does.
        settings.masterVolume = 1.0F;
        Require(IronGang::SaveUserSettings(path.string(), settings, error), "rewriting must succeed: " + error);
        Require(std::filesystem::exists(IronGang::BackupFilePath(path.string())),
                "the previous settings must be kept as a backup");

        // Out-of-range and wrong-typed values keep their defaults and say so.
        warnings.clear();
        WriteTempJson(path, R"JSON({"version":1,"masterVolume":5,"showHud":"yes","brightness":2})JSON");
        Require(IronGang::LoadUserSettings(path.string(), settings, error, &warnings),
                "bad values must not fail the load: " + error);
        Require(warnings.size() == 3, "each bad or unknown setting must be reported: " +
                                          std::to_string(warnings.size()));
        Require(std::fabs(settings.masterVolume - defaults.masterVolume) < 1e-6F && settings.showHud,
                "bad values must leave the defaults in place");

        // An unsupported version is a failure, and leaves the caller's settings untouched.
        IronGang::UserSettings untouched;
        untouched.masterVolume = 0.5F;
        WriteTempJson(path, R"JSON({"version":99})JSON");
        Require(!IronGang::LoadUserSettings(path.string(), untouched, error, &warnings),
                "an unsupported version must be refused");
        Require(std::fabs(untouched.masterVolume - 0.5F) < 1e-6F,
                "a failed load must leave the caller's settings alone");

        // Bindings round-trip through the settings file, which -- until a rebinding screen
        // exists -- is the only way a player can rebind at all.
        using Keys = Microsoft::Xna::Framework::Input::Keys;
        IronGang::UserSettings rebound;
        Require(!rebound.bindings.Rebind(IronGang::GameAction::Interact, Keys::F).has_value(),
                "F is unused, so rebinding onto it must report no conflict");
        Require(rebound.bindings.Get(IronGang::GameAction::Interact).primary == Keys::F,
                "the rebind must have applied before it is written out");
        rebound.bindings.Set(IronGang::GameAction::Sprint,
                             IronGang::ActionBinding{Keys::LeftControl, Keys::None});
        Require(IronGang::SaveUserSettings(path.string(), rebound, error),
                "writing rebound keys must succeed: " + error);
        warnings.clear();
        Require(IronGang::LoadUserSettings(path.string(), settings, error, &warnings),
                "reading rebound keys must succeed: " + error);
        Require(warnings.empty(), "a file the game just wrote must load without warnings");
        Require(settings.bindings.Get(IronGang::GameAction::Interact).primary == Keys::F,
                "a rebound key must survive the round trip");
        Require(settings.bindings.Get(IronGang::GameAction::Sprint).primary == Keys::LeftControl &&
                    settings.bindings.Get(IronGang::GameAction::Sprint).secondary == Keys::None,
                "an action bound to a single key must round-trip as a single key");
        Require(settings.bindings.Get(IronGang::GameAction::MoveForward).primary == Keys::W,
                "untouched actions must keep their shipped keys");

        // A binding the game cannot understand keeps the default and says so, per action, rather
        // than costing the whole file.
        warnings.clear();
        WriteTempJson(path,
                      R"JSON({"version":1,"bindings":{"move_forward":["Joystick"],"fly":["F"],
                              "interact":"E","sprint":["LeftControl"]}})JSON");
        Require(IronGang::LoadUserSettings(path.string(), settings, error, &warnings),
                "unusable bindings must not fail the load: " + error);
        Require(warnings.size() == 3, "each unusable binding must be reported: " +
                                          std::to_string(warnings.size()));
        Require(settings.bindings.Get(IronGang::GameAction::MoveForward).primary == Keys::W,
                "an unknown key name must leave the shipped binding");
        Require(settings.bindings.Get(IronGang::GameAction::Sprint).primary == Keys::LeftControl,
                "the bindings that were understood must still apply");

        // Settings inherit the bounded read every data file gets.
        {
            std::string deep = "{\"version\":";
            for (int level = 0; level < IronGang::kMaxJsonDataFileDepth + 5; ++level)
            {
                deep += "[";
            }
            for (int level = 0; level < IronGang::kMaxJsonDataFileDepth + 5; ++level)
            {
                deep += "]";
            }
            deep += "}";
            WriteTempJson(path, deep);
        }
        Require(!IronGang::LoadUserSettings(path.string(), settings, error, &warnings),
                "settings must inherit the JSON depth bound");

        // The shared atomic write: a failure leaves the existing file intact and no temporary
        // behind (the same guarantee the save file relies on).
        Require(IronGang::SaveUserSettings(path.string(), defaults, error), "restoring a good file: " + error);
        const std::string good = [&] {
            std::ifstream input(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        }();
        std::error_code permissionError;
        std::filesystem::permissions(directory, std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::remove, permissionError);
        if (!permissionError)
        {
            IronGang::UserSettings changed = defaults;
            changed.masterVolume = 0.0F;
            const bool wrote = IronGang::SaveUserSettings(path.string(), changed, error);
            std::filesystem::permissions(directory, std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::add, permissionError);
            if (!wrote)
            {
                std::ifstream input(path, std::ios::binary);
                const std::string after((std::istreambuf_iterator<char>(input)),
                                        std::istreambuf_iterator<char>());
                Require(after == good, "a failed settings write must leave the previous file intact");
                Require(!std::filesystem::exists(IronGang::TemporaryFilePath(path.string())),
                        "a failed settings write must not leave a temporary file");
            }
        }

        std::filesystem::remove_all(directory);
    }

    // plan_28 IG-28-003/004: menu navigation, where the easy-to-get-wrong parts are skipping
    // disabled entries, wrapping at both ends, and a menu with nothing selectable at all.
    void TestMenuModelSkipsDisabledAndWraps()
    {
        IronGang::MenuModel menu;
        Require(menu.GetSelected() == nullptr, "an empty menu has nothing selected");
        Require(menu.Activate() == IronGang::MenuAction::None, "an empty menu activates nothing");
        menu.MoveSelection(1); // must not crash or spin
        Require(menu.GetSelectedIndex() == 0, "an empty menu keeps its index");

        menu.SetItems({
            IronGang::MenuItem{IronGang::MenuAction::Resume, "Resume", true, {}},
            IronGang::MenuItem{IronGang::MenuAction::Save, "Save", true, {}},
            IronGang::MenuItem{IronGang::MenuAction::Load, "Load", false, "no save yet"},
            IronGang::MenuItem{IronGang::MenuAction::RestartMission, "Restart", false, "no checkpoint"},
            IronGang::MenuItem{IronGang::MenuAction::Quit, "Quit", true, {}},
        });
        Require(menu.GetSelectedIndex() == 0, "a new menu selects its first entry");
        Require(menu.Activate() == IronGang::MenuAction::Resume, "the first entry must activate");

        // Moving down skips both disabled entries in one step.
        menu.MoveSelection(1);
        Require(menu.GetSelectedIndex() == 1, "one step down lands on the next enabled entry");
        menu.MoveSelection(1);
        Require(menu.GetSelectedIndex() == 4,
                "the two disabled entries must be skipped, not stepped through");
        Require(menu.Activate() == IronGang::MenuAction::Quit, "the skipped-to entry must activate");

        // Wrapping, in both directions.
        menu.MoveSelection(1);
        Require(menu.GetSelectedIndex() == 0, "moving past the end must wrap to the start");
        menu.MoveSelection(-1);
        Require(menu.GetSelectedIndex() == 4, "moving before the start must wrap to the end");
        menu.MoveSelection(-1);
        Require(menu.GetSelectedIndex() == 1, "moving up must skip disabled entries too");

        // A multi-step move counts enabled entries, not raw indices.
        menu.MoveSelection(2);
        Require(menu.GetSelectedIndex() == 0,
                "two steps from the second enabled entry must pass Quit and wrap to Resume");
        menu.MoveSelection(0);
        Require(menu.GetSelectedIndex() == 0, "a zero move must change nothing");

        // A disabled entry never runs, even if something manages to select it.
        IronGang::MenuModel firstDisabled;
        firstDisabled.SetItems({
            IronGang::MenuItem{IronGang::MenuAction::Load, "Load", false, "no save yet"},
            IronGang::MenuItem{IronGang::MenuAction::Quit, "Quit", true, {}},
        });
        Require(firstDisabled.GetSelectedIndex() == 1,
                "a menu whose first entry is disabled must select the first enabled one instead");
        Require(firstDisabled.Activate() == IronGang::MenuAction::Quit,
                "the selected entry must be the enabled one");

        // Nothing selectable: the selection holds and activating does nothing, rather than the
        // search for an enabled entry spinning forever.
        IronGang::MenuModel allDisabled;
        allDisabled.SetItems({
            IronGang::MenuItem{IronGang::MenuAction::Save, "Save", false, "not now"},
            IronGang::MenuItem{IronGang::MenuAction::Load, "Load", false, "no save yet"},
        });
        const std::size_t before = allDisabled.GetSelectedIndex();
        allDisabled.MoveSelection(1);
        allDisabled.MoveSelection(-3);
        Require(allDisabled.GetSelectedIndex() == before,
                "a menu with nothing enabled must keep its selection where it is");
        Require(allDisabled.Activate() == IronGang::MenuAction::None,
                "a menu with nothing enabled must activate nothing");

        // Disabled entries keep their place and their reason -- hiding them would move every
        // entry below under the player's fingers between two frames.
        Require(menu.GetItems().size() == 5, "disabled entries must stay in the list");
        Require(menu.GetItems()[2].disabledReason == "no save yet",
                "a disabled entry must carry the reason it is disabled");
    }

    // plan_28 IG-28-008: one place decides what the game is listening to. Before this the answer
    // was spelled out again at every site that needed it, each free to disagree with the others.
    void TestInputContextResolvesByPrecedence()
    {
        IronGang::GameplaySignals signals;
        Require(IronGang::ResolveInputContext(signals) == IronGang::InputContext::OnFoot,
                "no signals at all means the player is on foot");

        signals.driving = true;
        Require(IronGang::ResolveInputContext(signals) == IronGang::InputContext::Driving,
                "driving must outrank being on foot");

        // Each signal in turn outranks the ones below it, and adding one never demotes the result.
        signals.vehicleTransitionActive = true;
        Require(IronGang::ResolveInputContext(signals) == IronGang::InputContext::VehicleTransition,
                "an enter/exit clip must outrank driving");
        signals.dialogueActive = true;
        Require(IronGang::ResolveInputContext(signals) == IronGang::InputContext::Dialogue,
                "dialogue must outrank the vehicle clip");
        signals.cutsceneActive = true;
        Require(IronGang::ResolveInputContext(signals) == IronGang::InputContext::Cutscene,
                "a cutscene must outrank dialogue");
        signals.districtTransitioning = true;
        Require(IronGang::ResolveInputContext(signals) == IronGang::InputContext::DistrictTransition,
                "a district load must outrank a cutscene -- the world it played in is being unloaded");
        signals.paused = true;
        Require(IronGang::ResolveInputContext(signals) == IronGang::InputContext::Paused,
                "pausing must outrank everything");

        // Only pausing stops the world. A cutscene, a conversation, and a district load all keep
        // the simulation running -- ambient traffic and the police do not wait for a conversation.
        Require(!IronGang::ContextAdvancesWorld(IronGang::InputContext::Paused),
                "a paused world must not advance");
        for (const IronGang::InputContext context :
             {IronGang::InputContext::DistrictTransition, IronGang::InputContext::Cutscene,
              IronGang::InputContext::Dialogue, IronGang::InputContext::VehicleTransition,
              IronGang::InputContext::Driving, IronGang::InputContext::OnFoot})
        {
            Require(IronGang::ContextAdvancesWorld(context),
                    std::string("the world must keep running during ") +
                        IronGang::InputContextName(context));
        }

        // Movement and interaction belong to the two contexts where the player is in control.
        for (const IronGang::InputContext context :
             {IronGang::InputContext::OnFoot, IronGang::InputContext::Driving})
        {
            Require(IronGang::ContextAllowsMovement(context) && IronGang::ContextAllowsInteraction(context),
                    std::string("the player must control the character while ") +
                        IronGang::InputContextName(context));
        }
        for (const IronGang::InputContext context :
             {IronGang::InputContext::Paused, IronGang::InputContext::DistrictTransition,
              IronGang::InputContext::Cutscene, IronGang::InputContext::Dialogue,
              IronGang::InputContext::VehicleTransition})
        {
            Require(!IronGang::ContextAllowsMovement(context) && !IronGang::ContextAllowsInteraction(context),
                    std::string("movement and interaction must be locked during ") +
                        IronGang::InputContextName(context));
        }

        // Every context has a name, and no two share one -- these end up in logs and reports.
        const IronGang::InputContext all[] = {
            IronGang::InputContext::Paused,        IronGang::InputContext::DistrictTransition,
            IronGang::InputContext::Cutscene,      IronGang::InputContext::Dialogue,
            IronGang::InputContext::VehicleTransition, IronGang::InputContext::Driving,
            IronGang::InputContext::OnFoot};
        for (std::size_t i = 0; i < std::size(all); ++i)
        {
            Require(std::strlen(IronGang::InputContextName(all[i])) > 0, "every context must be named");
            for (std::size_t j = i + 1; j < std::size(all); ++j)
            {
                Require(std::string(IronGang::InputContextName(all[i])) !=
                            IronGang::InputContextName(all[j]),
                        "two contexts must not share a name");
            }
        }
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

    // plan_22 IG-22-002: a witness has to be able to see. Until now anyone within a radius
    // reported everything, wall or no wall.
    void TestWitnessesCannotSeeThroughWalls()
    {
        using IronGang::Vector3;
        IronGang::WorldBox wall;
        wall.name = "warehouse";
        wall.center = {0.0F, 3.0F, 0.0F};
        wall.size = {10.0F, 6.0F, 10.0F};
        wall.collidable = true;

        // Straight through the middle, and every way of missing it.
        Require(IronGang::SegmentIntersectsBox({-20.0F, 3.0F, 0.0F}, {20.0F, 3.0F, 0.0F}, wall),
                "a segment through the box must intersect it");
        Require(!IronGang::SegmentIntersectsBox({-20.0F, 3.0F, 20.0F}, {20.0F, 3.0F, 20.0F}, wall),
                "a segment passing well to one side must not");
        Require(!IronGang::SegmentIntersectsBox({-20.0F, 30.0F, 0.0F}, {20.0F, 30.0F, 0.0F}, wall),
                "a segment passing over the top must not");
        Require(!IronGang::SegmentIntersectsBox({-20.0F, 3.0F, 0.0F}, {-12.0F, 3.0F, 0.0F}, wall),
                "a segment that stops short must not -- the box is beyond its end, not on it");
        Require(IronGang::SegmentIntersectsBox({0.0F, 3.0F, 0.0F}, {20.0F, 3.0F, 0.0F}, wall),
                "a segment starting inside the box must intersect it");
        Require(IronGang::SegmentIntersectsBox({0.0F, 3.0F, 0.0F}, {0.0F, 3.0F, 0.0F}, wall),
                "a degenerate segment inside the box must intersect it");
        Require(!IronGang::SegmentIntersectsBox({20.0F, 3.0F, 0.0F}, {20.0F, 3.0F, 0.0F}, wall),
                "a degenerate segment outside the box must not");
        // Parallel to a slab, which is the case the algorithm has to special-case.
        Require(!IronGang::SegmentIntersectsBox({-20.0F, 100.0F, 0.0F}, {20.0F, 100.0F, 0.0F}, wall),
                "a segment parallel to a slab and outside it must not intersect");

        // Line of sight across a world.
        std::vector<IronGang::WorldBox> boxes{wall};
        Require(!IronGang::HasLineOfSight({-20.0F, 1.5F, 0.0F}, {20.0F, 1.5F, 0.0F}, boxes),
                "a building between two points must block the view");
        Require(IronGang::HasLineOfSight({-20.0F, 1.5F, 20.0F}, {20.0F, 1.5F, 20.0F}, boxes),
                "open ground must not block the view");
        Require(IronGang::HasLineOfSight({-20.0F, 1.5F, 0.0F}, {20.0F, 1.5F, 0.0F}, {}),
                "an empty world blocks nothing");

        // Paint on the ground is not a wall: road markings and trigger decals are boxes too, and
        // treating them as occluders would blind every witness standing near a crossing.
        IronGang::WorldBox marking;
        marking.name = "stop_line";
        marking.center = {0.0F, 0.05F, 0.0F};
        marking.size = {5.0F, 0.08F, 0.5F};
        marking.collidable = false;
        Require(IronGang::HasLineOfSight({-20.0F, 0.05F, 0.0F}, {20.0F, 0.05F, 0.0F}, {marking}),
                "a non-collidable box must never block sight");

        // Against the real district: the warehouse stands between the sidewalk and the yard behind
        // it, while two points on the open road can see each other.
        IronGang::PrototypeWorld world;
        const std::vector<IronGang::WorldBox>& worldBoxes = world.GetBoxes();
        const IronGang::WorldBox* warehouse = nullptr;
        for (const IronGang::WorldBox& box : worldBoxes)
        {
            if (box.name == "warehouse")
            {
                warehouse = &box;
                break;
            }
        }
        Require(warehouse != nullptr, "the warehouse block must still contain a warehouse");
        const Vector3 nearSide = warehouse->center - Vector3(0.0F, 0.0F, warehouse->size.Z);
        const Vector3 farSide = warehouse->center + Vector3(0.0F, 0.0F, warehouse->size.Z);
        Require(!IronGang::HasLineOfSight({nearSide.X, 1.5F, nearSide.Z}, {farSide.X, 1.5F, farSide.Z},
                                          worldBoxes),
                "the warehouse must block sight straight through it");
        Require(IronGang::HasLineOfSight({0.0F, 1.5F, 20.0F}, {0.0F, 1.5F, 30.0F}, worldBoxes),
                "two points on the open road must see each other");
    }

    // plan_22 IG-22-001/011: running a red is an offence now that lights exist, and the player is
    // told which offence they are being chased for.
    void TestRunningARedLightIsAWitnessedOffence()
    {
        const IronGang::Vector3 line{3.0F, 0.4F, 8.0F};
        constexpr float kApproachYaw = 0.0F; // ForwardFromYaw(0) points down -Z
        constexpr float kLane = IronGang::kTrafficLaneHalfWidth;

        // The crossing test is a segment, not a position: at 20 m/s a 60 Hz frame covers a third
        // of a metre, so a car is behind the line one frame and well past it the next.
        Require(IronGang::CrossedLine({3.0F, 0.4F, 12.0F}, {3.0F, 0.4F, 4.0F}, line, kApproachYaw, kLane),
                "passing the line between two frames must count as crossing it");
        Require(!IronGang::CrossedLine({3.0F, 0.4F, 12.0F}, {3.0F, 0.4F, 9.0F}, line, kApproachYaw, kLane),
                "stopping short of the line must not count");
        Require(!IronGang::CrossedLine({3.0F, 0.4F, 4.0F}, {3.0F, 0.4F, 0.0F}, line, kApproachYaw, kLane),
                "driving on beyond the line must not count a second time");
        Require(!IronGang::CrossedLine({3.0F, 0.4F, 4.0F}, {3.0F, 0.4F, 12.0F}, line, kApproachYaw, kLane),
                "reversing back over the line must not count as running it");
        Require(!IronGang::CrossedLine({9.0F, 0.4F, 12.0F}, {9.0F, 0.4F, 4.0F}, line, kApproachYaw, kLane),
                "crossing the same plane on the pavement beside the lane must not count");
        Require(IronGang::CrossedLine({3.0F, 0.4F, 8.001F}, {3.0F, 0.4F, 7.999F}, line, kApproachYaw, kLane),
                "even a crossing that barely happens must count");

        // The police only react to what a witness sees.
        const IronGang::Vector3 crossing{3.0F, 0.4F, 8.0F};
        const IronGang::Vector3 spawn{20.0F, 0.0F, 0.0F};
        IronGang::PoliceObservation ranRed;
        ranRed.driving = true;
        ranRed.vehicleSpeedKph = 30.0F; // well under the speeding threshold
        ranRed.ranRedLight = true;

        IronGang::PoliceSystem unseen;
        unseen.Update(1.0F, ranRed, crossing, {}, spawn);
        Require(unseen.GetState() == IronGang::PoliceState::Clear,
                "running a red with nobody watching must not be noticed");
        Require(unseen.GetOffence() == IronGang::PoliceOffence::None,
                "an unwitnessed offence must leave no record");

        IronGang::PoliceSystem farAway;
        farAway.Update(1.0F, ranRed, crossing, {IronGang::Vector3(500.0F, 0.0F, 0.0F)}, spawn);
        Require(farAway.GetState() == IronGang::PoliceState::Clear,
                "a witness outside the radius must not see it either");

        IronGang::PoliceSystem seen;
        seen.Update(1.0F, ranRed, crossing, {crossing + IronGang::Vector3(6.0F, 0.0F, 0.0F)}, spawn);
        Require(seen.GetState() == IronGang::PoliceState::Dispatched,
                "running a red in front of a witness must dispatch a patrol");
        Require(seen.GetOffence() == IronGang::PoliceOffence::RanRedLight,
                "the recorded offence must be the one committed");
        Require(std::strlen(IronGang::PoliceOffenceName(seen.GetOffence())) > 0,
                "the offence must have player-facing text -- WANTED with no reason is the complaint "
                "every game like this gets");

        // Not driving is not an offence, however red the light.
        IronGang::PoliceObservation onFoot = ranRed;
        onFoot.driving = false;
        IronGang::PoliceSystem walking;
        walking.Update(1.0F, onFoot, crossing, {crossing}, spawn);
        Require(walking.GetState() == IronGang::PoliceState::Clear,
                "a pedestrian crossing against a light is not what this system is for");

        // Worse offences win the label: hitting someone while also speeding reports the collision.
        IronGang::PoliceObservation everything;
        everything.driving = true;
        everything.vehicleSpeedKph = 120.0F;
        everything.ranRedLight = true;
        IronGang::PoliceSystem worst;
        worst.Update(1.0F, everything, crossing, {crossing + IronGang::Vector3(1.0F, 0.0F, 0.0F)}, spawn);
        Require(worst.GetOffence() == IronGang::PoliceOffence::Collision,
                "the reported reason must be the worst thing the player did, not the first checked");

        // Speeding alone still reports speeding, and the record clears when the chase resolves.
        IronGang::PoliceObservation speeding;
        speeding.driving = true;
        speeding.vehicleSpeedKph = 120.0F;
        IronGang::PoliceSystem chase;
        chase.Update(1.0F, speeding, crossing, {crossing + IronGang::Vector3(6.0F, 0.0F, 0.0F)}, spawn);
        Require(chase.GetOffence() == IronGang::PoliceOffence::Speeding, "speeding must report speeding");
        IronGang::PoliceObservation quiet;
        quiet.driving = true;
        const IronGang::Vector3 escaped{5000.0F, 0.0F, 0.0F};
        for (int step = 0; step < 40; ++step)
        {
            chase.Update(1.0F, quiet, escaped, {}, spawn);
        }
        Require(chase.GetState() == IronGang::PoliceState::Clear, "escaping must end the chase");
        Require(chase.GetOffence() == IronGang::PoliceOffence::None,
                "a resolved chase must forget what it was for");
    }

    // plan_21 IG-21-003/007: the light's own timing, and the rule that decides who stops.
    void TestTrafficSignalCyclesAndOpposesItself()
    {
        IronGang::TrafficSignal signal;
        const IronGang::TrafficSignalTiming timing = signal.GetTiming();
        Require(timing.CycleSeconds() > 0.0F, "a signal must have a cycle");
        Require(signal.GetPhase() == IronGang::SignalPhase::Green, "a signal starts on green");

        // The phases follow in order and last exactly as long as their timings say.
        signal.Update(timing.greenSeconds - 0.01F);
        Require(signal.GetPhase() == IronGang::SignalPhase::Green, "green must last its full time");
        signal.Update(0.02F);
        Require(signal.GetPhase() == IronGang::SignalPhase::Amber, "green must be followed by amber");
        signal.Update(timing.amberSeconds);
        Require(signal.GetPhase() == IronGang::SignalPhase::Red, "amber must be followed by red");
        signal.Update(timing.redSeconds);
        Require(signal.GetPhase() == IronGang::SignalPhase::Green, "the cycle must wrap to green");

        // **The invariant that matters**: the two directions of a crossing are never both moving.
        IronGang::TrafficSignal crossing;
        crossing.Reset();
        for (int step = 0; step < 2000; ++step)
        {
            crossing.Update(timing.CycleSeconds() / 97.0F); // a step that does not divide the cycle
            const bool thisMoves = !IronGang::TrafficSignal::RequiresStop(crossing.GetPhase());
            const bool otherMoves = !IronGang::TrafficSignal::RequiresStop(crossing.GetOpposingPhase());
            Require(!(thisMoves && otherMoves),
                    std::string("both directions showed a moving phase at once: ") +
                        IronGang::SignalPhaseName(crossing.GetPhase()) + "/" +
                        IronGang::SignalPhaseName(crossing.GetOpposingPhase()));
        }

        // Both directions do get to move over a full cycle -- an intersection that never lets
        // anyone through satisfies the invariant above and is still broken.
        IronGang::TrafficSignal fairness;
        bool sawThisGreen = false;
        bool sawOpposingGreen = false;
        for (int step = 0; step < 400; ++step)
        {
            fairness.Update(timing.CycleSeconds() / 200.0F);
            sawThisGreen = sawThisGreen || fairness.GetPhase() == IronGang::SignalPhase::Green;
            sawOpposingGreen =
                sawOpposingGreen || fairness.GetOpposingPhase() == IronGang::SignalPhase::Green;
        }
        Require(sawThisGreen && sawOpposingGreen, "both directions must get a green within a cycle");

        // Offsets start a light partway through, and wrap rather than running off the end.
        IronGang::TrafficSignal offset;
        offset.Reset(timing.greenSeconds + timing.amberSeconds + 0.5F);
        Require(offset.GetPhase() == IronGang::SignalPhase::Red, "an offset must place the phase");
        offset.Reset(timing.CycleSeconds() * 3.5F);
        Require(offset.GetSecondsIntoCycle() < timing.CycleSeconds(),
                "an offset past the cycle must wrap into it");
        offset.Reset(-5.0F);
        Require(offset.GetSecondsIntoCycle() >= 0.0F, "a negative offset must not run the light backwards");

        // Amber stops traffic: at this scale, deciding whether a car "can make it" is a rule
        // nobody would notice and one that leaves cars in the crossing when the phase flips.
        Require(IronGang::TrafficSignal::RequiresStop(IronGang::SignalPhase::Red) &&
                    IronGang::TrafficSignal::RequiresStop(IronGang::SignalPhase::Amber) &&
                    !IronGang::TrafficSignal::RequiresStop(IronGang::SignalPhase::Green),
                "only green may let traffic through");

        // Bad timings and bad deltas are ignored rather than producing a light that skips a colour.
        IronGang::TrafficSignal tuned;
        IronGang::TrafficSignalTiming broken;
        broken.amberSeconds = 0.0F;
        tuned.Configure(broken);
        Require(tuned.GetTiming().amberSeconds > 0.0F, "a zero-length phase must be refused");
        const float before = tuned.GetSecondsIntoCycle();
        tuned.Update(-1.0F);
        tuned.Update(std::numeric_limits<float>::quiet_NaN());
        Require(std::fabs(tuned.GetSecondsIntoCycle() - before) < 1e-6F,
                "a negative or NaN delta must not move the light");

        // The world places a stop line per direction, and the two read opposing phases.
        IronGang::PrototypeWorld block;
        const std::vector<IronGang::TrafficStopLine>& stopLines = block.GetTrafficStopLines();
        Require(stopLines.size() == 2, "the warehouse block must have a signalled crossing");
        Require(stopLines[0].opposingPhase != stopLines[1].opposingPhase,
                "the crossing's two stop lines must read opposing phases");
        Require(IronGang::DistanceAheadInLane(IronGang::Vector3{3.0F, 0.4F, 30.0F}, stopLines[0].approachYaw,
                                              stopLines[0].position, IronGang::kTrafficLaneHalfWidth) <
                    IronGang::kNoObstacleAhead,
                "a vehicle approaching in the governed lane must see its stop line ahead");
        Require(IronGang::DistanceAheadInLane(IronGang::Vector3{3.0F, 0.4F, -30.0F}, stopLines[0].approachYaw,
                                              stopLines[0].position, IronGang::kTrafficLaneHalfWidth) ==
                    IronGang::kNoObstacleAhead,
                "a vehicle that has passed the line must not still see it");

        // A vehicle told the stop line is close brakes to a halt, and moves again once told the
        // way is clear -- the same braking that already existed, which is the point of modelling a
        // red light as an obstacle.
        IronGang::TrafficVehicle vehicle;
        IronGang::WaypointPath lane{{{3.0F, 0.4F, 38.0F}, {3.0F, 0.4F, -38.0F}}, true};
        vehicle.Reset(lane, 0, 6.0F);
        for (int frame = 0; frame < 240; ++frame)
        {
            vehicle.Update(1.0F / 60.0F, 0.4F); // something stopped just ahead
        }
        Require(vehicle.GetForwardSpeed() < 0.05F, "a vehicle must stop for a red light: " +
                                                std::to_string(vehicle.GetForwardSpeed()));
        for (int frame = 0; frame < 120; ++frame)
        {
            vehicle.Update(1.0F / 60.0F, IronGang::kNoObstacleAhead);
        }
        Require(vehicle.GetForwardSpeed() > 1.0F, "and must pull away once the light turns green");
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
        Require(northbound.IsWalking() && southbound.IsWalking(),
                "a pedestrian with a clear lane must report itself walking");
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
        Require(!follower.IsWalking(),
                "a pedestrian stopped behind another must report itself standing -- that is what "
                "picks an idle pose instead of sliding a walk cycle along");
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
        Require(follower.IsWalking(), "and must report itself walking again");

        // Fleeing ignores congestion on purpose: someone running from a car does not queue.
        IronGang::Pedestrian fleeing;
        fleeing.Reset(sidewalk, 0, 1.4F, 10.0F);
        const IronGang::Vector3 threat = fleeing.GetPosition() + IronGang::Vector3(0.0F, 0.0F, -2.0F);
        const IronGang::Vector3 beforeFlee = fleeing.GetPosition();
        fleeing.Update(0.1F, true, threat, 0.0F); // zero clearance: blocked in every sense
        Require(fleeing.IsFleeing(), "a threat must start the flee state");
        Require((fleeing.GetPosition() - beforeFlee).Length() > 0.01F,
                "a fleeing pedestrian must keep moving even with no clearance ahead");
        Require(fleeing.IsWalking(), "a fleeing pedestrian is moving, whatever is ahead of it");
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

        // Every unsafe moment blocks, and the reported one follows the input context's own
        // precedence -- one place decides what the game is doing, and saving asks it.
        Require(IronGang::SaveBlockReasonForContext(IronGang::InputContext::OnFoot) ==
                    IronGang::SaveBlockReason::None,
                "ordinary play must not block saving");
        Require(IronGang::SaveBlockReasonForContext(IronGang::InputContext::Driving) ==
                    IronGang::SaveBlockReason::None,
                "driving must not block saving");
        Require(IronGang::SaveBlockReasonForContext(IronGang::InputContext::Paused) ==
                    IronGang::SaveBlockReason::None,
                "pausing must not block saving -- the world is frozen and consistent");
        Require(IronGang::SaveBlockReasonForContext(IronGang::InputContext::VehicleTransition) ==
                    IronGang::SaveBlockReason::VehicleTransition,
                "entering or leaving the car must block saving");
        Require(IronGang::SaveBlockReasonForContext(IronGang::InputContext::DistrictTransition) ==
                    IronGang::SaveBlockReason::DistrictTransition,
                "a district load must block saving");
        Require(IronGang::SaveBlockReasonForContext(IronGang::InputContext::Dialogue) ==
                    IronGang::SaveBlockReason::Dialogue,
                "dialogue must block saving");
        Require(IronGang::SaveBlockReasonForContext(IronGang::InputContext::Cutscene) ==
                    IronGang::SaveBlockReason::Cutscene,
                "a cutscene must block saving");
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

    // plan_25 IG-25-001 / plan_34 IG-34-015: dialogue is versioned data whose every line carries a
    // stable id. plan.md's locked decision 10 asks for those ids "from day one"; the prototype's
    // `speaker|text` format had none, which made the decision quietly untrue for as long as it
    // shipped.
    void TestDialogueLinesCarryStableIds()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_dialogue.json";
        IronGang::DialogueSystem dialogue;
        std::string error;

        // The committed conversation loads, and its ids are what other content would reference.
        Require(dialogue.LoadFromFile(std::string(IRON_GANG_SOURCE_ASSET_DIR) +
                                          "/dialogues/prologue.dialogue.json",
                                      error),
                "the committed dialogue must load: " + error);
        Require(dialogue.GetLineCount() == 3, "the prologue must still be three lines");
        Require(dialogue.GetConversationId() == "prologue", "the conversation must be identified");
        const IronGang::DialogueLine* opening = dialogue.FindLine("prologue.mara.quiet_tonight");
        Require(opening != nullptr, "a committed line must be findable by its id");
        Require(opening->speaker == "Mara" && !opening->text.empty(),
                "the found line must carry its speaker and text");
        Require(dialogue.FindLine("prologue.mara.a_line_we_cut") == nullptr,
                "an id that does not exist must return nothing -- that is how a stale reference is "
                "caught rather than silently showing the first line");

        // Ids are independent of position: playing through does not change what an id resolves to.
        dialogue.Start();
        dialogue.Advance();
        Require(dialogue.FindLine("prologue.mara.quiet_tonight") == opening,
                "a line id must resolve the same way whatever the conversation is doing");

        // The built-in fallback uses the same ids, so a fallback line and its shipped counterpart
        // are the same line to anything referencing -- or translating -- it.
        IronGang::DialogueSystem fallback;
        fallback.LoadFallbackPrologue();
        Require(fallback.GetLineCount() == dialogue.GetLineCount(),
                "the fallback must match the shipped conversation's shape");
        for (std::size_t index = 0; index < fallback.GetLineCount(); ++index)
        {
            fallback.Start();
            Require(fallback.FindLine("prologue.mara.no_heroics") != nullptr,
                    "the fallback must carry the same ids as the file it stands in for");
        }

        // Malformed content is refused, and the previously loaded conversation survives -- half a
        // conversation is worse than the fallback.
        const auto rejects = [&](const std::string& json, const std::string& why) {
            WriteTempJson(path, json);
            IronGang::DialogueSystem target;
            Require(target.LoadFromFile(std::string(IRON_GANG_SOURCE_ASSET_DIR) +
                                            "/dialogues/prologue.dialogue.json",
                                        error),
                    "the good conversation must load first: " + error);
            Require(!target.LoadFromFile(path.string(), error), why);
            Require(target.GetLineCount() == 3, why + " (leaving the previous conversation intact)");
        };
        rejects(R"JSON({"version":99,"lines":[{"id":"a","speaker":"A","text":"t"}]})JSON",
                "an unsupported version must be refused");
        rejects(R"JSON({"version":1})JSON", "a file with no lines array must be refused");
        rejects(R"JSON({"version":1,"lines":[]})JSON", "a conversation with no lines must be refused");
        rejects(R"JSON({"version":1,"lines":[{"speaker":"A","text":"t"}]})JSON",
                "a line with no id must be refused -- that is the decision this format exists for");
        rejects(R"JSON({"version":1,"lines":[{"id":"a","text":"t"}]})JSON",
                "a line with no speaker must be refused");
        rejects(R"JSON({"version":1,"lines":[{"id":"a","speaker":"A","text":""}]})JSON",
                "a line with no text must be refused");
        rejects(R"JSON({"version":1,"lines":[{"id":"a","speaker":"A","text":"one"},
                        {"id":"a","speaker":"B","text":"two"}]})JSON",
                "a duplicate line id must be refused -- every reference to it would be ambiguous, "
                "and one translation would silently become both");
        rejects(R"JSON({"version":1,"lines":[{"id":"a","speaker":"A","text":"t","tone":"angry"}]})JSON",
                "an unknown field must be refused rather than silently dropped");
        rejects(R"JSON({"version":1,"lines":[{"id":7,"speaker":"A","text":"t"}]})JSON",
                "a non-string field must be refused");
        rejects(R"JSON(["not","an","object"])JSON", "a non-object root must be refused");

        std::filesystem::remove(path);
    }

    // plan_34 IG-34-007: property tests for the save round trip. Hand-written cases check the
    // situations someone thought of; this checks a few hundred nobody did -- odd characters in
    // variable names and values, floats that print badly, empty collections, long lists.
    void TestSaveRoundTripsRandomSnapshots()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_property.save";
        IronGang::RandomSource random(20260826);
        std::string error;

        // Characters chosen to poke at the line format: its separators, its quoting, and bytes
        // outside ASCII. A newline is deliberately absent -- the format documents one value per
        // line, and no value the game can produce contains one.
        const std::vector<std::string> textFragments = {
            "plain", "with space", "equals=sign", "colon:sign", "semi;colon", "quote\"mark",
            "dot.separated", "dash-separated", "under_score", "UPPER", "kri\xC5\xBE""ovatka",
            "emoji\xF0\x9F\x9A\x97", "trailing ", " leading", "0", "-1", "1e9",
        };
        const auto pickText = [&]() { return textFragments[random.NextIndex(
            static_cast<std::uint32_t>(textFragments.size()))]; };

        for (int iteration = 0; iteration < 250; ++iteration)
        {
            IronGang::SaveSnapshot written;
            written.missionId = random.NextBool() ? pickText() : std::string();
            written.missionStateId = pickText();
            written.playerPosition = {random.NextFloatInRange(-500.0F, 500.0F),
                                      random.NextFloatInRange(-10.0F, 10.0F),
                                      random.NextFloatInRange(-500.0F, 500.0F)};
            written.playerYaw = random.NextFloatInRange(-3.15F, 3.15F);
            written.vehiclePosition = {random.NextFloatInRange(-500.0F, 500.0F),
                                       random.NextFloatInRange(-10.0F, 10.0F),
                                       random.NextFloatInRange(-500.0F, 500.0F)};
            written.vehicleYaw = random.NextFloatInRange(-3.15F, 3.15F);
            written.vehicleSpeed = random.NextFloatInRange(-30.0F, 30.0F);
            written.vehicleIntegrity = random.NextUnitFloat();
            written.playerDriving = random.NextBool();
            written.districtId = random.NextBool() ? IronGang::DistrictId::WarehouseBlock
                                                   : IronGang::DistrictId::Countryside;

            const std::uint32_t variableCount = random.NextIndex(6);
            for (std::uint32_t index = 0; index < variableCount; ++index)
            {
                IronGang::MissionValue value;
                switch (random.NextIndex(4))
                {
                    case 0: value = IronGang::MissionValue::Bool(random.NextBool()); break;
                    case 1: value = IronGang::MissionValue::Int(
                                static_cast<int>(random.NextIndex(2000)) - 1000); break;
                    case 2: value = IronGang::MissionValue::Float(
                                random.NextFloatInRange(-1e6F, 1e6F)); break;
                    default: value = IronGang::MissionValue::String(pickText()); break;
                }
                written.missionVariables.push_back(
                    IronGang::MissionVariableSnapshot{"var_" + std::to_string(index), value});
            }

            const std::uint32_t completedCount = random.NextIndex(4);
            for (std::uint32_t index = 0; index < completedCount; ++index)
            {
                written.completedMissions.push_back("mission_" + std::to_string(index));
            }

            if (random.NextBool())
            {
                written.missionCheckpoint.stateId = pickText();
                written.missionCheckpoint.variables = written.missionVariables;
            }
            if (random.NextBool())
            {
                IronGang::WorldStateSnapshot checkpointWorld;
                checkpointWorld.playerPosition = {random.NextFloatInRange(-100.0F, 100.0F), 1.7F,
                                                  random.NextFloatInRange(-100.0F, 100.0F)};
                checkpointWorld.playerYaw = random.NextFloatInRange(-3.15F, 3.15F);
                checkpointWorld.vehiclePosition = {random.NextFloatInRange(-100.0F, 100.0F), 0.65F,
                                                  random.NextFloatInRange(-100.0F, 100.0F)};
                checkpointWorld.vehicleYaw = random.NextFloatInRange(-3.15F, 3.15F);
                checkpointWorld.vehicleSpeed = random.NextFloatInRange(-20.0F, 20.0F);
                checkpointWorld.playerDriving = random.NextBool();
                written.missionCheckpointWorld = checkpointWorld;
            }

            Require(IronGang::SaveGame::Write(path.string(), written, error),
                    "writing iteration " + std::to_string(iteration) + " must succeed: " + error);
            const std::optional<IronGang::SaveSnapshot> read =
                IronGang::SaveGame::Read(path.string(), error);
            Require(read.has_value(),
                    "reading iteration " + std::to_string(iteration) + " must succeed: " + error);

            const std::string where = " (iteration " + std::to_string(iteration) + ")";
            Require(read->missionId == written.missionId, "mission id must round-trip" + where);
            Require(read->missionStateId == written.missionStateId,
                    "mission state id must round-trip" + where);
            Require(read->playerDriving == written.playerDriving, "driving must round-trip" + where);
            Require(read->districtId == written.districtId, "district must round-trip" + where);
            Require(read->completedMissions == written.completedMissions,
                    "campaign progress must round-trip" + where);
            Require(read->missionCheckpoint.stateId == written.missionCheckpoint.stateId,
                    "checkpoint state must round-trip" + where);
            Require(read->missionCheckpointWorld.has_value() ==
                        written.missionCheckpointWorld.has_value(),
                    "the checkpoint world's presence must round-trip" + where);

            // Positions and angles survive to the precision the format promises.
            Require((read->playerPosition - written.playerPosition).Length() < 1e-2F,
                    "player position must round-trip" + where);
            Require((read->vehiclePosition - written.vehiclePosition).Length() < 1e-2F,
                    "vehicle position must round-trip" + where);
            Require(std::fabs(read->playerYaw - written.playerYaw) < 1e-4F,
                    "player yaw must round-trip" + where);
            Require(std::fabs(read->vehicleSpeed - written.vehicleSpeed) < 1e-3F,
                    "vehicle speed must round-trip" + where);
            Require(std::fabs(read->vehicleIntegrity - written.vehicleIntegrity) < 1e-4F,
                    "vehicle integrity must round-trip" + where);

            // Mission variables must survive **exactly**: they are typed data, not display values,
            // and a float that comes back nearly right is a mission condition that fires nearly
            // when it should.
            Require(read->missionVariables.size() == written.missionVariables.size(),
                    "every mission variable must survive" + where);
            for (std::size_t index = 0; index < written.missionVariables.size(); ++index)
            {
                const IronGang::MissionVariableSnapshot& expected = written.missionVariables[index];
                const IronGang::MissionVariableSnapshot& actual = read->missionVariables[index];
                Require(actual.name == expected.name, "variable names must keep their order" + where);
                Require(actual.value.GetType() == expected.value.GetType(),
                        "variable types must round-trip" + where);
                // Compared as **values**, not as their own text: comparing ToText() to ToText()
                // is self-referential, and passes happily even if the formatting loses precision
                // on both sides equally. (It did: a deliberate mutation to two-decimal float
                // output slipped through the text comparison and is caught by this one.)
                switch (expected.value.GetType())
                {
                    case IronGang::MissionValueType::Bool:
                        Require(actual.value.AsBool() == expected.value.AsBool(),
                                "bool \"" + expected.name + "\" must round-trip" + where);
                        break;
                    case IronGang::MissionValueType::Int:
                        Require(actual.value.AsInt() == expected.value.AsInt(),
                                "int \"" + expected.name + "\" must round-trip" + where);
                        break;
                    case IronGang::MissionValueType::Float:
                        Require(actual.value.AsFloat() == expected.value.AsFloat(),
                                "float \"" + expected.name + "\" must round-trip bit for bit" + where +
                                    ": wrote " + std::to_string(expected.value.AsFloat()) + ", read " +
                                    std::to_string(actual.value.AsFloat()));
                        break;
                    case IronGang::MissionValueType::String:
                        Require(actual.value.AsString() == expected.value.AsString(),
                                "string \"" + expected.name + "\" must round-trip exactly" + where +
                                    ": wrote [" + expected.value.AsString() + "], read [" +
                                    actual.value.AsString() + "]");
                        break;
                }
            }
        }

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

    // plan_26 IG-26-010: the dialogue track picks the subtitle by stable id as the timeline
    // passes each cue, and a skip lands on the same cue a full play-through would.
    // plan_30 IG-30-013: the half of screenshot capture that can be tested without a graphics
    // device -- deciding, from pixels alone, whether a frame looks like a rendered scene.
    // plan_30 IG-30-012: a QA repro case must play back exactly what was recorded, and refuse to
    // load anything that would silently play back something else.
    void TestInputScriptRecordsSparselyAndReplaysExactly()
    {
        const auto held = [](std::initializer_list<IronGang::GameAction> actions) {
            IronGang::HeldActions state{};
            for (IronGang::GameAction action : actions)
            {
                state[static_cast<std::size_t>(action)] = true;
            }
            return state;
        };

        IronGang::InputScriptRecorder recorder("round_trip");
        recorder.Record(IronGang::HeldActions{});                              // update 0
        recorder.Record(IronGang::HeldActions{});                              // update 1
        recorder.Record(IronGang::HeldActions{});                              // update 2
        recorder.Record(held({IronGang::GameAction::Confirm}));                // update 3
        recorder.Record(held({IronGang::GameAction::Confirm}));                // update 4
        recorder.Record(held({IronGang::GameAction::Confirm,
                              IronGang::GameAction::MoveForward}));            // update 5
        recorder.Record(IronGang::HeldActions{});                              // update 6

        Require(recorder.GetUpdateCount() == 7, "every update must be counted");
        // Sparse: four changes, not seven updates. Two identical updates in a row cost nothing.
        Require(recorder.GetSteps().size() == 4,
                "only changes may be recorded, got " + std::to_string(recorder.GetSteps().size()));
        Require(recorder.GetSteps()[1].update == 3, "the first Confirm must be recorded at update 3");

        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_script.json";
        std::string error;
        Require(recorder.Save(path.string(), error), "the script must be written: " + error);

        IronGang::InputScript script;
        Require(script.LoadFromFile(path.string(), error), "the script must load back: " + error);
        Require(script.GetId() == "round_trip", "the id must round-trip");
        Require(script.GetStepCount() == 4, "the step count must round-trip");
        Require(script.GetLastUpdate() == 6, "the last update must round-trip");

        // Replay the whole thing and compare against what was fed in, update by update.
        const IronGang::HeldActions expected[7] = {
            {}, {}, {},
            held({IronGang::GameAction::Confirm}),
            held({IronGang::GameAction::Confirm}),
            held({IronGang::GameAction::Confirm, IronGang::GameAction::MoveForward}),
            {},
        };
        script.Rewind();
        for (int update = 0; update < 7; ++update)
        {
            script.Advance();
            Require(script.GetUpdateIndex() == update, "playback must step one update at a time");
            for (std::size_t action = 0; action < IronGang::kGameActionCount; ++action)
            {
                Require(script.IsDown(static_cast<IronGang::GameAction>(action)) == expected[update][action],
                        "update " + std::to_string(update) + " must replay exactly what was recorded");
            }
        }

        // Edge-triggered, like the game's own WasPressed: held for three updates, pressed once.
        script.Rewind();
        int confirmPresses = 0;
        for (int update = 0; update < 7; ++update)
        {
            script.Advance();
            if (script.WasPressed(IronGang::GameAction::Confirm))
            {
                ++confirmPresses;
                Require(update == 3, "Confirm must read as pressed on the update it goes down");
            }
        }
        Require(confirmPresses == 1,
                "an action held across updates must press exactly once, got " + std::to_string(confirmPresses));

        Require(!script.IsFinished(), "sanity: update 6 is the last step, not past it");
        script.Advance();
        Require(script.IsFinished(), "playback must report finishing once it passes the last step");

        // An empty recorder writes nothing rather than a file that plays back nothing.
        const IronGang::InputScriptRecorder empty("empty");
        Require(!empty.Save(path.string(), error), "an empty recording must be refused");

        std::filesystem::remove(path);
    }

    void TestInputScriptRejectsUnusableRepros()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_bad_script.json";
        std::string error;
        IronGang::InputScript script;

        WriteTempJson(path, R"JSON({"id":"a","version":99,"steps":[{"update":0,"held":[]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "an unsupported version must be rejected");

        WriteTempJson(path, R"JSON({"id":"a","version":1,"steps":[]})JSON");
        Require(!script.LoadFromFile(path.string(), error),
                "a script with no steps would play back nothing and must be rejected");

        WriteTempJson(path, R"JSON({"id":"","version":1,"steps":[{"update":0,"held":[]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "an empty id must be rejected");

        WriteTempJson(path, R"JSON({"id":"a","version":1,"steps":[
            {"update":5,"held":[]},{"update":2,"held":[]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error),
                "steps out of ascending update order must be rejected");

        WriteTempJson(path, R"JSON({"id":"a","version":1,"steps":[
            {"update":2,"held":[]},{"update":2,"held":[]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "duplicate update indices must be rejected");

        WriteTempJson(path, R"JSON({"id":"a","version":1,"steps":[{"update":-1,"held":[]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "a negative update index must be rejected");

        // The reason a script names actions rather than keys: a renamed action is caught here.
        WriteTempJson(path, R"JSON({"id":"a","version":1,"steps":[{"update":0,"held":["fly"]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "an unknown action id must be rejected");
        Require(error.find("fly") != std::string::npos,
                "the error must name the unknown action: " + error);

        WriteTempJson(path, R"JSON({"id":"a","version":1,"steps":[{"update":0}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "a step with no \"held\" must be rejected");

        WriteTempJson(path, R"JSON({"id":"a","version":1,"steps":[{"held":[]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "a step with no \"update\" must be rejected");

        WriteTempJson(path, R"JSON({"id":"a","version":1,"tempo":2,"steps":[{"update":0,"held":[]}]})JSON");
        Require(!script.LoadFromFile(path.string(), error), "an unknown top-level field must be rejected");

        Require(!script.LoadFromFile(path.string() + ".missing", error),
                "a missing file must be rejected, not crash");

        std::filesystem::remove(path);
    }

    // The repro case committed alongside the tests must stay loadable and stay meaningful.
    void TestCommittedPrologueReproScriptIsUsable()
    {
        IronGang::InputScript script;
        std::string error;
        Require(script.LoadFromFile(std::string(IRON_GANG_SOURCE_ROOT) +
                                        "/tests/input-scripts/prologue_opening.inputscript.json",
                                    error),
                "the committed prologue repro must load: " + error);
        Require(script.GetId() == "prologue_opening", "the committed repro must keep its id");

        // It has to actually press Confirm and Interact, or it stops being the repro it claims to
        // be the moment someone edits it.
        script.Rewind();
        int confirms = 0;
        int interacts = 0;
        for (int update = 0; update <= script.GetLastUpdate(); ++update)
        {
            script.Advance();
            confirms += script.WasPressed(IronGang::GameAction::Confirm) ? 1 : 0;
            interacts += script.WasPressed(IronGang::GameAction::Interact) ? 1 : 0;
        }
        Require(confirms == 3,
                "the repro must press Confirm three times (skip the cutscene, then two dialogue "
                "lines), got " + std::to_string(confirms));
        Require(interacts == 1, "the repro must press Interact once, to enter the sedan");
    }

    void TestScreenshotSummaryDescribesAFrame()
    {
        constexpr int kWidth = 40;
        constexpr int kHeight = 20;
        const auto skyFrame = [&]() {
            std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kWidth) * kHeight * 4);
            for (std::size_t pixel = 0; pixel < rgba.size() / 4; ++pixel)
            {
                rgba[pixel * 4 + 0] = IronGang::kSkyClearRed;
                rgba[pixel * 4 + 1] = IronGang::kSkyClearGreen;
                rgba[pixel * 4 + 2] = IronGang::kSkyClearBlue;
                rgba[pixel * 4 + 3] = 255;
            }
            return rgba;
        };

        // A frame of nothing but sky: the renderer drew nothing. This is the failure the whole
        // capture exists to catch, and no other test in this suite can see it.
        std::vector<std::uint8_t> rgba = skyFrame();
        IronGang::ScreenshotSummary summary = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(summary.width == kWidth && summary.height == kHeight, "dimensions must round-trip");
        Require(summary.pixelCount == static_cast<std::size_t>(kWidth) * kHeight, "pixel count must match");
        Require(summary.nonSkyPixels == 0, "a sky-only frame must have no non-sky pixels");
        Require(summary.distinctColours == 1, "a sky-only frame has exactly one colour");
        std::string reason;
        Require(!IronGang::ScreenshotLooksRendered(summary, reason), "a sky-only frame must be rejected");
        Require(reason.find("nothing but sky") != std::string::npos,
                "the reason must say what is wrong: " + reason);

        // Near-sky pixels are still sky: anti-aliasing must not read as geometry.
        rgba = skyFrame();
        rgba[0] = static_cast<std::uint8_t>(IronGang::kSkyClearRed + 3);
        summary = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(summary.nonSkyPixels == 0, "a pixel within tolerance of the sky must not count as geometry");
        rgba[0] = static_cast<std::uint8_t>(IronGang::kSkyClearRed + 40);
        summary = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(summary.nonSkyPixels == 1, "a pixel well away from the sky colour must count");

        // A frame with sky, geometry, and enough colours to be a scene.
        rgba = skyFrame();
        for (std::size_t pixel = 0; pixel < rgba.size() / 4 / 2; ++pixel)
        {
            rgba[pixel * 4 + 0] = static_cast<std::uint8_t>(pixel % 200);
            rgba[pixel * 4 + 1] = static_cast<std::uint8_t>((pixel * 3) % 200);
            rgba[pixel * 4 + 2] = static_cast<std::uint8_t>((pixel * 7) % 200);
        }
        summary = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(std::abs(summary.NonSkyFraction() - 0.5) < 0.02,
                "half the frame painted must read as roughly half non-sky");
        Require(summary.distinctColours > 8, "a painted frame must have many colours");
        Require(IronGang::ScreenshotLooksRendered(summary, reason),
                "a frame with sky, geometry, and many colours must pass: " + reason);

        // A frame with no sky at all is NOT a failure -- the intro cutscene's downward
        // establishing shot is 99.7% non-sky and perfectly correct. This assertion exists because
        // the first version of this predicate rejected it.
        for (std::size_t pixel = 0; pixel < rgba.size() / 4; ++pixel)
        {
            rgba[pixel * 4 + 0] = static_cast<std::uint8_t>(pixel % 200);
            rgba[pixel * 4 + 1] = static_cast<std::uint8_t>((pixel * 3) % 200);
            rgba[pixel * 4 + 2] = static_cast<std::uint8_t>((pixel * 7) % 200);
        }
        summary = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(summary.NonSkyFraction() > 0.99 && IronGang::ScreenshotLooksRendered(summary, reason),
                "a frame full of geometry and no sky must pass: " + reason);

        // Two flat colours: a shader or format failure filling the screen.
        for (std::size_t pixel = 0; pixel < rgba.size() / 4; ++pixel)
        {
            const std::uint8_t value = static_cast<std::uint8_t>(pixel % 2 == 0 ? 10 : 250);
            rgba[pixel * 4 + 0] = value;
            rgba[pixel * 4 + 1] = value;
            rgba[pixel * 4 + 2] = value;
        }
        summary = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(!IronGang::ScreenshotLooksRendered(summary, reason),
                "a two-colour frame must be rejected as a flat fill");
        Require(reason.find("distinct colours") != std::string::npos,
                "the reason must say what is wrong: " + reason);

        // A buffer whose size does not match the dimensions is a caller error, not a scene.
        summary = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight + 1);
        Require(summary.pixelCount == 0 && !IronGang::ScreenshotLooksRendered(summary, reason),
                "a mismatched buffer must yield an empty summary that fails the predicate");
        summary = IronGang::SummarizeScreenshot({}, 0, 0);
        Require(summary.pixelCount == 0, "an empty capture must be empty, not a crash");
    }

    void TestScreenshotSummaryDigestAndSidecar()
    {
        constexpr int kWidth = 8;
        constexpr int kHeight = 8;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kWidth) * kHeight * 4, 90);
        const IronGang::ScreenshotSummary first = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        const IronGang::ScreenshotSummary again = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(first.digest == again.digest, "the digest must be stable for identical pixels");
        rgba[17] = 91;
        const IronGang::ScreenshotSummary changed = IronGang::SummarizeScreenshot(rgba, kWidth, kHeight);
        Require(changed.digest != first.digest,
                "a single changed byte must change the digest, or it cannot answer \"did this change\"");

        const std::filesystem::path path =
            std::filesystem::current_path() / "iron_gang_screenshot_summary.json";
        std::string error;
        Require(IronGang::WriteScreenshotSummary(path.string(), changed, error),
                "the summary sidecar must be written: " + error);
        std::ifstream stream(path);
        const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        stream.close();
        Require(text.find("\"width\": 8") != std::string::npos, "the sidecar must record the width");
        Require(text.find("\"digest\": \"" + std::to_string(changed.digest) + "\"") != std::string::npos,
                "the sidecar must record the digest as a string, since it does not fit a JSON double");
        Require(text.find("\"nonSkyFraction\"") != std::string::npos,
                "the sidecar must record the fraction a reviewer actually compares");
        std::filesystem::remove(path);
    }

    void TestCutsceneDialogueTrackSelectsLinesOverTime()
    {
        IronGang::CutsceneSequence sequence;
        sequence.duration = 5.0F;
        sequence.cameraKeyframes = {
            {0.0F, IronGang::Vector3(0.0F, 0.0F, 0.0F), IronGang::Vector3(1.0F, 0.0F, 0.0F)},
            {5.0F, IronGang::Vector3(9.0F, 0.0F, 0.0F), IronGang::Vector3(2.0F, 0.0F, 0.0F)},
        };
        sequence.dialogueCues = {{1.0F, "line.one"}, {3.0F, "line.two"}};

        IronGang::CutscenePlayer player;
        player.Start(sequence);
        Require(player.GetActiveCueLineId().empty(),
                "before the first cue the track must name no line at all, not the first one");

        player.Update(1.0F); // exactly on the first cue
        Require(player.GetActiveCueLineId() == "line.one",
                "a cue must become active the instant its time is reached");
        player.Update(1.5F); // 2.5s: past the first cue, before the second
        Require(player.GetActiveCueLineId() == "line.one",
                "a cue must stay active until the next one comes due");
        player.Update(1.0F); // 3.5s
        Require(player.GetActiveCueLineId() == "line.two", "the second cue must take over");
        player.Update(5.0F); // runs past the end
        Require(!player.IsActive() && player.GetActiveCueLineId() == "line.two",
                "a finished sequence must hold its last cue, not clear it");

        // The skip path: IG-26-004 requires the same terminal state a play-through produces, and
        // that now includes the dialogue track, not just the camera.
        IronGang::CutscenePlayer skipped;
        skipped.Start(sequence);
        skipped.Update(0.1F);
        Require(skipped.GetActiveCueLineId().empty(), "sanity: the skip starts before any cue");
        skipped.Skip();
        Require(skipped.GetActiveCueLineId() == "line.two",
                "a skipped cutscene must land on the last cue, not on whichever cue it had reached");
    }

    // plan_34 IG-34-015: the point of stable dialogue ids is that *other content* referencing a
    // line is checked. A cue naming a line the conversation no longer has must fail the load.
    void TestCutsceneRejectsStaleDialogueReference()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_cue_cutscene.json";
        const std::vector<std::string> knownLineIds{"prologue.mara.quiet_tonight",
                                                    "prologue.elias.take_the_sedan"};
        std::string error;
        IronGang::CutsceneSequence sequence;

        const auto writeWithCues = [&path](const std::string& cues) {
            WriteTempJson(path, R"JSON({
            "id": "cue_test",
            "version": 1,
            "duration": 4.0,
            "cameraKeyframes": [
                { "time": 0.0, "position": [0, 1, 2], "lookAt": [3, 4, 5] },
                { "time": 4.0, "position": [6, 7, 8], "lookAt": [9, 10, 11] }
            ],
            "dialogue": )JSON" + cues + "}");
        };

        writeWithCues(R"JSON([{"time":0.0,"lineId":"prologue.mara.quiet_tonight"},
                              {"time":2.0,"lineId":"prologue.elias.take_the_sedan"}])JSON");
        Require(IronGang::LoadCutsceneSequence(path.string(), knownLineIds, sequence, error),
                "cues naming lines the conversation has must load: " + error);
        Require(sequence.dialogueCues.size() == 2, "both cues must be parsed");
        Require(sequence.dialogueCues[1].lineId == "prologue.elias.take_the_sedan" &&
                    std::abs(sequence.dialogueCues[1].time - 2.0F) < 1e-4F,
                "cue time and line id must round-trip");

        // The whole point: the same file against a conversation that renamed one of those lines.
        const std::vector<std::string> renamed{"prologue.mara.quiet_tonight", "prologue.elias.take_the_van"};
        Require(!IronGang::LoadCutsceneSequence(path.string(), renamed, sequence, error),
                "a cue naming a line the conversation no longer contains must be rejected");
        Require(error.find("prologue.elias.take_the_sedan") != std::string::npos,
                "the error must name the stale line, or it is not actionable: " + error);

        Require(!IronGang::LoadCutsceneSequence(path.string(), {}, sequence, error),
                "cues against an empty conversation must be rejected, not silently dropped");

        writeWithCues(R"JSON([{"time":2.0,"lineId":"prologue.elias.take_the_sedan"},
                              {"time":1.0,"lineId":"prologue.mara.quiet_tonight"}])JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), knownLineIds, sequence, error),
                "cues out of ascending time order must be rejected");

        writeWithCues(R"JSON([{"time":9.0,"lineId":"prologue.mara.quiet_tonight"}])JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), knownLineIds, sequence, error),
                "a cue past the sequence's own duration must be rejected");

        writeWithCues(R"JSON([{"lineId":"prologue.mara.quiet_tonight"}])JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), knownLineIds, sequence, error),
                "a cue with no time must be rejected");

        std::filesystem::remove(path);
    }

    // The shipped intro and the shipped conversation must agree -- this is the check that would
    // have failed if a line were renamed in one file and not the other.
    void TestShippedCutsceneCuesResolveAgainstShippedDialogue()
    {
        const std::string assetRoot(IRON_GANG_SOURCE_ASSET_DIR);
        IronGang::DialogueSystem dialogue;
        std::string error;
        Require(dialogue.LoadFromFile(assetRoot + "/dialogues/prologue.dialogue.json", error),
                "the shipped prologue conversation must load: " + error);

        std::vector<std::string> lineIds;
        for (std::size_t index = 0; index < dialogue.GetLineCount(); ++index)
        {
            lineIds.push_back(dialogue.GetLineId(index));
        }

        IronGang::CutsceneSequence sequence;
        Require(IronGang::LoadCutsceneSequence(assetRoot + "/cutscenes/prologue_intro.cutscene.json",
                                               lineIds, sequence, error),
                "the shipped intro cutscene must validate against the shipped conversation: " + error);
        Require(!sequence.dialogueCues.empty(), "the shipped intro must actually use its dialogue track");

        // And the binding the game performs: every cue resolves to a line with real text.
        for (const IronGang::CutsceneDialogueCue& cue : sequence.dialogueCues)
        {
            Require(dialogue.SelectLine(cue.lineId), "cue \"" + cue.lineId + "\" must select a line");
            const IronGang::DialogueLine* line = dialogue.GetCurrentLine();
            Require(line != nullptr && line->id == cue.lineId && !line->text.empty(),
                    "selecting a cued line must make exactly that line current, with text");
        }
        Require(!dialogue.SelectLine("prologue.nobody.said_this"),
                "selecting an id the conversation lacks must fail rather than land on line 0");
    }

    void TestCutsceneValidationRejectsMalformedData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_bad_cutscene.json";
        std::string error;
        IronGang::CutsceneSequence sequence;

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), {}, sequence, error),
                "an empty cameraKeyframes array must be rejected");

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[
            {"time":0.5,"position":[0,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), {}, sequence, error),
                "a first keyframe not at time 0 must be rejected");

        WriteTempJson(path, R"JSON({"duration":2.0,"cameraKeyframes":[
            {"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]},
            {"time":0.0,"position":[1,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), {}, sequence, error),
                "keyframes that are not strictly ascending in time must be rejected");

        WriteTempJson(path, R"JSON({"duration":1.0,"cameraKeyframes":[
            {"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]},
            {"time":2.0,"position":[1,0,0],"lookAt":[1,0,0]}
        ]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), {}, sequence, error),
                "a duration shorter than the last keyframe's time must be rejected");

        WriteTempJson(path, R"JSON({"cameraKeyframes":[{"time":0.0,"position":[0,0,0],"lookAt":[1,0,0]}]})JSON");
        Require(!IronGang::LoadCutsceneSequence(path.string(), {}, sequence, error),
                "a missing \"duration\" must be rejected");

        Require(!IronGang::LoadCutsceneSequence((path.string() + ".does-not-exist"), {}, sequence, error),
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
        Require(IronGang::LoadCutsceneSequence(path.string(), {}, sequence, error),
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
    // plan_20 IG-20-003: which clip each locomotion state asks for, and what it falls back to.
    // plan_08 IG-08-014: the base colours the .cnj pipeline drops. A model with no entry stays
    // white, which is exactly how everything rendered before this data existed.
    // plan_25 IG-25-003: the wrapping a subtitle needs. The failure this replaces was a single
    // unwrapped HUD line running off the right edge of the screen mid-word.
    // plan_16 IG-16-003: the follow camera sits a fixed distance behind the player, so standing
    // with a building at your back puts it inside the building. It must be pulled in instead.
    // plan_16 IG-16-004: which affordance is offered, and the hysteresis that stops two nearby
    // ones trading the prompt every frame.
    // plan_27 IG-27-001/026/027: the bus graph every sound plays through.
    // plan_27 IG-27-003/007: distance and direction, from a listener that is the active camera.
    // plan_14 IG-14-001/002: the road layout used to be C++ literals. Loading it from data must
    // reproduce it exactly -- that equivalence is the whole point of the migration.
    // plan_14 IG-14-007/008: pavements as their own graph, and the same equivalence requirement
    // the road graph had -- data must reproduce the hand-authored layout exactly.
    // plan_20 IG-20-012: crossing at the marked crossing, and waiting for the signal.
    void TestPedestrianCrossingRespectsTheSignal()
    {
        using IronGang::PedestrianMayCross;
        using IronGang::SignalPhase;

        // Only a stopped-traffic phase lets someone step off the kerb. Amber stops *vehicles*
        // (TrafficSignal::RequiresStop) but is not a moment to start walking: a car already
        // committed to the junction is still coming through.
        Require(PedestrianMayCross(SignalPhase::Red, true), "a red for traffic is a walk signal");
        Require(!PedestrianMayCross(SignalPhase::Green, true), "green for traffic means wait");
        Require(!PedestrianMayCross(SignalPhase::Amber, true),
                "amber is not a moment to start walking, even though vehicles must stop for it");
        // An unsignalled crossing is a give-way, and giving way is the driver's job -- waiting
        // there would leave people at an empty road forever.
        Require(PedestrianMayCross(SignalPhase::Green, false),
                "an unsignalled crossing must never hold anyone");

        const std::vector<IronGang::Vector3> kerbs{IronGang::Vector3(-7.5F, 0.9F, 0.0F),
                                                   IronGang::Vector3(7.5F, 0.9F, 0.0F)};

        // Allowed: no obstacle anywhere along the crossing.
        Require(IronGang::PedestrianCrossingClearance(kerbs[0], kerbs, true) == IronGang::kNoObstacleAhead,
                "a pedestrian who may cross must not be obstructed");

        // Not allowed, standing on the kerb: a full stop.
        Require(IronGang::PedestrianCrossingClearance(kerbs[0], kerbs, false) == 0.0F,
                "a pedestrian at the kerb who may not cross must be stopped");
        Require(IronGang::PedestrianCrossingClearance(kerbs[1], kerbs, false) == 0.0F,
                "either kerb must hold, not just the first");

        // Not allowed, but already out in the road: they must be let finish. A signal changing
        // mid-crossing must not freeze someone in a live lane.
        Require(IronGang::PedestrianCrossingClearance(IronGang::Vector3(0.0F, 0.9F, 0.0F), kerbs, false) ==
                    IronGang::kNoObstacleAhead,
                "a pedestrian already in the road must be allowed to finish crossing");
        Require(IronGang::PedestrianCrossingClearance(IronGang::Vector3(-5.0F, 0.9F, 0.0F), kerbs, false) ==
                    IronGang::kNoObstacleAhead,
                "past the kerb hold radius is past holding");
        // Just inside the radius still holds.
        Require(IronGang::PedestrianCrossingClearance(
                    IronGang::Vector3(-7.5F + IronGang::kKerbHoldRadiusMetres * 0.5F, 0.9F, 0.0F), kerbs,
                    false) == 0.0F,
                "within the kerb hold radius must still hold");

        Require(IronGang::PedestrianCrossingClearance(kerbs[0], {}, false) == IronGang::kNoObstacleAhead,
                "with no kerbs there is nothing to hold anyone at");

        // End to end against a real signal cycle: a pedestrian held at the kerb must actually get
        // to cross within one cycle, or the crossing is a wall rather than a wait.
        IronGang::TrafficSignal signal;
        signal.Reset();
        IronGang::Pedestrian pedestrian;
        IronGang::WaypointPath crossing;
        crossing.points = {kerbs[0], kerbs[1]};
        crossing.loop = true;
        pedestrian.Reset(crossing, 0, 1.4F);

        constexpr float kStep = 1.0F / 60.0F;
        int updates = 0;
        int heldUpdates = 0;
        const float startX = pedestrian.GetPathPosition().X;
        while (std::abs(pedestrian.GetPathPosition().X - startX) < 3.0F && updates < 60 * 60)
        {
            signal.Update(kStep);
            const bool mayCross = PedestrianMayCross(signal.GetPhase(), true);
            const float clearance =
                IronGang::PedestrianCrossingClearance(pedestrian.GetPosition(), kerbs, mayCross);
            if (clearance == 0.0F)
            {
                ++heldUpdates;
            }
            pedestrian.Update(kStep, false, IronGang::Vector3(), clearance);
            ++updates;
        }
        Require(updates < 60 * 60,
                "a pedestrian must get across within a minute; a signal that never lets them is a "
                "wall, not a wait");
        Require(heldUpdates > 0,
                "the pedestrian must actually have been held at some point, or this test proves "
                "nothing about the signal");
    }

    void TestSidewalkGraphReplacesTheHandAuthoredLayoutExactly()
    {
        const IronGang::PrototypeWorld builtIn(IronGang::DistrictId::WarehouseBlock);
        Require(builtIn.GetSidewalkGraph().IsEmpty(),
                "with no asset root a district must keep its built-in pavements");

        const IronGang::PrototypeWorld fromData(IronGang::DistrictId::WarehouseBlock,
                                                std::string(IRON_GANG_SOURCE_ASSET_DIR));
        Require(!fromData.GetSidewalkGraph().IsEmpty(),
                "the shipped sidewalk graph must load; an empty graph here means it silently fell "
                "back and the rest of this test would compare nothing");

        const std::vector<IronGang::WaypointPath>& expected = builtIn.GetSidewalkPaths();
        const std::vector<IronGang::WaypointPath>& actual = fromData.GetSidewalkPaths();
        Require(actual.size() == expected.size(),
                "the pavement count must match: " + std::to_string(actual.size()) + " vs " +
                    std::to_string(expected.size()));
        for (std::size_t path = 0; path < expected.size(); ++path)
        {
            Require(actual[path].loop == expected[path].loop, "the loop flag must match");
            Require(actual[path].points.size() == expected[path].points.size(),
                    "pavement " + std::to_string(path) + " must have the same number of points");
            for (std::size_t point = 0; point < expected[path].points.size(); ++point)
            {
                Require((actual[path].points[point] - expected[path].points[point]).Length() < 1e-3F,
                        "pavement " + std::to_string(path) + " point " + std::to_string(point) +
                            " must match the hand-authored one");
            }
        }

        // The two things the road graph deliberately does not carry.
        const IronGang::SidewalkGraph& graph = fromData.GetSidewalkGraph();
        Require(graph.GetCrossings().size() == 1, "the shipped district must declare its crossing");
        Require(graph.GetCrossings()[0].signalControlled,
                "the main crossing is signalled -- an unsignalled crossing is a give-way, not a wait");
        Require(graph.GetCrossings()[0].roadSegmentId == "main_northbound",
                "the crossing must name the road segment it crosses");
        Require(graph.GetEntrances().size() == 2, "the shipped district must declare its doors");
        for (const IronGang::SidewalkEntrance& entrance : graph.GetEntrances())
        {
            Require(graph.FindNode(entrance.nodeId) != nullptr,
                    "every entrance must resolve to a pavement node");
        }
    }

    void TestSidewalkGraphRejectsUnusableData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_sidewalks.json";
        std::string error;
        IronGang::SidewalkGraph graph;
        const std::vector<std::string> buildings{"hotel"};
        const std::vector<std::string> roads{"main_northbound"};

        const std::string nodes =
            R"JSON("nodes":[{"id":"a","position":[0,0,0]},{"id":"b","position":[0,0,10]}])JSON";
        const auto write = [&path, &nodes](const std::string& rest) {
            WriteTempJson(path, "{\"id\":\"t\",\"version\":1," + nodes + "," + rest + "}");
        };
        const std::string oneWalkway = R"JSON("walkways":[{"id":"w","from":"a","to":"b","width":3}])JSON";

        write(oneWalkway);
        Require(graph.LoadFromFile(path.string(), buildings, roads, error),
                "a minimal well-formed sidewalk graph must load: " + error);

        WriteTempJson(path, "{\"id\":\"t\",\"version\":7," + nodes + "," + oneWalkway + "}");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "an unsupported version must be rejected");

        write(R"JSON("walkways":[])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "a graph with no walkways has nobody walking on it and must be rejected");

        write(R"JSON("walkways":[{"id":"w","from":"a","to":"nowhere","width":3}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "a walkway to a missing node must be rejected");

        write(R"JSON("walkways":[{"id":"w","from":"a","to":"a","width":3}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "a zero-length walkway must be rejected");

        write(R"JSON("walkways":[{"id":"w","from":"a","to":"b","width":0}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "a walkway with no width must be rejected");

        write(R"JSON("walkways":[{"id":"w","from":"a","to":"b","width":3},
                                 {"id":"w","from":"b","to":"a","width":3}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "duplicate walkway ids must be rejected");

        // The stale-reference rule, both ways round: a door into a building the district does not
        // contain, and a crossing over a road that is not there.
        write(oneWalkway + R"JSON(,"entrances":[{"id":"e","node":"a","building":"casino"}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "an entrance into a building the district lacks must be rejected");
        Require(error.find("casino") != std::string::npos,
                "the error must name the missing building: " + error);

        write(oneWalkway + R"JSON(,"crossings":[{"id":"c","from":"a","to":"b","roadSegment":"tunnel"}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "a crossing over a road segment the district lacks must be rejected");

        // ...but an empty known-set means "do not check", so a district with no road graph still
        // loads its pavements.
        Require(graph.LoadFromFile(path.string(), buildings, {}, error),
                "with no road ids to check against, a crossing must load: " + error);

        write(oneWalkway + R"JSON(,"entrances":[{"id":"e","node":"nowhere","building":"hotel"}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "an entrance on a missing node must be rejected");

        write(oneWalkway + R"JSON(,"crossings":[{"id":"c","from":"a","to":"b","signalControlled":"yes"}])JSON");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "a non-boolean signalControlled must be rejected");

        WriteTempJson(path, "{\"id\":\"t\",\"version\":1,\"lighting\":\"gas\"," + nodes + "," + oneWalkway + "}");
        Require(!graph.LoadFromFile(path.string(), buildings, roads, error),
                "an unknown top-level field must be rejected");

        Require(!graph.LoadFromFile(path.string() + ".missing", buildings, roads, error),
                "a missing file must be rejected, not crash");

        std::filesystem::remove(path);
    }

    void TestRoadGraphReplacesTheHandAuthoredLayoutExactly()
    {
        const IronGang::PrototypeWorld builtIn(IronGang::DistrictId::WarehouseBlock);
        Require(builtIn.GetRoadGraph().IsEmpty(),
                "with no asset root a district must keep its built-in layout");

        const IronGang::PrototypeWorld fromData(IronGang::DistrictId::WarehouseBlock,
                                                std::string(IRON_GANG_SOURCE_ASSET_DIR));
        Require(!fromData.GetRoadGraph().IsEmpty(),
                "the shipped warehouse-block road graph must load; an empty graph here means it "
                "silently fell back and the rest of this test would compare nothing");
        Require(fromData.GetRoadGraph().GetId() == "warehouse_block", "the graph must keep its id");

        const IronGang::WaypointPath& expected = builtIn.GetTrafficLoop();
        const IronGang::WaypointPath& actual = fromData.GetTrafficLoop();
        Require(actual.loop == expected.loop, "the loop flag must match");
        Require(actual.points.size() == expected.points.size(),
                "the data-driven loop must have the same number of points: " +
                    std::to_string(actual.points.size()) + " vs " + std::to_string(expected.points.size()));
        for (std::size_t index = 0; index < expected.points.size(); ++index)
        {
            Require((actual.points[index] - expected.points[index]).Length() < 1e-3F,
                    "traffic loop point " + std::to_string(index) + " must match the hand-authored one");
        }

        const std::vector<IronGang::TrafficStopLine>& expectedLines = builtIn.GetTrafficStopLines();
        const std::vector<IronGang::TrafficStopLine>& actualLines = fromData.GetTrafficStopLines();
        Require(actualLines.size() == expectedLines.size(), "the stop-line count must match");
        for (std::size_t index = 0; index < expectedLines.size(); ++index)
        {
            Require((actualLines[index].position - expectedLines[index].position).Length() < 1e-3F,
                    "stop line " + std::to_string(index) + " must be in the same place");
            Require((actualLines[index].signalPosition - expectedLines[index].signalPosition).Length() < 1e-3F,
                    "stop line " + std::to_string(index) + "'s signal must be in the same place");
            Require(std::abs(actualLines[index].approachYaw - expectedLines[index].approachYaw) < 1e-3F,
                    "stop line " + std::to_string(index) + "'s approach yaw must match -- it is derived "
                    "from the segment rather than authored, so this pins the derivation");
            Require(actualLines[index].opposingPhase == expectedLines[index].opposingPhase,
                    "stop line " + std::to_string(index) + "'s phase must match, or the crossing shows "
                    "green in both directions at once");
        }

        // Lane geometry: lane 0 is half a width right of the centreline, in the direction of travel.
        IronGang::Vector3 point;
        Require(fromData.GetRoadGraph().GetLanePoint("main_northbound", 0, 0.0F, point),
                "lane 0 of the northbound segment must resolve");
        Require(std::abs(point.X - 3.0F) < 1e-3F,
                "the northbound lane must sit right of the centreline at x=+3, got " +
                    std::to_string(point.X));
        Require(fromData.GetRoadGraph().GetLanePoint("main_southbound", 0, 0.0F, point),
                "lane 0 of the southbound segment must resolve");
        Require(std::abs(point.X + 3.0F) < 1e-3F,
                "travelling the other way, right is the other side: x=-3, got " + std::to_string(point.X));
        Require(!fromData.GetRoadGraph().GetLanePoint("main_northbound", 5, 0.0F, point),
                "a lane the segment does not have must not resolve");
        Require(!fromData.GetRoadGraph().GetLanePoint("no_such_road", 0, 0.0F, point),
                "an unknown segment must not resolve");

        // The schema carries the turn links plan_14 IG-14-001 names, even though this district's
        // straight there-and-back does not use them yet.
        Require(fromData.GetRoadGraph().GetTurns().size() == 2,
                "the shipped graph must declare its turn links");
    }

    void TestRoadGraphRejectsUnusableData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_roads.json";
        std::string error;
        IronGang::RoadGraph graph;

        const std::string nodes =
            R"JSON("nodes":[{"id":"a","position":[0,0,0]},{"id":"b","position":[0,0,10]}])JSON";
        const auto write = [&path, &nodes](const std::string& rest) {
            WriteTempJson(path, "{\"id\":\"t\",\"version\":1," + nodes + "," + rest + "}");
        };

        write(R"JSON("segments":[{"id":"s","from":"a","to":"b","laneCount":1,"laneWidth":3,"speedLimitKph":50}])JSON");
        Require(graph.LoadFromFile(path.string(), error), "a minimal well-formed graph must load: " + error);

        WriteTempJson(path, "{\"id\":\"t\",\"version\":2," + nodes +
                                R"JSON(,"segments":[{"id":"s","from":"a","to":"b","laneCount":1,"laneWidth":3,"speedLimitKph":50}])JSON" "}");
        Require(!graph.LoadFromFile(path.string(), error), "an unsupported version must be rejected");

        write(R"JSON("segments":[])JSON");
        Require(!graph.LoadFromFile(path.string(), error),
                "a graph with no segments has nothing to drive on and must be rejected");

        write(R"JSON("segments":[{"id":"s","from":"a","to":"nowhere","laneCount":1,"laneWidth":3,"speedLimitKph":50}])JSON");
        Require(!graph.LoadFromFile(path.string(), error), "a dangling node reference must be rejected");

        write(R"JSON("segments":[{"id":"s","from":"a","to":"a","laneCount":1,"laneWidth":3,"speedLimitKph":50}])JSON");
        Require(!graph.LoadFromFile(path.string(), error), "a zero-length segment must be rejected");

        write(R"JSON("segments":[
            {"id":"s","from":"a","to":"b","laneCount":1,"laneWidth":3,"speedLimitKph":50},
            {"id":"s","from":"b","to":"a","laneCount":1,"laneWidth":3,"speedLimitKph":50}])JSON");
        Require(!graph.LoadFromFile(path.string(), error), "duplicate segment ids must be rejected");

        for (const char* bad : {R"("laneCount":0,"laneWidth":3,"speedLimitKph":50)",
                                R"("laneCount":1,"laneWidth":0,"speedLimitKph":50)",
                                R"("laneCount":1,"laneWidth":3,"speedLimitKph":0)"})
        {
            write(std::string(R"JSON("segments":[{"id":"s","from":"a","to":"b",)JSON") + bad + "}]");
            Require(!graph.LoadFromFile(path.string(), error),
                    std::string("a non-positive lane count, width or speed limit must be rejected: ") + bad);
        }

        write(R"JSON("segments":[{"id":"s","from":"a","to":"b","laneCount":1,"laneWidth":3,"speedLimitKph":50}],
              "turns":[{"from":"s","to":"missing"}])JSON");
        Require(!graph.LoadFromFile(path.string(), error), "a turn to a missing segment must be rejected");

        write(R"JSON("segments":[{"id":"s","from":"a","to":"b","laneCount":1,"laneWidth":3,"speedLimitKph":50}],
              "stopLines":[{"segment":"missing","distance":1,"signalPosition":[0,0,0]}])JSON");
        Require(!graph.LoadFromFile(path.string(), error),
                "a stop line on a missing segment must be rejected");

        // Beyond the end of its own segment: the line would be placed at the segment's end and
        // silently stop traffic in the wrong place.
        write(R"JSON("segments":[{"id":"s","from":"a","to":"b","laneCount":1,"laneWidth":3,"speedLimitKph":50}],
              "stopLines":[{"segment":"s","distance":99,"signalPosition":[0,0,0]}])JSON");
        Require(!graph.LoadFromFile(path.string(), error),
                "a stop line past the end of its segment must be rejected");

        WriteTempJson(path, "{\"id\":\"t\",\"version\":1,\"weather\":\"rain\"," + nodes +
                                R"JSON(,"segments":[{"id":"s","from":"a","to":"b","laneCount":1,"laneWidth":3,"speedLimitKph":50}])JSON" "}");
        Require(!graph.LoadFromFile(path.string(), error), "an unknown top-level field must be rejected");

        WriteTempJson(path, "{\"id\":\"t\",\"version\":1,\"nodes\":[{\"id\":\"a\",\"position\":[0,0,0]},"
                            "{\"id\":\"a\",\"position\":[0,0,9]}],"
                            R"JSON("segments":[{"id":"s","from":"a","to":"a","laneCount":1,"laneWidth":3,"speedLimitKph":50}])JSON" "}");
        Require(!graph.LoadFromFile(path.string(), error), "duplicate node ids must be rejected");

        Require(!graph.LoadFromFile(path.string() + ".missing", error),
                "a missing file must be rejected, not crash");

        std::filesystem::remove(path);
    }

    void TestSpatialAudioAttenuationAndPan()
    {
        // Facing -Z, which is the game's own yaw-0 convention; right is therefore +X.
        IronGang::AudioListener listener;
        listener.position = IronGang::Vector3(0.0F, 0.0F, 0.0F);
        listener.forward = IronGang::Vector3(0.0F, 0.0F, -1.0F);

        const IronGang::SpatialFalloff falloff{10.0F, 50.0F};

        // Inside the reference distance nothing is attenuated: a source is not quieter for being
        // two metres away instead of one.
        IronGang::SpatialGain gain =
            IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 0.0F, -2.0F), falloff);
        Require(std::abs(gain.attenuation - 1.0F) < 1e-5F, "inside the reference distance is full volume");
        Require(std::abs(gain.pan) < 1e-5F, "a source straight ahead must be centred");

        // At and beyond the maximum it is silent.
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 0.0F, -50.0F), falloff);
        Require(gain.attenuation == 0.0F, "at the maximum distance a source must be silent");
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 0.0F, -500.0F), falloff);
        Require(gain.attenuation == 0.0F, "beyond the maximum distance a source must stay silent");

        // Halfway between reference and maximum is half volume, and it decreases monotonically.
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 0.0F, -30.0F), falloff);
        Require(std::abs(gain.attenuation - 0.5F) < 1e-5F,
                "the rolloff must be linear between reference and maximum, got " +
                    std::to_string(gain.attenuation));
        float previous = 1.0F;
        for (int metres = 10; metres <= 50; metres += 5)
        {
            const float value = IronGang::ComputeSpatialGain(
                                    listener, IronGang::Vector3(0.0F, 0.0F, -static_cast<float>(metres)),
                                    falloff)
                                    .attenuation;
            Require(value <= previous + 1e-5F, "attenuation must never rise with distance");
            previous = value;
        }

        // Pan: right of the listener is positive, left negative, behind is centred.
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(20.0F, 0.0F, 0.0F), falloff);
        Require(gain.pan > 0.9F, "a source to the listener's right must pan right, got " +
                                     std::to_string(gain.pan));
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(-20.0F, 0.0F, 0.0F), falloff);
        Require(gain.pan < -0.9F, "a source to the listener's left must pan left, got " +
                                      std::to_string(gain.pan));
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 0.0F, 20.0F), falloff);
        Require(std::abs(gain.pan) < 1e-5F, "a source directly behind has no left or right");

        // Turning the listener turns the stereo field with it.
        listener.forward = IronGang::Vector3(1.0F, 0.0F, 0.0F); // now facing +X
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 0.0F, 20.0F), falloff);
        Require(gain.pan > 0.9F,
                "the same world position must pan right once the listener faces +X, got " +
                    std::to_string(gain.pan));

        // Height must not leak into pan: a source directly overhead has no left or right, and
        // treating the vertical component as lateral makes sounds swing as the camera pitches.
        listener.forward = IronGang::Vector3(0.0F, 0.0F, -1.0F);
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 20.0F, 0.0F), falloff);
        Require(std::abs(gain.pan) < 1e-5F, "a source overhead must be centred");
        Require(gain.attenuation < 1.0F,
                "height must still count toward distance even though it does not pan");

        // Degenerate: emitter exactly on the listener.
        gain = IronGang::ComputeSpatialGain(listener, listener.position, falloff);
        Require(std::abs(gain.attenuation - 1.0F) < 1e-5F && std::abs(gain.pan) < 1e-5F,
                "a source on top of the listener is full volume and centred, not a division by zero");

        // Presets: each is a real, ordered choice rather than the same numbers four times.
        const IronGang::SpatialFalloff voice = IronGang::SpatialFalloffFor(IronGang::SpatialPreset::Voice);
        const IronGang::SpatialFalloff car = IronGang::SpatialFalloffFor(IronGang::SpatialPreset::Vehicle);
        const IronGang::SpatialFalloff ambience =
            IronGang::SpatialFalloffFor(IronGang::SpatialPreset::Ambience);
        Require(voice.maximumMetres < car.maximumMetres,
                "a voice must not carry as far as a car");
        Require(car.maximumMetres < ambience.maximumMetres,
                "ambience is a place, not a point, and must outreach a car");
        Require(car.referenceMetres >= 10.0F,
                "the vehicle preset's reference distance must cover the camera boom, or the "
                "player's own engine attenuates behind them");
        for (const IronGang::SpatialPreset preset :
             {IronGang::SpatialPreset::Voice, IronGang::SpatialPreset::Vehicle,
              IronGang::SpatialPreset::Effect, IronGang::SpatialPreset::Ambience})
        {
            const IronGang::SpatialFalloff value = IronGang::SpatialFalloffFor(preset);
            Require(value.referenceMetres > 0.0F && value.maximumMetres > value.referenceMetres,
                    "every preset must have a positive reference inside its maximum");
        }

        // An inverted falloff must not produce a negative or growing gain.
        gain = IronGang::ComputeSpatialGain(listener, IronGang::Vector3(0.0F, 0.0F, -20.0F),
                                            IronGang::SpatialFalloff{40.0F, 10.0F});
        Require(gain.attenuation >= 0.0F && gain.attenuation <= 1.0F,
                "a maximum below the reference must still yield a gain in [0,1]");
    }

    void TestAudioBusGraphMixing()
    {
        IronGang::AudioBusGraph buses;

        // Untouched, a bus is transparent: what a sound asks for is what it gets.
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Vehicle, 0.7F) - 0.7F) < 1e-5F,
                "a fresh graph must not change a requested volume");

        // Master scales everything...
        buses.SetVolume(IronGang::AudioBus::Master, 0.5F);
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Vehicle, 0.8F) - 0.4F) < 1e-5F,
                "Master must scale every other bus");
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Master, 0.8F) - 0.4F) < 1e-5F,
                "Master must scale itself exactly once, not twice");

        // ...and a bus scales only itself, which is the whole point of having categories.
        buses.SetVolume(IronGang::AudioBus::Vehicle, 0.5F);
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Vehicle, 0.8F) - 0.2F) < 1e-5F,
                "a bus volume must multiply with Master");
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Effects, 0.8F) - 0.4F) < 1e-5F,
                "setting one bus must not touch another");

        // Clamping, both ends.
        buses.SetVolume(IronGang::AudioBus::Master, 5.0F);
        Require(std::abs(buses.GetVolume(IronGang::AudioBus::Master) - 1.0F) < 1e-5F,
                "a volume above 1 must clamp");
        buses.SetVolume(IronGang::AudioBus::Master, -2.0F);
        Require(std::abs(buses.GetVolume(IronGang::AudioBus::Master)) < 1e-5F,
                "a negative volume must clamp to silence");
        buses.SetVolume(IronGang::AudioBus::Master, 1.0F);
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Effects, 3.0F) - 1.0F) < 1e-5F,
                "a requested volume above 1 must clamp too");

        // Muting a bus silences it and nothing else; muting Master silences everything.
        buses.SetVolume(IronGang::AudioBus::Vehicle, 1.0F);
        buses.SetMuted(IronGang::AudioBus::Vehicle, true);
        Require(buses.GetEffectiveVolume(IronGang::AudioBus::Vehicle, 1.0F) == 0.0F,
                "a muted bus must be silent");
        Require(buses.GetEffectiveVolume(IronGang::AudioBus::Effects, 1.0F) > 0.0F,
                "muting one bus must not silence another");
        buses.SetMuted(IronGang::AudioBus::Vehicle, false);
        buses.SetMuted(IronGang::AudioBus::Master, true);
        Require(buses.GetEffectiveVolume(IronGang::AudioBus::Effects, 1.0F) == 0.0F,
                "muting Master must silence every bus");
        buses.SetMuted(IronGang::AudioBus::Master, false);

        // Ids round-trip, so a saved mix survives renaming the enum.
        for (std::size_t index = 0; index < IronGang::kAudioBusCount; ++index)
        {
            const auto bus = static_cast<IronGang::AudioBus>(index);
            const std::string id = IronGang::AudioBusId(bus);
            Require(!id.empty(), "every bus must have an id");
            IronGang::AudioBus parsed{};
            Require(IronGang::ParseAudioBusId(id, parsed) && parsed == bus,
                    "the id \"" + id + "\" must parse back to its own bus");
        }
        IronGang::AudioBus unused{};
        Require(!IronGang::ParseAudioBusId("subwoofer", unused), "an unknown id must be rejected");
    }

    // plan_27 IG-27-004: dialogue ducking, including that it ramps rather than snaps.
    void TestDialogueDucking()
    {
        IronGang::AudioBusGraph buses;
        constexpr float kStep = 1.0F / 60.0F;

        Require(std::abs(buses.GetDuckGain() - 1.0F) < 1e-5F, "nothing is ducked to begin with");

        buses.SetDialogueActive(true);
        // It must not snap: one update in, the duck is under way but nowhere near the target.
        buses.Update(kStep);
        Require(buses.GetDuckGain() < 1.0F, "the duck must start immediately");
        Require(buses.GetDuckGain() > IronGang::kDialogueDuckGain + 0.1F,
                "the duck must ramp, not snap -- one 60 Hz update reached " +
                    std::to_string(buses.GetDuckGain()));

        int attackUpdates = 1;
        while (buses.GetDuckGain() > IronGang::kDialogueDuckGain + 1e-4F && attackUpdates < 600)
        {
            buses.Update(kStep);
            ++attackUpdates;
        }
        Require(std::abs(buses.GetDuckGain() - IronGang::kDialogueDuckGain) < 1e-4F,
                "the duck must reach its target and stop there");
        Require(attackUpdates > 4,
                "the attack must take several updates, got " + std::to_string(attackUpdates));

        // Ducked buses are quieter; dialogue and UI are not touched.
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Vehicle, 1.0F) -
                         IronGang::kDialogueDuckGain) < 1e-4F,
                "the vehicle bus must be ducked while dialogue plays");
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Dialogue, 1.0F) - 1.0F) < 1e-4F,
                "the dialogue bus must never duck itself");
        Require(std::abs(buses.GetEffectiveVolume(IronGang::AudioBus::Ui, 1.0F) - 1.0F) < 1e-4F,
                "a menu click going quiet because someone is talking is a bug, not a mix");

        // Release: slower than the attack, which is what stops short lines pumping the mix.
        buses.SetDialogueActive(false);
        int releaseUpdates = 0;
        while (buses.GetDuckGain() < 1.0F - 1e-4F && releaseUpdates < 600)
        {
            buses.Update(kStep);
            ++releaseUpdates;
        }
        Require(std::abs(buses.GetDuckGain() - 1.0F) < 1e-4F, "the duck must return all the way to 1");
        Require(releaseUpdates > attackUpdates,
                "the release must be slower than the attack (attack " + std::to_string(attackUpdates) +
                    ", release " + std::to_string(releaseUpdates) + ")");

        // A zero or negative delta must not move the ramp at all.
        buses.SetDialogueActive(true);
        buses.Update(kStep);
        const float mid = buses.GetDuckGain();
        buses.Update(0.0F);
        buses.Update(-1.0F);
        Require(std::abs(buses.GetDuckGain() - mid) < 1e-6F,
                "a zero or negative delta must leave the ramp untouched");

        // Deterministic: the same delta sequence from the same start gives the same gain.
        IronGang::AudioBusGraph first;
        IronGang::AudioBusGraph second;
        first.SetDialogueActive(true);
        second.SetDialogueActive(true);
        for (int i = 0; i < 7; ++i)
        {
            first.Update(kStep);
            second.Update(kStep);
        }
        Require(first.GetDuckGain() == second.GetDuckGain(),
                "the ramp must be a pure function of the deltas it was given");
    }

    void TestInteractionPromptSelection()
    {
        const auto target = [](const char* id, const char* label, float x, float radius, bool available) {
            return IronGang::InteractionTarget{id, label, IronGang::Vector3(x, 0.0F, 0.0F), radius,
                                               available};
        };

        IronGang::InteractionPromptSelector selector;
        std::vector<IronGang::InteractionTarget> targets{
            target("sedan", "Enter the sedan", 0.0F, 3.0F, true),
            target("door", "Open the door", 10.0F, 3.0F, true),
        };

        // Out of range of everything.
        IronGang::InteractionPrompt prompt = selector.Select(IronGang::Vector3(5.0F, 0.0F, 0.0F), targets, "E");
        Require(!prompt.visible, "nothing in range must offer no prompt");
        Require(selector.GetCurrentTargetId().empty(), "nothing in range must clear the sticky target");

        // In range of the sedan, and the key the player would actually press is in the text.
        prompt = selector.Select(IronGang::Vector3(1.0F, 0.0F, 0.0F), targets, "E");
        Require(prompt.visible && prompt.targetId == "sedan", "the in-range target must be offered");
        Require(prompt.text == "[E] Enter the sedan", "the prompt must name the bound key: " + prompt.text);
        prompt = selector.Select(IronGang::Vector3(1.0F, 0.0F, 0.0F), targets, "F");
        Require(prompt.text == "[F] Enter the sedan",
                "rebinding must change the prompt without touching content: " + prompt.text);
        prompt = selector.Select(IronGang::Vector3(1.0F, 0.0F, 0.0F), targets, "");
        Require(prompt.text.find("unbound") != std::string::npos,
                "an unbound action must say so rather than show empty brackets: " + prompt.text);

        // Height must not decide: a target overhead is not nearer than one at your feet.
        selector.Clear();
        prompt = selector.Select(IronGang::Vector3(1.0F, 40.0F, 0.0F), targets, "E");
        Require(prompt.visible, "distance must be measured in the XZ plane, ignoring height");

        // Unavailable targets are skipped entirely.
        selector.Clear();
        targets[0].available = false;
        prompt = selector.Select(IronGang::Vector3(1.0F, 0.0F, 0.0F), targets, "E");
        Require(!prompt.visible, "an unavailable target must not be offered");
        targets[0].available = true;

        // The nearest wins when both are in range.
        selector.Clear();
        targets[1].position = IronGang::Vector3(4.0F, 0.0F, 0.0F);
        prompt = selector.Select(IronGang::Vector3(3.0F, 0.0F, 0.0F), targets, "E");
        Require(prompt.targetId == "door",
                "the nearer of two in-range targets must win, got " + prompt.targetId);

        // Hysteresis: having chosen "door", stepping to where "sedan" is strictly nearer must NOT
        // swap the prompt while "door" is still within its enlarged radius. Without this, standing
        // on the boundary makes the prompt flicker every frame.
        // At x=0.5 the door is 3.5 m away -- **outside** its plain 3 m radius but inside the
        // enlarged 4.05 m one -- while the sedan is only 0.5 m away. Only the enlarged radius can
        // keep the prompt on the door here, which is what makes this the case that actually tests
        // it: an earlier version of this test used x=1.9, where the door is still inside its plain
        // radius, and removing the enlargement passed.
        prompt = selector.Select(IronGang::Vector3(0.5F, 0.0F, 0.0F), targets, "E");
        Require(prompt.targetId == "door",
                "the target already being offered must keep the prompt out to its enlarged radius, "
                "got " + prompt.targetId);
        // Far enough out of the enlarged radius (3.0 * 1.35 = 4.05), the sticky target lets go.
        prompt = selector.Select(IronGang::Vector3(-0.5F, 0.0F, 0.0F), targets, "E");
        Require(prompt.targetId == "sedan",
                "once the sticky target is out of its enlarged radius the nearer one must take over, "
                "got " + prompt.targetId);

        // A sticky target that becomes unavailable must be given up immediately, not held.
        targets[0].available = false;
        prompt = selector.Select(IronGang::Vector3(-0.5F, 0.0F, 0.0F), targets, "E");
        Require(prompt.targetId != "sedan",
                "an unavailable sticky target must be dropped at once, got " + prompt.targetId);

        // Clear() drops the sticky target, so a prompt cannot resume on a stale one.
        selector.Clear();
        Require(selector.GetCurrentTargetId().empty(), "Clear() must drop the sticky target");
        Require(!selector.Select(IronGang::Vector3(0.0F, 0.0F, 0.0F), {}, "E").visible,
                "an empty target list must offer no prompt");
        Require(!selector.Select(IronGang::Vector3(0.0F, 0.0F, 0.0F),
                                 {target("zero", "Nothing", 0.0F, 0.0F, true)}, "E").visible,
                "a zero-radius target must never be offered");
    }

    void TestCameraObstructionPullsIn()
    {
        std::vector<IronGang::WorldBox> boxes;
        // A wall 4 m behind the target, 1 m thick, tall and wide enough not to be missed.
        boxes.push_back(IronGang::WorldBox{"wall", IronGang::Vector3(0.0F, 3.0F, 4.5F),
                                           IronGang::Vector3(20.0F, 6.0F, 1.0F),
                                           Microsoft::Xna::Framework::Color(255, 255, 255, 255), true});

        const IronGang::Vector3 target(0.0F, 1.5F, 0.0F);
        const IronGang::Vector3 desired(0.0F, 1.5F, 8.0F); // 8 m behind, straight through the wall

        IronGang::CameraObstruction result =
            IronGang::ResolveCameraObstruction(target, desired, boxes, 0.35F, 0.6F);
        Require(result.obstructed, "a wall between the target and the camera must be detected");
        Require(result.position.Z < 4.0F,
                "the camera must end up in front of the wall's near face (z=4.0), got " +
                    std::to_string(result.position.Z));
        Require(result.position.Z > 0.0F, "the camera must stay behind the target, not in front of it");
        // The skin: it stops short of the surface, because a camera exactly on it still clips.
        Require(std::abs(result.position.Z - (4.0F - 0.35F)) < 1e-3F,
                "the camera must stop exactly one skin width in front of the wall, got " +
                    std::to_string(result.position.Z));

        // Nothing in the way: the camera is left exactly where it was asked to be. A collision
        // system that nudges an unobstructed camera is worse than none.
        boxes[0].center = IronGang::Vector3(0.0F, 3.0F, -20.0F);
        result = IronGang::ResolveCameraObstruction(target, desired, boxes, 0.35F, 0.6F);
        Require(!result.obstructed, "a wall nowhere near the segment must not obstruct");
        Require(std::abs(result.position.Z - 8.0F) < 1e-4F,
                "an unobstructed camera must be left exactly where it was asked to be");
        Require(std::abs(result.fraction - 1.0F) < 1e-4F, "an unobstructed camera is at fraction 1");

        // Paint on the ground is not a wall -- the same rule HasLineOfSight() uses. Without this a
        // lane marking would pull the camera in every time the player stood on a road.
        boxes[0].center = IronGang::Vector3(0.0F, 3.0F, 4.5F);
        boxes[0].collidable = false;
        result = IronGang::ResolveCameraObstruction(target, desired, boxes, 0.35F, 0.6F);
        Require(!result.obstructed, "a non-collidable box must be ignored");

        // The nearest of several obstructions wins.
        boxes[0].collidable = true;
        boxes.push_back(IronGang::WorldBox{"closer", IronGang::Vector3(0.0F, 3.0F, 2.0F),
                                           IronGang::Vector3(20.0F, 6.0F, 0.5F),
                                           Microsoft::Xna::Framework::Color(255, 255, 255, 255), true});
        result = IronGang::ResolveCameraObstruction(target, desired, boxes, 0.35F, 0.6F);
        Require(result.position.Z < 2.0F,
                "the nearest obstruction must win, got z=" + std::to_string(result.position.Z));

        // The target itself inside geometry: the camera must not collapse onto the player's own
        // head, or the view becomes the inside of their model.
        std::vector<IronGang::WorldBox> enclosing;
        enclosing.push_back(IronGang::WorldBox{"inside", IronGang::Vector3(0.0F, 1.5F, 4.0F),
                                               IronGang::Vector3(40.0F, 10.0F, 40.0F),
                                               Microsoft::Xna::Framework::Color(255, 255, 255, 255), true});
        result = IronGang::ResolveCameraObstruction(target, desired, enclosing, 0.35F, 0.6F);
        Require(std::abs((result.position - target).Length() - 0.6F) < 1e-3F,
                "with the target inside geometry the camera must hold the minimum standoff rather "
                "than collapse onto the player's own head, got " +
                    std::to_string((result.position - target).Length()) + " m");

        // And the case that made the minimum a distance rather than a fraction: a wall one metre
        // behind a target on the end of a 7.5 m boom. A fractional minimum of 0.18 would put the
        // camera 1.35 m back -- through the wall it was pulled in to avoid.
        std::vector<IronGang::WorldBox> nearWall;
        nearWall.push_back(IronGang::WorldBox{"near", IronGang::Vector3(0.0F, 3.0F, 1.5F),
                                              IronGang::Vector3(20.0F, 6.0F, 1.0F),
                                              Microsoft::Xna::Framework::Color(255, 255, 255, 255), true});
        result = IronGang::ResolveCameraObstruction(target, IronGang::Vector3(0.0F, 1.5F, 7.5F),
                                                    nearWall, 0.35F, 0.6F);
        Require(result.position.Z < 1.0F,
                "a wall one metre behind the target must not be overridden by the minimum standoff, "
                "got z=" + std::to_string(result.position.Z));

        // Degenerate: target and camera at the same point.
        result = IronGang::ResolveCameraObstruction(target, target, boxes, 0.35F, 0.6F);
        Require(!result.obstructed, "a zero-length segment has nothing to pull in along");
    }

    // The same thing against the district the game actually loads, rather than a hand-made wall.
    void TestCameraObstructionAgainstRealDistrict()
    {
        const IronGang::PrototypeWorld world(IronGang::DistrictId::WarehouseBlock);
        const std::vector<IronGang::WorldBox>& boxes = world.GetBoxes();

        // The apartments block spans x in [9, 27], z in [12, 26]. Stand just outside its west face
        // facing away from it, which is exactly the situation that put the camera inside a wall.
        const IronGang::Vector3 target(8.0F, 1.25F, 19.0F);
        const IronGang::Vector3 desired(15.5F, 4.65F, 19.0F); // 7.5 m "behind", deep inside the block

        const IronGang::CameraObstruction result = IronGang::ResolveCameraObstruction(target, desired, boxes);
        Require(result.obstructed,
                "standing against the apartments with your back to them must obstruct the camera");
        Require(result.position.X < 9.0F,
                "the camera must end up outside the apartments' west face (x=9), got x=" +
                    std::to_string(result.position.X));

        // And out in the open street, nothing may move it: the road, sidewalks, lane markings and
        // the warehouse target decal are all non-collidable, and pulling the camera in on any of
        // them would break the camera everywhere the player normally walks.
        const IronGang::Vector3 streetTarget(0.0F, 1.25F, 20.0F);
        const IronGang::Vector3 streetCamera(0.0F, 4.65F, 27.5F);
        const IronGang::CameraObstruction street =
            IronGang::ResolveCameraObstruction(streetTarget, streetCamera, boxes);
        Require(!street.obstructed,
                "the spawn-point camera must be unobstructed, or the game starts with the camera "
                "shoved into the player's back");
    }

    void TestSubtitleWrapping()
    {
        using IronGang::WrapSubtitleText;

        std::vector<std::string> lines = WrapSubtitleText("No heroics.", 40);
        Require(lines.size() == 1 && lines[0] == "No heroics.",
                "a line that already fits must be returned unchanged");

        lines = WrapSubtitleText("Iron City is quiet tonight. That usually means trouble is already moving.", 30);
        Require(lines.size() >= 3, "a long line must wrap onto several lines");
        for (const std::string& line : lines)
        {
            Require(line.size() <= 30,
                    "no wrapped line may exceed the limit: \"" + line + "\" is " +
                        std::to_string(line.size()));
            Require(!line.empty(), "wrapping must never produce an empty line");
            Require(line.front() != ' ' && line.back() != ' ',
                    "wrapped lines must not start or end with a space: \"" + line + "\"");
        }
        // Nothing may be lost: rejoining the lines must reproduce the words in order.
        std::string rejoined;
        for (const std::string& line : lines)
        {
            if (!rejoined.empty())
            {
                rejoined += ' ';
            }
            rejoined += line;
        }
        Require(rejoined == "Iron City is quiet tonight. That usually means trouble is already moving.",
                "wrapping must not drop or reorder any word, got \"" + rejoined + "\"");

        // Exact fit, and one character over.
        lines = WrapSubtitleText("abcde fghij", 11);
        Require(lines.size() == 1, "a line of exactly the limit must not wrap");
        lines = WrapSubtitleText("abcde fghijk", 11);
        Require(lines.size() == 2 && lines[0] == "abcde" && lines[1] == "fghijk",
                "one character over the limit must wrap at the space");

        // A word longer than a whole line has to be hard-split, or it runs off the edge -- which is
        // the exact failure a subtitle exists to prevent.
        lines = WrapSubtitleText("short supercalifragilistic", 10);
        for (const std::string& line : lines)
        {
            Require(line.size() <= 10, "an over-long word must be split, not allowed to overflow");
        }
        std::string joined;
        for (const std::string& line : lines)
        {
            joined += line;
        }
        Require(joined == "shortsupercalifragilistic",
                "hard-splitting must not lose characters, got \"" + joined + "\"");

        Require(WrapSubtitleText("", 20).empty(), "empty text must produce no lines");
        Require(WrapSubtitleText("   ", 20).empty(), "whitespace-only text must produce no lines");
        Require(WrapSubtitleText("a  b", 20).size() == 1 && WrapSubtitleText("a  b", 20)[0] == "a b",
                "runs of spaces must collapse");
        Require(WrapSubtitleText("anything", 0).empty(), "a zero-width line must produce nothing, not loop");
    }

    void TestSubtitleLayoutStaysOnScreen()
    {
        // The real font's advance: its cell plus its own spacing. Taken from the font's own
        // constants rather than written as 9, so the two cannot drift apart silently.
        constexpr float kGlyph = static_cast<float>(IronGang::kFont8x8Advance);
        static_assert(IronGang::kFont8x8Advance == IronGang::kFont8x8GlyphSize + IronGang::kFont8x8Spacing,
                      "the advance a subtitle wraps on is the cell plus the spacing");
        const std::string longLine =
            "Iron City is quiet tonight. That usually means trouble is already moving.";

        for (const auto& size : {std::pair<float, float>(1280.0F, 720.0F),
                                 std::pair<float, float>(640.0F, 360.0F),
                                 std::pair<float, float>(1920.0F, 1080.0F),
                                 std::pair<float, float>(320.0F, 240.0F)})
        {
            const IronGang::SubtitleLayout layout =
                IronGang::ComputeSubtitleLayout("Mara", longLine, size.first, size.second, kGlyph, kGlyph);
            Require(!layout.IsEmpty(), "a subtitle must be produced at any reasonable screen size");
            Require(layout.scale >= 1.0F, "the scale must never shrink the font below its own pixels");

            // The panel must be fully on screen...
            Require(layout.panelX >= 0.0F && layout.panelX + layout.panelWidth <= size.first + 0.5F,
                    "the panel must fit horizontally at " + std::to_string(size.first) + "x" +
                        std::to_string(size.second));
            Require(layout.panelY >= 0.0F && layout.panelY + layout.panelHeight <= size.second,
                    "the panel must fit vertically");
            // ...and the text must be inside the panel, which is the bug this replaces.
            const float cell = kGlyph * layout.scale;
            for (const std::string& line : layout.lines)
            {
                const float right = layout.textX + static_cast<float>(line.size()) * cell;
                Require(right <= layout.panelX + layout.panelWidth + 0.5F,
                        "\"" + line + "\" runs past the panel it was wrapped for");
                Require(right <= size.first, "no line may run off the screen");
                // The panel is sized from the longest line, so "the text fits the panel" alone
                // cannot catch a wrap that is simply too wide -- both grow together. This is the
                // rule that actually bounds it, and a mutation widening the wrap by two characters
                // passed every other assertion here until it was added.
                Require(static_cast<float>(line.size()) * cell <=
                            size.first * IronGang::kSubtitleWidthFraction + 0.5F,
                        "\"" + line + "\" is wider than a subtitle is allowed to be at " +
                            std::to_string(size.first) + " wide");
            }
            const float bottom = layout.textY + static_cast<float>(layout.lines.size()) * layout.lineHeight;
            Require(bottom <= layout.panelY + layout.panelHeight + 0.5F,
                    "the last line must sit inside the panel");
        }

        // Degenerate inputs must produce nothing rather than a panel of nonsense.
        Require(IronGang::ComputeSubtitleLayout("Mara", "", 1280.0F, 720.0F, kGlyph, kGlyph).IsEmpty(),
                "an empty line must produce no subtitle");
        Require(IronGang::ComputeSubtitleLayout("Mara", "hello", 0.0F, 720.0F, kGlyph, kGlyph).IsEmpty(),
                "a zero-width screen must produce no subtitle");
        Require(IronGang::ComputeSubtitleLayout("Mara", "hello", 1280.0F, 720.0F, 0.0F, kGlyph).IsEmpty(),
                "a zero-width glyph must produce no subtitle, not divide by zero");

        // A short line must not sit in the middle of a full-width bar.
        const IronGang::SubtitleLayout shortLayout =
            IronGang::ComputeSubtitleLayout("Mara", "No.", 1280.0F, 720.0F, kGlyph, kGlyph);
        const IronGang::SubtitleLayout longLayout =
            IronGang::ComputeSubtitleLayout("Mara", longLine, 1280.0F, 720.0F, kGlyph, kGlyph);
        Require(shortLayout.panelWidth < longLayout.panelWidth,
                "the panel must be sized to its content, not to the screen");
    }

    // Every shipped dialogue line must lay out cleanly at the resolution the game runs at.
    void TestShippedDialogueFitsTheSubtitle()
    {
        IronGang::DialogueSystem dialogue;
        std::string error;
        Require(dialogue.LoadFromFile(std::string(IRON_GANG_SOURCE_ASSET_DIR) +
                                          "/dialogues/prologue.dialogue.json",
                                      error),
                "the shipped conversation must load: " + error);
        for (std::size_t index = 0; index < dialogue.GetLineCount(); ++index)
        {
            const IronGang::DialogueLine* line = dialogue.FindLine(dialogue.GetLineId(index));
            Require(line != nullptr, "every shipped line must resolve");
            const IronGang::SubtitleLayout layout =
                IronGang::ComputeSubtitleLayout(line->speaker, line->text, 1280.0F, 720.0F,
                                              static_cast<float>(IronGang::kFont8x8Advance),
                                              static_cast<float>(IronGang::kFont8x8Advance));
            Require(!layout.IsEmpty(), "shipped line \"" + line->id + "\" must produce a subtitle");
            Require(layout.lines.size() <= 3,
                    "shipped line \"" + line->id + "\" wraps onto " +
                        std::to_string(layout.lines.size()) +
                        " lines; a subtitle nobody can read in one glance needs rewriting, not a "
                        "taller panel");
            Require(layout.panelX + layout.panelWidth <= 1280.0F,
                    "shipped line \"" + line->id + "\" overflows the screen");
        }
    }

    void TestModelMaterialsLoadAndDefault()
    {
        IronGang::ModelMaterialTable table;
        Require(!table.Contains("warehouse"), "an unloaded table must contain nothing");
        Require(std::abs(table.GetBaseColor("warehouse").X - 1.0F) < 1e-4F,
                "an unknown model must default to white, not black -- a failed load must not blank "
                "the screen");

        std::string error;
        Require(table.LoadFromFile(std::string(IRON_GANG_SOURCE_ASSET_DIR) + "/models/model-materials.json",
                                   error),
                "the shipped model materials must load: " + error);
        Require(table.GetCount() >= 5, "every imported model needs an entry");

        // The four models IronGangGame actually imports, plus the warehouse.
        for (const char* modelId : {"warehouse", "vehicle_body", "vehicle_cabin", "vehicle_windshield",
                                    "vehicle_wheel"})
        {
            Require(table.Contains(modelId),
                    std::string("the shipped table must cover the imported model ") + modelId);
        }

        // The warehouse's own MC3 base colour, which is what stopped it rendering as a white slab.
        const IronGang::Vector3 warehouse = table.GetBaseColor("warehouse");
        Require(std::abs(warehouse.X - 0.42F) < 1e-4F && std::abs(warehouse.Y - 0.36F) < 1e-4F &&
                    std::abs(warehouse.Z - 0.31F) < 1e-4F,
                "the warehouse must carry the base colour warehouse.mc3.xml declares");
        // And the sedan's, which is a dark red, not the pale grey it used to draw as.
        const IronGang::Vector3 body = table.GetBaseColor("vehicle_body");
        Require(body.X > body.Y && body.X > body.Z && body.X < 0.6F,
                "the sedan body must be a dark red, not a neutral grey");
    }

    void TestModelMaterialsRejectUnusableData()
    {
        const std::filesystem::path path = std::filesystem::current_path() / "iron_gang_materials.json";
        std::string error;
        IronGang::ModelMaterialTable table;

        WriteTempJson(path, R"JSON({"version":9,"models":[{"modelId":"a","baseColor":[0,0,0]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "an unsupported version must be rejected");

        WriteTempJson(path, R"JSON({"models":[{"modelId":"a","baseColor":[0,0,0]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a missing version must be rejected");

        WriteTempJson(path, R"JSON({"version":1})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a missing \"models\" array must be rejected");

        WriteTempJson(path, R"JSON({"version":1,"models":[
            {"modelId":"a","baseColor":[0,0,0]},{"modelId":"a","baseColor":[1,1,1]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a duplicate model id must be rejected");

        WriteTempJson(path, R"JSON({"version":1,"models":[{"modelId":"","baseColor":[0,0,0]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "an empty model id must be rejected");

        WriteTempJson(path, R"JSON({"version":1,"models":[{"modelId":"a","baseColor":[0,0]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a two-component colour must be rejected");

        WriteTempJson(path, R"JSON({"version":1,"models":[{"modelId":"a","baseColor":[0,0,0,0]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a four-component colour must be rejected");

        // Out of range would either clip silently or make one model brighter than the sun the rest
        // of the scene is lit by.
        WriteTempJson(path, R"JSON({"version":1,"models":[{"modelId":"a","baseColor":[1.4,0,0]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a component above 1 must be rejected");
        WriteTempJson(path, R"JSON({"version":1,"models":[{"modelId":"a","baseColor":[-0.1,0,0]}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a negative component must be rejected");

        WriteTempJson(path, R"JSON({"version":1,"models":[{"modelId":"a"}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "a missing baseColor must be rejected");

        WriteTempJson(path, R"JSON({"version":1,"finish":"matte","models":[]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "an unknown top-level field must be rejected");

        WriteTempJson(path, R"JSON({"version":1,"models":[{"modelId":"a","baseColor":[0,0,0],"gloss":1}]})JSON");
        Require(!table.LoadFromFile(path.string(), error), "an unknown per-model field must be rejected");

        Require(!table.LoadFromFile(path.string() + ".missing", error),
                "a missing file must be rejected, not crash");

        // A rejected file must leave the previous contents intact rather than half-applied.
        Require(table.GetCount() == 0, "a table that never loaded successfully must stay empty");

        std::filesystem::remove(path);
    }

    void TestPedestrianAnimationSelection()
    {
        using IronGang::PedestrianAnimation;
        using IronGang::SelectPedestrianAnimation;

        Require(SelectPedestrianAnimation(false, false, false) == PedestrianAnimation::Idle,
                "standing still must be Idle");
        Require(SelectPedestrianAnimation(true, false, false) == PedestrianAnimation::Walk,
                "walking must be Walk");
        Require(SelectPedestrianAnimation(false, true, false) == PedestrianAnimation::Turn,
                "pivoting on the spot must be Turn, not Idle");
        // Fleeing outranks everything: someone running from a car is not idling, and is not
        // politely turning on the spot either.
        Require(SelectPedestrianAnimation(false, true, true) == PedestrianAnimation::Walk,
                "fleeing must outrank turning");
        Require(SelectPedestrianAnimation(false, false, true) == PedestrianAnimation::Walk,
                "fleeing must outrank standing still");

        Require(std::string(IronGang::PedestrianAnimationClipName(PedestrianAnimation::Turn)) == "Turn",
                "Turn must ask for the Turn clip");
        Require(std::string(IronGang::PedestrianAnimationClipName(PedestrianAnimation::Idle)) == "Idle",
                "Idle must ask for the Idle clip");
        // assets/generated is not committed, so a checkout whose asset build predates the Turn
        // clip must still animate rather than freeze on whatever pose it last held.
        Require(std::string(IronGang::PedestrianAnimationFallbackClipName(PedestrianAnimation::Turn)) == "Walk",
                "a missing Turn clip must fall back to Walk, where the legs at least move");
        Require(std::string(IronGang::PedestrianAnimationFallbackClipName(PedestrianAnimation::Idle)) == "Idle",
                "a state that cannot be missing must fall back to itself");
    }

    // plan_20 IG-20-003: reversing at the end of a pavement used to be a 180-degree snap in a
    // single frame. It is now a bounded turn the pedestrian stands still for.
    void TestPedestrianTurnsInPlaceInsteadOfSnapping()
    {
        IronGang::WaypointPath path;
        // A short two-point pavement, so the pedestrian reaches the far end and must reverse.
        path.points = {IronGang::Vector3(0.0F, 0.9F, 0.0F), IronGang::Vector3(0.0F, 0.9F, 4.0F)};
        path.loop = true;

        IronGang::Pedestrian pedestrian;
        pedestrian.Reset(path, 0, 1.6F);

        constexpr float kStep = 1.0F / 60.0F;
        const IronGang::Vector3 noThreat;

        // Walk until the pedestrian starts turning. It must not be turning while walking down the
        // straight, and it must reach the turn within a few seconds.
        int updates = 0;
        while (!pedestrian.IsTurningInPlace() && updates < 600)
        {
            pedestrian.Update(kStep, false, noThreat);
            ++updates;
        }
        Require(pedestrian.IsTurningInPlace(),
                "the pedestrian must reach the end of the pavement and start turning");
        Require(!pedestrian.IsWalking(),
                "a pedestrian pivoting on the spot must not also report walking, or the animation "
                "slides a walk cycle sideways");

        // The turn must take real time and stay within the rate limit, and the pedestrian must not
        // travel while doing it.
        const float startYaw = pedestrian.GetYaw();
        int turningUpdates = 0;
        while (pedestrian.IsTurningInPlace() && turningUpdates < 600)
        {
            const float before = pedestrian.GetYaw();
            const IronGang::Vector3 positionBefore = pedestrian.GetPathPosition();
            pedestrian.Update(kStep, false, noThreat);
            ++turningUpdates;
            // The update that *ends* the turn legitimately walks; every update still turning
            // afterwards must not have moved at all.
            if (pedestrian.IsTurningInPlace())
            {
                Require((pedestrian.GetPathPosition() - positionBefore).Length() < 1e-6F,
                        "a pedestrian pivoting on the spot must not travel while turning");
            }
            float delta = pedestrian.GetYaw() - before;
            while (delta > std::numbers::pi_v<float>) { delta -= 2.0F * std::numbers::pi_v<float>; }
            while (delta <= -std::numbers::pi_v<float>) { delta += 2.0F * std::numbers::pi_v<float>; }
            Require(std::abs(delta) <= IronGang::kPedestrianTurnRate * kStep + 1e-4F,
                    "the heading must never move faster than kPedestrianTurnRate");
            Require(std::abs(pedestrian.GetTurnRate()) <= IronGang::kPedestrianTurnRate + 1e-3F,
                    "the reported turn rate must respect the same limit");
        }
        Require(!pedestrian.IsTurningInPlace(), "the turn must finish");
        // A 180-degree reversal minus the in-place threshold, at the turn rate: about
        // (pi - 0.6) / 3.5 = 0.726 s, i.e. roughly 44 updates. Anything near 1 is the old snap.
        Require(turningUpdates > 30,
                "the reversal must take many updates, not snap in one (took " +
                    std::to_string(turningUpdates) + ")");
        float turned = pedestrian.GetYaw() - startYaw;
        while (turned > std::numbers::pi_v<float>) { turned -= 2.0F * std::numbers::pi_v<float>; }
        while (turned <= -std::numbers::pi_v<float>) { turned += 2.0F * std::numbers::pi_v<float>; }
        Require(std::abs(turned) > 1.5F,
                "the pedestrian must actually have turned most of the way round");

        // And it walks again afterwards, back the way it came.
        const float zBeforeWalkingBack = pedestrian.GetPathPosition().Z;
        for (int i = 0; i < 30; ++i)
        {
            pedestrian.Update(kStep, false, noThreat);
        }
        Require(pedestrian.IsWalking(), "the pedestrian must resume walking once the turn is done");
        Require(pedestrian.GetPathPosition().Z < zBeforeWalkingBack,
                "having turned round, it must walk back the way it came");
    }

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
        const IronGang::PoliceUpdateWorkload onFootWorkload = police.Update(1.0F, IronGang::PoliceObservation{false, 120.0F, false}, origin, {IronGang::Vector3(1.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(onFootWorkload.witnessChecks == 0 && onFootWorkload.patrolUpdates == 0,
                "police workload must count only loops that actually execute");
        Require(police.GetState() == IronGang::PoliceState::Clear,
                "an offense while not driving must never be witnessed");

        // Driving fast, but the only witness is far outside the witness radius (15 units).
        const IronGang::PoliceUpdateWorkload farWitnessWorkload = police.Update(1.0F, IronGang::PoliceObservation{true, 120.0F, false}, origin, {IronGang::Vector3(1000.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(farWitnessWorkload.witnessChecks == 1 && farWitnessWorkload.patrolUpdates == 0,
                "police workload must count each tested witness even when no offense is seen");
        Require(police.GetState() == IronGang::PoliceState::Clear,
                "a witness outside the witness radius must not trigger a chase");

        // Driving over the speed threshold with a witness inside the radius: must dispatch.
        police.Update(1.0F, IronGang::PoliceObservation{true, 100.0F, false}, origin, {IronGang::Vector3(5.0F, 0.0F, 0.0F)}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Dispatched,
                "speeding witnessed within radius must dispatch a patrol car");
        Require(police.GetActivePatrolCount() == 1, "dispatch must spawn exactly one patrol car");
        Require(std::abs(police.GetPatrolPosition(0).X - spawnPosition.X) < 1e-4F,
                "the dispatched patrol car must appear at the given spawn position");

        // Dispatched has a fixed delay (2s) before patrol cars actually start moving/chasing.
        police.Update(1.0F, IronGang::PoliceObservation{true, 0.0F, false}, origin, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Dispatched,
                "the dispatch delay must not elapse after only 1 of its 2 seconds");
        police.Update(1.5F, IronGang::PoliceObservation{true, 0.0F, false}, origin, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Chasing,
                "the dispatch delay must elapse and start the chase");

        // One chase tick at normal speed: hand-verified pursuit math (patrol starts 20 units from
        // the player at (0,0,0); closes 9 units/s for 1s).
        police.Update(1.0F, IronGang::PoliceObservation{true, 0.0F, false}, origin, {}, spawnPosition);
        Require(std::abs(police.GetPatrolPosition(0).X - 11.0F) < 1e-4F,
                "the patrol car must close the distance to the player at its own patrol speed");

        // A single long step (19s more, 20s total chase time) must both escalate (a second patrol
        // car appears) and let both patrol cars close in (clamped so neither overshoots the
        // player's position).
        const IronGang::PoliceUpdateWorkload escalationWorkload =
            police.Update(19.0F, IronGang::PoliceObservation{true, 0.0F, false}, origin, {}, spawnPosition);
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
        police.Update(1.0F, IronGang::PoliceObservation{true, 0.0F, false}, farAway, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Chasing,
                "the resolve distance must be sustained, not trigger instantly");
        police.Update(1.0F, IronGang::PoliceObservation{true, 0.0F, false}, farAway, {}, spawnPosition);
        Require(police.GetState() == IronGang::PoliceState::Chasing,
                "resolve must require the full sustain duration (2 of 3 seconds so far)");
        police.Update(1.0F, IronGang::PoliceObservation{true, 0.0F, false}, farAway, {}, spawnPosition);
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
        TestSaveRoundTripsRandomSnapshots();
        TestCheckpointWorldSurvivesSaveLoad();
        TestRandomSourceIsDeterministicAndUniform();
        TestCampaignGraphUnlocksAndRejectsCycles();
        TestCampaignProgressSurvivesSaveLoad();
        TestCountrysideMissionRunsAndFailsOnAWreck();
        TestInputBindingsDetectConflictsWithinContexts();
        TestUserSettingsRoundTripAndFallBack();
        TestMenuModelSkipsDisabledAndWraps();
        TestInputContextResolvesByPrecedence();
        TestLocomotionAcceleratesAndDecelerates();
        TestVehicleDamageDistinguishesCrashesFromBraking();
        TestJsonDataFileIsBoundedBeforeParsing();
        TestVehicleConfigLoadsValidatesAndFallsBack();
        TestWitnessesCannotSeeThroughWalls();
        TestRunningARedLightIsAWitnessedOffence();
        TestTrafficSignalCyclesAndOpposesItself();
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
        TestInputScriptRecordsSparselyAndReplaysExactly();
        TestInputScriptRejectsUnusableRepros();
        TestCommittedPrologueReproScriptIsUsable();
        TestScreenshotSummaryDescribesAFrame();
        TestScreenshotSummaryDigestAndSidecar();
        TestCutsceneDialogueTrackSelectsLinesOverTime();
        TestCutsceneRejectsStaleDialogueReference();
        TestShippedCutsceneCuesResolveAgainstShippedDialogue();
        TestCutsceneValidationRejectsMalformedData();
        TestDialogueFallback();
        TestDialogueLinesCarryStableIds();
        TestWaypointPathAdvancesAndWraps();
        TestTrafficVehicleAcceleratesAndBrakes();
        TestPedestrianCrossingRespectsTheSignal();
        TestSidewalkGraphReplacesTheHandAuthoredLayoutExactly();
        TestSidewalkGraphRejectsUnusableData();
        TestRoadGraphReplacesTheHandAuthoredLayoutExactly();
        TestRoadGraphRejectsUnusableData();
        TestSpatialAudioAttenuationAndPan();
        TestAudioBusGraphMixing();
        TestDialogueDucking();
        TestInteractionPromptSelection();
        TestCameraObstructionPullsIn();
        TestCameraObstructionAgainstRealDistrict();
        TestSubtitleWrapping();
        TestSubtitleLayoutStaysOnScreen();
        TestShippedDialogueFitsTheSubtitle();
        TestModelMaterialsLoadAndDefault();
        TestModelMaterialsRejectUnusableData();
        TestPedestrianAnimationSelection();
        TestPedestrianTurnsInPlaceInsteadOfSnapping();
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
