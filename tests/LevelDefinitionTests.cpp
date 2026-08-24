#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

#include "LevelDefinition.hpp"
#include "World.hpp"
#include "CampaignProgress.hpp"

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
}

int main()
{
    Expect(
        WolfCna::CampaignProgress::ParseHighestUnlocked("WOLF-CNA-PROGRESS-1\n1\n", 3) == 1,
        "campaign progress restores an unlocked sector");
    Expect(
        WolfCna::CampaignProgress::ParseHighestUnlocked("broken\n2\n", 3) == 0,
        "invalid campaign progress safely locks later sectors");
    Expect(
        WolfCna::CampaignProgress::ParseHighestUnlocked(
            WolfCna::CampaignProgress::SerializeHighestUnlocked(8, 3), 3) == 2,
        "campaign progress clamps to the available sector count");

    const WolfCna::LevelDefinition starterLevel = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/starter.level");
    Expect(starterLevel.Rows().size() == 18, "starter level row count");
    Expect(starterLevel.Rows().front().size() == 35, "starter level width");
    Expect(starterLevel.PlayerStartX() == 3 && starterLevel.PlayerStartZ() == 3, "starter spawn");

    const WolfCna::LevelDefinition sectorTwo = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/sector-02.level");
    const WolfCna::LevelDefinition sectorThree = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/sector-03.level");
    Expect(sectorTwo.Rows().size() >= 12 && sectorTwo.PlayerStartX() >= 0, "sector two is a valid authored level");
    Expect(sectorThree.Rows().size() >= 12 && sectorThree.PlayerStartX() >= 0, "sector three is a valid authored level");

    const WolfCna::LevelDefinition level = WolfCna::LevelDefinition::Parse(
        "#####\n#P..#\n#####\n",
        "valid.level");
    Expect(level.Rows().size() == 3, "valid level row count");
    Expect(level.Rows().front().size() == 5, "valid level width");
    Expect(level.PlayerStartX() == 1 && level.PlayerStartZ() == 1, "player spawn position");

    ExpectParseFailure("#####\n#P.#\n#####\n", "different width");
    ExpectParseFailure("#####\n#X.P#\n#####\n", "unknown symbol");
    ExpectParseFailure("#####\n#...#\n#####\n", "no player spawn");
    ExpectParseFailure("#####\n#P.P#\n#####\n", "more than one player spawn");

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
    Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "shot kills doorway hound");
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

    WolfCna::World houndWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PK.#\n#####\n",
        "hound.level"));
    Expect(houndWorld.FireHitscan(combatPlayer, lookDirection).score == 200, "hound kill awards score");

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
    Expect(guardProjectileDamage == 12, "guard projectile damages a player at range");

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
        scoutDamageWorld.Update(0.05f, Microsoft::Xna::Framework::Vector3(1.8f, 0.62f, 1.5f), 0.5f) == 9,
        "damage multiplier scales hound attacks for difficulty");

    return EXIT_SUCCESS;
}
