#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "LevelDefinition.hpp"
#include "World.hpp"
#include "CampaignProgress.hpp"
#include "Campaign.hpp"
#include "ExplorationMap.hpp"
#include "RunSave.hpp"
#include "RunRules.hpp"

namespace
{
    using DistanceGrid = std::vector<std::vector<int>>;

    DistanceGrid BuildDistances(
        const std::vector<std::string>& rows,
        int startX,
        int startZ)
    {
        DistanceGrid distances(rows.size(), std::vector<int>(rows.front().size(), -1));
        std::queue<std::pair<int, int>> frontier;
        frontier.emplace(startX, startZ);
        distances[static_cast<std::size_t>(startZ)][static_cast<std::size_t>(startX)] = 0;

        while (!frontier.empty())
        {
            const auto [x, z] = frontier.front();
            frontier.pop();
            constexpr std::array<std::pair<int, int>, 4> Directions = {
                std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};
            for (const auto [dx, dz] : Directions)
            {
                const int nextX = x + dx;
                const int nextZ = z + dz;
                if (nextX < 0 || nextZ < 0 ||
                    nextZ >= static_cast<int>(rows.size()) ||
                    nextX >= static_cast<int>(rows[static_cast<std::size_t>(nextZ)].size()) ||
                    distances[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] >= 0 ||
                    rows[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] == '#' ||
                    rows[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] == 'Y')
                    continue;

                distances[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] =
                    distances[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] + 1;
                frontier.emplace(nextX, nextZ);
            }
        }

        return distances;
    }

