#include "IronShadows/Dialogue/DialogueSystem.hpp"
#include "IronShadows/Gameplay/PlayerController.hpp"
#include "IronShadows/Gameplay/VehicleController.hpp"
#include "IronShadows/Missions/PrototypeMission.hpp"
#include "IronShadows/Persistence/SaveGame.hpp"
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
        IronShadows::VehicleController vehicle;
        vehicle.Reset(world.GetVehicleSpawn(), world.GetVehicleSpawnYaw());
        const IronShadows::Vector3 before = vehicle.GetPosition();
        IronShadows::VehicleInput input;
        input.throttle = 1.0F;
        for (int i = 0; i < 120; ++i)
        {
            vehicle.Update(1.0F / 60.0F, input, world);
        }
        Require(vehicle.GetSpeed() > 1.0F, "vehicle must accelerate");
        Require(IronShadows::DistanceSquaredXZ(before, vehicle.GetPosition()) > 1.0F,
                "vehicle must move from spawn");
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

        std::string error;
        Require(IronShadows::SaveGame::Write(path.string(), source, error), "save write failed: " + error);
        const auto loaded = IronShadows::SaveGame::Read(path.string(), error);
        Require(loaded.has_value(), "save read failed: " + error);
        Require(loaded->missionState == source.missionState, "mission state round-trip failed");
        Require(std::abs(loaded->vehicleSpeed - source.vehicleSpeed) < 0.001F,
                "vehicle speed round-trip failed");
        Require(loaded->playerDriving, "driving flag round-trip failed");
        std::filesystem::remove(path);
    }
}

int main()
{
    try
    {
        TestWorldCollision();
        TestVehicleMotion();
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
