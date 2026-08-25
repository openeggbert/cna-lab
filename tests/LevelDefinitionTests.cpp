#include <algorithm>
#include <array>
#include <cmath>
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
#include "Combat.hpp"
#include "Controls.hpp"
#include "DoorMotion.hpp"
#include "ExplorationMap.hpp"
#include "HudStatus.hpp"
#include "RunSave.hpp"
#include "RunRules.hpp"
#include "Scoring.hpp"
#include "SpatialAudio.hpp"

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

    bool CanReachCampaignExitWithAuthoredAccess(const WolfCna::LevelDefinition& level)
    {
        struct State
        {
            int x = 0;
            int z = 0;
            int access = 0;
        };

        const auto& rows = level.Rows();
        std::vector<std::vector<std::array<bool, 4>>> visited(
            rows.size(),
            std::vector<std::array<bool, 4>>(rows.front().size()));
        std::queue<State> frontier;
        frontier.push({level.PlayerStartX(), level.PlayerStartZ(), 0});
        visited[static_cast<std::size_t>(level.PlayerStartZ())]
            [static_cast<std::size_t>(level.PlayerStartX())][0] = true;
        constexpr std::array<std::pair<int, int>, 4> Directions = {
            std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};

        while (!frontier.empty())
        {
            const State state = frontier.front();
            frontier.pop();
            for (const auto [dx, dz] : Directions)
            {
                const int x = state.x + dx;
                const int z = state.z + dz;
                if (x < 0 || z < 0 || z >= static_cast<int>(rows.size()) ||
                    x >= static_cast<int>(rows[static_cast<std::size_t>(z)].size()))
                {
                    continue;
                }
                const char symbol = rows[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
                if (symbol == '#' || symbol == 'Y' ||
                    (symbol == 'Q' && (state.access & WolfCna::World::CyanAccess) == 0) ||
                    (symbol == 'q' && (state.access & WolfCna::World::AmberAccess) == 0))
                {
                    continue;
                }
                if (symbol == 'E' || symbol == 'X')
                    return true;

                int access = state.access;
                if (symbol == 'C')
                    access |= WolfCna::World::CyanAccess;
                else if (symbol == 'c')
                    access |= WolfCna::World::AmberAccess;
                if (visited[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)]
                    [static_cast<std::size_t>(access)])
                {
                    continue;
                }
                visited[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)]
                    [static_cast<std::size_t>(access)] = true;
                frontier.push({x, z, access});
            }
        }
        return false;
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
        int largeAmmoPickups = 0;
        int smallAmmoPickups = 0;
        int repeaterPickups = 0;
        int heavyWeaponPickups = 0;
        int guards = 0;
        int hounds = 0;
        int rapidTroopers = 0;
        int heavyUnits = 0;
        int bosses = 0;
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
                else if (symbol == 'H' || symbol == 'h')
                {
                    ++healthPickups;
                    healthDistances.push_back(
                        startDistances[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)]);
                }
                else if (symbol == 'A')
                    ++largeAmmoPickups;
                else if (symbol == 'a')
                    ++smallAmmoPickups;
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
                else if (symbol == 'Z')
                    ++bosses;
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
            largeAmmoPickups * 8 + smallAmmoPickups * 4 +
            repeaterPickups * 8 + heavyWeaponPickups * 14 +
            guards * 3 + rapidTroopers * 5 + heavyUnits * 8;
        const int hitsToClear =
            guards * 3 + hounds * 2 + rapidTroopers * 4 + heavyUnits * 8 + bosses * 24;
        Expect(
            guaranteedAmmo >= hitsToClear,
            std::string(name) + " has enough guaranteed ammunition for a full clear");
    }
}