    void Expect(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void ExpectParseFailure(std::string_view text, std::string_view expectedMessage)
    {
        try
        {
            (void)WolfCna::LevelDefinition::Parse(text, "test.level");
            Expect(false, "invalid level was accepted");
        }
        catch (const std::runtime_error& error)
        {
            Expect(
                std::string_view(error.what()).find(expectedMessage) != std::string_view::npos,
                "level error did not explain the cause");
        }
    }

    void ExpectCampaignLayout(
        const WolfCna::LevelDefinition& level,
        std::string_view name)
    {
        const auto& rows = level.Rows();
        Expect(rows.size() == 64, std::string(name) + " has 64 rows");
        for (const std::string& row : rows)
            Expect(row.size() == 64, std::string(name) + " has 64 columns");

        const DistanceGrid startDistances =
            BuildDistances(rows, level.PlayerStartX(), level.PlayerStartZ());
        int reachable = 0;
        for (const auto& distanceRow : startDistances)
        {
            reachable += static_cast<int>(std::count_if(
                distanceRow.begin(),
                distanceRow.end(),
                [](int distance) { return distance >= 0; }));
        }

        int walkable = 0;
        int exits = 0;
        int relays = 0;
        int terminals = 0;
        int plants = 0;
        int tables = 0;
        int healthPickups = 0;
        int ammoPickups = 0;
        int repeaterPickups = 0;
        int heavyWeaponPickups = 0;
        int guards = 0;
        int hounds = 0;
        int rapidTroopers = 0;
        int heavyUnits = 0;
        int patrolMarkers = 0;
        int ambushEnemies = 0;
        std::pair<int, int> exitPosition{-1, -1};
        std::pair<int, int> relayPosition{-1, -1};
        std::pair<int, int> terminalPosition{-1, -1};
        std::vector<int> healthDistances;
        for (int z = 0; z < static_cast<int>(rows.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(rows[static_cast<std::size_t>(z)].size()); ++x)
            {
                const char symbol = rows[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
                if (symbol != '#' && symbol != 'Y')
                    ++walkable;
                if (symbol == 'E')
                {
                    ++exits;
                    exitPosition = {x, z};
                }
                else if (symbol == 'O')
                {
                    ++relays;
                    relayPosition = {x, z};
                }
                else if (symbol == 'M')
                {
                    ++terminals;
                    terminalPosition = {x, z};
                }
                else if (symbol == 'I')
                    ++plants;
                else if (symbol == 'Y')
                    ++tables;
                else if (symbol == 'H')
                {
                    ++healthPickups;
                    healthDistances.push_back(
                        startDistances[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)]);
                }
                else if (symbol == 'A')
                    ++ammoPickups;
                else if (symbol == 'W')
                    ++repeaterPickups;
                else if (symbol == 'V')
                    ++heavyWeaponPickups;
                else if (symbol == '^' || symbol == '>' || symbol == 'v' || symbol == '<')
                    ++patrolMarkers;
                else if (symbol == 'G' || symbol == 'g')
                {
                    ++guards;
                    ambushEnemies += symbol == 'g' ? 1 : 0;
                }
                else if (symbol == 'K' || symbol == 'k')
                {
                    ++hounds;
                    ambushEnemies += symbol == 'k' ? 1 : 0;
                }
                else if (symbol == 'F' || symbol == 'f')
                {
                    ++rapidTroopers;
                    ambushEnemies += symbol == 'f' ? 1 : 0;
                }
                else if (symbol == 'U' || symbol == 'u')
                {
                    ++heavyUnits;
                    ambushEnemies += symbol == 'u' ? 1 : 0;
                }
            }
        }

        int elevatorApproaches = 0;
        constexpr std::array<std::pair<int, int>, 4> Directions = {
            std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};
        for (int z = 0; z < 64; ++z)
        {
            for (int x = 0; x < 64; ++x)
            {
                if (rows[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != 'E')
                    continue;
                for (const auto [dx, dz] : Directions)
                {
                    const int nextX = x + dx;
                    const int nextZ = z + dz;
                    if (nextX >= 0 && nextZ >= 0 && nextX < 64 && nextZ < 64 &&
                        rows[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] != '#')
                        ++elevatorApproaches;
                }
            }
        }
        Expect(walkable >= 1500, std::string(name) + " uses a substantial part of its footprint");
        Expect(reachable == walkable, std::string(name) + " has no disconnected rooms");
        Expect(exits == 1, std::string(name) + " has one exit");
        Expect(elevatorApproaches == 1, std::string(name) + " exit is a three-sided elevator cabin");
        Expect(relays == 1, std::string(name) + " has one power relay");
        Expect(terminals == 1, std::string(name) + " has one terminal");
        Expect(plants == 3, std::string(name) + " has three sector plants");
        Expect(tables == 2, std::string(name) + " has two polygonal tables");
        Expect(patrolMarkers >= 1, std::string(name) + " has an authored patrol route marker");
        Expect(ambushEnemies >= 1, std::string(name) + " has an authored ambush encounter");
        Expect(healthPickups >= 2, std::string(name) + " has recovery beyond one health kit");
        Expect(
            std::any_of(
                healthDistances.begin(),
                healthDistances.end(),
                [](int distance) { return distance >= 0 && distance <= 15; }),
            std::string(name) + " offers early recovery");
        Expect(
            std::any_of(
                healthDistances.begin(),
                healthDistances.end(),
                [](int distance) { return distance >= 50; }),
            std::string(name) + " preserves recovery for the later route");

        const DistanceGrid relayDistances =
            BuildDistances(rows, relayPosition.first, relayPosition.second);
        const DistanceGrid terminalDistances =
            BuildDistances(rows, terminalPosition.first, terminalPosition.second);
        const auto distanceAt = [](const DistanceGrid& distances, const std::pair<int, int>& position)
        {
            return distances[static_cast<std::size_t>(position.second)]
                [static_cast<std::size_t>(position.first)];
        };
        const int relayFirstRoute =
            distanceAt(startDistances, relayPosition) +
            distanceAt(relayDistances, terminalPosition) +
            distanceAt(terminalDistances, exitPosition);
        const int terminalFirstRoute =
            distanceAt(startDistances, terminalPosition) +
            distanceAt(terminalDistances, relayPosition) +
            distanceAt(relayDistances, exitPosition);
        const int shortestObjectiveRoute = std::min(relayFirstRoute, terminalFirstRoute);
        Expect(
            shortestObjectiveRoute >= 90 && shortestObjectiveRoute <= 130,
            std::string(name) + " has a substantial but bounded objective route");

        const int guaranteedAmmo =
            ammoPickups * 6 + repeaterPickups * 6 + heavyWeaponPickups * 10 +
            guards * 3 + rapidTroopers * 5 + heavyUnits * 8;
        const int hitsToClear = guards * 3 + hounds * 2 + rapidTroopers * 4 + heavyUnits * 8;
        Expect(
            guaranteedAmmo >= hitsToClear,
            std::string(name) + " has enough guaranteed ammunition for a full clear");
    }
}

int main()
{
    constexpr WolfCna::LifeLossResult restartedLife = WolfCna::ResolveLifeLoss(
        3,
        42500,
        80000,
        1200,
        40000);
    Expect(
        restartedLife.remainingLives == 2 && restartedLife.restartSector &&
            restartedLife.score == 1200 && restartedLife.nextExtraLifeScore == 40000,
        "life loss rolls score and the extra-life threshold back to sector entry");
    constexpr WolfCna::LifeLossResult finalLife = WolfCna::ResolveLifeLoss(
        1,
        42500,
        80000,
        1200,
        40000);
    Expect(
        finalLife.remainingLives == 0 && !finalLife.restartSector &&
            finalLife.score == 42500 && finalLife.nextExtraLifeScore == 80000,
        "final life loss enters game over without scheduling a sector restart");

    constexpr WolfCna::DifficultyProfile scoutProfile =
        WolfCna::GetDifficultyProfile(WolfCna::Difficulty::Scout);
    constexpr WolfCna::DifficultyProfile operativeProfile =
        WolfCna::GetDifficultyProfile(WolfCna::Difficulty::Operative);
    constexpr WolfCna::DifficultyProfile veteranProfile =
        WolfCna::GetDifficultyProfile(WolfCna::Difficulty::Veteran);
    Expect(
        scoutProfile.incomingDamageMultiplier < operativeProfile.incomingDamageMultiplier &&
            operativeProfile.incomingDamageMultiplier < veteranProfile.incomingDamageMultiplier,
        "incoming damage increases monotonically with difficulty");
    Expect(
        scoutProfile.enemyHealthMultiplier < operativeProfile.enemyHealthMultiplier &&
            operativeProfile.enemyHealthMultiplier < veteranProfile.enemyHealthMultiplier,
        "enemy health increases monotonically with difficulty");
    Expect(
        scoutProfile.enemySpeedMultiplier < operativeProfile.enemySpeedMultiplier &&
            operativeProfile.enemySpeedMultiplier < veteranProfile.enemySpeedMultiplier,
        "enemy movement speed increases monotonically with difficulty");
    Expect(
        scoutProfile.enemyAttackIntervalMultiplier > operativeProfile.enemyAttackIntervalMultiplier &&
            operativeProfile.enemyAttackIntervalMultiplier > veteranProfile.enemyAttackIntervalMultiplier,
        "enemy firing intervals decrease monotonically with difficulty");
    Expect(
        scoutProfile.ammunitionMultiplier > operativeProfile.ammunitionMultiplier &&
            operativeProfile.ammunitionMultiplier > veteranProfile.ammunitionMultiplier &&
            scoutProfile.startingAmmunition > operativeProfile.startingAmmunition &&
            operativeProfile.startingAmmunition > veteranProfile.startingAmmunition,
        "available ammunition decreases monotonically with difficulty");

    const WolfCna::CampaignProfile legacyProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-1\n1\n", 3);
    Expect(
        legacyProfile.highestUnlocked == 1 && legacyProfile.soundVolume == 4 &&
            legacyProfile.difficulty == 1 && legacyProfile.fieldOfView == 72,
        "legacy campaign progress retains default settings");
    const WolfCna::CampaignProfile booleanSoundProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-2\n1\n0\n2\n", 3);
    Expect(
        booleanSoundProfile.highestUnlocked == 1 && booleanSoundProfile.soundVolume == 0 &&
            booleanSoundProfile.difficulty == 2 && booleanSoundProfile.fieldOfView == 72,
        "version two sound toggle migrates to volume");
    const WolfCna::CampaignProfile volumeProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-3\n1\n2\n0\n", 3);
    Expect(
        volumeProfile.highestUnlocked == 1 && volumeProfile.soundVolume == 2 &&
            volumeProfile.difficulty == 0 && volumeProfile.fieldOfView == 72,
        "version three profile migrates to the default view angle");
    const WolfCna::CampaignProfile savedProfile = WolfCna::CampaignProgress::Parse(
        WolfCna::CampaignProgress::Serialize(
            WolfCna::CampaignProfile{
                .highestUnlocked = 8,
                .soundVolume = 3,
                .difficulty = 2,
                .fieldOfView = 84},
            3),
        3);
    Expect(
        savedProfile.highestUnlocked == 2 && savedProfile.soundVolume == 3 &&
            savedProfile.difficulty == 2 && savedProfile.fieldOfView == 84,
        "campaign profile restores clamped unlocks, volume, difficulty and view angle");
    const WolfCna::CampaignProfile invalidProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-4\n2\n5\n1\n72\n", 3);
    Expect(
        invalidProfile.highestUnlocked == 0 && invalidProfile.soundVolume == 4 && invalidProfile.difficulty == 1,
        "invalid campaign profile safely restores defaults");
    const WolfCna::CampaignProfile invalidViewProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-4\n2\n4\n1\n73\n", 3);
    Expect(
        invalidViewProfile.highestUnlocked == 0 && invalidViewProfile.fieldOfView == 72,
        "unsupported view angle safely restores defaults");

    Expect(WolfCna::CampaignSectors.size() == 6, "campaign includes its hidden sector");
    Expect(
        WolfCna::SelectableCampaignSectors.size() == 5 &&
            WolfCna::GetSelectableCampaignSector(4).kind ==
                WolfCna::CampaignSectorKind::Boss,
        "five main sectors are selectable and the finale is a boss sector");
    Expect(
        WolfCna::GetCampaignSector(0).chapter == 1 &&
            WolfCna::GetCampaignSector(4).chapter == 1 &&
            WolfCna::GetCampaignSector(2).chapter == 2 &&
            WolfCna::GetCampaignSector(5).chapterName == "WARDEN NETWORK",
        "campaign metadata groups main and secret sectors into two named chapters");
    Expect(
        WolfCna::CampaignDestination(0, WolfCna::CampaignExitRoute::Standard) == 1 &&
            WolfCna::CampaignDestination(1, WolfCna::CampaignExitRoute::Standard) == 2 &&
            WolfCna::CampaignDestination(2, WolfCna::CampaignExitRoute::Standard) == 3 &&
            WolfCna::CampaignDestination(3, WolfCna::CampaignExitRoute::Standard) == 5 &&
            !WolfCna::CampaignDestination(5, WolfCna::CampaignExitRoute::Standard),
        "main campaign route reaches the boss and then terminates");
    Expect(
        WolfCna::CampaignDestination(1, WolfCna::CampaignExitRoute::Secret) == 4 &&
            WolfCna::CampaignDestination(4, WolfCna::CampaignExitRoute::Standard) == 2 &&
            !WolfCna::CampaignDestination(0, WolfCna::CampaignExitRoute::Secret),
        "secret foundry route visits the reservoir and returns to the labs");
    Expect(
        WolfCna::HighestUnlockAfterCompletion(0, 0) == 1 &&
            WolfCna::HighestUnlockAfterCompletion(3, 3) == 4 &&
            WolfCna::HighestUnlockAfterCompletion(4, 2) == 2,
        "main completion unlocks menu sectors while hidden completion does not leak entries");

    const WolfCna::LevelDefinition starterLevel = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/starter.level");
    ExpectCampaignLayout(starterLevel, "starter level");
    Expect(starterLevel.PlayerStartX() == 5 && starterLevel.PlayerStartZ() == 7, "starter spawn");

    const WolfCna::LevelDefinition sectorTwo = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/sector-02.level");
    const WolfCna::LevelDefinition sectorThree = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/sector-03.level");
    const WolfCna::LevelDefinition sectorFour = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/sector-04.level");
    const WolfCna::LevelDefinition hiddenReservoir = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/hidden-reservoir.level");
    const WolfCna::LevelDefinition wardenCore = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/warden-core.level");
    ExpectCampaignLayout(sectorTwo, "sector two");
    ExpectCampaignLayout(sectorThree, "sector three");
    ExpectCampaignLayout(sectorFour, "sector four");
    Expect(sectorFour.Rows() != starterLevel.Rows(), "sector four is not the starter layout");
    Expect(sectorFour.Rows() != sectorTwo.Rows(), "sector four is not the foundry layout");
    Expect(sectorFour.Rows() != sectorThree.Rows(), "sector four is not the labs layout");
    for (const auto* extraSector : {&hiddenReservoir, &wardenCore})
    {
        Expect(extraSector->Rows().size() == 64, "extra campaign sector has 64 rows");
        Expect(
            std::all_of(
                extraSector->Rows().begin(),
                extraSector->Rows().end(),
                [](const std::string& row) { return row.size() == 64; }),
            "extra campaign sector has 64 columns");
        const DistanceGrid distances = BuildDistances(
            extraSector->Rows(),
            extraSector->PlayerStartX(),
            extraSector->PlayerStartZ());
        int walkable = 0;
        int reachable = 0;
        for (int z = 0; z < 64; ++z)
        {
            for (int x = 0; x < 64; ++x)
            {
                const char symbol = extraSector->Rows()[static_cast<std::size_t>(z)]
                    [static_cast<std::size_t>(x)];
                if (symbol != '#' && symbol != 'Y')
                    ++walkable;
                if (distances[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] >= 0)
                    ++reachable;
            }
        }
        Expect(reachable == walkable, "extra campaign sector has no disconnected rooms");
    }
    Expect(
        std::count_if(
            sectorTwo.Rows().begin(),
            sectorTwo.Rows().end(),
            [](const std::string& row) { return row.find('X') != std::string::npos; }) == 1,
        "foundry contains one discoverable hidden-sector elevator");
    Expect(
        std::count_if(
            wardenCore.Rows().begin(),
            wardenCore.Rows().end(),
            [](const std::string& row) { return row.find('Z') != std::string::npos; }) == 1,
        "warden core contains one original boss encounter");

    const std::array<const WolfCna::LevelDefinition*, 6> campaignLevels{
        &starterLevel,
        &sectorTwo,
        &sectorThree,
        &sectorFour,
        &hiddenReservoir,
        &wardenCore};
    for (const WolfCna::LevelDefinition* campaignLevel : campaignLevels)
    {
        const WolfCna::World scoutWorld(*campaignLevel, WolfCna::Difficulty::Scout);
        const WolfCna::World operativeWorld(*campaignLevel, WolfCna::Difficulty::Operative);
        const WolfCna::World veteranWorld(*campaignLevel, WolfCna::Difficulty::Veteran);
        const WolfCna::World::DifficultyBalance scout = scoutWorld.GetDifficultyBalance();
        const WolfCna::World::DifficultyBalance operative = operativeWorld.GetDifficultyBalance();
        const WolfCna::World::DifficultyBalance veteran = veteranWorld.GetDifficultyBalance();
        const WolfCna::World::EnemyBehaviorStats scoutBehavior =
            scoutWorld.GetEnemyBehaviorStats();
        const WolfCna::World::EnemyBehaviorStats operativeBehavior =
            operativeWorld.GetEnemyBehaviorStats();
        const WolfCna::World::EnemyBehaviorStats veteranBehavior =
            veteranWorld.GetEnemyBehaviorStats();
        Expect(
            scout.activeEnemies < operative.activeEnemies &&
                operative.activeEnemies < veteran.activeEnemies,
            "campaign enemy count increases monotonically with difficulty");
        Expect(
            scout.totalEnemyHealth < operative.totalEnemyHealth &&
                operative.totalEnemyHealth < veteran.totalEnemyHealth,
            "campaign enemy health budget increases monotonically with difficulty");
        Expect(
            scout.fixedAmmunition > operative.fixedAmmunition &&
                operative.fixedAmmunition > veteran.fixedAmmunition,
            "campaign fixed ammunition decreases monotonically with difficulty");
        const int scoutSupply = scoutProfile.startingAmmunition +
            scout.fixedAmmunition + scout.potentialDroppedAmmunition;
        const int operativeSupply = operativeProfile.startingAmmunition +
            operative.fixedAmmunition + operative.potentialDroppedAmmunition;
        const int veteranSupply = veteranProfile.startingAmmunition +
            veteran.fixedAmmunition + veteran.potentialDroppedAmmunition;
        Expect(
            scoutSupply > operativeSupply && operativeSupply > veteranSupply,
            "campaign total ammunition supply decreases monotonically with difficulty");
        Expect(
            scoutBehavior.patrollingEnemies >= 1 && operativeBehavior.patrollingEnemies >= 1 &&
                veteranBehavior.patrollingEnemies >= 1 &&
                scoutBehavior.ambushEnemies >= 1 && operativeBehavior.ambushEnemies >= 1 &&
                veteranBehavior.ambushEnemies >= 1,
            "every difficulty retains each sector's authored patrol and ambush encounters");
    }

    const WolfCna::LevelDefinition difficultyLevel = WolfCna::LevelDefinition::Parse(
        "##########\n#PGGGGAW.#\n##########\n",
        "difficulty.level");
    const WolfCna::World difficultyScout(difficultyLevel, WolfCna::Difficulty::Scout);
    const WolfCna::World difficultyOperative(difficultyLevel, WolfCna::Difficulty::Operative);
    const WolfCna::World difficultyVeteran(difficultyLevel, WolfCna::Difficulty::Veteran);
    const WolfCna::World::DifficultyBalance focusedScout = difficultyScout.GetDifficultyBalance();
    const WolfCna::World::DifficultyBalance focusedOperative = difficultyOperative.GetDifficultyBalance();
    const WolfCna::World::DifficultyBalance focusedVeteran = difficultyVeteran.GetDifficultyBalance();
    Expect(
        focusedScout.activeEnemies == 2 && focusedOperative.activeEnemies == 3 &&
            focusedVeteran.activeEnemies == 4,
        "stable encounter tiers activate two, three and four focused enemies");
    Expect(
        focusedScout.totalEnemyHealth == 4 && focusedOperative.totalEnemyHealth == 9 &&
            focusedVeteran.totalEnemyHealth == 16,
        "focused enemy health is scaled after spawn-tier selection");
    Expect(
        focusedScout.fixedAmmunition == 20 && focusedOperative.fixedAmmunition == 12 &&
            focusedVeteran.fixedAmmunition == 8,
        "focused fixed ammunition follows the selected resource profile");
    Expect(
        focusedScout.potentialDroppedAmmunition == 10 &&
            focusedOperative.potentialDroppedAmmunition == 9 &&
            focusedVeteran.potentialDroppedAmmunition == 8,
        "focused enemy drops follow the selected resource profile");
    const WolfCna::LevelDefinition cadenceLevel = WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "difficulty-cadence.level");
    const auto countGuardShots = [&cadenceLevel](WolfCna::Difficulty difficulty)
    {
        WolfCna::World cadenceWorld(cadenceLevel, difficulty);
        const Microsoft::Xna::Framework::Vector3 target(1.5f, 0.62f, 1.5f);
        int shots = 0;
        for (int tick = 0; tick < 120; ++tick)
        {
            static_cast<void>(cadenceWorld.Update(0.05f, target));
            shots += cadenceWorld.ConsumeGuardShotCount();
        }
        return shots;
    };
    const int scoutShots = countGuardShots(WolfCna::Difficulty::Scout);
    const int operativeShots = countGuardShots(WolfCna::Difficulty::Operative);
    const int veteranShots = countGuardShots(WolfCna::Difficulty::Veteran);
    Expect(
        scoutShots < operativeShots && operativeShots < veteranShots,
        "guard firing frequency increases monotonically with difficulty");

    const WolfCna::LevelDefinition level = WolfCna::LevelDefinition::Parse(
        "#####\n#P..#\n#####\n",
        "valid.level");
    Expect(level.Rows().size() == 3, "valid level row count");
    Expect(level.Rows().front().size() == 5, "valid level width");
    Expect(level.PlayerStartX() == 1 && level.PlayerStartZ() == 1, "player spawn position");

    WolfCna::ExplorationMap exploration(level);
    Expect(!exploration.IsVisited(1, 1), "new exploration map starts hidden");
    Expect(exploration.Visit(1.5f, 1.5f), "entering a floor cell reveals it");
    Expect(exploration.IsVisited(1, 1), "visited floor cell stays revealed");
    Expect(!exploration.Visit(1.5f, 1.5f), "revisiting a cell does not reveal it twice");
    Expect(!exploration.Visit(0.5f, 0.5f), "wall cells cannot become visited");
    Expect(!exploration.IsVisited(2, 1), "unvisited floor remains hidden");
    const WolfCna::LevelDefinition goalLevel = WolfCna::LevelDefinition::Parse(
        "#####\n#P.E#\n#####\n",
        "goal.level");
    exploration.Reset(goalLevel);
    Expect(exploration.GoalX() == 3 && exploration.GoalZ() == 1, "automap records the sector goal");
    Expect(!exploration.IsVisited(3, 1), "goal marker does not reveal its unvisited cell");
    exploration.Reset(starterLevel);
    Expect(exploration.Width() == 64 && exploration.Height() == 64, "sector reset resizes exploration");
    Expect(exploration.GoalX() >= 0 && exploration.GoalZ() >= 0, "campaign sector reset records its goal");
    Expect(!exploration.IsVisited(1, 1), "sector reset clears prior exploration");

    const WolfCna::LevelDefinition saveLevel = WolfCna::LevelDefinition::Parse(
        "###########\n#PDGHAOME.#\n###########\n",
        "run-save.level");
    WolfCna::World saveWorld(saveLevel, WolfCna::Difficulty::Operative);
    const Microsoft::Xna::Framework::Vector3 savePlayer(1.5f, 0.62f, 1.5f);
    const Microsoft::Xna::Framework::Vector3 saveLook(1.0f, 0.0f, 0.0f);
    Expect(
        saveWorld.TryActivate(savePlayer, saveLook, false) ==
            WolfCna::World::InteractionResult::DoorOpened,
        "save fixture opens its door");
    static_cast<void>(saveWorld.Update(0.5f, savePlayer));
    for (int hit = 0; hit < 3; ++hit)
        static_cast<void>(saveWorld.FireHitscan(savePlayer, saveLook));
    Expect(
        saveWorld.CollectPickups(
            Microsoft::Xna::Framework::Vector3(4.5f, 0.62f, 1.5f),
            50).health == 25,
        "save fixture collects health before snapshot");
    Expect(
        saveWorld.CollectPickups(
            Microsoft::Xna::Framework::Vector3(5.5f, 0.62f, 1.5f)).ammo == 6,
        "save fixture collects fixed ammunition before snapshot");
    Expect(
        saveWorld.TryActivate(
            Microsoft::Xna::Framework::Vector3(5.5f, 0.62f, 1.5f),
            saveLook,
            false) == WolfCna::World::InteractionResult::RelayActivated,
        "save fixture activates its relay");
    Expect(
        saveWorld.TryActivate(
            Microsoft::Xna::Framework::Vector3(6.5f, 0.62f, 1.5f),
            saveLook,
            false) == WolfCna::World::InteractionResult::TerminalActivated,
        "save fixture activates its terminal");
    WolfCna::ExplorationMap saveExploration(saveLevel);
    static_cast<void>(saveExploration.Visit(1.5f, 1.5f));
    static_cast<void>(saveExploration.Visit(2.5f, 1.5f));
    WolfCna::RunSaveState runSaveState{
        .levelIndex = 0,
        .difficulty = 1,
        .playerX = 2.5f,
        .playerY = 0.62f,
        .playerZ = 1.5f,
        .yaw = 1.25f,
        .health = 75,
        .ammunition = 18,
        .score = 1234,
        .lives = 2,
        .nextExtraLifeScore = 40000,
        .sectorEntryScore = 1000,
        .sectorEntryNextExtraLifeScore = 40000,
        .levelElapsedSeconds = 42.5f,
        .hasSecurityCard = false,
        .weapon = 1,
        .lastFirearm = 1,
        .hasRepeater = false,
        .hasHeavyWeapon = false,
        .exploredCells = saveExploration.CaptureVisited(),
        .world = saveWorld.CaptureSaveState()};
    std::string saveError;
    const std::string serializedRunSave = WolfCna::RunSave::Serialize(runSaveState);
    const std::optional<WolfCna::RunSaveState> parsedRunSave =
        WolfCna::RunSave::Parse(serializedRunSave, saveError);
    Expect(parsedRunSave.has_value() && saveError.empty(), "versioned run save parses");
    Expect(
        parsedRunSave->sectorEntryScore == 1000 &&
            parsedRunSave->sectorEntryNextExtraLifeScore == 40000,
        "run save preserves the sector restart checkpoint");
    Expect(
        WolfCna::RunSave::Serialize(*parsedRunSave) == serializedRunSave,
        "run save serialization is a deterministic round trip");
    std::string legacyRunSave = serializedRunSave;
    legacyRunSave.replace(
        legacyRunSave.find("WOLF-CNA-RUN-SAVE-2"),
        std::string("WOLF-CNA-RUN-SAVE-2").size(),
        "WOLF-CNA-RUN-SAVE-1");
    const std::size_t legacyCheckpoint = legacyRunSave.find(" 1000 40000 42.5");
    Expect(legacyCheckpoint != std::string::npos, "run-save fixture locates its v2 checkpoint fields");
    legacyRunSave.erase(legacyCheckpoint, std::string(" 1000 40000").size());
    const std::optional<WolfCna::RunSaveState> parsedLegacyRunSave =
        WolfCna::RunSave::Parse(legacyRunSave, saveError);
    Expect(
        parsedLegacyRunSave.has_value() &&
            parsedLegacyRunSave->sectorEntryScore == parsedLegacyRunSave->score &&
            parsedLegacyRunSave->sectorEntryNextExtraLifeScore ==
                parsedLegacyRunSave->nextExtraLifeScore,
        "version one run saves migrate to a safe load-time restart checkpoint");
    WolfCna::World restoredSaveWorld(saveLevel, WolfCna::Difficulty::Operative);
    Expect(
        restoredSaveWorld.RestoreSaveState(parsedRunSave->world),
        "world snapshot restores into the matching sector and difficulty");
    Expect(
        !restoredSaveWorld.Collides(2.5f, 1.5f, 0.1f) &&
            restoredSaveWorld.GetCompletionStats().defeatedEnemies == 1 &&
            restoredSaveWorld.AreObjectivesComplete(),
        "world restore preserves doors, defeated enemies and objective state");
    Expect(
        restoredSaveWorld.CollectPickups(
            Microsoft::Xna::Framework::Vector3(3.5f, 0.62f, 1.5f)).ammo == 3,
        "world restore preserves a dynamic enemy ammunition drop");
    WolfCna::World restartedSaveWorld(saveLevel, WolfCna::Difficulty::Operative);
    Expect(
        restartedSaveWorld.Collides(2.5f, 1.5f, 0.1f) &&
            restartedSaveWorld.GetCompletionStats().defeatedEnemies == 0 &&
            !restartedSaveWorld.AreObjectivesComplete(),
        "reconstructing a sector restores doors, enemies and objectives to authored state");
    WolfCna::ExplorationMap restoredExploration(saveLevel);
    Expect(
        restoredExploration.RestoreVisited(parsedRunSave->exploredCells) &&
            restoredExploration.IsVisited(1, 1) && restoredExploration.IsVisited(2, 1) &&
            !restoredExploration.IsVisited(3, 1),
        "automap snapshot restores exactly the visited cells");
    std::vector<bool> invalidExploration = parsedRunSave->exploredCells;
    invalidExploration[0] = true;
    Expect(
        !restoredExploration.RestoreVisited(invalidExploration),
        "automap restore rejects visited wall cells");
    WolfCna::World::SaveState mismatchedWorldState = parsedRunSave->world;
    mismatchedWorldState.enemies.clear();
    Expect(
        !restoredSaveWorld.RestoreSaveState(mismatchedWorldState),
        "world restore rejects mismatched entity counts");
    Expect(
        !WolfCna::RunSave::Parse("NOT-A-WOLF-CNA-SAVE\n", saveError).has_value(),
        "run save rejects an unsupported header");
    Expect(
        !WolfCna::RunSave::Parse(serializedRunSave + "TRAILING\n", saveError).has_value(),
        "run save rejects trailing data");

    const std::filesystem::path saveTestPath =
        std::filesystem::temp_directory_path() / "wolf-cna-run-save-test.dat";
    std::error_code removeError;
    std::filesystem::remove(saveTestPath, removeError);
    std::filesystem::remove(saveTestPath.string() + ".tmp", removeError);
    std::filesystem::remove(saveTestPath.string() + ".bak", removeError);
    Expect(
        WolfCna::RunSave::SaveFile(saveTestPath, runSaveState, saveError),
        "run save writes a new slot through a temporary file");
    runSaveState.score = 4321;
    Expect(
        WolfCna::RunSave::SaveFile(saveTestPath, runSaveState, saveError),
        "run save safely replaces an occupied slot");
    const std::optional<WolfCna::RunSaveState> loadedRunSave =
        WolfCna::RunSave::LoadFile(saveTestPath, saveError);
    Expect(
        loadedRunSave.has_value() && loadedRunSave->score == 4321 &&
            !std::filesystem::exists(saveTestPath.string() + ".tmp") &&
            !std::filesystem::exists(saveTestPath.string() + ".bak"),
        "run save loads the replacement and leaves no staging files");
    std::filesystem::remove(saveTestPath, removeError);

    ExpectParseFailure("#####\n#P.#\n#####\n", "different width");
    ExpectParseFailure("#####\n#@.P#\n#####\n", "unknown symbol");
    ExpectParseFailure("#####\n#...#\n#####\n", "no player spawn");
    ExpectParseFailure("#####\n#P.P#\n#####\n", "more than one player spawn");
    const WolfCna::LevelDefinition decoratedLevel = WolfCna::LevelDefinition::Parse(
        "######\n#PBRL#\n######\n",
        "decorated.level");
    Expect(decoratedLevel.Rows().front().size() == 6, "decoration symbols are accepted");
    const WolfCna::LevelDefinition patrolAndAmbushLevel = WolfCna::LevelDefinition::Parse(
        "########\n#P.G>g.#\n########\n",
        "patrol-and-ambush.level");
    Expect(
        patrolAndAmbushLevel.Rows()[1][4] == '>' &&
            patrolAndAmbushLevel.Rows()[1][5] == 'g',
        "patrol arrows and ambush enemy symbols are accepted");
    ExpectParseFailure(
        "#######\n#P....#\n#..R..#\n#.....#\n#######\n",
        "without an adjacent wall");
    ExpectParseFailure(
        "#####\n#P.>#\n#####\n",
        "patrol marker pointing into a blocked cell");

    WolfCna::World doorWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PD.#\n#####\n",
        "door.level"));
    const Microsoft::Xna::Framework::Vector3 playerPosition(1.5f, 0.62f, 1.5f);
    const Microsoft::Xna::Framework::Vector3 lookDirection(1.0f, 0.0f, 0.0f);
    Expect(doorWorld.Collides(2.5f, 1.5f, 0.1f), "closed door blocks movement");

    Expect(
        doorWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::DoorOpened,
        "normal door activates");
    static_cast<void>(doorWorld.Update(0.2f, playerPosition));
    Expect(doorWorld.Collides(2.5f, 1.5f, 0.1f), "partly open door still blocks movement");

    static_cast<void>(doorWorld.Update(0.3f, playerPosition));
    Expect(!doorWorld.Collides(2.5f, 1.5f, 0.1f), "sufficiently open door allows movement");
    Expect(
        doorWorld.FireHitscan(playerPosition, Microsoft::Xna::Framework::Vector3(-1.0f, 0.0f, 0.0f)),
        "hitscan hits the first wall");
    static_cast<void>(doorWorld.Update(0.1f, playerPosition));
    static_cast<void>(doorWorld.Update(4.0f, playerPosition));
    static_cast<void>(doorWorld.Update(0.5f, playerPosition));
    Expect(doorWorld.Collides(2.5f, 1.5f, 0.1f), "door closes after its delay");

    WolfCna::World occupiedDoorWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PD.#\n#####\n",
        "occupied-door.level"));
    Expect(
        occupiedDoorWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::DoorOpened,
        "occupied door activates");
    const Microsoft::Xna::Framework::Vector3 playerInDoorway(2.5f, 0.62f, 1.5f);
    static_cast<void>(occupiedDoorWorld.Update(0.6f, playerInDoorway));
    static_cast<void>(occupiedDoorWorld.Update(8.0f, playerInDoorway));
    Expect(!occupiedDoorWorld.Collides(2.5f, 1.5f, 0.1f), "door remains open around the player");
    static_cast<void>(occupiedDoorWorld.Update(4.0f, playerPosition));
    static_cast<void>(occupiedDoorWorld.Update(0.6f, playerPosition));
    Expect(occupiedDoorWorld.Collides(2.5f, 1.5f, 0.1f), "door closes after the player leaves");

    WolfCna::World secretWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PS..#\n######\n",
        "secret.level"));
    Expect(secretWorld.Collides(2.5f, 1.5f, 0.1f), "secret wall blocks movement before it is found");
    Expect(
        secretWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::SecretRevealed,
        "secret wall is revealed through interaction");
    static_cast<void>(secretWorld.Update(0.5f, playerPosition));
    static_cast<void>(secretWorld.Update(5.0f, playerPosition));
    Expect(!secretWorld.Collides(2.5f, 1.5f, 0.1f), "secret wall remains open after discovery");
    const WolfCna::World::CompletionStats secretStats = secretWorld.GetCompletionStats();
    Expect(secretStats.foundSecrets == 1 && secretStats.totalSecrets == 1, "secret discovery is counted once");

    WolfCna::World bodyDoorWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PDK.#\n######\n",
        "body-door.level"));
    Expect(
        bodyDoorWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::DoorOpened,
        "body door activates");
    static_cast<void>(bodyDoorWorld.Update(0.5f, playerPosition));
    Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "first shot hits doorway hound");
    Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "second shot kills doorway hound");
    static_cast<void>(bodyDoorWorld.Update(4.0f, playerPosition));
    static_cast<void>(bodyDoorWorld.Update(0.5f, playerPosition));
    Expect(!bodyDoorWorld.Collides(2.5f, 1.5f, 0.1f), "dead hound keeps the door open");

    WolfCna::World securityDoorWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PQ.#\n#####\n",
        "security-door.level"));
    Expect(securityDoorWorld.Collides(2.5f, 1.5f, 0.1f), "closed security door blocks movement");
    Expect(
        securityDoorWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::DoorLocked,
        "security door reports that it is locked");
    static_cast<void>(securityDoorWorld.Update(0.5f, playerPosition));
    Expect(securityDoorWorld.Collides(2.5f, 1.5f, 0.1f), "security door needs an access card");
    Expect(
        securityDoorWorld.TryActivate(playerPosition, lookDirection, true) == WolfCna::World::InteractionResult::DoorOpened,
        "security door activates with an access card");
    static_cast<void>(securityDoorWorld.Update(0.5f, playerPosition));
    Expect(!securityDoorWorld.Collides(2.5f, 1.5f, 0.1f), "security door opens safely");

    WolfCna::World pickupWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PHA#\n#####\n",
        "pickup.level"));
    const WolfCna::World::PickupResult healthPickup = pickupWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f));
    const WolfCna::World::PickupResult ammoPickup = pickupWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(3.5f, 0.62f, 1.5f));
    Expect(healthPickup.health == 25 && healthPickup.ammo == 0, "health pickup is collected once");
    Expect(ammoPickup.health == 0 && ammoPickup.ammo == 6, "ammo pickup is collected once");

    WolfCna::World fullHealthPickupWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PH.#\n#####\n",
        "full-health-pickup.level"));
    const Microsoft::Xna::Framework::Vector3 healthPosition(2.5f, 0.62f, 1.5f);
    Expect(
        fullHealthPickupWorld.CollectPickups(healthPosition, 100).health == 0,
        "health kit remains when health is already full");
    Expect(
        fullHealthPickupWorld.CollectPickups(healthPosition, 90).health == 10,
        "previously skipped health kit can be collected after taking damage");
    Expect(
        fullHealthPickupWorld.CollectPickups(healthPosition, 50).health == 0,
        "health kit is consumed only once after it heals the player");

    WolfCna::World weaponPickupWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PWV.#\n######\n",
        "weapon-pickup.level"));
    const WolfCna::World::PickupResult repeaterPickup = weaponPickupWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f));
    const WolfCna::World::PickupResult heavyWeaponPickup = weaponPickupWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(3.5f, 0.62f, 1.5f));
    Expect(
        repeaterPickup.repeaterWeapons == 1 && repeaterPickup.ammo == 6,
        "repeater pickup grants the weapon and six rounds");
    Expect(
        heavyWeaponPickup.heavyWeapons == 1 && heavyWeaponPickup.ammo == 10,
        "heavy weapon pickup grants the weapon and ten rounds");

    WolfCna::World cardWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PC.#\n#####\n",
        "card.level"));
    const WolfCna::World::PickupResult cardPickup = cardWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f));
    Expect(cardPickup.accessCards == 1, "security card is collected once");

    WolfCna::World goalCheatWorld(WolfCna::LevelDefinition::Parse(
        "########\n#POM..E#\n########\n",
        "goal-cheat.level"));
    const std::optional<WolfCna::World::ExitApproach> goalApproach =
        goalCheatWorld.GetExitApproach();
    Expect(goalApproach.has_value(), "goal cheat finds an authored elevator approach");
    Expect(
        goalApproach->position.X == 5.5f && goalApproach->position.Z == 1.5f,
        "goal cheat destination is one cell outside the elevator");
    Expect(
        goalApproach->lookDirection.X == 1.0f && goalApproach->lookDirection.Z == 0.0f,
        "goal cheat faces the player toward the elevator doors");
    Expect(!goalCheatWorld.AreObjectivesComplete(), "goal cheat lookup does not activate optional systems");
    const Microsoft::Xna::Framework::Vector3 goalInteractionPosition(5.5f, 0.62f, 1.5f);
    Expect(
        goalCheatWorld.TryActivate(goalInteractionPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::ExitActivated,
        "elevator action works immediately after the positioning cheat");
    Expect(
        goalCheatWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::RelayActivated,
        "normal play activates the power relay");
    Expect(
        goalCheatWorld.TryActivate(
            Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f),
            lookDirection,
            false) == WolfCna::World::InteractionResult::TerminalActivated,
        "normal play activates the terminal");
    Expect(goalCheatWorld.AreObjectivesComplete(), "normal interactions complete the optional systems");
    Expect(
        goalCheatWorld.TryActivate(goalInteractionPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::ExitActivated,
        "the elevator action also works after walking to its approach");

    WolfCna::World secretExitWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PX.#\n#####\n",
        "secret-exit.level"));
    Expect(
        secretExitWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::SecretExitActivated,
        "hidden elevator reports the secret campaign route");
    Expect(
        secretExitWorld.ReachedExitRoute(
            Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f)) ==
            WolfCna::World::ExitRoute::Secret,
        "contact with a hidden elevator preserves its route type");

    const WolfCna::LevelDefinition bossLevel = WolfCna::LevelDefinition::Parse(
        "##########\n#P.....Z.#\n##########\n",
        "boss.level");
    WolfCna::World bossWorld(bossLevel, WolfCna::Difficulty::Operative);
    const WolfCna::World::BossStatus initialBoss = bossWorld.GetBossStatus();
    Expect(
        initialBoss.present && !initialBoss.defeated && initialBoss.health == 32 &&
            initialBoss.maximumHealth == 32,
        "original boss starts with its dedicated health budget");
    for (int shot = 0; shot < 31; ++shot)
    {
        Expect(
            bossWorld.FireHitscan(playerPosition, lookDirection),
            "each pre-final boss shot registers");
    }
    Expect(
        bossWorld.GetBossStatus().health == 1 && !bossWorld.GetBossStatus().defeated,
        "boss survives repeated hits before its final health point");
    const WolfCna::World::AttackResult bossFinalHit =
        bossWorld.FireHitscan(playerPosition, lookDirection);
    Expect(
        bossFinalHit.score == 5000 && bossWorld.GetBossStatus().defeated,
        "defeating the Warden awards its dedicated score");

    WolfCna::World bossExitWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PZ.E#\n######\n",
        "boss-exit.level"));
    const Microsoft::Xna::Framework::Vector3 bossExitApproach(3.5f, 0.62f, 1.5f);
    Expect(
        bossExitWorld.TryActivate(bossExitApproach, lookDirection, false) ==
            WolfCna::World::InteractionResult::ExitSealed &&
            !bossExitWorld.ReachedExitRoute(
                Microsoft::Xna::Framework::Vector3(4.5f, 0.62f, 1.5f)),
        "living boss keeps the campaign-completion elevator in lockdown");
    for (int shot = 0; shot < 32; ++shot)
        static_cast<void>(bossExitWorld.FireHitscan(playerPosition, lookDirection));
    Expect(
        bossExitWorld.TryActivate(bossExitApproach, lookDirection, false) ==
            WolfCna::World::InteractionResult::ExitActivated,
        "defeating the boss releases the campaign-completion elevator");

    WolfCna::World bossBurstWorld(bossLevel, WolfCna::Difficulty::Operative);
    static_cast<void>(bossBurstWorld.Update(0.05f, playerPosition));
    static_cast<void>(bossBurstWorld.Update(0.65f, playerPosition));
    static_cast<void>(bossBurstWorld.Update(0.05f, playerPosition));
    Expect(
        bossBurstWorld.CaptureSaveState().projectiles.size() == 3,
        "Warden ranged attack emits a deterministic three-projectile fan");

    WolfCna::World exitWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PET#\n#####\n",
        "exit.level"));
    Expect(
        exitWorld.ReachedExit(Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f)),
        "exit is reached on contact");
    const WolfCna::World::PickupResult goldPickup = exitWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(3.5f, 0.62f, 1.5f));
    Expect(goldPickup.gold == 100, "gold pickup is collected");
    Expect(
        exitWorld.GetCompletionStats().collectedGold == 1 && exitWorld.GetCompletionStats().totalGold == 1,
        "gold collection appears in completion statistics");

    WolfCna::World treasureWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PTJN#\n######\n",
        "treasure.level"));
    Expect(
        treasureWorld.CollectPickups(Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f)).gold == 100,
        "gold bars award 100 score");
    Expect(
        treasureWorld.CollectPickups(Microsoft::Xna::Framework::Vector3(3.5f, 0.62f, 1.5f)).gold == 250,
        "golden goblet awards 250 score");
    Expect(
        treasureWorld.CollectPickups(Microsoft::Xna::Framework::Vector3(4.5f, 0.62f, 1.5f)).gold == 500,
        "peace medallion awards 500 score");
    Expect(
        treasureWorld.GetCompletionStats().collectedGold == 3 &&
            treasureWorld.GetCompletionStats().totalGold == 3,
        "all treasure variants appear in completion statistics");

    WolfCna::World terminalWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PME#\n#####\n",
        "terminal.level"));
    const Microsoft::Xna::Framework::Vector3 terminalExit(3.5f, 0.62f, 1.5f);
    Expect(terminalWorld.ReachedExit(terminalExit), "elevator is available independently of terminal state");
    Expect(!terminalWorld.Collides(3.5f, 1.5f, 0.1f), "elevator entrance is open from sector start");
    Expect(
        terminalWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::TerminalActivated,
        "terminal activates when used from the front");
    Expect(terminalWorld.AreObjectivesComplete(), "terminal-only fixture reports optional system completion");

    WolfCna::World relayWorld(WolfCna::LevelDefinition::Parse(
        "######\n#POME#\n######\n",
        "relay.level"));
    Expect(
        relayWorld.GetObjectiveStatus().activatedRelays == 0 &&
            relayWorld.GetObjectiveStatus().activatedTerminals == 0,
        "new objective status starts incomplete");
    Expect(
        relayWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::RelayActivated,
        "power relay activates when used from the front");
    Expect(
        relayWorld.GetObjectiveStatus().activatedRelays == 1 &&
            relayWorld.GetObjectiveStatus().activatedTerminals == 0,
        "objective status reports relay progress independently");
    Expect(!relayWorld.AreObjectivesComplete(), "power relay alone leaves the optional terminal incomplete");
    Expect(
        relayWorld.TryActivate(
            Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f),
            lookDirection,
            false) == WolfCna::World::InteractionResult::TerminalActivated,
        "terminal remains independently required after the relay");
    Expect(relayWorld.AreObjectivesComplete(), "relay and terminal together complete optional systems");
    Expect(
        relayWorld.GetObjectiveStatus().activatedRelays == 1 &&
            relayWorld.GetObjectiveStatus().activatedTerminals == 1,
        "objective status reports full completion");

    WolfCna::World tableWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PY.#\n#####\n",
        "table.level"));
    Expect(tableWorld.Collides(2.5f, 1.5f, 0.22f), "polygonal table has matching collision");

    WolfCna::World combatWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "combat.level"));
    const Microsoft::Xna::Framework::Vector3 combatPlayer(1.5f, 0.62f, 1.5f);
    Expect(combatWorld.FireHitscan(combatPlayer, lookDirection).score == 0, "wounding a guard has no score");
    Expect(combatWorld.FireHitscan(combatPlayer, lookDirection).score == 0, "second guard wound has no score");
    Expect(combatWorld.FireHitscan(combatPlayer, lookDirection).score == 100, "guard kill awards score");
    Expect(
        combatWorld.GetCompletionStats().defeatedEnemies == 1 && combatWorld.GetCompletionStats().totalEnemies == 1,
        "enemy defeat appears in completion statistics");
    const Microsoft::Xna::Framework::Vector3 enemyDropPosition(2.5f, 0.62f, 1.5f);
    Expect(
        combatWorld.CollectPickups(enemyDropPosition).ammo == 3,
        "defeated guard drops three rounds");
    Expect(
        combatWorld.CollectPickups(enemyDropPosition).ammo == 0,
        "guard ammunition drop is collected once");

    WolfCna::World houndWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PK.#\n#####\n",
        "hound.level"));
    Expect(houndWorld.FireHitscan(combatPlayer, lookDirection).score == 0, "first shot wounds a hound");
    Expect(houndWorld.FireHitscan(combatPlayer, lookDirection).score == 200, "second hound shot awards score");
    Expect(houndWorld.CollectPickups(enemyDropPosition).ammo == 0, "hound does not drop ammunition");

    WolfCna::World rapidTrooperWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PF.#\n#####\n",
        "rapid-trooper.level"));
    for (int hit = 0; hit < 3; ++hit)
        Expect(rapidTrooperWorld.FireHitscan(combatPlayer, lookDirection).score == 0, "rapid trooper survives early hits");
    Expect(rapidTrooperWorld.FireHitscan(combatPlayer, lookDirection).score == 250, "rapid trooper awards score");
    Expect(
        rapidTrooperWorld.CollectPickups(enemyDropPosition).ammo == 5,
        "defeated rapid trooper drops five rounds");

    WolfCna::World heavyUnitWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PU.#\n#####\n",
        "heavy-unit.level"));
    for (int hit = 0; hit < 7; ++hit)
        Expect(heavyUnitWorld.FireHitscan(combatPlayer, lookDirection).score == 0, "heavy unit survives early hits");
    Expect(heavyUnitWorld.FireHitscan(combatPlayer, lookDirection).score == 500, "heavy unit awards score");
    Expect(
        heavyUnitWorld.CollectPickups(enemyDropPosition).ammo == 8,
        "defeated heavy unit drops eight rounds");

    WolfCna::World damageWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "damage.level"));
    int guardProjectileDamage = 0;
    int guardShots = 0;
    int activeProjectileImpacts = 0;
    for (int tick = 0; tick < 20; ++tick)
    {
        guardProjectileDamage += damageWorld.Update(0.05f, combatPlayer);
        guardShots += damageWorld.ConsumeGuardShotCount();
        activeProjectileImpacts = std::max(
            activeProjectileImpacts,
            damageWorld.ActiveEnemyImpactCount());
    }
    const WolfCna::World::EnemyAudioEvents projectileImpactEvents =
        damageWorld.ConsumeEnemyAudioEvents();
    Expect(guardShots >= 1, "guard emits a shot at the player");
    Expect(guardProjectileDamage == 8, "guard projectile damages a player at range");
    Expect(activeProjectileImpacts > 0, "enemy projectile hit creates a temporary visual impact");
    Expect(projectileImpactEvents.projectileImpacts == 1, "enemy projectile hit emits one impact event");

    WolfCna::World coordinatedFireWorld(WolfCna::LevelDefinition::Parse(
        "######\n#P.GG#\n######\n",
        "coordinated-fire.level"));
    int coordinatedShots = 0;
    for (int tick = 0; tick < 10; ++tick)
    {
        static_cast<void>(coordinatedFireWorld.Update(0.05f, combatPlayer));
        coordinatedShots += coordinatedFireWorld.ConsumeGuardShotCount();
    }
    Expect(
        coordinatedShots == 1,
        "only the nearest visible ranged enemy fires after its reaction delay");

    WolfCna::World patrolPerceptionWorld(WolfCna::LevelDefinition::Parse(
        "########\n#P.G>..#\n########\n",
        "patrol-perception.level"));
    static_cast<void>(patrolPerceptionWorld.Update(0.5f, combatPlayer));
    Expect(
        patrolPerceptionWorld.ConsumeEnemyAudioEvents().guardAlerts == 0,
        "a patrol does not see a player behind its facing direction");
    const WolfCna::World::EnemyBehaviorStats movingPatrol =
        patrolPerceptionWorld.GetEnemyBehaviorStats();
    Expect(
        movingPatrol.patrollingEnemies == 1 && movingPatrol.totalTravelDistance > 0.3f,
        "an authored patrol marker moves an unaware enemy");
    static_cast<void>(patrolPerceptionWorld.FireHitscan(
        combatPlayer,
        Microsoft::Xna::Framework::Vector3(-1.0f, 0.0f, 0.0f)));
    static_cast<void>(patrolPerceptionWorld.Update(0.05f, combatPlayer));
    Expect(
        patrolPerceptionWorld.GetEnemyBehaviorStats().alertingEnemies == 1 &&
            patrolPerceptionWorld.ConsumeEnemyAudioEvents().guardAlerts == 1,
        "firearm noise starts a reaction delay for an enemy facing away");

    WolfCna::World ambushPerceptionWorld(WolfCna::LevelDefinition::Parse(
        "#######\n#P.g..#\n#######\n",
        "ambush-perception.level"));
    static_cast<void>(ambushPerceptionWorld.FireHitscan(
        combatPlayer,
        Microsoft::Xna::Framework::Vector3(-1.0f, 0.0f, 0.0f)));
    static_cast<void>(ambushPerceptionWorld.Update(0.1f, combatPlayer));
    Expect(
        ambushPerceptionWorld.GetEnemyBehaviorStats().idleEnemies == 1 &&
            ambushPerceptionWorld.ConsumeEnemyAudioEvents().guardAlerts == 0,
        "an ambush enemy ignores weapon noise while the player remains outside its view");
    static_cast<void>(ambushPerceptionWorld.Update(
        0.05f,
        Microsoft::Xna::Framework::Vector3(2.9f, 0.62f, 1.5f)));
    Expect(
        ambushPerceptionWorld.GetEnemyBehaviorStats().alertingEnemies == 1,
        "close awareness reveals an ambush enemy approached from behind");

    WolfCna::World enemyDoorWorld(WolfCna::LevelDefinition::Parse(
        "########\n#P.D.G.#\n########\n",
        "enemy-door.level"));
    static_cast<void>(enemyDoorWorld.FireHitscan(
        combatPlayer,
        Microsoft::Xna::Framework::Vector3(-1.0f, 0.0f, 0.0f)));
    bool ordinaryDoorOpened = false;
    int enemyDoorAudioEvents = 0;
    for (int tick = 0; tick < 120 && !ordinaryDoorOpened; ++tick)
    {
        static_cast<void>(enemyDoorWorld.Update(0.05f, combatPlayer));
        enemyDoorAudioEvents += enemyDoorWorld.ConsumeEnemyAudioEvents().doorsOpened;
        ordinaryDoorOpened = !enemyDoorWorld.Collides(3.5f, 1.5f, 0.1f);
    }
    Expect(
        ordinaryDoorOpened && enemyDoorAudioEvents == 1,
        "an alerted enemy opens one ordinary door on its path");

    WolfCna::World enemySecurityDoorWorld(WolfCna::LevelDefinition::Parse(
        "########\n#P.Q.G.#\n########\n",
        "enemy-security-door.level"));
    static_cast<void>(enemySecurityDoorWorld.FireHitscan(
        combatPlayer,
        Microsoft::Xna::Framework::Vector3(-1.0f, 0.0f, 0.0f)));
    for (int tick = 0; tick < 80; ++tick)
        static_cast<void>(enemySecurityDoorWorld.Update(0.05f, combatPlayer));
    Expect(
        enemySecurityDoorWorld.Collides(3.5f, 1.5f, 0.1f) &&
            enemySecurityDoorWorld.GetEnemyBehaviorStats().idleEnemies == 1,
        "locked security doors block both noise propagation and enemy navigation");

    WolfCna::World searchWorld(WolfCna::LevelDefinition::Parse(
        "########\n#P....G#\n########\n#......#\n########\n",
        "enemy-search.level"));
    static_cast<void>(searchWorld.FireHitscan(
        combatPlayer,
        Microsoft::Xna::Framework::Vector3(-1.0f, 0.0f, 0.0f)));
    const Microsoft::Xna::Framework::Vector3 hiddenPlayer(1.5f, 0.62f, 3.5f);
    bool searchedLastKnownPosition = false;
    for (int tick = 0; tick < 180 && !searchedLastKnownPosition; ++tick)
    {
        static_cast<void>(searchWorld.Update(0.05f, hiddenPlayer));
        searchedLastKnownPosition =
            searchWorld.GetEnemyBehaviorStats().searchingEnemies == 1;
    }
    Expect(
        searchedLastKnownPosition,
        "an alerted enemy searches the last heard position instead of tracking a hidden player");

    WolfCna::World houndAudioWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PK.#\n#####\n",
        "hound-audio.level"));
    int houndAttacks = 0;
    const WolfCna::World::EnemyAudioEvents firstHoundEvents =
        (static_cast<void>(houndAudioWorld.Update(0.05f, combatPlayer)), houndAudioWorld.ConsumeEnemyAudioEvents());
    Expect(firstHoundEvents.houndAlerts == 1, "hound emits an alert when it sees the player");
    for (int tick = 0; tick < 10; ++tick)
    {
        static_cast<void>(houndAudioWorld.Update(0.05f, combatPlayer));
        houndAttacks += houndAudioWorld.ConsumeEnemyAudioEvents().houndAttacks;
    }
    Expect(houndAttacks >= 1, "hound emits an attack event on a close-range hit");

    WolfCna::World scoutDamageWorld(
        WolfCna::LevelDefinition::Parse(
            "#####\n#PK.#\n#####\n",
            "scout-damage.level"),
        WolfCna::Difficulty::Scout);
    int scoutHoundDamage = 0;
    for (int tick = 0; tick < 10; ++tick)
    {
        scoutHoundDamage += scoutDamageWorld.Update(
            0.05f,
            Microsoft::Xna::Framework::Vector3(1.8f, 0.62f, 1.5f));
    }
    Expect(
        scoutHoundDamage == 10,
        "selected difficulty profile scales hound damage after its reaction delay");

    return EXIT_SUCCESS;
}
