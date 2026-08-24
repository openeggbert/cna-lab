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
            "map\n"
            ".d..\n"
            "B!E.\n"
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
        Check(level.Map.Get(1, 0).Visual == CopperBoots::TileVisual::Decoration &&
                  level.Map.Get(1, 0).Collision == CopperBoots::TileCollision::None,
              "decoration keeps visual and collision independent");
        Check(level.Map.Get(0, 1) == CopperBoots::Tiles::Breakable,
              "breakable glyph maps to breakable solid");
        Check(level.Map.Get(1, 1) == CopperBoots::Tiles::Hazard,
              "hazard glyph maps to hazard semantics");
        Check(level.Map.Get(2, 1) == CopperBoots::Tiles::Exit,
              "exit glyph maps to exit semantics");

        std::string malformed(source);
        const std::size_t row = malformed.find(".d..\n");
        malformed.erase(row, 1);
        bool threwLineError = false;
        try {
            (void)CopperBoots::LevelDefinition::Parse(malformed, "broken.cbl");
        }
        catch (const std::runtime_error& error) {
            threwLineError = std::string_view(error.what()).starts_with(
                "broken.cbl:15:");
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
        Check(failsAt(replaced("B!E.", "B?E."), "case.cbl:16:"),
              "unknown map glyph reports its row");
        Check(failsAt(replaced("checkpoint 2 2",
                               "spawn 1 2\ncheckpoint 2 2"),
                      "case.cbl:5:"),
              "duplicate ordered directive is rejected deterministically");
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
}

int main()
{
    TestSimulationClock();
    TestTileBounds();
    TestLevelParsing();
    TestMovementAndJump();
    TestVariableJumpHeight();
    TestCameraBounds();

    if (failures != 0) {
        std::cerr << failures << " gameplay test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Copper Boots gameplay tests passed\n";
    return EXIT_SUCCESS;
}