int main()
{
    const WolfCna::SpatialAudioMix centeredAudio =
        WolfCna::CalculateSpatialAudioMix(0.0f, 0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.8f);
    const WolfCna::SpatialAudioMix rightAudio =
        WolfCna::CalculateSpatialAudioMix(0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f, 0.8f);
    const WolfCna::SpatialAudioMix leftAudio =
        WolfCna::CalculateSpatialAudioMix(0.0f, 0.0f, 0.0f, -1.0f, -2.0f, 0.0f, 0.8f);
    const WolfCna::SpatialAudioMix distantAudio =
        WolfCna::CalculateSpatialAudioMix(0.0f, 0.0f, 0.0f, -1.0f, 0.0f, -8.0f, 0.8f);
    const WolfCna::SpatialAudioMix inaudibleAudio =
        WolfCna::CalculateSpatialAudioMix(0.0f, 0.0f, 0.0f, -1.0f, 0.0f, -20.0f, 0.8f);
    Expect(
        std::abs(centeredAudio.pan) < 0.001f &&
            std::abs(centeredAudio.volume - 0.8f) < 0.001f,
        "a close centered source retains volume without stereo bias");
    Expect(rightAudio.pan > 0.99f && leftAudio.pan < -0.99f,
        "listener orientation places side sources in opposite stereo channels");
    Expect(distantAudio.volume > 0.0f && distantAudio.volume < centeredAudio.volume,
        "world audio attenuates with distance");
    Expect(inaudibleAudio.volume == 0.0f,
        "world audio is silent beyond its bounded range");

    const WolfCna::DoorPanelOffset northSouthPositive =
        WolfCna::CalculateLateralDoorOffset(true, 0.75f,
            WolfCna::SelectDoorSlideDirection(2, 2, true, true));
    const WolfCna::DoorPanelOffset northSouthNegative =
        WolfCna::CalculateLateralDoorOffset(true, 0.75f,
            WolfCna::SelectDoorSlideDirection(3, 2, true, true));
    const WolfCna::DoorPanelOffset eastWestPositive =
        WolfCna::CalculateLateralDoorOffset(false, 0.75f,
            WolfCna::SelectDoorSlideDirection(2, 2, true, true));
    const WolfCna::DoorPanelOffset onlyWestPocket =
        WolfCna::CalculateLateralDoorOffset(false, 0.75f,
            WolfCna::SelectDoorSlideDirection(2, 2, true, false));
    const WolfCna::DoorPanelOffset closedDoor =
        WolfCna::CalculateLateralDoorOffset(false, 0.0f, 1);
    Expect(
        northSouthPositive.x == 0.0f && northSouthPositive.z == 0.75f &&
            northSouthNegative.x == 0.0f && northSouthNegative.z == -0.75f,
        "north-south door panels alternate right-to-left and left-to-right travel");
    Expect(
        eastWestPositive.x == 0.75f && eastWestPositive.z == 0.0f,
        "east-west door panels slide along their horizontal long axis");
    Expect(onlyWestPocket.x == -0.75f && onlyWestPocket.z == 0.0f,
        "a one-sided doorway always slides into its actual wall pocket");
    Expect(closedDoor.x == 0.0f && closedDoor.z == 0.0f,
        "a closed door remains at floor-level doorway coordinates");

    Expect(
        WolfCna::SelectHudPortraitState(100, false, false, false, 0) ==
            WolfCna::HudPortraitState::ReadyA &&
        WolfCna::SelectHudPortraitState(100, false, false, false, 1) ==
            WolfCna::HudPortraitState::ReadyB,
        "healthy HUD portrait alternates its two idle expressions");
    Expect(
        WolfCna::SelectHudPortraitState(50, false, false, false, 0) ==
            WolfCna::HudPortraitState::Wounded &&
        WolfCna::SelectHudPortraitState(20, false, false, false, 0) ==
            WolfCna::HudPortraitState::Critical,
        "HUD portrait follows the documented wounded and critical health bands");
    Expect(
        WolfCna::SelectHudPortraitState(100, false, true, false, 0) ==
            WolfCna::HudPortraitState::Attacking &&
        WolfCna::SelectHudPortraitState(100, true, true, false, 0) ==
            WolfCna::HudPortraitState::Hurt &&
        WolfCna::SelectHudPortraitState(100, true, true, true, 0) ==
            WolfCna::HudPortraitState::Defeated,
        "defeat, recent damage and attack states use explicit display priority");

    const WolfCna::WeaponSpec knifeSpec =
        WolfCna::GetWeaponSpec(WolfCna::PlayerWeapon::Knife);
    const WolfCna::WeaponSpec sidearmSpec =
        WolfCna::GetWeaponSpec(WolfCna::PlayerWeapon::Sidearm);
    const WolfCna::WeaponSpec repeaterSpec =
        WolfCna::GetWeaponSpec(WolfCna::PlayerWeapon::Repeater);
    const WolfCna::WeaponSpec heavySpec =
        WolfCna::GetWeaponSpec(WolfCna::PlayerWeapon::HeavyAutomatic);
    Expect(
        knifeSpec.range == 0.9f && !knifeSpec.emitsNoise &&
            sidearmSpec.range == 12.0f && sidearmSpec.nearDamage == 2 &&
            repeaterSpec.automatic && repeaterSpec.cadenceSeconds == 0.12f &&
            heavySpec.automatic && heavySpec.nearDamage == 3 &&
            heavySpec.cadenceSeconds < repeaterSpec.cadenceSeconds,
        "every player weapon has a distinct range, damage, cadence and firing role");
    const std::uint32_t combatSeed = WolfCna::CombatSeedForSector(2, 1);
    const WolfCna::FirearmShot standingShot = WolfCna::ResolveFirearmShot(
        WolfCna::PlayerWeapon::Repeater, 3, combatSeed, 17, false);
    const WolfCna::FirearmShot replayedShot = WolfCna::ResolveFirearmShot(
        WolfCna::PlayerWeapon::Repeater, 3, combatSeed, 17, false);
    const WolfCna::FirearmShot movingShot = WolfCna::ResolveFirearmShot(
        WolfCna::PlayerWeapon::Repeater, 3, combatSeed, 17, true);
    Expect(
        standingShot.emitted && standingShot.ammunitionAfter == 2 &&
            standingShot.sequenceAfter == 18 &&
            standingShot.yawOffsetRadians == replayedShot.yawOffsetRadians &&
            std::abs(standingShot.yawOffsetRadians) <= repeaterSpec.standingSpreadRadians &&
            std::abs(movingShot.yawOffsetRadians) >=
                std::abs(standingShot.yawOffsetRadians),
        "explicit seed and sequence replay spread while movement widens accuracy loss");
    const WolfCna::FirearmShot nextAutomaticShot = WolfCna::ResolveFirearmShot(
        WolfCna::PlayerWeapon::Repeater,
        standingShot.ammunitionAfter,
        combatSeed,
        standingShot.sequenceAfter,
        false);
    const WolfCna::FirearmShot emptyShot = WolfCna::ResolveFirearmShot(
        WolfCna::PlayerWeapon::HeavyAutomatic, 0, combatSeed, 25, false);
    Expect(
        nextAutomaticShot.emitted && nextAutomaticShot.ammunitionAfter == 1 &&
            nextAutomaticShot.sequenceAfter == 19 &&
            nextAutomaticShot.yawOffsetRadians != standingShot.yawOffsetRadians &&
            !emptyShot.emitted &&
            emptyShot.ammunitionAfter == 0 && emptyShot.sequenceAfter == 25,
        "each automatic projectile consumes exactly one round and empty weapons emit none");
    Expect(
        WolfCna::CombatSeedForSector(2, 1) != WolfCna::CombatSeedForSector(3, 1) &&
            WolfCna::CombatSeedForSector(2, 1) != WolfCna::CombatSeedForSector(2, 2),
        "combat seeds are explicit and distinct per sector and difficulty");

    WolfCna::ControlSettings controls;
    Expect(
        WolfCna::AreValidControlSettings(controls) &&
            controls.bindings[WolfCna::ControlIndex(WolfCna::ControlAction::MoveForward)] ==
                WolfCna::Keys::Up &&
            controls.bindings[WolfCna::ControlIndex(WolfCna::ControlAction::TurnLeft)] ==
                WolfCna::Keys::Left &&
            controls.bindings[WolfCna::ControlIndex(WolfCna::ControlAction::Action)] ==
                WolfCna::Keys::Space,
        "default controls preserve classic arrow movement and Space action");
    const WolfCna::RebindResult swappedControl = WolfCna::RebindControl(
        controls,
        WolfCna::ControlAction::MoveForward,
        WolfCna::Keys::A);
    Expect(
        swappedControl.accepted &&
            swappedControl.swappedAction == WolfCna::ControlAction::StrafeLeft &&
            controls.bindings[WolfCna::ControlIndex(WolfCna::ControlAction::MoveForward)] ==
                WolfCna::Keys::A &&
            controls.bindings[WolfCna::ControlIndex(WolfCna::ControlAction::StrafeLeft)] ==
                WolfCna::Keys::Up &&
            WolfCna::AreValidControlSettings(controls),
        "rebinding a used key swaps actions without leaving a conflict");
    const WolfCna::ControlSettings controlsBeforeReservedKey = controls;
    Expect(
        !WolfCna::RebindControl(
            controls,
            WolfCna::ControlAction::Action,
            WolfCna::Keys::P).accepted &&
            controls == controlsBeforeReservedKey,
        "pause and menu-safe keys cannot be assigned to gameplay actions");
    Expect(
        WolfCna::IsControlDown(
            WolfCna::KeyboardState{WolfCna::Keys::RightShift},
            WolfCna::ControlSettings{},
            WolfCna::ControlAction::Run),
        "right Shift activates the normalized default run binding");
    const WolfCna::MovementInput diagonalMovement =
        WolfCna::NormalizeMovementInput({1.0f, 1.0f});
    Expect(
        std::abs(diagonalMovement.forward * diagonalMovement.forward +
                diagonalMovement.strafe * diagonalMovement.strafe - 1.0f) < 0.0001f,
        "diagonal run and strafe input is normalized to direct movement speed");

    const WolfCna::ControlSettings wasdDefaults;
    Expect(
        WolfCna::IsControlDown(
            WolfCna::KeyboardState{WolfCna::Keys::W},
            wasdDefaults,
            WolfCna::ControlAction::MoveForward) &&
            WolfCna::IsControlDown(
                WolfCna::KeyboardState{WolfCna::Keys::S},
                wasdDefaults,
                WolfCna::ControlAction::MoveBackward) &&
            WolfCna::IsControlDown(
                WolfCna::KeyboardState{WolfCna::Keys::Up},
                wasdDefaults,
                WolfCna::ControlAction::MoveForward) &&
            WolfCna::IsControlDown(
                WolfCna::KeyboardState{WolfCna::Keys::Down},
                wasdDefaults,
                WolfCna::ControlAction::MoveBackward),
        "W and S walk without taking the classic arrows away");
    Expect(
        !WolfCna::IsControlDown(
            WolfCna::KeyboardState{WolfCna::Keys::W},
            wasdDefaults,
            WolfCna::ControlAction::MoveBackward) &&
            !WolfCna::IsControlDown(
                WolfCna::KeyboardState{WolfCna::Keys::Q},
                wasdDefaults,
                WolfCna::ControlAction::MoveForward),
        "a secondary key drives only its own action");
    WolfCna::ControlSettings claimAlternate;
    const WolfCna::RebindResult claimed = WolfCna::RebindControl(
        claimAlternate,
        WolfCna::ControlAction::Attack,
        WolfCna::Keys::W);
    Expect(
        claimed.accepted &&
            claimAlternate.alternateBindings[
                WolfCna::ControlIndex(WolfCna::ControlAction::MoveForward)] ==
                WolfCna::Keys::None &&
            WolfCna::AreValidControlSettings(claimAlternate) &&
            !WolfCna::IsControlDown(
                WolfCna::KeyboardState{WolfCna::Keys::W},
                claimAlternate,
                WolfCna::ControlAction::MoveForward),
        "rebinding onto a secondary key releases it instead of driving two actions");
    WolfCna::ControlSettings duplicateAlternate;
    duplicateAlternate.alternateBindings[
        WolfCna::ControlIndex(WolfCna::ControlAction::TurnLeft)] = WolfCna::Keys::W;
    WolfCna::ControlSettings shadowingAlternate;
    shadowingAlternate.alternateBindings[
        WolfCna::ControlIndex(WolfCna::ControlAction::Run)] = WolfCna::Keys::Up;
    Expect(
        !WolfCna::AreValidControlSettings(duplicateAlternate) &&
            !WolfCna::AreValidControlSettings(shadowingAlternate),
        "a secondary key may not duplicate another secondary or any primary binding");

    const WolfCna::ControlSettings defaultMouse;
    Expect(
        defaultMouse.mouseEnabled &&
            defaultMouse.mouseSensitivityStep == WolfCna::DefaultMouseSensitivityStep &&
            WolfCna::MouseSensitivityPercent(defaultMouse.mouseSensitivityStep) == 100,
        "mouse control defaults to enabled at unscaled sensitivity");
    Expect(
        WolfCna::MouseYawDeltaRadians(120, defaultMouse) > 0.0f &&
            WolfCna::MouseYawDeltaRadians(-120, defaultMouse) ==
                -WolfCna::MouseYawDeltaRadians(120, defaultMouse) &&
            WolfCna::MouseYawDeltaRadians(0, defaultMouse) == 0.0f,
        "mouse yaw follows pointer direction symmetrically around a still pointer");
    WolfCna::ControlSettings slowMouse = defaultMouse;
    slowMouse.mouseSensitivityStep = 0;
    WolfCna::ControlSettings fastMouse = defaultMouse;
    fastMouse.mouseSensitivityStep = WolfCna::MaximumMouseSensitivityStep;
    Expect(
        WolfCna::MouseYawDeltaRadians(120, slowMouse) <
                WolfCna::MouseYawDeltaRadians(120, defaultMouse) &&
            WolfCna::MouseYawDeltaRadians(120, defaultMouse) <
                WolfCna::MouseYawDeltaRadians(120, fastMouse) &&
            WolfCna::MouseSensitivityPercent(0) == 40 &&
            WolfCna::MouseSensitivityPercent(WolfCna::MaximumMouseSensitivityStep) == 160,
        "every mouse sensitivity step strictly increases the yaw one count produces");
    WolfCna::ControlSettings disabledMouse = defaultMouse;
    disabledMouse.mouseEnabled = false;
    Expect(
        WolfCna::MouseYawDeltaRadians(400, disabledMouse) == 0.0f,
        "disabling mouse control leaves the keyboard turn keys as the only yaw source");
    Expect(
        WolfCna::MouseYawDeltaRadians(100000, defaultMouse) ==
                WolfCna::MaximumMouseYawRadiansPerFrame &&
            WolfCna::MouseYawDeltaRadians(-100000, fastMouse) ==
                -WolfCna::MaximumMouseYawRadiansPerFrame,
        "a displacement spike cannot turn more than half a circle in a single frame");
    // A 180-degree flick is ~1428 counts. Splitting it across frames must deliver the same
    // rotation as one burst, or fast aiming would silently lose travel and the maximum turn
    // rate would depend on the frame rate.
    const float flickInOneFrame = WolfCna::MouseYawDeltaRadians(1428, defaultMouse);
    float flickAcrossFrames = 0.0f;
    for (int frame = 0; frame < 6; ++frame)
        flickAcrossFrames += WolfCna::MouseYawDeltaRadians(238, defaultMouse);
    Expect(
        std::abs(flickInOneFrame - 3.14159f) < 0.01f &&
            std::abs(flickAcrossFrames - flickInOneFrame) < 0.001f,
        "equal hand travel yaws equally whether it arrives in one frame or across six");
    Expect(
        std::abs(WolfCna::WrapYawRadians(
                100.0f * WolfCna::MaximumMouseYawRadiansPerFrame + 0.25f) - 0.25f) < 0.001f &&
            std::abs(WolfCna::WrapYawRadians(-0.25f) + 0.25f) < 0.001f &&
            std::abs(WolfCna::WrapYawRadians(0.25f) - 0.25f) < 0.001f,
        "yaw wraps to a bounded range so long sessions keep constant aim precision");
    Expect(
        defaultMouse.mouseButtons[0] == WolfCna::MouseButtonAction::Attack &&
            defaultMouse.mouseButtons[1] == WolfCna::MouseButtonAction::Action &&
            defaultMouse.mouseButtons[2] == WolfCna::MouseButtonAction::StrafeModifier,
        "mouse buttons default to the original attack, use and strafe assignment");
    Expect(
        !defaultMouse.mouseYMovesForward &&
            WolfCna::MouseForwardAxis(50, defaultMouse) == 0.0f,
        "vertical mouse travel is ignored until the classic behaviour is switched on");
    WolfCna::ControlSettings verticalMouse = defaultMouse;
    verticalMouse.mouseYMovesForward = true;
    Expect(
        WolfCna::MouseForwardAxis(-20, verticalMouse) > 0.0f &&
            WolfCna::MouseForwardAxis(20, verticalMouse) < 0.0f,
        "pushing the mouse away moves forward and pulling it back moves backward");
    Expect(
        WolfCna::MouseForwardAxis(-100000, verticalMouse) == 1.0f &&
            WolfCna::MouseForwardAxis(100000, verticalMouse) == -1.0f &&
            WolfCna::MouseStrafeAxis(100000, defaultMouse) == 1.0f,
        "mouse movement axes saturate instead of exceeding full stick deflection");
    Expect(
        std::abs(WolfCna::MouseForwardAxis(-40, verticalMouse)) ==
                2.0f * std::abs(WolfCna::MouseStrafeAxis(40, verticalMouse)),
        "vertical travel drives movement at twice the horizontal gain, as in 1992");
    Expect(
        WolfCna::MouseStrafeAxis(40, disabledMouse) == 0.0f &&
            WolfCna::MouseForwardAxis(-40, disabledMouse) == 0.0f,
        "switching the mouse off silences its movement axes as well as its yaw");
    WolfCna::ControlSettings invalidButtons = defaultMouse;
    invalidButtons.mouseButtons[1] = static_cast<WolfCna::MouseButtonAction>(97);
    Expect(
        !WolfCna::AreValidControlSettings(invalidButtons),
        "an unknown persisted mouse-button action is rejected");
    WolfCna::ControlSettings invalidMouse = defaultMouse;
    invalidMouse.mouseSensitivityStep = WolfCna::MaximumMouseSensitivityStep + 1;
    WolfCna::ControlSettings negativeMouse = defaultMouse;
    negativeMouse.mouseSensitivityStep = -1;
    Expect(
        !WolfCna::AreValidControlSettings(invalidMouse) &&
            !WolfCna::AreValidControlSettings(negativeMouse) &&
            WolfCna::AreValidControlSettings(defaultMouse),
        "out-of-range mouse sensitivity is rejected alongside the other control settings");

    Expect(WolfCna::CompletionPercentage(3, 4) == 75, "completion percentage uses integer progress");
    Expect(WolfCna::CompletionPercentage(0, 0) == 100, "empty completion categories count as perfect");
    const WolfCna::CompletionScore mixedCompletion = WolfCna::CalculateCompletionScore(
        8, 8, 2, 4, 0, 1, 100.1f, 120);
    Expect(
        mixedCompletion.killPercentage == 100 &&
            mixedCompletion.treasurePercentage == 50 &&
            mixedCompletion.secretPercentage == 0 &&
            mixedCompletion.elapsedSeconds == 101 &&
            mixedCompletion.timeBonus == 380 &&
            mixedCompletion.killPerfectBonus == 1500 &&
            mixedCompletion.treasurePerfectBonus == 0 &&
            mixedCompletion.secretPerfectBonus == 0 &&
            mixedCompletion.totalBonus == 2880,
        "completion scoring combines clear, rounded-time and perfect-category bonuses");
    const WolfCna::CompletionScore perfectCompletion = WolfCna::CalculateCompletionScore(
        4, 4, 3, 3, 2, 2, 180.0f, 180);
    Expect(
        perfectCompletion.timeBonus == 0 && perfectCompletion.totalBonus == 5500,
        "perfect categories award all three bonuses at the target-time boundary");
    const WolfCna::CompletionScore slowCompletion = WolfCna::CalculateCompletionScore(
        0, 4, 0, 3, 0, 2, 181.0f, 180);
    Expect(
        slowCompletion.totalBonus == 1000 && slowCompletion.timeBonus == 0,
        "finishing beyond target time retains only the base clear award");

    std::vector<WolfCna::HighScoreEntry> scoreTable{
        {"CCC", 3000}, {"AAA", 5000}, {"BAD", -2}, {"BB", 9000}, {"BBB", 3000}};
    scoreTable = WolfCna::NormalizeHighScores(std::move(scoreTable));
    Expect(
        scoreTable.size() == 3 && scoreTable[0] == WolfCna::HighScoreEntry{"AAA", 5000} &&
            scoreTable[1] == WolfCna::HighScoreEntry{"CCC", 3000} &&
            scoreTable[2] == WolfCna::HighScoreEntry{"BBB", 3000},
        "high scores reject malformed rows, sort descending and preserve tie order");
    std::vector<WolfCna::HighScoreEntry> fullScoreTable;
    for (int index = 0; index < 8; ++index)
        fullScoreTable.push_back({"AAA", 8000 - index * 1000});
    Expect(
        !WolfCna::QualifiesForHighScores(fullScoreTable, 1000) &&
            WolfCna::QualifiesForHighScores(fullScoreTable, 1001),
        "a full high-score table requires strictly beating its final entry");
    fullScoreTable = WolfCna::InsertHighScore(
        std::move(fullScoreTable),
        WolfCna::HighScoreEntry{"NEW", 8500});
    Expect(
        fullScoreTable.size() == WolfCna::MaximumHighScoreEntries &&
            fullScoreTable.front() == WolfCna::HighScoreEntry{"NEW", 8500} &&
            fullScoreTable.back().score == 2000,
        "inserting a qualifying score keeps the bounded best eight entries");

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
    constexpr WolfCna::DifficultyProfile phantomProfile =
        WolfCna::GetDifficultyProfile(WolfCna::Difficulty::Phantom);
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
    const WolfCna::CampaignProfile fieldOfViewProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-4\n1\n3\n2\n84\n", 3);
    Expect(
        fieldOfViewProfile.highestUnlocked == 1 &&
            fieldOfViewProfile.soundVolume == 3 &&
            fieldOfViewProfile.difficulty == 2 &&
            fieldOfViewProfile.fieldOfView == 84 &&
            fieldOfViewProfile.highScores.empty(),
        "version four profile migrates settings with an empty high-score table");
    const WolfCna::CampaignProfile highScoreProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-5\n1\n3\n2\n84\n1\nACE 4200\n",
        3);
    Expect(
        highScoreProfile.highestUnlocked == 1 &&
            highScoreProfile.controls == WolfCna::ControlSettings{} &&
            highScoreProfile.highScores ==
                std::vector<WolfCna::HighScoreEntry>{{"ACE", 4200}},
        "version five high scores migrate with classic control defaults");
    WolfCna::ControlSettings savedControls;
    savedControls.turnSensitivityStep = 4;
    static_cast<void>(WolfCna::RebindControl(
        savedControls,
        WolfCna::ControlAction::Map,
        WolfCna::Keys::M));
    const WolfCna::CampaignProfile savedProfile = WolfCna::CampaignProgress::Parse(
        WolfCna::CampaignProgress::Serialize(
            WolfCna::CampaignProfile{
                .highestUnlocked = 8,
                .soundVolume = 3,
                .difficulty = 2,
                .fieldOfView = 84,
                .controls = savedControls,
                .highScores = {{"LOW", 1200}, {"TOP", 9800}, {"BAD", -1}}},
            3),
        3);
    Expect(
        savedProfile.highestUnlocked == 2 && savedProfile.soundVolume == 3 &&
            savedProfile.difficulty == 2 && savedProfile.fieldOfView == 84 &&
            savedProfile.controls == savedControls &&
            savedProfile.highScores == std::vector<WolfCna::HighScoreEntry>{{"TOP", 9800}, {"LOW", 1200}},
        "campaign profile restores controls, settings and a normalized high-score table");
    WolfCna::CampaignProfile phantomSource;
    phantomSource.highestUnlocked = 2;
    phantomSource.difficulty = static_cast<int>(WolfCna::Difficulty::Phantom);
    const WolfCna::CampaignProfile phantomRoundTrip = WolfCna::CampaignProgress::Parse(
        WolfCna::CampaignProgress::Serialize(phantomSource, 3),
        3);
    Expect(
        phantomRoundTrip.difficulty == 3,
        "the fourth difficulty survives a profile round trip");
    const WolfCna::CampaignProfile outOfRangeDifficulty = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-4\n1\n3\n4\n84\n", 3);
    Expect(
        outOfRangeDifficulty.highestUnlocked == 0 && outOfRangeDifficulty.difficulty == 1,
        "a difficulty past the last rung is still rejected");
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
    const WolfCna::CampaignProfile invalidHighScoreProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-5\n2\n4\n1\n72\n1\nA!A 5000\n",
        3);
    Expect(
        invalidHighScoreProfile.highestUnlocked == 0 &&
            invalidHighScoreProfile.highScores.empty(),
        "malformed initials invalidate the new profile without accepting partial scores");
    const WolfCna::CampaignProfile duplicateControlProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-6\n2\n4\n1\n72\n2\n10\n"
        "0 38\n1 40\n2 37\n3 39\n4 65\n5 65\n6 160\n7 32\n8 162\n9 9\n0\n",
        3);
    Expect(
        duplicateControlProfile.highestUnlocked == 0 &&
            duplicateControlProfile.controls == WolfCna::ControlSettings{},
        "duplicate persisted bindings invalidate the profile and restore classic controls");

    constexpr std::string_view classicBindings =
        "0 38\n1 40\n2 37\n3 39\n4 65\n5 68\n6 160\n7 32\n8 162\n9 9\n";
    const WolfCna::CampaignProfile controlOnlyProfile = WolfCna::CampaignProgress::Parse(
        std::string("WOLF-CNA-PROGRESS-6\n2\n4\n1\n72\n3\n10\n") +
            std::string(classicBindings) + "1\nACE 4200\n",
        3);
    WolfCna::ControlSettings migratedControls;
    migratedControls.turnSensitivityStep = 3;
    Expect(
        controlOnlyProfile.highestUnlocked == 2 &&
            controlOnlyProfile.controls == migratedControls &&
            controlOnlyProfile.controls.mouseEnabled &&
            controlOnlyProfile.controls.mouseSensitivityStep ==
                WolfCna::DefaultMouseSensitivityStep &&
            controlOnlyProfile.highScores ==
                std::vector<WolfCna::HighScoreEntry>{{"ACE", 4200}},
        "version six profiles keep their bindings and adopt the default mouse settings");
    WolfCna::ControlSettings mouseControls;
    mouseControls.turnSensitivityStep = 3;
    mouseControls.mouseEnabled = false;
    mouseControls.mouseSensitivityStep = 4;
    const WolfCna::CampaignProfile mouseProfile = WolfCna::CampaignProgress::Parse(
        WolfCna::CampaignProgress::Serialize(
            WolfCna::CampaignProfile{
                .highestUnlocked = 1,
                .soundVolume = 2,
                .difficulty = 0,
                .fieldOfView = 96,
                .controls = mouseControls,
                .highScores = {}},
            3),
        3);
    Expect(
        mouseProfile.controls == mouseControls &&
            !mouseProfile.controls.mouseEnabled &&
            mouseProfile.controls.mouseSensitivityStep == 4 &&
            mouseProfile.fieldOfView == 96,
        "a disabled mouse and its sensitivity survive a profile round trip");
    // Version 10 shipped a view-size step whose feature was reverted. Profiles in that
    // format exist on disk, so it must still load with everything else intact.
    const WolfCna::CampaignProfile revertedViewSizeProfile = WolfCna::CampaignProgress::Parse(
        std::string("WOLF-CNA-PROGRESS-10\n1\n3\n2\n84\n2\n1\n2\n0\n3\n1\n3\n2\n10\n") +
            std::string(classicBindings) +
            "10\n0 87\n1 83\n2 0\n3 0\n4 0\n5 0\n6 0\n7 0\n8 0\n9 0\n" + "2\n0\n",
        3);
    Expect(
        revertedViewSizeProfile.highestUnlocked == 1 &&
            revertedViewSizeProfile.soundVolume == 3 &&
            revertedViewSizeProfile.difficulty == 2 &&
            revertedViewSizeProfile.fieldOfView == 84 &&
            revertedViewSizeProfile.controls.alternateBindings[
                WolfCna::ControlIndex(WolfCna::ControlAction::MoveForward)] ==
                WolfCna::Keys::W,
        "a profile written by the reverted view-size version still loads its settings");

    // Version 8 stored primaries but no secondaries. A profile that already moved forward
    // with W must not also receive W as a secondary somewhere else.
    constexpr std::string_view wKeyBindings =
        "0 87\n1 40\n2 37\n3 39\n4 65\n5 68\n6 160\n7 32\n8 162\n9 9\n";
    const WolfCna::CampaignProfile wasdMigrationProfile = WolfCna::CampaignProgress::Parse(
        std::string("WOLF-CNA-PROGRESS-8\n1\n4\n1\n72\n2\n1\n2\n0\n3\n1\n3\n2\n10\n") +
            std::string(wKeyBindings) + "0\n",
        3);
    Expect(
        wasdMigrationProfile.highestUnlocked == 1 &&
            wasdMigrationProfile.controls.bindings[
                WolfCna::ControlIndex(WolfCna::ControlAction::MoveForward)] ==
                WolfCna::Keys::W &&
            wasdMigrationProfile.controls.alternateBindings[
                WolfCna::ControlIndex(WolfCna::ControlAction::MoveForward)] ==
                WolfCna::Keys::None &&
            wasdMigrationProfile.controls.alternateBindings[
                WolfCna::ControlIndex(WolfCna::ControlAction::MoveBackward)] ==
                WolfCna::Keys::S &&
            WolfCna::AreValidControlSettings(wasdMigrationProfile.controls),
        "migration only grants a secondary key where the stored layout leaves it free");
    const WolfCna::CampaignProfile mouseLookOnlyProfile = WolfCna::CampaignProgress::Parse(
        std::string("WOLF-CNA-PROGRESS-7\n2\n4\n1\n72\n3\n0\n4\n10\n") +
            std::string(classicBindings) + "0\n",
        3);
    Expect(
        mouseLookOnlyProfile.highestUnlocked == 2 &&
            !mouseLookOnlyProfile.controls.mouseEnabled &&
            mouseLookOnlyProfile.controls.mouseSensitivityStep == 4 &&
            !mouseLookOnlyProfile.controls.mouseYMovesForward &&
            mouseLookOnlyProfile.controls.mouseButtons ==
                WolfCna::ControlSettings{}.mouseButtons,
        "version seven keeps its look settings and adopts the classic button assignment");
    WolfCna::ControlSettings remappedButtons;
    remappedButtons.mouseYMovesForward = true;
    remappedButtons.mouseButtons = {
        WolfCna::MouseButtonAction::Run,
        WolfCna::MouseButtonAction::None,
        WolfCna::MouseButtonAction::Attack};
    const WolfCna::CampaignProfile remappedProfile = WolfCna::CampaignProgress::Parse(
        WolfCna::CampaignProgress::Serialize(
            WolfCna::CampaignProfile{
                .highestUnlocked = 1,
                .soundVolume = 2,
                .difficulty = 0,
                .fieldOfView = 60,
                .controls = remappedButtons,
                .highScores = {}},
            3),
        3);
    Expect(
        remappedProfile.controls == remappedButtons &&
            remappedProfile.controls.mouseYMovesForward &&
            remappedProfile.controls.mouseButtons[2] == WolfCna::MouseButtonAction::Attack,
        "reassigned mouse buttons and the vertical axis survive a profile round trip");
    const WolfCna::CampaignProfile invalidMouseFlagProfile = WolfCna::CampaignProgress::Parse(
        std::string("WOLF-CNA-PROGRESS-8\n2\n4\n1\n72\n3\n2\n2\n0\n3\n1\n3\n2\n10\n") +
            std::string(classicBindings) + "0\n",
        3);
    const WolfCna::CampaignProfile invalidMouseSpeedProfile = WolfCna::CampaignProgress::Parse(
        std::string("WOLF-CNA-PROGRESS-8\n2\n4\n1\n72\n3\n1\n9\n0\n3\n1\n3\n2\n10\n") +
            std::string(classicBindings) + "0\n",
        3);
    const WolfCna::CampaignProfile invalidButtonProfile = WolfCna::CampaignProgress::Parse(
        std::string("WOLF-CNA-PROGRESS-8\n2\n4\n1\n72\n3\n1\n2\n0\n3\n1\n97\n2\n10\n") +
            std::string(classicBindings) + "0\n",
        3);
    Expect(
        invalidButtonProfile.highestUnlocked == 0 &&
            invalidButtonProfile.controls == WolfCna::ControlSettings{},
        "an out-of-range persisted button action invalidates the whole profile");
    Expect(
        invalidMouseFlagProfile.highestUnlocked == 0 &&
            invalidMouseFlagProfile.controls == WolfCna::ControlSettings{} &&
            invalidMouseSpeedProfile.highestUnlocked == 0 &&
            invalidMouseSpeedProfile.controls == WolfCna::ControlSettings{},
        "corrupt mouse fields invalidate the profile instead of loading a broken sensitivity");

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
    // Music is now one track per sector rather than one per chapter, so this checks the
    // range and that the sectors genuinely spread across the tracks instead of sharing one.
    {
        std::vector<int> sectorThemes;
        for (const WolfCna::CampaignSector& musicSector : WolfCna::CampaignSectors)
            sectorThemes.push_back(musicSector.audioTheme);
        const bool inRange = std::all_of(
            sectorThemes.begin(),
            sectorThemes.end(),
            [](int theme) { return theme >= 0 && theme < 5; });
        std::vector<int> distinct = sectorThemes;
        std::sort(distinct.begin(), distinct.end());
        distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
        Expect(
            inRange && distinct.size() >= 4 &&
                WolfCna::GetCampaignSector(5).audioTheme == 4,
            "each sector carries its own music track and the boss sector its own");
    }
    Expect(
        std::all_of(
            WolfCna::CampaignSectors.begin(),
            WolfCna::CampaignSectors.end(),
            [](const WolfCna::CampaignSector& sector) { return sector.targetSeconds > 0; }) &&
            WolfCna::GetCampaignSector(0).targetSeconds <
                WolfCna::GetCampaignSector(3).targetSeconds,
        "every sector has an authored positive target time with longer late-sector pacing");
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
    Expect(starterLevel.PlayerStartX() == 14 && starterLevel.PlayerStartZ() == 5, "starter spawn");

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
        Expect(
            CanReachCampaignExitWithAuthoredAccess(*campaignLevel),
            "campaign exit remains reachable while respecting both access-card colors");
        const std::string campaignSymbols = [&]()
        {
            std::string symbols;
            for (const std::string& row : campaignLevel->Rows())
                symbols += row;
            return symbols;
        }();
        Expect(
            campaignSymbols.find('p') != std::string::npos &&
                campaignSymbols.find('H') != std::string::npos &&
                campaignSymbols.find('h') != std::string::npos,
            "each campaign sector includes the fourth treasure and both health sizes");
        if (campaignSymbols.find('Q') != std::string::npos)
            Expect(campaignSymbols.find('C') != std::string::npos,
                "every cyan lock has an authored cyan card");
        if (campaignSymbols.find('q') != std::string::npos)
            Expect(campaignSymbols.find('c') != std::string::npos,
                "every amber lock has an authored amber card");

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
        const int scoutClearRounds = (scout.totalEnemyHealth + scout.activeEnemies) / 2;
        const int operativeClearRounds =
            (operative.totalEnemyHealth + operative.activeEnemies) / 2;
        const int veteranClearRounds = (veteran.totalEnemyHealth + veteran.activeEnemies) / 2;
        // A margin, not a bare pass. The budget used to land exactly on zero for Veteran,
        // which left no room to tune the ladder upwards: one extra enemy or a slightly
        // lower multiplier immediately made a sector unclearable.
        Expect(
            scoutSupply >= scoutClearRounds + 15 &&
                operativeSupply >= operativeClearRounds + 15 &&
                veteranSupply >= veteranClearRounds + 15,
            "every difficulty keeps a working margin over its close-sidearm clear budget");

        // The eight-step tier cycle reserves the full authored roster for the top rung, so
        // enemy count now rises across all four rungs and each must keep its own margin.
        const WolfCna::World phantomWorld(*campaignLevel, WolfCna::Difficulty::Phantom);
        const WolfCna::World::DifficultyBalance phantom = phantomWorld.GetDifficultyBalance();
        const int phantomSupply = phantomProfile.startingAmmunition +
            phantom.fixedAmmunition + phantom.potentialDroppedAmmunition;
        const int phantomClearRounds = (phantom.totalEnemyHealth + phantom.activeEnemies) / 2;
        Expect(
            phantom.activeEnemies > veteran.activeEnemies &&
                phantom.totalEnemyHealth > veteran.totalEnemyHealth &&
                phantomSupply >= phantomClearRounds + 15,
            "the top rung faces the whole roster and keeps a working margin");
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
        focusedScout.totalEnemyHealth == 8 && focusedOperative.totalEnemyHealth == 15 &&
            focusedVeteran.totalEnemyHealth == 24,
        "focused enemy health is scaled after spawn-tier selection");
    Expect(
        focusedScout.fixedAmmunition == 26 && focusedOperative.fixedAmmunition == 16 &&
            focusedVeteran.fixedAmmunition == 12,
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
            shots += cadenceWorld.ConsumeRangedShotCount();
        }
        return shots;
    };
    const int scoutShots = countGuardShots(WolfCna::Difficulty::Scout);
    const int operativeShots = countGuardShots(WolfCna::Difficulty::Operative);
    const int veteranShots = countGuardShots(WolfCna::Difficulty::Veteran);
    Expect(
        scoutShots < operativeShots && operativeShots < veteranShots,
        "guard firing frequency increases monotonically with difficulty");

    Expect(
        WolfCna::DifficultyCount == 4 &&
            std::string_view(WolfCna::DifficultyName(WolfCna::Difficulty::Phantom)) ==
                "PHANTOM" &&
            WolfCna::IsValidDifficultyValue(3) && !WolfCna::IsValidDifficultyValue(4) &&
            !WolfCna::IsValidDifficultyValue(-1),
        "the classic four-rung difficulty ladder is complete and range-checked");
    Expect(
        phantomProfile.incomingDamageMultiplier > veteranProfile.incomingDamageMultiplier &&
            phantomProfile.enemySpeedMultiplier > veteranProfile.enemySpeedMultiplier &&
            phantomProfile.enemyAttackIntervalMultiplier <
                veteranProfile.enemyAttackIntervalMultiplier &&
            phantomProfile.reactionDelayMultiplier <
                veteranProfile.reactionDelayMultiplier &&
            phantomProfile.hearingRangeMultiplier >
                veteranProfile.hearingRangeMultiplier &&
            phantomProfile.maximumRangedAttackers >
                veteranProfile.maximumRangedAttackers,
        "phantom escalates every behavioural lever beyond veteran");
    Expect(
        phantomProfile.maximumEnemySpawnTier > veteranProfile.maximumEnemySpawnTier &&
            veteranProfile.maximumEnemySpawnTier >
                operativeProfile.maximumEnemySpawnTier &&
            operativeProfile.maximumEnemySpawnTier > scoutProfile.maximumEnemySpawnTier,
        "every rung faces a strictly larger share of each level's authored encounters");
    Expect(
        scoutProfile.reactionDelayMultiplier > operativeProfile.reactionDelayMultiplier &&
            operativeProfile.reactionDelayMultiplier >
                veteranProfile.reactionDelayMultiplier &&
            scoutProfile.hearingRangeMultiplier < operativeProfile.hearingRangeMultiplier &&
            operativeProfile.hearingRangeMultiplier <
                veteranProfile.hearingRangeMultiplier,
        "enemies notice the player faster and hear further as difficulty rises");
    Expect(
        scoutProfile.maximumRangedAttackers <= operativeProfile.maximumRangedAttackers &&
            operativeProfile.maximumRangedAttackers <
                veteranProfile.maximumRangedAttackers,
        "more ranged enemies may fire at once as difficulty rises");

    // Two guards with line of sight. The throttle used to pin every difficulty to one
    // shooter, so extra spawns never became extra incoming fire; Veteran must now out-shoot
    // Operative by more than its cadence alone would explain.
    const WolfCna::LevelDefinition crossfireLevel = WolfCna::LevelDefinition::Parse(
        "######\n#PGG.#\n######\n",
        "difficulty-crossfire.level");
    const auto countCrossfireShots = [&crossfireLevel](WolfCna::Difficulty difficulty)
    {
        WolfCna::World crossfireWorld(crossfireLevel, difficulty);
        const Microsoft::Xna::Framework::Vector3 target(1.5f, 0.62f, 1.5f);
        int shots = 0;
        for (int tick = 0; tick < 120; ++tick)
        {
            static_cast<void>(crossfireWorld.Update(0.05f, target));
            shots += crossfireWorld.ConsumeRangedShotCount();
        }
        return shots;
    };
    // The compass needs a goal to point at in every sector, and the position has to be
    // the elevator itself rather than its approach cell.
    for (const WolfCna::CampaignSector& compassSector : WolfCna::CampaignSectors)
    {
        const WolfCna::LevelDefinition compassLevel =
            WolfCna::LevelDefinition::LoadFromFile(std::string(compassSector.file));
        const WolfCna::World compassWorld(compassLevel, WolfCna::Difficulty::Operative);
        const auto goal = compassWorld.GetExitPosition();
        Expect(
            goal.has_value() &&
                goal->X > 0.0f && goal->X < 64.0f &&
                goal->Z > 0.0f && goal->Z < 64.0f,
            "every sector reports an exit position for the compass to bear on");
    }

    Expect(
        WolfCna::World::PropTypeForSymbol('0') == WolfCna::World::PropType::SteelDrum &&
            WolfCna::World::PropTypeForSymbol('9') ==
                WolfCna::World::PropType::ArchiveCabinet &&
            WolfCna::World::PropTypeForSymbol('s') ==
                WolfCna::World::PropType::RubblePile &&
            !WolfCna::World::PropTypeForSymbol('.').has_value() &&
            !WolfCna::World::PropTypeForSymbol('G').has_value() &&
            !WolfCna::World::PropTypeForSymbol('S').has_value(),
        "prop symbols map to prop types and nothing else does");
    Expect(
        WolfCna::World::IsSolidPropSymbol('0') && WolfCna::World::IsSolidPropSymbol('9') &&
            WolfCna::World::IsSolidPropSymbol('Y') &&
            WolfCna::World::IsSolidPropSymbol('s') &&
            !WolfCna::World::IsSolidPropSymbol('.') &&
            !WolfCna::World::IsSolidPropSymbol('S') &&
            !WolfCna::World::IsSolidPropSymbol('H'),
        "props block like the authored table while floor and pickups stay walkable");

    const WolfCna::LevelDefinition propLevel = WolfCna::LevelDefinition::Parse(
        "#####\n#P.0#\n#...#\n#####\n",
        "prop.level");
    const WolfCna::World propWorld(propLevel, WolfCna::Difficulty::Operative);
    Expect(
        propWorld.Props().size() == 1 &&
            propWorld.Props().front().type == WolfCna::World::PropType::SteelDrum,
        "a prop symbol becomes exactly one placed prop");
    Expect(
        propWorld.Collides(3.5f, 1.5f, 0.22f) &&
            !propWorld.Collides(1.5f, 2.5f, 0.22f),
        "a prop cell blocks the player while the floor beside it stays clear");
    // The billboard quad runs from y=0 to y=1, so its origin is its bottom edge and the
    // prop position must be the floor. Offsetting by half the height left every prop
    // hovering in mid-air.
    Expect(
        propWorld.Props().front().position.Y == 0.0f,
        "props stand on the floor rather than floating above it");

    // Placement must never plug a corridor: every authored sector still has to be
    // walkable end to end, which the route audits above already prove, so this only
    // pins that props are actually present in every one of them.
    for (const WolfCna::CampaignSector& propSector : WolfCna::CampaignSectors)
    {
        const WolfCna::LevelDefinition sectorLevel =
            WolfCna::LevelDefinition::LoadFromFile(std::string(propSector.file));
        const WolfCna::World sectorWorld(sectorLevel, WolfCna::Difficulty::Operative);
        const std::size_t rubble = static_cast<std::size_t>(std::count_if(
            sectorWorld.Props().begin(),
            sectorWorld.Props().end(),
            [](const WolfCna::World::Prop& prop)
            {
                return prop.type == WolfCna::World::PropType::RubblePile;
            }));
        Expect(
            sectorWorld.Props().size() >= 6 && rubble >= 2,
            "every campaign sector is furnished with solid props including rubble");

        // A prop must stand in open floor, never in a corridor. Counting open neighbours
        // is not enough: a one-cell corridor with a side branch has three of them, and
        // that is exactly how a rubble pile ended up plugging a passage. Requiring the
        // whole 3x3 block to be plain floor makes a corridor placement impossible, and
        // the route audits cannot catch this because an alternative way around leaves
        // every cell still reachable.
        const std::vector<std::string>& propRows = sectorLevel.Rows();
        for (const WolfCna::World::Prop& placed : sectorWorld.Props())
        {
            const int propX = static_cast<int>(placed.position.X);
            const int propZ = static_cast<int>(placed.position.Z);
            bool openArea = propX >= 1 && propZ >= 1 &&
                propZ + 1 < static_cast<int>(propRows.size()) &&
                propX + 1 < static_cast<int>(propRows[static_cast<std::size_t>(propZ)].size());
            for (int dz = -1; openArea && dz <= 1; ++dz)
            {
                for (int dx = -1; openArea && dx <= 1; ++dx)
                {
                    if (dx == 0 && dz == 0)
                        continue;
                    const char neighbour =
                        propRows[static_cast<std::size_t>(propZ + dz)]
                            [static_cast<std::size_t>(propX + dx)];
                    if (neighbour != '.')
                        openArea = false;
                }
            }
            Expect(openArea, "every prop stands in open floor and never blocks a corridor");
        }
    }

    // A wide room whose guards sit beyond the base engagement range, so this measures the
    // whole package -- reach, reaction, cadence, damage and simultaneous shooters -- rather
    // than any single multiplier. The top rungs must land clearly more damage here, which is
    // the complaint this pins: Phantom looked harsher on paper without feeling harder.
    const WolfCna::LevelDefinition pressureLevel = WolfCna::LevelDefinition::Parse(
        "##############\n#P...G...G..G#\n#....G...G..G#\n##############\n",
        "difficulty-pressure.level");
    const auto measurePressure = [&pressureLevel](WolfCna::Difficulty difficulty)
    {
        WolfCna::World pressureWorld(pressureLevel, difficulty);
        const Microsoft::Xna::Framework::Vector3 target(1.5f, 0.62f, 1.5f);
        int damage = 0;
        for (int tick = 0; tick < 200; ++tick)
            damage += pressureWorld.Update(0.05f, target);
        return damage;
    };
    const int operativePressure = measurePressure(WolfCna::Difficulty::Operative);
    const int veteranPressure = measurePressure(WolfCna::Difficulty::Veteran);
    const int phantomPressure = measurePressure(WolfCna::Difficulty::Phantom);
    Expect(
        operativePressure < veteranPressure &&
            phantomPressure > veteranPressure * 3 / 2,
        "each rung lands clearly more damage under sustained pressure");
    Expect(
        scoutProfile.startingLives > operativeProfile.startingLives &&
            operativeProfile.startingLives == veteranProfile.startingLives &&
            veteranProfile.startingLives > phantomProfile.startingLives &&
            scoutProfile.healthPickupMultiplier >
                operativeProfile.healthPickupMultiplier &&
            operativeProfile.healthPickupMultiplier >
                veteranProfile.healthPickupMultiplier &&
            veteranProfile.healthPickupMultiplier >
                phantomProfile.healthPickupMultiplier,
        "player resilience falls as difficulty rises, through kits and attempts");

    // Behavioural, not just the table: the same authored kit must actually hand back
    // fewer points on a harder rung. Starting health stays 100 everywhere, as in 1992.
    const WolfCna::LevelDefinition kitLevel = WolfCna::LevelDefinition::Parse(
        "#####\n#P.H#\n#####\n",
        "health-scaling.level");
    const auto kitValue = [&kitLevel](WolfCna::Difficulty difficulty)
    {
        WolfCna::World kitWorld(kitLevel, difficulty);
        const WolfCna::World::PickupResult taken =
            kitWorld.CollectPickups(Microsoft::Xna::Framework::Vector3(3.5f, 0.0f, 1.5f), 60);
        return taken.health;
    };
    const int scoutKit = kitValue(WolfCna::Difficulty::Scout);
    const int operativeKit = kitValue(WolfCna::Difficulty::Operative);
    const int veteranKit = kitValue(WolfCna::Difficulty::Veteran);
    const int phantomKit = kitValue(WolfCna::Difficulty::Phantom);
    Expect(
        scoutKit > operativeKit && operativeKit > veteranKit &&
            veteranKit > phantomKit && phantomKit > 0,
        "one authored health kit restores strictly less on each harder rung");

    Expect(
        phantomProfile.startingLives < veteranProfile.startingLives &&
            phantomProfile.healthPickupMultiplier <
                veteranProfile.healthPickupMultiplier &&
            veteranProfile.wakeRangeMultiplier > operativeProfile.wakeRangeMultiplier &&
            phantomProfile.wakeRangeMultiplier > veteranProfile.wakeRangeMultiplier,
        "the top rungs also cut the player's margin and engage from further out");

    const int operativeCrossfire = countCrossfireShots(WolfCna::Difficulty::Operative);
    const int veteranCrossfire = countCrossfireShots(WolfCna::Difficulty::Veteran);
    Expect(
        veteranCrossfire > operativeCrossfire &&
            veteranCrossfire - operativeCrossfire > veteranShots - operativeShots,
        "a second visible shooter adds incoming fire beyond the cadence difference");

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
    for (int hit = 0; hit < 5; ++hit)
        static_cast<void>(saveWorld.FireHitscan(savePlayer, saveLook));
    Expect(
        saveWorld.CollectPickups(
            Microsoft::Xna::Framework::Vector3(4.5f, 0.62f, 1.5f),
            50).health == 25,
        "save fixture collects health before snapshot");
    Expect(
        saveWorld.CollectPickups(
            Microsoft::Xna::Framework::Vector3(5.5f, 0.62f, 1.5f)).ammo == 8,
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
        .accessMask = WolfCna::World::CyanAccess,
        .weapon = 1,
        .lastFirearm = 1,
        .hasRepeater = false,
        .hasHeavyWeapon = false,
        .combatShotSequence = 37,
        .exploredCells = saveExploration.CaptureVisited(),
        .world = saveWorld.CaptureSaveState()};
    std::string saveError;
    const std::string serializedRunSave = WolfCna::RunSave::Serialize(runSaveState);
    const std::optional<WolfCna::RunSaveState> parsedRunSave =
        WolfCna::RunSave::Parse(serializedRunSave, saveError);
    Expect(parsedRunSave.has_value() && saveError.empty(), "versioned run save parses");
    Expect(
        parsedRunSave->sectorEntryScore == 1000 &&
            parsedRunSave->sectorEntryNextExtraLifeScore == 40000 &&
            parsedRunSave->accessMask == WolfCna::World::CyanAccess &&
            parsedRunSave->combatShotSequence == 37,
        "run save preserves checkpoints, access cards and deterministic combat sequence");
    Expect(
        WolfCna::RunSave::Serialize(*parsedRunSave) == serializedRunSave,
        "run save serialization is a deterministic round trip");
    WolfCna::RunSaveState legacyFormatRunSaveState = runSaveState;
    legacyFormatRunSaveState.world.doors.clear();
    std::string versionFourRunSave = WolfCna::RunSave::Serialize(legacyFormatRunSaveState);
    const std::size_t versionFourGameStart = versionFourRunSave.find('\n') + 1;
    const std::size_t versionFourGameEnd = versionFourRunSave.find('\n', versionFourGameStart);
    const std::size_t combatSequenceStart = versionFourRunSave.rfind(' ', versionFourGameEnd);
    Expect(
        versionFourGameStart > 0 && versionFourGameEnd != std::string::npos &&
            combatSequenceStart != std::string::npos,
        "run-save fixture locates the version-five combat sequence");
    versionFourRunSave.erase(combatSequenceStart, versionFourGameEnd - combatSequenceStart);
    versionFourRunSave.replace(
        versionFourRunSave.find("WOLF-CNA-RUN-SAVE-5"),
        std::string("WOLF-CNA-RUN-SAVE-5").size(),
        "WOLF-CNA-RUN-SAVE-4");
    const std::optional<WolfCna::RunSaveState> parsedVersionFourRunSave =
        WolfCna::RunSave::Parse(versionFourRunSave, saveError);
    Expect(
        parsedVersionFourRunSave.has_value() &&
            parsedVersionFourRunSave->combatShotSequence == 0,
        "version four run saves migrate with a fresh deterministic combat sequence");
    std::string versionThreeRunSave = versionFourRunSave;
    versionThreeRunSave.replace(
        versionThreeRunSave.find("WOLF-CNA-RUN-SAVE-4"),
        std::string("WOLF-CNA-RUN-SAVE-4").size(),
        "WOLF-CNA-RUN-SAVE-3");
    const std::optional<WolfCna::RunSaveState> parsedVersionThreeRunSave =
        WolfCna::RunSave::Parse(versionThreeRunSave, saveError);
    Expect(
        parsedVersionThreeRunSave.has_value() &&
            parsedVersionThreeRunSave->world.doors.empty(),
        "version three run saves migrate without push-wall direction fields");
    std::string versionTwoRunSave = versionThreeRunSave;
    versionTwoRunSave.replace(
        versionTwoRunSave.find("WOLF-CNA-RUN-SAVE-3"),
        std::string("WOLF-CNA-RUN-SAVE-3").size(),
        "WOLF-CNA-RUN-SAVE-2");
    const std::optional<WolfCna::RunSaveState> parsedVersionTwoRunSave =
        WolfCna::RunSave::Parse(versionTwoRunSave, saveError);
    Expect(
        parsedVersionTwoRunSave.has_value() &&
            parsedVersionTwoRunSave->accessMask == WolfCna::World::CyanAccess,
        "version two security-card state migrates to cyan access");
    std::string legacyRunSave = versionTwoRunSave;
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
    WolfCna::RunSaveState maximumLivesSave = runSaveState;
    maximumLivesSave.lives = 99;
    Expect(
        WolfCna::RunSave::Parse(
            WolfCna::RunSave::Serialize(maximumLivesSave),
            saveError).has_value(),
        "run save accepts the capped maximum of 99 lives");
    maximumLivesSave.lives = 100;
    Expect(
        !WolfCna::RunSave::Parse(
            WolfCna::RunSave::Serialize(maximumLivesSave),
            saveError).has_value(),
        "run save rejects lives above the gameplay cap");
    WolfCna::RunSaveState invalidCombatSequenceSave = runSaveState;
    invalidCombatSequenceSave.combatShotSequence = -1;
    Expect(
        !WolfCna::RunSave::Parse(
            WolfCna::RunSave::Serialize(invalidCombatSequenceSave),
            saveError).has_value(),
        "run save rejects an invalid deterministic combat sequence");

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
    ExpectParseFailure(
        "#####\n#P..#\n#.D.#\n#...#\n#####\n",
        "sliding door without a wall pocket");
    ExpectParseFailure(
        "#####\n#PS##\n#####\n",
        "push wall without a safe destination and approach");
    ExpectParseFailure(
        "######\n#PSD.#\n######\n",
        "push wall without a safe destination and approach");

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

    WolfCna::World manualCloseDoorWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PD.#\n#####\n",
        "manual-close-door.level"));
    Expect(
        manualCloseDoorWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::DoorOpened,
        "manual-close fixture opens normally");
    static_cast<void>(manualCloseDoorWorld.Update(0.6f, playerPosition));
    Expect(
        manualCloseDoorWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::DoorClosing,
        "action deliberately starts closing a fully open ordinary door");
    static_cast<void>(manualCloseDoorWorld.Update(0.2f, playerPosition));
    Expect(
        manualCloseDoorWorld.Collides(2.5f, 1.5f, 0.1f),
        "manual closing uses the same synchronized collision threshold");

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
    const Microsoft::Xna::Framework::Vector3 playerAtDoorEdge(1.85f, 0.62f, 1.5f);
    Expect(
        occupiedDoorWorld.TryActivate(playerAtDoorEdge, lookDirection, false) ==
            WolfCna::World::InteractionResult::DoorCloseBlocked,
        "manual close refuses to move a door while the player overlaps its doorway");
    static_cast<void>(occupiedDoorWorld.Update(4.0f, playerPosition));
    static_cast<void>(occupiedDoorWorld.Update(0.6f, playerPosition));
    Expect(occupiedDoorWorld.Collides(2.5f, 1.5f, 0.1f), "door closes after the player leaves");

    WolfCna::World secretWorld(WolfCna::LevelDefinition::Parse(
        "#######\n#PS...#\n#######\n",
        "secret.level"));
    Expect(secretWorld.Collides(2.5f, 1.5f, 0.1f), "secret wall blocks movement before it is found");
    Expect(secretWorld.IsPushWallAtCell(2, 1), "unactivated push wall occupies its authored cell");
    Expect(
        secretWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::SecretRevealed,
        "secret wall starts moving away from the player");
    static_cast<void>(secretWorld.Update(0.7f, playerPosition));
    Expect(
        secretWorld.Collides(2.55f, 1.5f, 0.1f) &&
            secretWorld.IsPushWallAtCell(2, 1) && secretWorld.IsPushWallAtCell(3, 1),
        "moving push wall has physical collision across both touched cells");
    static_cast<void>(secretWorld.Update(3.0f, playerPosition));
    Expect(
        !secretWorld.Collides(2.5f, 1.5f, 0.1f) &&
            !secretWorld.Collides(3.5f, 1.5f, 0.1f) &&
            secretWorld.Collides(4.5f, 1.5f, 0.1f),
        "push wall permanently clears its source and intermediate passage");
    Expect(
        secretWorld.IsActivatedPushWallSource(2, 1) &&
            !secretWorld.IsPushWallAtCell(2, 1) && secretWorld.IsPushWallAtCell(4, 1),
        "automap queries track the push wall's final physical cell");
    const WolfCna::World::CompletionStats secretStats = secretWorld.GetCompletionStats();
    Expect(secretStats.foundSecrets == 1 && secretStats.totalSecrets == 1, "secret discovery is counted once");
    Expect(
        secretWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::None &&
            secretWorld.GetCompletionStats().foundSecrets == 1,
        "an activated push wall cannot be triggered or counted twice");
    const WolfCna::World::SaveState pushedSecretSave = secretWorld.CaptureSaveState();
    WolfCna::World restoredSecretWorld(WolfCna::LevelDefinition::Parse(
        "#######\n#PS...#\n#######\n",
        "restored-secret.level"));
    Expect(
        restoredSecretWorld.RestoreSaveState(pushedSecretSave) &&
            restoredSecretWorld.IsPushWallAtCell(4, 1) &&
            !restoredSecretWorld.IsPushWallAtCell(2, 1),
        "save state restores push direction, distance and final collision");

    WolfCna::World pausedSecretWorld(WolfCna::LevelDefinition::Parse(
        "#######\n#PS...#\n#######\n",
        "paused-secret.level"));
    Expect(
        pausedSecretWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::SecretRevealed,
        "moving-obstacle push-wall fixture activates");
    static_cast<void>(pausedSecretWorld.Update(0.4f, playerPosition));
    const Microsoft::Xna::Framework::Vector3 playerInPushPath(4.0f, 0.62f, 1.5f);
    static_cast<void>(pausedSecretWorld.Update(3.0f, playerInPushPath));
    Expect(
        pausedSecretWorld.CaptureSaveState().doors.front().opening &&
            !pausedSecretWorld.Collides(4.0f, 1.5f, 0.22f),
        "moving push wall pauses before overlapping a player in its path");
    static_cast<void>(pausedSecretWorld.Update(3.0f, playerPosition));
    Expect(
        !pausedSecretWorld.CaptureSaveState().doors.front().opening &&
            pausedSecretWorld.IsPushWallAtCell(4, 1),
        "paused push wall resumes and completes after the path clears");

    WolfCna::World oneCellSecretWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PS.##\n######\n",
        "one-cell-secret.level"));
    Expect(
        oneCellSecretWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::SecretRevealed,
        "a push wall accepts one safe destination when a second is unavailable");
    static_cast<void>(oneCellSecretWorld.Update(2.0f, playerPosition));
    Expect(
        oneCellSecretWorld.CaptureSaveState().doors.front().pushDistanceCells == 1 &&
            !oneCellSecretWorld.Collides(2.5f, 1.5f, 0.1f) &&
            oneCellSecretWorld.Collides(3.5f, 1.5f, 0.1f),
        "one-cell push distance clears only the authored source");

    WolfCna::World blockedSecretWorld(WolfCna::LevelDefinition::Parse(
        "########\n#PS..G.#\n########\n",
        "blocked-secret.level"));
    WolfCna::World::SaveState blockedSecretState = blockedSecretWorld.CaptureSaveState();
    blockedSecretState.enemies.front().positionX = 3.5f;
    blockedSecretState.enemies.front().positionZ = 1.5f;
    blockedSecretState.enemies.front().lastKnownX = 3.5f;
    blockedSecretState.enemies.front().lastKnownZ = 1.5f;
    Expect(
        blockedSecretWorld.RestoreSaveState(blockedSecretState) &&
            blockedSecretWorld.TryActivate(playerPosition, lookDirection, false) ==
                WolfCna::World::InteractionResult::SecretBlocked &&
            blockedSecretWorld.GetCompletionStats().foundSecrets == 0,
        "an occupied destination blocks push-wall activation without counting a secret");

    WolfCna::World crossingSecretWorld(WolfCna::LevelDefinition::Parse(
        "##########\n#PS...S..#\n##########\n",
        "crossing-secret.level"));
    Expect(
        crossingSecretWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::SecretRevealed &&
            crossingSecretWorld.TryActivate(
                Microsoft::Xna::Framework::Vector3(7.5f, 0.62f, 1.5f),
                Microsoft::Xna::Framework::Vector3(-1.0f, 0.0f, 0.0f),
                false) == WolfCna::World::InteractionResult::SecretBlocked &&
            crossingSecretWorld.GetCompletionStats().foundSecrets == 1,
        "a reserved push-wall path prevents two moving wall blocks from overlapping");

    WolfCna::World bodyDoorWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PDK.#\n######\n",
        "body-door.level"));
    Expect(
        bodyDoorWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::DoorOpened,
        "body door activates");
    static_cast<void>(bodyDoorWorld.Update(0.5f, playerPosition));
    for (int hit = 0; hit < 3; ++hit)
        Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "early shot hits doorway hound");
    Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "fourth shot kills doorway hound");
    static_cast<void>(bodyDoorWorld.Update(4.0f, playerPosition));
    static_cast<void>(bodyDoorWorld.Update(0.5f, playerPosition));
    Expect(!bodyDoorWorld.Collides(2.5f, 1.5f, 0.1f), "dead hound keeps the door open");
    Expect(
        bodyDoorWorld.TryActivate(playerPosition, lookDirection, false) ==
            WolfCna::World::InteractionResult::DoorCloseBlocked,
        "manual close refuses to move a door held by a defeated hound");

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
        securityDoorWorld.TryActivate(
            playerPosition,
            lookDirection,
            WolfCna::World::CyanAccess) == WolfCna::World::InteractionResult::DoorOpened,
        "cyan security door activates with its matching access card");
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
    Expect(ammoPickup.health == 0 && ammoPickup.ammo == 8, "large ammo pickup grants eight rounds");
    Expect(
        healthPickup.pickupAudioPositions.size() == 1 &&
            healthPickup.ammunitionAudioPositions.empty() &&
            ammoPickup.pickupAudioPositions.empty() &&
            ammoPickup.ammunitionAudioPositions.size() == 1,
        "health and ammunition pickups report their positions to the matching audio channel");

    WolfCna::World smallSupplyWorld(WolfCna::LevelDefinition::Parse(
        "######\n#Pha.#\n######\n",
        "small-supply.level"));
    Expect(
        smallSupplyWorld.CollectPickups(
            Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f),
            94).health == 6,
        "small field dressing heals up to ten without exceeding 100 percent");
    const Microsoft::Xna::Framework::Vector3 smallAmmoPosition(3.5f, 0.62f, 1.5f);
    Expect(
        smallSupplyWorld.CollectPickups(smallAmmoPosition, 100, 99).ammo == 0,
        "ammunition remains available while the shared supply is full");
    Expect(
        smallSupplyWorld.CollectPickups(smallAmmoPosition, 100, 97).ammo == 2,
        "small ammunition pickup fills only the remaining capacity");

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
        repeaterPickup.repeaterWeapons == 1 && repeaterPickup.ammo == 8,
        "repeater pickup grants the weapon and eight rounds");
    Expect(
        heavyWeaponPickup.heavyWeapons == 1 && heavyWeaponPickup.ammo == 14,
        "heavy weapon pickup grants the weapon and fourteen rounds");

    WolfCna::World duplicateWeaponWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PW.#\n#####\n",
        "duplicate-weapon.level"));
    const Microsoft::Xna::Framework::Vector3 duplicateWeaponPosition(2.5f, 0.62f, 1.5f);
    Expect(
        duplicateWeaponWorld.CollectPickups(
            duplicateWeaponPosition, 100, 99, 2, 0, true, false).ammo == 0,
        "owned weapon pickup remains while ammunition is full");
    const WolfCna::World::PickupResult duplicateWeaponPickup =
        duplicateWeaponWorld.CollectPickups(
            duplicateWeaponPosition, 100, 95, 2, 0, true, false);
    Expect(
        duplicateWeaponPickup.repeaterWeapons == 0 && duplicateWeaponPickup.ammo == 4,
        "duplicate repeater converts into documented ammunition up to the cap");

    WolfCna::World cardWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PC.#\n#####\n",
        "card.level"));
    const WolfCna::World::PickupResult cardPickup = cardWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f));
    Expect(
        cardPickup.accessMask == WolfCna::World::CyanAccess,
        "cyan security card is collected once");

    WolfCna::World amberDoorWorld(WolfCna::LevelDefinition::Parse(
        "######\n#Pcq.#\n######\n",
        "amber-door.level"));
    const WolfCna::World::PickupResult amberCard = amberDoorWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f));
    Expect(
        amberCard.accessMask == WolfCna::World::AmberAccess &&
            amberDoorWorld.TryActivate(
                Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f),
                lookDirection,
                WolfCna::World::CyanAccess) ==
                WolfCna::World::InteractionResult::AmberDoorLocked &&
            amberDoorWorld.TryActivate(
                Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f),
                lookDirection,
                amberCard.accessMask) == WolfCna::World::InteractionResult::DoorOpened,
        "amber card is distinct and opens only the matching amber door");

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
        initialBoss.present && !initialBoss.defeated && initialBoss.health == 48 &&
            initialBoss.maximumHealth == 48,
        "original boss starts with its dedicated health budget");
    for (int shot = 0; shot < 47; ++shot)
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
    for (int shot = 0; shot < 48; ++shot)
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
        "#######\n#PTJNp#\n#######\n",
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
        treasureWorld.CollectPickups(Microsoft::Xna::Framework::Vector3(5.5f, 0.62f, 1.5f)).gold == 1000,
        "peace prism awards 1000 score");
    Expect(
        treasureWorld.GetCompletionStats().collectedGold == 4 &&
            treasureWorld.GetCompletionStats().totalGold == 4,
        "all treasure variants appear in completion statistics");

    WolfCna::World recoveryWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#Pr.#\n#####\n",
        "recovery.level"));
    const WolfCna::World::PickupResult recoveryPickup = recoveryWorld.CollectPickups(
        Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f),
        37);
    Expect(
        recoveryPickup.health == 63 && recoveryPickup.extraLives == 1,
        "rare recovery beacon restores full health and grants one life");

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
    const auto fireSidearm = [&combatPlayer, &lookDirection, &sidearmSpec](WolfCna::World& target)
    {
        return target.FireHitscan(
            combatPlayer,
            lookDirection,
            sidearmSpec.range,
            sidearmSpec.emitsNoise,
            sidearmSpec.nearDamage,
            sidearmSpec.farDamage,
            sidearmSpec.falloffStart);
    };
    Expect(fireSidearm(combatWorld).score == 0, "wounding a guard has no score");
    Expect(fireSidearm(combatWorld).score == 0, "second guard wound has no score");
    Expect(fireSidearm(combatWorld).score == 100, "third close sidearm hit defeats a guard");
    Expect(
        combatWorld.GetCompletionStats().defeatedEnemies == 1 && combatWorld.GetCompletionStats().totalEnemies == 1,
        "enemy defeat appears in completion statistics");

    WolfCna::World nearDamageWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "near-damage.level"));
    const WolfCna::World::AttackResult nearDamage = fireSidearm(nearDamageWorld);
    WolfCna::World farDamageWorld(WolfCna::LevelDefinition::Parse(
        "##############\n#P.........G.#\n##############\n",
        "far-damage.level"));
    const WolfCna::World::AttackResult farDamage = fireSidearm(farDamageWorld);
    Expect(
        nearDamage.hit && nearDamage.damage == 2 && nearDamage.distance < 2.0f &&
            farDamage.hit && farDamage.damage == 1 && farDamage.distance > 8.0f,
        "sidearm damage falls from two to one across its documented range");

    WolfCna::World blockedShotWorld(WolfCna::LevelDefinition::Parse(
        "#######\n#PD.G.#\n#######\n",
        "blocked-shot.level"));
    Expect(
        !fireSidearm(blockedShotWorld).hit,
        "closed dynamic doors block player hitscan before enemies are tested");
    const Microsoft::Xna::Framework::Vector3 enemyDropPosition(2.5f, 0.62f, 1.5f);
    Expect(
        combatWorld.CollectPickups(enemyDropPosition).ammo == 3,
        "defeated guard drops three rounds");
    Expect(
        combatWorld.CollectPickups(enemyDropPosition).ammo == 0,
        "guard ammunition drop is collected once");

    WolfCna::World carriedWeaponDropWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "carried-weapon-drop.level"));
    for (int hit = 0; hit < 3; ++hit)
        static_cast<void>(fireSidearm(carriedWeaponDropWorld));
    Expect(
        carriedWeaponDropWorld.CollectPickups(
            enemyDropPosition, 100, 0, 3).ammo == 5,
        "guard drop scales upward for the carried heavy automatic");

    WolfCna::World houndWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PK.#\n#####\n",
        "hound.level"));
    Expect(fireSidearm(houndWorld).score == 0, "first shot wounds a hound");
    const WolfCna::World::AttackResult defeatedHound = fireSidearm(houndWorld);
    Expect(
        defeatedHound.score == 200 && defeatedHound.defeatedHound,
        "second hound shot awards score and requests the distinct defeat voice");
    Expect(houndWorld.CollectPickups(enemyDropPosition).ammo == 0, "hound does not drop ammunition");

    WolfCna::World rapidTrooperWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PF.#\n#####\n",
        "rapid-trooper.level"));
    for (int hit = 0; hit < 3; ++hit)
        Expect(fireSidearm(rapidTrooperWorld).score == 0, "rapid trooper survives early hits");
    Expect(fireSidearm(rapidTrooperWorld).score == 250, "rapid trooper awards score");
    Expect(
        rapidTrooperWorld.CollectPickups(enemyDropPosition).ammo == 5,
        "defeated rapid trooper drops five rounds");

    WolfCna::World heavyUnitWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PU.#\n#####\n",
        "heavy-unit.level"));
    for (int hit = 0; hit < 7; ++hit)
        Expect(fireSidearm(heavyUnitWorld).score == 0, "heavy unit survives early hits");
    Expect(fireSidearm(heavyUnitWorld).score == 500, "heavy unit awards score");
    Expect(
        heavyUnitWorld.CollectPickups(enemyDropPosition).ammo == 8,
        "defeated heavy unit drops eight rounds");

    const std::array<std::pair<char, WolfCna::World::RangedEnemyAudioKind>, 4>
        rangedAudioArchetypes = {{
            {'G', WolfCna::World::RangedEnemyAudioKind::Guard},
            {'F', WolfCna::World::RangedEnemyAudioKind::RapidTrooper},
            {'U', WolfCna::World::RangedEnemyAudioKind::HeavyUnit},
            {'Z', WolfCna::World::RangedEnemyAudioKind::Boss}}};
    for (const auto [symbol, expectedKind] : rangedAudioArchetypes)
    {
        std::string levelText = "#####\n#P";
        levelText.push_back(symbol);
        levelText += ".#\n#####\n";
        const std::string sourceName =
            std::string("ranged-audio-") + symbol + ".level";
        WolfCna::World audioWorld(WolfCna::LevelDefinition::Parse(
            levelText,
            sourceName));
        bool heardTypedAlert = false;
        bool heardTypedShot = false;
        for (int tick = 0; tick < 80 && (!heardTypedAlert || !heardTypedShot); ++tick)
        {
            static_cast<void>(audioWorld.Update(0.05f, combatPlayer));
            const WolfCna::World::EnemyAudioEvents audioEvents =
                audioWorld.ConsumeEnemyAudioEvents();
            heardTypedAlert = heardTypedAlert || std::any_of(
                audioEvents.rangedAlertSources.begin(),
                audioEvents.rangedAlertSources.end(),
                [expectedKind](const WolfCna::World::RangedEnemyAudioEvent& event)
                {
                    return event.kind == expectedKind;
                });
            const std::vector<WolfCna::World::RangedEnemyAudioEvent> shotEvents =
                audioWorld.ConsumeRangedShotAudioEvents();
            heardTypedShot = heardTypedShot || std::any_of(
                shotEvents.begin(),
                shotEvents.end(),
                [expectedKind](const WolfCna::World::RangedEnemyAudioEvent& event)
                {
                    return event.kind == expectedKind;
                });
        }
        Expect(
            heardTypedAlert && heardTypedShot,
            std::string("ranged audio events retain archetype identity for ") + symbol);
    }

    WolfCna::World damageWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "damage.level"));
    int guardProjectileDamage = 0;
    int guardShots = 0;
    int activeProjectileImpacts = 0;
    for (int tick = 0; tick < 20; ++tick)
    {
        guardProjectileDamage += damageWorld.Update(0.05f, combatPlayer);
        guardShots += static_cast<int>(
            damageWorld.ConsumeRangedShotAudioEvents().size());
        activeProjectileImpacts = std::max(
            activeProjectileImpacts,
            damageWorld.ActiveEnemyImpactCount());
    }
    const WolfCna::World::EnemyAudioEvents projectileImpactEvents =
        damageWorld.ConsumeEnemyAudioEvents();
    Expect(guardShots >= 1, "guard emits a shot at the player");
    Expect(guardProjectileDamage == 8, "guard projectile damages a player at range");
    Expect(activeProjectileImpacts > 0, "enemy projectile hit creates a temporary visual impact");
    Expect(
        projectileImpactEvents.projectileImpacts == 1 &&
            projectileImpactEvents.projectileImpactPositions.size() == 1,
        "enemy projectile hit emits one positioned impact event");

    WolfCna::World coordinatedFireWorld(WolfCna::LevelDefinition::Parse(
        "######\n#P.GG#\n######\n",
        "coordinated-fire.level"));
    int coordinatedShots = 0;
    for (int tick = 0; tick < 10; ++tick)
    {
        static_cast<void>(coordinatedFireWorld.Update(0.05f, combatPlayer));
        coordinatedShots += coordinatedFireWorld.ConsumeRangedShotCount();
    }
    Expect(
        coordinatedShots == 1,
        "only the nearest visible ranged enemy fires after its reaction delay");

    WolfCna::World patrolPerceptionWorld(WolfCna::LevelDefinition::Parse(
        "########\n#P.G>..#\n########\n",
        "patrol-perception.level"));
    static_cast<void>(patrolPerceptionWorld.Update(0.5f, combatPlayer));
    Expect(
        patrolPerceptionWorld.ConsumeEnemyAudioEvents().rangedAlerts == 0,
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
            patrolPerceptionWorld.ConsumeEnemyAudioEvents().rangedAlerts == 1,
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
            ambushPerceptionWorld.ConsumeEnemyAudioEvents().rangedAlerts == 0,
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
    Expect(firstHoundEvents.houndAlertPositions.size() == 1,
        "hound alert carries its world position");
    int houndBarks = 0;
    for (int tick = 0; tick < 10; ++tick)
    {
        static_cast<void>(houndAudioWorld.Update(0.05f, combatPlayer));
        const WolfCna::World::EnemyAudioEvents houndEvents =
            houndAudioWorld.ConsumeEnemyAudioEvents();
        houndAttacks += houndEvents.houndAttacks;
        houndBarks += houndEvents.houndBarks;
    }
    Expect(houndAttacks >= 1, "hound emits an attack event on a close-range hit");
    for (int tick = 0; tick < 150; ++tick)
    {
        static_cast<void>(houndAudioWorld.Update(0.05f, combatPlayer));
        const WolfCna::World::EnemyAudioEvents houndEvents =
            houndAudioWorld.ConsumeEnemyAudioEvents();
        houndBarks += houndEvents.houndBarks;
        if (houndEvents.houndBarks > 0)
        {
            Expect(houndEvents.houndBarkPositions.size() ==
                static_cast<std::size_t>(houndEvents.houndBarks),
                "every occasional hound bark carries an emitter position");
        }
    }
    Expect(houndBarks >= 1, "a living hound occasionally barks");

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
