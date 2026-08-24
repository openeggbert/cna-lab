#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "CopperBoots/Camera2D.hpp"
#include "CopperBoots/LevelDefinition.hpp"
#include "CopperBoots/SimulationClock.hpp"
#include "CopperBoots/TileMap.hpp"
#include "CopperBoots/WorldSimulation.hpp"

namespace
{
    int failures = 0;

    void Check(const bool condition, const std::string_view message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    void CheckNear(const float actual, const float expected, const float tolerance,
                   const std::string_view message)
    {
        Check(std::abs(actual - expected) <= tolerance, message);
    }

    [[nodiscard]] std::string MakeCrawlerLevel(const int width,
                                               const int crawlerX,
                                               const int spawnX,
                                               const int spawnFootY)
    {
        std::string result =
            "copper-boots-level 1\n"
            "name Crawler Workshop\n"
            "size " + std::to_string(width) + " 5\n" +
            "spawn " + std::to_string(spawnX) + ' ' +
                std::to_string(spawnFootY) + "\n" +
            "checkpoint " + std::to_string(spawnX) + ' ' +
                std::to_string(spawnFootY) + "\n" +
            "parallax 0.1 0.25 0.5\n"
            "legend\n"
            ". empty\n"
            "# solid\n"
            "B breakable\n"
            "! hazard\n"
            "E exit\n"
            "d decoration\n"
            "G cog\n"
            "? cog-block\n"
            "o empty-block\n"
            "C crawler\n"
            "c crawler-fall\n"
            "map\n";
        const std::string emptyRow(static_cast<std::size_t>(width), '.');
        result += emptyRow + '\n';
        result += emptyRow + '\n';
        result += emptyRow + '\n';
        std::string crawlerRow = emptyRow;
        crawlerRow[static_cast<std::size_t>(crawlerX)] = 'C';
        result += crawlerRow + '\n';
        result += std::string(static_cast<std::size_t>(width), '#') + '\n';
        return result;
    }

    void TestSimulationClock()
    {
        CopperBoots::SimulationClock clock;
        Check(clock.AddFrameTime(1.0 / 120.0) == 0,
              "half a tick does not update");
        Check(clock.AddFrameTime(1.0 / 120.0) == 1,
              "two half ticks produce one update");

        CopperBoots::SimulationClock stalled;
        Check(stalled.AddFrameTime(1.0) ==
                  CopperBoots::SimulationClock::MaximumStepsPerFrame,
              "pathological frame is catch-up capped");
        Check(stalled.DroppedSteps() == 7,
              "clamped 250 ms frame drops seven of fifteen available steps");
        Check(stalled.RemainderSeconds() <
                  CopperBoots::SimulationClock::TickSeconds,
              "dropped backlog retains less than one tick");
    }

    void TestTileBounds()
    {
        CopperBoots::TileMap map(2, 2);
        Check(map.IsSolid(-1, 0), "left of map is solid");
        Check(map.IsSolid(2, 0), "right of map is solid");
        Check(!map.IsSolid(0, -1), "above map is empty");
        Check(map.IsSolid(0, 2), "below map is solid");
        map.Set(1, 1, CopperBoots::Tiles::Ruin);
        Check(map.IsSolid(1, 1), "set solid tile is returned");
        map.Set(0, 0, CopperBoots::Tiles::Decoration);
        Check(!map.IsSolid(0, 0),
              "decorative visual does not imply solid collision");
    }

    void TestLevelParsing()
    {
        constexpr std::string_view source =
            "copper-boots-level 1\n"
            "name Parser Workshop\n"
            "size 4 3\n"
            "spawn 1 2\n"
            "checkpoint 2 2\n"
            "parallax 0.1 0.25 0.5\n"
            "legend\n"
            ". empty\n"
            "# solid\n"
            "B breakable\n"
            "! hazard\n"
            "E exit\n"
            "d decoration\n"
            "G cog\n"
            "? cog-block\n"
            "o empty-block\n"
            "C crawler\n"
            "c crawler-fall\n"
            "map\n"
            "d?oC\n"
            "BG!E\n"
            "####\n";

        const CopperBoots::LevelDefinition level =
            CopperBoots::LevelDefinition::Parse(source, "memory.cbl");
        Check(level.Name == "Parser Workshop", "level name is parsed");
        Check(level.Map.Width() == 4 && level.Map.Height() == 3,
              "level dimensions are parsed");
        Check(level.SpawnTileX == 1 && level.SpawnFootTileY == 2,
              "spawn coordinate is parsed");
        Check(level.CheckpointTileX == 2 && level.CheckpointFootTileY == 2,
              "checkpoint coordinate is parsed");
        CheckNear(level.ParallaxFactors[1], 0.25F, 0.0001F,
                  "parallax factors are parsed");
        Check(level.Map.Get(0, 0).Visual == CopperBoots::TileVisual::Decoration &&
                  level.Map.Get(0, 0).Collision == CopperBoots::TileCollision::None,
              "decoration keeps visual and collision independent");
        Check(level.Map.Get(0, 1) == CopperBoots::Tiles::Breakable,
              "breakable glyph maps to breakable solid");
        Check(level.Map.Get(2, 1) == CopperBoots::Tiles::Hazard,
              "hazard glyph maps to hazard semantics");
        Check(level.Map.Get(3, 1) == CopperBoots::Tiles::Exit,
              "exit glyph maps to exit semantics");
        Check(level.Cogs.size() == 1 && level.Cogs[0].X == 1 &&
                  level.Cogs[0].Y == 1,
              "cog glyph becomes an object coordinate");
        Check(level.Map.Get(1, 1) == CopperBoots::Tiles::Empty,
              "cog marker does not become a collision tile");
        Check(level.InteractiveBlocks.size() == 2 &&
                  level.InteractiveBlocks[0].Content == CopperBoots::BlockContent::Cog &&
                  level.InteractiveBlocks[1].Content == CopperBoots::BlockContent::None,
              "interactive block contents are parsed separately from tiles");
        Check(level.Crawlers.size() == 1 &&
                  level.Crawlers[0].Position.X == 3 &&
                  level.Crawlers[0].Position.Y == 0 &&
                  !level.Crawlers[0].FallsAtEdges,
              "crawler glyph becomes an object coordinate");

        std::string malformed(source);
        const std::size_t row = malformed.find("d?oC\n");
        malformed.erase(row, 1);
        bool threwLineError = false;
        try {
            (void)CopperBoots::LevelDefinition::Parse(malformed, "broken.cbl");
        }
        catch (const std::runtime_error& error) {
            threwLineError = std::string_view(error.what()).starts_with(
                "broken.cbl:20:");
        }
        Check(threwLineError, "malformed map reports source and line number");

        const auto failsAt = [](std::string text,
                                const std::string_view expectedPrefix) {
            try {
                (void)CopperBoots::LevelDefinition::Parse(text, "case.cbl");
            }
            catch (const std::runtime_error& error) {
                return std::string_view(error.what()).starts_with(expectedPrefix);
            }
            return false;
        };
        const auto replaced = [source](const std::string_view before,
                                       const std::string_view after) {
            std::string result(source);
            const std::size_t position = result.find(before);
            result.replace(position, before.size(), after);
            return result;
        };

        Check(failsAt(replaced("copper-boots-level 1",
                               "copper-boots-level 2"), "case.cbl:1:"),
              "unsupported level version reports line one");
        Check(failsAt(replaced("size 4 3", "size 0 3"), "case.cbl:3:"),
              "invalid dimensions report their directive");
        Check(failsAt(replaced("spawn 1 2", "spoon 1 2"), "case.cbl:4:"),
              "missing spawn directive reports its line");
        Check(failsAt(replaced("checkpoint 2 2", "checkpoint 5 2"),
                      "case.cbl:5:"),
              "out-of-bounds checkpoint reports its line");
        Check(failsAt(replaced("BG!E", "BGXE"), "case.cbl:21:"),
              "unknown map glyph reports its row");
        Check(failsAt(replaced("checkpoint 2 2",
                               "spawn 1 2\ncheckpoint 2 2"),
                      "case.cbl:5:"),
              "duplicate ordered directive is rejected deterministically");

        CopperBoots::WorldSimulation collectibleWorld;
        collectibleWorld.LoadLevel(
            CopperBoots::LevelDefinition::Parse(source, "collectible.cbl"));
        collectibleWorld.Update({}, static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds));
        Check(collectibleWorld.CollectedCogCount() == 1 &&
                  collectibleWorld.Score() == 100,
              "overlap collects one cog and awards score");
        Check(collectibleWorld.LastEvents().CogsCollected == 1 &&
                  collectibleWorld.LastEvents().ScoreAdded == 100,
              "collection emits a one-tick event");
        collectibleWorld.Update({}, static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds));
        Check(collectibleWorld.CollectedCogCount() == 1 &&
                  collectibleWorld.LastEvents().CogsCollected == 0,
              "continued overlap cannot collect a cog twice");
        collectibleWorld.LoadLevel(
            CopperBoots::LevelDefinition::Parse(source, "reload.cbl"));
        Check(collectibleWorld.CollectedCogCount() == 0 &&
                  collectibleWorld.Score() == 0 &&
                  !collectibleWorld.Cogs()[0].Collected,
              "reloading a level resets transient cog progress");

