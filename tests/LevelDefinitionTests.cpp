#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "LevelDefinition.hpp"
#include "World.hpp"
#include "CampaignProgress.hpp"
#include "ExplorationMap.hpp"

namespace
{
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

        std::vector<std::vector<bool>> visited(64, std::vector<bool>(64, false));
        std::queue<std::pair<int, int>> frontier;
        frontier.emplace(level.PlayerStartX(), level.PlayerStartZ());
        visited[static_cast<std::size_t>(level.PlayerStartZ())]
            [static_cast<std::size_t>(level.PlayerStartX())] = true;

        int reachable = 0;
        while (!frontier.empty())
        {
            const auto [x, z] = frontier.front();
            frontier.pop();
            ++reachable;
            constexpr std::array<std::pair<int, int>, 4> Directions = {
                std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};
            for (const auto [dx, dz] : Directions)
            {
                const int nextX = x + dx;
                const int nextZ = z + dz;
                if (nextX < 0 || nextZ < 0 || nextX >= 64 || nextZ >= 64 ||
                    visited[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] ||
                    rows[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] == '#' ||
                    rows[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] == 'Y')
                    continue;
                visited[static_cast<std::size_t>(nextZ)][static_cast<std::size_t>(nextX)] = true;
                frontier.emplace(nextX, nextZ);
            }
        }

        int walkable = 0;
        int exits = 0;
        int relays = 0;
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
        for (const std::string& row : rows)
        {
            for (const char symbol : row)
            {
                if (symbol != '#' && symbol != 'Y')
                    ++walkable;
                if (symbol == 'E')
                    ++exits;
                else if (symbol == 'O')
                    ++relays;
                else if (symbol == 'I')
                    ++plants;
                else if (symbol == 'Y')
                    ++tables;
                else if (symbol == 'H')
                    ++healthPickups;
                else if (symbol == 'A')
                    ++ammoPickups;
                else if (symbol == 'W')
                    ++repeaterPickups;
                else if (symbol == 'V')
                    ++heavyWeaponPickups;
                else if (symbol == 'G')
                    ++guards;
                else if (symbol == 'K')
                    ++hounds;
                else if (symbol == 'F')
                    ++rapidTroopers;
                else if (symbol == 'U')
                    ++heavyUnits;
            }
        }
        Expect(walkable >= 1500, std::string(name) + " uses a substantial part of its footprint");
        Expect(reachable == walkable, std::string(name) + " has no disconnected rooms");
        Expect(exits == 1, std::string(name) + " has one exit");
        Expect(relays == 1, std::string(name) + " has one power relay");
        Expect(plants == 3, std::string(name) + " has three sector plants");
        Expect(tables == 2, std::string(name) + " has two polygonal tables");
        Expect(healthPickups >= 2, std::string(name) + " has recovery beyond one health kit");
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
    const WolfCna::CampaignProfile legacyProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-1\n1\n", 3);
    Expect(
        legacyProfile.highestUnlocked == 1 && legacyProfile.soundEnabled && legacyProfile.difficulty == 1,
        "legacy campaign progress retains default settings");
    const WolfCna::CampaignProfile savedProfile = WolfCna::CampaignProgress::Parse(
        WolfCna::CampaignProgress::Serialize(
            WolfCna::CampaignProfile{.highestUnlocked = 8, .soundEnabled = false, .difficulty = 2},
            3),
        3);
    Expect(
        savedProfile.highestUnlocked == 2 && !savedProfile.soundEnabled && savedProfile.difficulty == 2,
        "campaign profile restores clamped unlocks, sound and difficulty");
    const WolfCna::CampaignProfile invalidProfile = WolfCna::CampaignProgress::Parse(
        "WOLF-CNA-PROGRESS-2\n2\n4\n1\n", 3);
    Expect(
        invalidProfile.highestUnlocked == 0 && invalidProfile.soundEnabled && invalidProfile.difficulty == 1,
        "invalid campaign profile safely restores defaults");

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
    ExpectCampaignLayout(sectorTwo, "sector two");
    ExpectCampaignLayout(sectorThree, "sector three");
    ExpectCampaignLayout(sectorFour, "sector four");
    Expect(sectorFour.Rows() != starterLevel.Rows(), "sector four is not the starter layout");
    Expect(sectorFour.Rows() != sectorTwo.Rows(), "sector four is not the foundry layout");
    Expect(sectorFour.Rows() != sectorThree.Rows(), "sector four is not the labs layout");

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

