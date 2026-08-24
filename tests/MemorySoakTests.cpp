#include "IronGang/Core/PerformanceProfiler.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"
#include "IronGang/Persistence/SaveGame.hpp"
#include "IronGang/Physics/PhysicsWorld.hpp"
#include "IronGang/World/DistrictManager.hpp"
#include "IronGang/World/PrototypeWorld.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    constexpr std::size_t kDefaultCycles = 200;
    constexpr std::size_t kMinimumCycles = 40;
    constexpr std::size_t kMaximumCycles = 100000;
    constexpr std::uint64_t kAllowedCurrentRssGrowthBytes = 8ULL * 1024ULL * 1024ULL;
    constexpr std::uint64_t kAllowedPeakRssGrowthBytes = 16ULL * 1024ULL * 1024ULL;
    constexpr double kAllowedRssSlopeBytesPerCycle = 32.0 * 1024.0;

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    struct TemporarySave final
    {
        std::filesystem::path path;

        ~TemporarySave()
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    };

    std::size_t ParseCycles(int argc, char** argv)
    {
        std::size_t cycles = kDefaultCycles;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--cycles" && index + 1 < argc)
            {
                const std::string value = argv[++index];
                std::size_t parsedCharacters = 0;
                unsigned long long parsedCycles = 0;
                try
                {
                    parsedCycles = std::stoull(value, &parsedCharacters);
                }
                catch (const std::exception&)
                {
                    throw std::runtime_error("--cycles must be an integer");
                }
                if (parsedCharacters != value.size())
                {
                    throw std::runtime_error("--cycles must be an integer");
                }
                if (parsedCycles < kMinimumCycles || parsedCycles > kMaximumCycles)
                {
                    throw std::runtime_error("--cycles must be between 40 and 100000");
                }
                cycles = static_cast<std::size_t>(parsedCycles);
            }
            else
            {
                throw std::runtime_error("usage: iron_gang_memory_soak_tests [--cycles N]");
            }
        }
        return cycles;
    }

    void AdvanceMissionToDriving(IronGang::PrototypeMission& mission,
                                 const IronGang::PrototypeWorld& world)
    {
        mission.Reset();
        mission.Update(true, world.GetPlayerSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), false, world.GetWarehouseGoal());
        mission.Update(true, world.GetVehicleSpawn(), world.GetVehicleSpawn(), true, world.GetWarehouseGoal());
        Require(mission.GetState() == IronGang::PrototypeMissionState::DriveToWarehouse,
                "mission did not reach the save checkpoint");
    }

    void SaveLoadAndCompleteMission(IronGang::PrototypeMission& mission,
                                    const IronGang::PrototypeWorld& world,
                                    const std::filesystem::path& savePath)
    {
        AdvanceMissionToDriving(mission, world);
        IronGang::SaveSnapshot snapshot;
        snapshot.missionState = mission.GetState();
        snapshot.playerPosition = world.GetVehicleSpawn();
        snapshot.vehiclePosition = world.GetVehicleSpawn();
        snapshot.playerDriving = true;
        snapshot.districtId = world.GetId();

        std::string error;
        Require(IronGang::SaveGame::Write(savePath.string(), snapshot, error),
                "save write failed during soak: " + error);
        const std::optional<IronGang::SaveSnapshot> loaded = IronGang::SaveGame::Read(savePath.string(), error);
        Require(loaded.has_value(), "save read failed during soak: " + error);
        Require(loaded->missionState == IronGang::PrototypeMissionState::DriveToWarehouse &&
                    loaded->districtId == IronGang::DistrictId::WarehouseBlock && loaded->playerDriving,
                "save/load changed mission, district, or driving state during soak");

        IronGang::PrototypeMission resumed;
        resumed.SetState(loaded->missionState);
        resumed.Update(true,
                       world.GetWarehouseGoal().bounds.center,
                       world.GetWarehouseGoal().bounds.center,
                       true,
                       world.GetWarehouseGoal());
        Require(resumed.IsCompleted(), "loaded mission could not complete during soak");

        mission.Reset();
        Require(mission.GetState() == IronGang::PrototypeMissionState::Introduction,
                "mission reset failed during soak");
    }

    void CompleteTransition(IronGang::DistrictManager& districts,
                            IronGang::Physics::PhysicsWorld& physics,
                            IronGang::DistrictId expectedDistrict)
    {
        districts.RequestTransition(physics);
        districts.Update(1.0F);
        physics.Step(1.0F / 60.0F);
        Require(!districts.IsTransitioning() && districts.ConsumeArrival(),
                "district transition did not complete during soak");
        Require(!districts.ConsumeArrival(), "district arrival fired twice during soak");
        Require(districts.GetWorld().GetId() == expectedDistrict,
                "district transition reached the wrong target during soak");
    }

    double LinearSlopeBytesPerCycle(const std::vector<std::pair<std::size_t, std::uint64_t>>& samples)
    {
        if (samples.size() < 2)
        {
            return 0.0;
        }
        double sumX = 0.0;
        double sumY = 0.0;
        for (const auto& [cycle, rss] : samples)
        {
            sumX += static_cast<double>(cycle);
            sumY += static_cast<double>(rss);
        }
        const double meanX = sumX / static_cast<double>(samples.size());
        const double meanY = sumY / static_cast<double>(samples.size());
        double numerator = 0.0;
        double denominator = 0.0;
        for (const auto& [cycle, rss] : samples)
        {
            const double x = static_cast<double>(cycle) - meanX;
            numerator += x * (static_cast<double>(rss) - meanY);
            denominator += x * x;
        }
        return denominator > 0.0 ? numerator / denominator : 0.0;
    }

    void WriteSignedDelta(std::uint64_t before, std::uint64_t after)
    {
        if (after < before)
        {
            std::cout << '-' << (before - after);
        }
        else
        {
            std::cout << (after - before);
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        const std::size_t cycles = ParseCycles(argc, argv);
        const std::size_t warmupCycles = std::min<std::size_t>(20, cycles / 4);
        const std::size_t checkpointEvery = std::max<std::size_t>(1, (cycles - warmupCycles) / 10);
        const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
        TemporarySave temporarySave{
            std::filesystem::temp_directory_path() /
            ("iron_gang_memory_soak_" + std::to_string(uniqueSuffix) + ".save")};

        IronGang::Physics::PhysicsWorld physics;
        IronGang::DistrictManager districts;
        districts.Initialize(physics);
        const std::size_t warehouseBodyCount = physics.GetBodyCount();
        Require(warehouseBodyCount > 0, "warehouse district must create static physics bodies");
        IronGang::PrototypeWorld missionWorld(IronGang::DistrictId::WarehouseBlock);
        IronGang::PrototypeMission mission;

        std::uint64_t baselineCurrentRss = 0;
        std::uint64_t baselinePeakRss = 0;
        std::vector<std::pair<std::size_t, std::uint64_t>> currentRssCheckpoints;
        for (std::size_t cycle = 1; cycle <= cycles; ++cycle)
        {
            SaveLoadAndCompleteMission(mission, missionWorld, temporarySave.path);
            CompleteTransition(districts, physics, IronGang::DistrictId::Countryside);
            CompleteTransition(districts, physics, IronGang::DistrictId::WarehouseBlock);
            Require(physics.GetBodyCount() == warehouseBodyCount,
                    "district round trip leaked or lost static physics bodies");

            if (cycle == warmupCycles)
            {
                baselineCurrentRss = IronGang::PerformanceProfiler::ReadCurrentResidentBytes();
                baselinePeakRss = IronGang::PerformanceProfiler::ReadPeakResidentBytes();
            }
            if (cycle > warmupCycles &&
                ((cycle - warmupCycles) % checkpointEvery == 0 || cycle == cycles))
            {
                const std::uint64_t current = IronGang::PerformanceProfiler::ReadCurrentResidentBytes();
                if (current > 0)
                {
                    currentRssCheckpoints.emplace_back(cycle, current);
                }
            }
        }

        const std::uint64_t finalCurrentRss = IronGang::PerformanceProfiler::ReadCurrentResidentBytes();
        const std::uint64_t finalPeakRss = IronGang::PerformanceProfiler::ReadPeakResidentBytes();
        const bool rssKnown = baselineCurrentRss > 0 && baselinePeakRss > 0 && finalCurrentRss > 0 &&
                              finalPeakRss > 0 && !currentRssCheckpoints.empty();
        const double slope = rssKnown ? LinearSlopeBytesPerCycle(currentRssCheckpoints) : 0.0;

        if (rssKnown)
        {
            Require(finalCurrentRss <= baselineCurrentRss + kAllowedCurrentRssGrowthBytes,
                    "current RSS grew beyond the bounded soak allowance");
            Require(finalPeakRss <= baselinePeakRss + kAllowedPeakRssGrowthBytes,
                    "peak RSS grew beyond the bounded soak allowance");
            Require(slope <= kAllowedRssSlopeBytesPerCycle,
                    "current RSS trend exceeds the bounded soak slope allowance");
        }

        std::cout << "{\"cycles\":" << cycles << ",\"warmup_cycles\":" << warmupCycles
                  << ",\"checkpoint_every\":" << checkpointEvery
                  << ",\"checkpoints\":" << currentRssCheckpoints.size()
                  << ",\"rss_known\":" << (rssKnown ? "true" : "false")
                  << ",\"baseline_current_rss_bytes\":" << baselineCurrentRss
                  << ",\"final_current_rss_bytes\":" << finalCurrentRss
                  << ",\"current_rss_delta_bytes\":";
        WriteSignedDelta(baselineCurrentRss, finalCurrentRss);
        std::cout << ",\"baseline_peak_rss_bytes\":" << baselinePeakRss
                  << ",\"final_peak_rss_bytes\":" << finalPeakRss
                  << ",\"peak_rss_delta_bytes\":";
        WriteSignedDelta(baselinePeakRss, finalPeakRss);
        std::cout << ",\"rss_slope_bytes_per_cycle\":" << std::llround(slope)
                  << ",\"allowed_current_growth_bytes\":" << kAllowedCurrentRssGrowthBytes
                  << ",\"allowed_peak_growth_bytes\":" << kAllowedPeakRssGrowthBytes
                  << ",\"allowed_slope_bytes_per_cycle\":"
                  << static_cast<std::uint64_t>(kAllowedRssSlopeBytesPerCycle)
                  << ",\"warehouse_body_count\":" << warehouseBodyCount << "}\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "memory-soak: " << exception.what() << '\n';
        return 1;
    }
}