        constexpr std::string_view blockSource =
            "copper-boots-level 1\n"
            "name Block Workshop\n"
            "size 4 4\n"
            "spawn 1 3\n"
            "checkpoint 1 3\n"
            "parallax 0.1 0.25 0.5\n"
            "legend\n"
            ". empty\n"
            "# solid\n"
            "B breakable\n"
            "! hazard\n"
            "E exit\n"
            "d decoration\n"
            "G cog\n"
            "? cog-block\n"
            "o empty-block\n"
            "C crawler\n"
            "c crawler-fall\n"
            "map\n"
            "....\n"
            "B?o.\n"
            "....\n"
            "####\n";
        CopperBoots::PlayerInput headHit;
        headHit.JumpPressed = true;
        headHit.JumpHeld = true;
        constexpr float fixedTick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);

        CopperBoots::WorldSimulation contentBlockWorld;
        contentBlockWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            blockSource, "blocks.cbl"));
        contentBlockWorld.Update(headHit, fixedTick);
        Check(contentBlockWorld.LastEvents().BlocksBumped == 1 &&
                  contentBlockWorld.LastEvents().BlockContentsReleased == 1,
              "first cog-block ceiling hit bumps and releases content");
        Check(contentBlockWorld.Level().Get(1, 1) == CopperBoots::Tiles::UsedBlock &&
                  contentBlockWorld.Level().IsSolid(1, 1),
              "used block visual changes while collision stays solid");
        Check(contentBlockWorld.BlockVisualOffset(1, 1) < 0,
              "block bump is exposed as a visual-only offset");
        contentBlockWorld.ResetPlayer();
        contentBlockWorld.Update(headHit, fixedTick);
        Check(contentBlockWorld.LastEvents().BlockContentsReleased == 0,
              "used block cannot release its content twice");

        std::string emptyBlockSource(blockSource);
        emptyBlockSource.replace(emptyBlockSource.find("spawn 1 3"), 9,
                                 "spawn 2 3");
        CopperBoots::WorldSimulation emptyBlockWorld;
        emptyBlockWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            emptyBlockSource, "empty-block.cbl"));
        emptyBlockWorld.Update(headHit, fixedTick);
        Check(emptyBlockWorld.LastEvents().BlocksBumped == 1 &&
                  emptyBlockWorld.LastEvents().BlockContentsReleased == 0 &&
                  emptyBlockWorld.Level().Get(2, 1) == CopperBoots::Tiles::UsedBlock,
              "empty interactive block bumps once and becomes used");

        std::string breakableSource(blockSource);
        breakableSource.replace(breakableSource.find("spawn 1 3"), 9,
                                "spawn 0 3");
        CopperBoots::WorldSimulation breakableWorld;
        breakableWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            breakableSource, "breakable.cbl"));
        breakableWorld.Update(headHit, fixedTick);
        Check(breakableWorld.LastEvents().BlocksBumped == 1 &&
                  breakableWorld.LastEvents().BlocksBroken == 0 &&
                  breakableWorld.Level().Get(0, 1) == CopperBoots::Tiles::Breakable,
              "unplated ceiling hit cannot break a marked block");
        breakableWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            breakableSource, "breakable.cbl"));
        breakableWorld.SetPlayerPlated(true);
        breakableWorld.Update(headHit, fixedTick);
        Check(breakableWorld.LastEvents().BlocksBroken == 1 &&
                  breakableWorld.Level().Get(0, 1) == CopperBoots::Tiles::Empty,
              "plated ceiling hit breaks a marked block");
    }

    void TestMovementAndJump()
    {
        CopperBoots::WorldSimulation world;
        const float startX = world.Player().X;
        const float floorY = world.Player().Y;
        constexpr float tick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);

        CopperBoots::PlayerInput move;
        move.Move = 1.0F;
        for (int i = 0; i < 30; ++i)
            world.Update(move, tick);
        Check(world.Player().X > startX + 20.0F,
              "holding right moves the player");
        Check(world.Player().VelocityX <= 72.01F,
              "walk speed is capped");

        CopperBoots::PlayerInput coast;
        for (int i = 0; i < 12; ++i)
            world.Update(coast, tick);
        CheckNear(world.Player().VelocityX, 0.0F, 0.01F,
                  "ground braking reaches rest");

        CopperBoots::PlayerInput jump;
        jump.JumpPressed = true;
        jump.JumpHeld = true;
        world.Update(jump, tick);
        float apexY = world.Player().Y;
        for (int i = 0; i < 100; ++i) {
            jump.JumpPressed = false;
            world.Update(jump, tick);
            apexY = std::min(apexY, world.Player().Y);
        }
        Check(apexY < floorY - 25.0F, "held jump has a visible apex");
        Check(world.Player().Grounded, "player lands after jump");
        CheckNear(world.Player().Y, floorY, 0.01F,
                  "landing snaps to stable floor height");
    }

    void TestVariableJumpHeight()
    {
        constexpr float tick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);
        CopperBoots::WorldSimulation heldWorld;
        CopperBoots::WorldSimulation tapWorld;

        CopperBoots::PlayerInput held;
        held.JumpPressed = true;
        held.JumpHeld = true;
        CopperBoots::PlayerInput tap = held;

        float heldApex = heldWorld.Player().Y;
        float tapApex = tapWorld.Player().Y;
        for (int i = 0; i < 60; ++i) {
            heldWorld.Update(held, tick);
            tap.JumpHeld = i < 2;
            tapWorld.Update(tap, tick);
            held.JumpPressed = false;
            tap.JumpPressed = false;
            heldApex = std::min(heldApex, heldWorld.Player().Y);
            tapApex = std::min(tapApex, tapWorld.Player().Y);
        }
        Check(heldApex < tapApex - 12.0F,
              "holding jump produces more height than tapping");
    }

    void TestCameraBounds()
    {
        CopperBoots::Camera2D camera(320.0F, 180.0F);
        camera.SetWorldBounds(1'000.0F, 300.0F);
        camera.SnapTo(-100.0F, -100.0F);
        CheckNear(camera.X(), 0.0F, 0.001F, "camera clamps left");
        CheckNear(camera.Y(), 0.0F, 0.001F, "camera clamps top");
        camera.SnapTo(2'000.0F, 2'000.0F);
        CheckNear(camera.X(), 680.0F, 0.001F, "camera clamps right");
        CheckNear(camera.Y(), 120.0F, 0.001F, "camera clamps bottom");
    }

    void TestClockworkCrawler()
    {
        constexpr float tick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);

        CopperBoots::WorldSimulation stompWorld;
        stompWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeCrawlerLevel(40, 2, 2, 1), "stomp.cbl"));
        bool stomped = false;
        for (int i = 0; i < 90; ++i) {
            stompWorld.Update({}, tick);
            if (stompWorld.LastEvents().EnemiesDefeated == 1) {
                stomped = true;
                break;
            }
        }
        Check(stomped && stompWorld.Crawlers()[0].Defeated &&
                  stompWorld.Score() == 200,
              "falling top contact defeats crawler and awards score");
        CopperBoots::PlayerInput runAway;
        runAway.Move = 1.0F;
        runAway.Run = true;
        for (int i = 0; i < 280; ++i)
            stompWorld.Update(runAway, tick);
        Check(stompWorld.Crawlers()[0].Defeated &&
                  !stompWorld.Crawlers()[0].Active,
              "defeated crawler state persists after leaving its activation range");

        CopperBoots::WorldSimulation sideWorld;
        sideWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeCrawlerLevel(8, 2, 2, 4), "side.cbl"));
        sideWorld.Update({}, tick);
        Check(sideWorld.LastEvents().PlayerDamaged == 1 &&
                  sideWorld.Player().InvulnerabilityTicks > 0,
              "side contact damages player and starts invulnerability");
        sideWorld.Update({}, tick);
        Check(sideWorld.LastEvents().PlayerDamaged == 0,
              "continued crawler contact cannot damage during invulnerability");

        CopperBoots::WorldSimulation platedWorld;
        platedWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeCrawlerLevel(8, 2, 2, 4), "plated-side.cbl"));
        platedWorld.SetPlayerPlated(true);
        platedWorld.Update({}, tick);
        Check(!platedWorld.Player().Plated &&
                  platedWorld.LastEvents().PlayerDamaged == 1,
              "crawler contact consumes plated protection");

        CopperBoots::WorldSimulation patrolWorld;
        patrolWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeCrawlerLevel(8, 2, 7, 4), "patrol.cbl"));
        for (int i = 0; i < 120; ++i)
            patrolWorld.Update({}, tick);
        Check(patrolWorld.Crawlers()[0].Direction == 1,
              "turn-edge crawler reverses at the left world wall");

        constexpr std::string_view fallingSource =
            "copper-boots-level 1\n"
            "name Falling Crawler Workshop\n"
            "size 8 6\n"
            "spawn 7 5\n"
            "checkpoint 7 5\n"
            "parallax 0.1 0.25 0.5\n"
            "legend\n"
            ". empty\n"
            "# solid\n"
            "B breakable\n"
            "! hazard\n"
            "E exit\n"
            "d decoration\n"
            "G cog\n"
            "? cog-block\n"
            "o empty-block\n"
            "C crawler\n"
            "c crawler-fall\n"
            "map\n"
            "........\n"
            "........\n"
            "..c.....\n"
            "..##....\n"
            "........\n"
            "########\n";
        CopperBoots::WorldSimulation fallingWorld;
        fallingWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            fallingSource, "falling.cbl"));
        const float initialCrawlerY = fallingWorld.Crawlers()[0].Y;
        for (int i = 0; i < 90; ++i)
            fallingWorld.Update({}, tick);
        Check(fallingWorld.Crawlers()[0].EdgePolicy ==
                  CopperBoots::CrawlerEdgePolicy::Fall &&
                  fallingWorld.Crawlers()[0].Y > initialCrawlerY + 20.0F,
              "fall-edge crawler walks off a platform and lands below");

        CopperBoots::WorldSimulation activationWorld;
        activationWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeCrawlerLevel(40, 30, 2, 4), "activation.cbl"));
        activationWorld.Update({}, tick);
        Check(!activationWorld.Crawlers()[0].Active,
              "crawler outside camera margin remains inactive");
        for (int i = 0; i < 250; ++i)
            activationWorld.Update(runAway, tick);
        Check(activationWorld.Crawlers()[0].Active,
              "crawler activates when camera approaches its range");
    }
}

int main()
{
    TestSimulationClock();
    TestTileBounds();
    TestLevelParsing();
    TestMovementAndJump();
    TestVariableJumpHeight();
    TestCameraBounds();
    TestClockworkCrawler();

    if (failures != 0) {
        std::cerr << failures << " gameplay test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Copper Boots gameplay tests passed\n";
    return EXIT_SUCCESS;
}