    WolfCna::MapToggleLatch mapToggle;
    Expect(!mapToggle.Update(false), "released map key does not toggle");
    Expect(mapToggle.Update(true), "pressing Tab toggles the map");
    Expect(!mapToggle.Update(true), "holding Tab does not repeatedly toggle the map");
    Expect(!mapToggle.Update(false), "releasing Tab rearms the map toggle");
    Expect(mapToggle.Update(true), "the next Tab press toggles the map again");
    mapToggle.Reset();
    Expect(mapToggle.Update(true), "reset clears the Tab key latch");

    ExpectParseFailure("#####\n#P.#\n#####\n", "different width");
    ExpectParseFailure("#####\n#X.P#\n#####\n", "unknown symbol");
    ExpectParseFailure("#####\n#...#\n#####\n", "no player spawn");
    ExpectParseFailure("#####\n#P.P#\n#####\n", "more than one player spawn");
    const WolfCna::LevelDefinition decoratedLevel = WolfCna::LevelDefinition::Parse(
        "######\n#PBRL#\n######\n",
        "decorated.level");
    Expect(decoratedLevel.Rows().front().size() == 6, "decoration symbols are accepted");
    ExpectParseFailure(
        "#######\n#P....#\n#..R..#\n#.....#\n#######\n",
        "without an adjacent wall");

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
    Expect(!terminalWorld.ReachedExit(terminalExit), "terminal locks the exit until activated");
    Expect(
        terminalWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::InteractionResult::TerminalActivated,
        "terminal activates when used from the front");
    Expect(terminalWorld.IsExitUnlocked(), "terminal unlocks the exit");
    Expect(terminalWorld.ReachedExit(terminalExit), "activated terminal allows the exit");

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
    Expect(!relayWorld.IsExitUnlocked(), "power relay alone does not bypass the terminal");
    Expect(
        relayWorld.TryActivate(
            Microsoft::Xna::Framework::Vector3(2.5f, 0.62f, 1.5f),
            lookDirection,
            false) == WolfCna::World::InteractionResult::TerminalActivated,
        "terminal remains independently required after the relay");
    Expect(relayWorld.IsExitUnlocked(), "relay and terminal together unlock the exit");
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
    for (int tick = 0; tick < 20; ++tick)
    {
        guardProjectileDamage += damageWorld.Update(0.05f, combatPlayer);
        guardShots += damageWorld.ConsumeGuardShotCount();
    }
    Expect(guardShots >= 1, "guard emits a shot at the player");
    Expect(guardProjectileDamage == 8, "guard projectile damages a player at range");

    WolfCna::World coordinatedFireWorld(WolfCna::LevelDefinition::Parse(
        "######\n#P.GG#\n######\n",
        "coordinated-fire.level"));
    static_cast<void>(coordinatedFireWorld.Update(0.05f, combatPlayer));
    Expect(
        coordinatedFireWorld.ConsumeGuardShotCount() == 1,
        "only the nearest visible ranged enemy fires at one time");

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

    WolfCna::World scoutDamageWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PK.#\n#####\n",
        "scout-damage.level"));
    Expect(
        scoutDamageWorld.Update(0.05f, Microsoft::Xna::Framework::Vector3(1.8f, 0.62f, 1.5f), 0.5f) == 7,
        "damage multiplier scales hound attacks for difficulty");

    return EXIT_SUCCESS;
}
