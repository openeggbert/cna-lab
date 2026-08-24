#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "CopperBoots/Camera2D.hpp"
#include "CopperBoots/LevelDefinition.hpp"
#include "CopperBoots/InputActionAdapter.hpp"
#include "CopperBoots/ParallaxLayer.hpp"
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
            "P plated-block\n"
            "A plating\n"
            "R capacitor-block\n"
            "K capacitor\n"
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

    [[nodiscard]] std::string MakeDeathLevel(const bool hazard)
    {
        std::string result =
            "copper-boots-level 1\n"
            "name Death Workshop\n"
            "size 8 5\n"
            "spawn 1 4\n"
            "checkpoint 5 4\n"
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
            "P plated-block\n"
            "A plating\n"
            "R capacitor-block\n"
            "K capacitor\n"
            "C crawler\n"
            "c crawler-fall\n"
            "map\n"
            "........\n"
            "........\n"
            "........\n";
        result += hazard ? ".!......\n" : "........\n";
        result += hazard ? "########\n" : "#.######\n";
        return result;
    }

    [[nodiscard]] std::string MakeProjectileLevel(const bool crawler,
                                                  const bool wall)
    {
        constexpr int width = 40;
        std::string result =
            "copper-boots-level 1\n"
            "name Projectile Workshop\n"
            "size 40 5\n"
            "spawn 2 4\n"
            "checkpoint 2 4\n"
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
            "P plated-block\n"
            "A plating\n"
            "R capacitor-block\n"
            "K capacitor\n"
            "C crawler\n"
            "c crawler-fall\n"
            "map\n";
        std::string upper(static_cast<std::size_t>(width), '.');
        if (wall)
            upper[6] = '#';
        result += upper + '\n';
        result += upper + '\n';
        result += upper + '\n';
        std::string objectRow(static_cast<std::size_t>(width), '.');
        objectRow[2] = 'K';
        if (crawler)
            objectRow[10] = 'C';
        if (wall)
            objectRow[6] = '#';
        result += objectRow + '\n';
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

    void TestInputActionAdapter()
    {
        CopperBoots::InputActionAdapter adapter;
        CopperBoots::InputSnapshot snapshot;
        snapshot.AnalogMove = 0.19F;
        CheckNear(adapter.Sample(snapshot).Move, 0.0F, 0.001F,
                  "analog movement inside dead zone is neutral");
        snapshot.AnalogMove = 0.65F;
        CheckNear(adapter.Sample(snapshot).Move, 0.65F, 0.001F,
                  "analog movement outside dead zone is preserved");
        snapshot.Left = true;
        CheckNear(adapter.Sample(snapshot).Move, -1.0F, 0.001F,
                  "digital movement overrides analog stick");
        snapshot.Right = true;
        CheckNear(adapter.Sample(snapshot).Move, 0.0F, 0.001F,
                  "opposing digital movement is neutral even with analog input");

        snapshot.Left = false;
        snapshot.Right = false;
        snapshot.AnalogMove = 0.0F;
        snapshot.Jump = true;
        snapshot.Attack = true;
        snapshot.AimUp = true;
        CopperBoots::PlayerInput first = adapter.Sample(snapshot);
        Check(first.JumpPressed && first.AttackPressed && first.Aim == -1,
              "new jump and attack holds emit upward-aimed edges");
        CopperBoots::PlayerInput pending = adapter.Sample(snapshot);
        Check(pending.JumpPressed && pending.AttackPressed,
              "edges stay pending across render samples without a simulation tick");
        adapter.ConsumeEdges();
        CopperBoots::PlayerInput held = adapter.Sample(snapshot);
        Check(!held.JumpPressed && !held.AttackPressed && held.JumpHeld,
              "consumed held actions do not auto-repeat");
        snapshot.Jump = false;
        snapshot.Attack = false;
        snapshot.AimDown = true;
        Check(adapter.Sample(snapshot).Aim == 0,
              "opposing aim directions are neutral");
        snapshot.AimUp = false;
        Check(adapter.Sample(snapshot).Aim == 1,
              "down aim maps to positive aim action");
        snapshot.Jump = true;
        Check(adapter.Sample(snapshot).JumpPressed,
              "release then press produces a fresh jump edge");
        snapshot.Pause = true;
        snapshot.Interact = true;
        CopperBoots::PlayerInput pause = adapter.Sample(snapshot);
        Check(pause.PausePressed && pause.InteractHeld,
              "pause edge and interaction hold map independently");
        adapter.ConsumeEdges();
        Check(!adapter.Sample(snapshot).PausePressed,
              "held pause cannot toggle repeatedly after consumption");
        snapshot = {};
        (void)adapter.Sample(snapshot);
        snapshot.Pause = true;
        Check(adapter.Sample(snapshot).PausePressed,
              "disconnect/release then reconnect/press produces a clean pause edge");
    }

    void TestTileBounds()
    {
        CopperBoots::TileMap map(2, 2);
        Check(map.IsSolid(-1, 0), "left of map is solid");
        Check(map.IsSolid(2, 0), "right of map is solid");
        Check(!map.IsSolid(0, -1), "above map is empty");
        Check(!map.IsSolid(0, 2), "below map is open for fall-death handling");
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
            "P plated-block\n"
            "A plating\n"
            "R capacitor-block\n"
            "K capacitor\n"
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
                "broken.cbl:24:");
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
        Check(failsAt(replaced("BG!E", "BGXE"), "case.cbl:25:"),
              "unknown map glyph reports its row");
        Check(failsAt(replaced("checkpoint 2 2",
                               "spawn 1 2\ncheckpoint 2 2"),
                      "case.cbl:5:"),
              "duplicate ordered directive is rejected deterministically");

        CopperBoots::WorldSimulation collectibleWorld;
        collectibleWorld.LoadLevel(
            CopperBoots::LevelDefinition::Parse(source, "collectible.cbl"));
        Check(collectibleWorld.LevelName() == "Parser Workshop",
              "loaded level name remains available to read-only HUD state");
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
            "P plated-block\n"
            "A plating\n"
            "R capacitor-block\n"
            "K capacitor\n"
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

        std::string powerBlockSource(blockSource);
        powerBlockSource.replace(powerBlockSource.find("B?o."), 4, "BPo.");
        CopperBoots::WorldSimulation powerBlockWorld;
        powerBlockWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            powerBlockSource, "power-block.cbl"));
        powerBlockWorld.Update(headHit, fixedTick);
        Check(powerBlockWorld.LastEvents().PowerUpsReleased == 1 &&
                  powerBlockWorld.PlatingPickups().size() == 1 &&
                  powerBlockWorld.PlatingPickups()[0].EmergenceTicks == 23,
              "plated block releases one emerging jacket module");
        const float releasedY = powerBlockWorld.PlatingPickups()[0].Y;
        for (int i = 0; i < 23; ++i)
            powerBlockWorld.Update({}, fixedTick);
        Check(powerBlockWorld.PlatingPickups()[0].EmergenceTicks == 0 &&
                  powerBlockWorld.PlatingPickups()[0].Y < releasedY - 10.0F,
              "jacket module emerges for exactly 24 fixed ticks");
        const float emergedX = powerBlockWorld.PlatingPickups()[0].X;
        powerBlockWorld.Update({}, fixedTick);
        Check(powerBlockWorld.PlatingPickups()[0].X > emergedX,
              "emerged jacket module begins deterministic ground movement");

        std::string freePowerSource(blockSource);
        freePowerSource.replace(freePowerSource.find("B?o."), 4, ".A..");
        CopperBoots::WorldSimulation freePowerWorld;
        freePowerWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            freePowerSource, "free-power.cbl"));
        freePowerWorld.Update({}, fixedTick);
        Check(freePowerWorld.Player().Plated &&
                  freePowerWorld.Player().PowerTransitionTicks > 0 &&
                  freePowerWorld.LastEvents().PowerUpsCollected == 1 &&
                  freePowerWorld.Score() == 500,
              "jacket pickup grants plating, transition, and score once");
        freePowerWorld.Update({}, fixedTick);
        Check(freePowerWorld.LastEvents().PowerUpsCollected == 0 &&
                  freePowerWorld.Score() == 500,
              "collected jacket module cannot score twice");

        std::string capacitorBlockSource(blockSource);
        capacitorBlockSource.replace(capacitorBlockSource.find("B?o."), 4,
                                     "BRo.");
        CopperBoots::WorldSimulation capacitorBlockWorld;
        capacitorBlockWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            capacitorBlockSource, "capacitor-block.cbl"));
        capacitorBlockWorld.Update(headHit, fixedTick);
        Check(capacitorBlockWorld.LastEvents().CapacitorsReleased == 1 &&
                  capacitorBlockWorld.CapacitorPickups().size() == 1 &&
                  capacitorBlockWorld.CapacitorPickups()[0].EmergenceTicks == 23,
              "capacitor block releases one emerging ability pickup");
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

    void TestControllerRanges()
    {
        constexpr float tick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);

        CopperBoots::WorldSimulation rightWorld;
        CopperBoots::PlayerInput right;
        right.Move = 1.0F;
        for (int i = 0; i < 6; ++i)
            rightWorld.Update(right, tick);
        CheckNear(rightWorld.Player().VelocityX, 72.0F, 0.01F,
                  "walk reaches positive cap after six ticks");
        for (int i = 0; i < 12; ++i) {
            right.Move = -1.0F;
            rightWorld.Update(right, tick);
        }
        CheckNear(rightWorld.Player().VelocityX, -72.0F, 0.01F,
                  "ground reversal reaches negative walk cap in twelve ticks");

        CopperBoots::WorldSimulation runWorld;
        CopperBoots::PlayerInput run;
        run.Move = 1.0F;
        run.Run = true;
        for (int i = 0; i < 12; ++i)
            runWorld.Update(run, tick);
        CheckNear(runWorld.Player().VelocityX, 128.0F, 0.01F,
                  "run reaches positive cap");
        run.Move = -1.0F;
        for (int i = 0; i < 24; ++i)
            runWorld.Update(run, tick);
        CheckNear(runWorld.Player().VelocityX, -128.0F, 0.01F,
                  "run reaches negative cap");

        CopperBoots::WorldSimulation releaseWorld;
        run.Move = 1.0F;
        for (int i = 0; i < 12; ++i)
            releaseWorld.Update(run, tick);
        run.Run = false;
        releaseWorld.Update(run, tick);
        Check(releaseWorld.Player().VelocityX > 72.0F &&
                  releaseWorld.Player().VelocityX < 128.0F,
              "releasing run decelerates toward walk cap without snapping");

        CopperBoots::WorldSimulation airWorld;
        CopperBoots::PlayerInput jumpRight;
        jumpRight.Move = 1.0F;
        jumpRight.JumpPressed = true;
        jumpRight.JumpHeld = true;
        airWorld.Update(jumpRight, tick);
        const float firstAirVelocity = airWorld.Player().VelocityX;
        jumpRight.JumpPressed = false;
        airWorld.Update(jumpRight, tick);
        const float airAccelerationStep =
            airWorld.Player().VelocityX - firstAirVelocity;
        Check(airAccelerationStep > 7.0F && airAccelerationStep < 7.3F,
              "air acceleration is the limited 430 pixels-per-second-squared step");

        CopperBoots::WorldSimulation heldJumpWorld;
        const float startY = heldJumpWorld.Player().Y;
        CopperBoots::PlayerInput heldJump;
        heldJump.JumpPressed = true;
        heldJump.JumpHeld = true;
        int apexTick = 0;
        int landingTick = 0;
        float apexY = startY;
        for (int i = 1; i <= 100; ++i) {
            heldJumpWorld.Update(heldJump, tick);
            heldJump.JumpPressed = false;
            if (heldJumpWorld.Player().Y < apexY) {
                apexY = heldJumpWorld.Player().Y;
                apexTick = i;
            }
            if (i > 1 && heldJumpWorld.Player().Grounded) {
                landingTick = i;
                break;
            }
        }
        Check(apexTick >= 15 && apexTick <= 18,
              "held jump apex tick stays in tuned range");
        Check(startY - apexY >= 40.0F && startY - apexY <= 46.0F,
              "held jump apex height stays in tuned range");
        Check(landingTick >= 31 && landingTick <= 39,
              "held jump total airtime stays in tuned range");
        for (int i = 0; i < 10; ++i)
            heldJumpWorld.Update(heldJump, tick);
        Check(heldJumpWorld.Player().Grounded &&
                  std::abs(heldJumpWorld.Player().Y - startY) < 0.01F,
              "held jump without another press cannot auto-repeat after landing");

        CopperBoots::WorldSimulation walkJumpWorld;
        CopperBoots::WorldSimulation runJumpWorld;
        CopperBoots::PlayerInput walkJump;
        walkJump.Move = 1.0F;
        walkJump.JumpPressed = true;
        walkJump.JumpHeld = true;
        CopperBoots::PlayerInput runningJump = walkJump;
        runningJump.Run = true;
        for (int i = 0; i < 30; ++i) {
            walkJumpWorld.Update(walkJump, tick);
            runJumpWorld.Update(runningJump, tick);
            walkJump.JumpPressed = false;
            runningJump.JumpPressed = false;
        }
        Check(runJumpWorld.Player().X > walkJumpWorld.Player().X + 15.0F,
              "run state increases deterministic horizontal jump reach");
    }

    void TestLedgeFallAndTerminalVelocity()
    {
        constexpr float tick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);
        std::string edgeSource = MakeProjectileLevel(false, false);
        const std::string fullFloor(40, '#');
        const std::string shortFloor = "####" + std::string(36, '.');
        edgeSource.replace(edgeSource.rfind(fullFloor), fullFloor.size(),
                           shortFloor);

        CopperBoots::WorldSimulation world;
        world.LoadLevel(CopperBoots::LevelDefinition::Parse(
            edgeSource, "ledge.cbl"));
        CopperBoots::PlayerInput run;
        run.Move = 1.0F;
        run.Run = true;
        bool enteredFalling = false;
        float maximumFallVelocity = 0.0F;
        bool died = false;
        for (int i = 0; i < 180; ++i) {
            world.Update(run, tick);
            enteredFalling = enteredFalling ||
                             world.Player().Motion == CopperBoots::PlayerMotion::Falling;
            maximumFallVelocity = std::max(maximumFallVelocity,
                                           world.Player().VelocityY);
            if (world.LastEvents().PlayerDied == 1) {
                died = true;
                break;
            }
        }
        Check(enteredFalling, "walking from a ledge enters falling state");
        Check(maximumFallVelocity <= 420.01F && maximumFallVelocity >= 399.0F,
              "fall velocity reaches but never exceeds terminal cap");
        Check(died, "long ledge fall emits one out-of-world death event");
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

        camera.SetWorldBounds(1'000.0F, 1'000.0F);
        camera.SnapTo(500.0F, 400.0F);
        const float initialX = camera.BaseX();
        for (int i = 0; i < 120; ++i)
            camera.Update(500.0F, 400.0F, 100.0F, 1.0F / 60.0F);
        Check(camera.BaseX() > initialX + 27.0F && camera.BaseX() < 368.1F,
              "camera converges on velocity look-ahead target");

        camera.SetVerticalPolicy(CopperBoots::CameraVerticalPolicy::Locked);
        const float lockedY = camera.BaseY();
        camera.Update(500.0F, 900.0F, 0.0F, 1.0F);
        CheckNear(camera.BaseY(), lockedY, 0.001F,
                  "locked vertical policy preserves base Y");
        camera.SetVerticalPolicy(CopperBoots::CameraVerticalPolicy::Follow);

        const float baseX = camera.BaseX();
        const float baseY = camera.BaseY();
        camera.SetShakeOffset(7.0F, -5.0F);
        CheckNear(camera.X(), baseX + 7.0F, 0.001F,
                  "shake offset is isolated from camera base X");
        CheckNear(camera.Y(), baseY - 5.0F, 0.001F,
                  "shake offset is isolated from camera base Y");
        camera.ClearShake();
        CheckNear(camera.X(), baseX, 0.001F, "clearing shake restores base X");
        camera.SnapTo(2'000.0F, 400.0F);
        camera.SetShakeOffset(100.0F, 0.0F);
        CheckNear(camera.X(), 680.0F, 0.001F,
                  "shake cannot expose beyond right world bound");
        camera.ClearShake();
    }

    void TestParallaxDescriptor()
    {
        const CopperBoots::ParallaxLayer layer{
            0.25F, 0.5F, 64, 120, 20, {60, 83, 112},
            CopperBoots::ParallaxGeometry::BlockSilhouette, true, false};
        Check(layer.WrappedOffset(252.0F) == 63,
              "parallax offset reaches last pixel before repeat seam");
        Check(layer.WrappedOffset(256.0F) == 0,
              "parallax offset wraps without a missing seam pixel");
        Check(layer.WrappedOffset(-4.0F) == 63,
              "parallax wrapping is deterministic for negative camera input");
        CopperBoots::ParallaxLayer fixed = layer;
        fixed.Fixed = true;
        Check(fixed.WrappedOffset(999.0F) == 0,
              "fixed parallax layer ignores camera movement");
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
                  sideWorld.Player().Dead && sideWorld.Lives() == 2,
              "unprotected side contact damages player and starts death");
        sideWorld.Update({}, tick);
        Check(sideWorld.LastEvents().PlayerDamaged == 0,
              "continued crawler contact cannot damage during death");

        CopperBoots::WorldSimulation platedWorld;
        platedWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeCrawlerLevel(8, 2, 2, 4), "plated-side.cbl"));
        platedWorld.SetPlayerPlated(true);
        platedWorld.Update({}, tick);
        Check(!platedWorld.Player().Plated &&
                  !platedWorld.Player().Dead &&
                  platedWorld.Player().InvulnerabilityTicks > 0 &&
                  platedWorld.LastEvents().PlayerDamaged == 1,
              "crawler contact consumes plated protection and grants invulnerability");

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
            "P plated-block\n"
            "A plating\n"
            "R capacitor-block\n"
            "K capacitor\n"
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
        bool activatedOnApproach = false;
        for (int i = 0; i < 250; ++i) {
            activationWorld.Update(runAway, tick);
            activatedOnApproach = activatedOnApproach ||
                                  activationWorld.Crawlers()[0].Active;
        }
        Check(activatedOnApproach,
              "crawler activates when camera approaches its range");
    }

    void TestDeathAndRespawn()
    {
        constexpr float tick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);
        CopperBoots::WorldSimulation hazardWorld;
        hazardWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeDeathLevel(true), "hazard.cbl"));
        hazardWorld.Update({}, tick);
        Check(hazardWorld.Player().Dead && hazardWorld.Lives() == 2 &&
                  hazardWorld.LastEvents().PlayerDied == 1,
              "hazard starts one death and decrements one life");
        for (int i = 0; i < 44; ++i)
            hazardWorld.Update({}, tick);
        Check(hazardWorld.Player().Dead,
              "death state remains bounded through tick 44");
        hazardWorld.Update({}, tick);
        Check(!hazardWorld.Player().Dead &&
                  hazardWorld.LastEvents().PlayerRespawned == 1 &&
                  std::abs(hazardWorld.Player().X - 80.0F) < 0.01F,
              "tick 45 respawns at external checkpoint coordinate");
        hazardWorld.Update({}, tick);
        Check(hazardWorld.LastEvents().PlayerDied == 0 &&
                  hazardWorld.Lives() == 2,
              "respawn away from hazard does not repeat death");

        CopperBoots::WorldSimulation fallWorld;
        fallWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeDeathLevel(false), "fall.cbl"));
        bool fellOut = false;
        for (int i = 0; i < 120; ++i) {
            fallWorld.Update({}, tick);
            if (fallWorld.LastEvents().PlayerDied == 1) {
                fellOut = true;
                break;
            }
        }
        Check(fellOut && fallWorld.Player().Dead && fallWorld.Lives() == 2,
              "falling through an open lower boundary starts death once");
    }

    void TestArcProjectiles()
    {
        constexpr float tick = static_cast<float>(
            CopperBoots::SimulationClock::TickSeconds);
        const auto activeCount = [](const CopperBoots::WorldSimulation& world) {
            int count = 0;
            for (const CopperBoots::ProjectileState& projectile :
                 world.Projectiles()) {
                count += static_cast<int>(projectile.Active);
            }
            return count;
        };

        CopperBoots::WorldSimulation poolWorld;
        poolWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeProjectileLevel(false, false), "projectile-pool.cbl"));
        poolWorld.Update({}, tick);
        Check(poolWorld.Player().ArcCapacitor &&
                  poolWorld.LastEvents().CapacitorsCollected == 1 &&
                  poolWorld.Score() == 750,
              "capacitor pickup grants attack ability and score");
        CopperBoots::PlayerInput fire;
        fire.AttackPressed = true;
        poolWorld.Update(fire, tick);
        Check(activeCount(poolWorld) == 1 &&
                  poolWorld.LastEvents().ProjectilesFired == 1,
              "edge-triggered attack occupies first projectile slot");
        fire.AttackPressed = false;
        poolWorld.Update(fire, tick);
        fire.AttackPressed = true;
        poolWorld.Update(fire, tick);
        Check(activeCount(poolWorld) == 2,
              "second attack occupies second projectile slot");
        fire.AttackPressed = false;
        poolWorld.Update(fire, tick);
        fire.AttackPressed = true;
        poolWorld.Update(fire, tick);
        Check(activeCount(poolWorld) == 2 &&
                  poolWorld.LastEvents().ProjectilesFired == 0,
              "third attack is rejected while both slots are live");

        CopperBoots::WorldSimulation upWorld;
        upWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeProjectileLevel(false, false), "aim-up.cbl"));
        upWorld.Update({}, tick);
        fire.Aim = -1;
        upWorld.Update(fire, tick);
        Check(upWorld.Projectiles()[0].VelocityY < -100.0F &&
                  std::abs(upWorld.Projectiles()[0].VelocityX) < 130.0F,
              "up aim selects upward arc trajectory");

        CopperBoots::WorldSimulation bounceWorld;
        bounceWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeProjectileLevel(false, false), "bounce.cbl"));
        bounceWorld.Update({}, tick);
        fire.Aim = 1;
        bounceWorld.Update(fire, tick);
        bool bounced = false;
        for (int i = 0; i < 60 && bounceWorld.Projectiles()[0].Active; ++i) {
            bounceWorld.Update({}, tick);
            bounced = bounced || bounceWorld.Projectiles()[0].VelocityY < -100.0F;
        }
        Check(bounced, "down-aimed projectile bounces from solid floor");

        CopperBoots::WorldSimulation wallWorld;
        wallWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeProjectileLevel(false, true), "projectile-wall.cbl"));
        wallWorld.Update({}, tick);
        fire.Aim = 0;
        wallWorld.Update(fire, tick);
        for (int i = 0; i < 90 && activeCount(wallWorld) != 0; ++i)
            wallWorld.Update({}, tick);
        Check(activeCount(wallWorld) == 0,
              "projectile dies when its horizontal path hits a wall");

        CopperBoots::WorldSimulation enemyWorld;
        enemyWorld.LoadLevel(CopperBoots::LevelDefinition::Parse(
            MakeProjectileLevel(true, false), "projectile-enemy.cbl"));
        enemyWorld.Update({}, tick);
        enemyWorld.Update(fire, tick);
        bool projectileDefeat = false;
        for (int i = 0; i < 120; ++i) {
            enemyWorld.Update({}, tick);
            projectileDefeat = projectileDefeat ||
                enemyWorld.LastEvents().EnemiesDefeated == 1;
            if (projectileDefeat)
                break;
        }
        Check(projectileDefeat && enemyWorld.Crawlers()[0].Defeated,
              "projectile overlap defeats crawler deterministically");

        for (int i = 0; i < 240 && activeCount(upWorld) != 0; ++i)
            upWorld.Update({}, tick);
        Check(activeCount(upWorld) == 0,
              "projectile outside camera cleanup margin is retired");
    }
}

int main()
{
    TestSimulationClock();
    TestInputActionAdapter();
    TestTileBounds();
    TestLevelParsing();
    TestMovementAndJump();
    TestVariableJumpHeight();
    TestControllerRanges();
    TestLedgeFallAndTerminalVelocity();
    TestCameraBounds();
    TestParallaxDescriptor();
    TestClockworkCrawler();
    TestDeathAndRespawn();
    TestArcProjectiles();

    if (failures != 0) {
        std::cerr << failures << " gameplay test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Copper Boots gameplay tests passed\n";
    return EXIT_SUCCESS;
}
