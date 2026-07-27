#include "IronShadows/Dialogue/DialogueSystem.hpp"
#include "IronShadows/Gameplay/PlayerController.hpp"
#include "IronShadows/Gameplay/VehicleController.hpp"
#include "IronShadows/Missions/PrototypeMission.hpp"
#include "IronShadows/Persistence/SaveGame.hpp"
#include "IronShadows/Physics/PhysicsWorld.hpp"
#include "IronShadows/World/DistrictManager.hpp"
#include "IronShadows/World/PrototypeWorld.hpp"

#include <cmath>
#include <filesystem>
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
