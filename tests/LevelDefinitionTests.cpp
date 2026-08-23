#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

#include "LevelDefinition.hpp"
#include "World.hpp"

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
    const WolfCna::LevelDefinition starterLevel = WolfCna::LevelDefinition::LoadFromFile(
        "assets/levels/starter.level");
    Expect(starterLevel.Rows().size() == 15, "starter level row count");
    Expect(starterLevel.PlayerStartX() == 1 && starterLevel.PlayerStartZ() == 1, "starter spawn");

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
        doorWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::DoorActivation::Opened,
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

    WolfCna::World bodyDoorWorld(WolfCna::LevelDefinition::Parse(
        "######\n#PDG.#\n######\n",
        "body-door.level"));
    Expect(
        bodyDoorWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::DoorActivation::Opened,
        "body door activates");
    static_cast<void>(bodyDoorWorld.Update(0.5f, playerPosition));
    static_cast<void>(bodyDoorWorld.Update(1.0f, playerPosition));
    Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "first shot hits doorway guard");
    Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "second shot hits doorway guard");
    Expect(bodyDoorWorld.FireHitscan(playerPosition, lookDirection), "third shot kills doorway guard");
    static_cast<void>(bodyDoorWorld.Update(4.0f, playerPosition));
    static_cast<void>(bodyDoorWorld.Update(0.5f, playerPosition));
    Expect(!bodyDoorWorld.Collides(2.5f, 1.5f, 0.1f), "dead guard keeps the door open");

    WolfCna::World securityDoorWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PQ.#\n#####\n",
        "security-door.level"));
    Expect(securityDoorWorld.Collides(2.5f, 1.5f, 0.1f), "closed security door blocks movement");
    Expect(
        securityDoorWorld.TryActivate(playerPosition, lookDirection, false) == WolfCna::World::DoorActivation::Locked,
        "security door reports that it is locked");
    static_cast<void>(securityDoorWorld.Update(0.5f, playerPosition));
    Expect(securityDoorWorld.Collides(2.5f, 1.5f, 0.1f), "security door needs an access card");
    Expect(
        securityDoorWorld.TryActivate(playerPosition, lookDirection, true) == WolfCna::World::DoorActivation::Opened,
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

    WolfCna::World combatWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "combat.level"));
    const Microsoft::Xna::Framework::Vector3 combatPlayer(1.5f, 0.62f, 1.5f);
    Expect(combatWorld.FireHitscan(combatPlayer, lookDirection).score == 0, "wounding a guard has no score");
    Expect(combatWorld.FireHitscan(combatPlayer, lookDirection).score == 0, "second guard wound has no score");
    Expect(combatWorld.FireHitscan(combatPlayer, lookDirection).score == 100, "guard kill awards score");

    WolfCna::World houndWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PK.#\n#####\n",
        "hound.level"));
    Expect(houndWorld.FireHitscan(combatPlayer, lookDirection).score == 200, "hound kill awards score");

    WolfCna::World damageWorld(WolfCna::LevelDefinition::Parse(
        "#####\n#PG.#\n#####\n",
        "damage.level"));
    Expect(damageWorld.Update(0.3f, combatPlayer) == 0, "guard starts pursuing a player");
    Expect(damageWorld.Update(1.0f, combatPlayer) > 0, "guard damages a nearby player");

    return EXIT_SUCCESS;
}
