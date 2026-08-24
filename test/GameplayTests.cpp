#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "CopperBoots/Camera2D.hpp"
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
        map.Set(1, 1, CopperBoots::TileKind::Solid);
        Check(map.IsSolid(1, 1), "set solid tile is returned");
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

