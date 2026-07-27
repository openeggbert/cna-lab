// Gate M4 (plan_39 IS-39-005): character, trigger, raycast, and vehicle prototypes proven behind
// IronShadows::Physics::PhysicsWorld (Jolt Physics, see plan_15-physics-integration.md). Headless
// -- Jolt is a pure CPU library, no GraphicsDevice/window needed, matching CoreTests.cpp's style.

#include "IronShadows/Physics/PhysicsWorld.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    using namespace IronShadows::Physics;
    using Microsoft::Xna::Framework::Vector3;

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void StepFor(PhysicsWorld& world, float totalSeconds, float stepSeconds = 1.0F / 60.0F)
    {
        for (float t = 0.0F; t < totalSeconds; t += stepSeconds)
        {
            world.Step(stepSeconds);
        }
    }

    void TestRaycastHitsStaticFloor()
    {
        PhysicsWorld world;
        const RigidBodyHandle floor =
            world.CreateStaticBody(ShapeDesc::Box(Vector3(50.0F, 0.5F, 50.0F)), Vector3(0.0F, 0.0F, 0.0F));

        const RaycastHit hit = world.Raycast(Vector3(0.0F, 10.0F, 0.0F), Vector3(0.0F, -1.0F, 0.0F), 20.0F);
        Require(hit.hit, "raycast must hit the static floor");
        Require(hit.body.value == floor.value, "raycast hit body must be the floor");
        Require(std::abs(hit.point.Y - 0.5F) < 0.05F, "raycast hit point must be near the floor's top surface");
        Require(hit.normal.Y > 0.9F, "raycast hit normal must point up, away from the floor");

        const RaycastHit miss = world.Raycast(Vector3(0.0F, 10.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F), 1.0F);
        Require(!miss.hit, "a short horizontal ray at height 10 must not hit the floor");
    }

    void TestDynamicBodySettlesOnFloor()
    {
        PhysicsWorld world;
        (void)world.CreateStaticBody(ShapeDesc::Box(Vector3(50.0F, 0.5F, 50.0F)), Vector3(0.0F, 0.0F, 0.0F));
        const RigidBodyHandle box =
            world.CreateDynamicBody(ShapeDesc::Box(Vector3(0.5F, 0.5F, 0.5F)), Vector3(0.0F, 5.0F, 0.0F), 10.0F);

        StepFor(world, 3.0F);

        const Vector3 finalPosition = world.GetBodyPosition(box);
        Require(std::abs(finalPosition.Y - 1.0F) < 0.1F,
               "a box dropped above a floor must come to rest on top of it (expected y near 1.0)");

        world.DestroyBody(box);
    }

    void TestTriggerFiresEnterAndExit()
    {
        PhysicsWorld world;
        // Tall on purpose: this test is about horizontal enter/exit detection, not gravity, so a
        // generous Y half-extent keeps the falling mover inside the trigger's vertical range for
        // the whole horizontal crossing instead of falling out of it before reaching Z=0.
        const RigidBodyHandle trigger =
            world.CreateTrigger(ShapeDesc::Box(Vector3(2.0F, 50.0F, 2.0F)), Vector3(0.0F, 0.0F, 0.0F));
        const RigidBodyHandle mover =
            world.CreateDynamicBody(ShapeDesc::Box(Vector3(0.3F, 0.3F, 0.3F)), Vector3(0.0F, 0.0F, -10.0F), 1.0F);
        world.SetBodyLinearVelocity(mover, Vector3(0.0F, 0.0F, 8.0F));

        bool sawEnter = false;
        bool sawExit = false;
        for (int i = 0; i < 240 && !(sawEnter && sawExit); ++i)
        {
            world.Step(1.0F / 60.0F);
            for (const TriggerEvent& event : world.ConsumeTriggerEvents())
            {
                if (event.trigger.value != trigger.value || event.other.value != mover.value)
                {
                    continue;
                }
                if (event.entered)
                {
                    sawEnter = true;
                }
                else
                {
                    sawExit = true;
                }
            }
        }

        Require(sawEnter, "a body passing through the trigger volume must fire an enter event");
        Require(sawExit, "a body leaving the trigger volume must fire an exit event");

        world.DestroyBody(trigger);
        world.DestroyBody(mover);
    }

    void TestCharacterControllerIsBlockedByWallAndGrounded()
    {
        PhysicsWorld world;
        (void)world.CreateStaticBody(ShapeDesc::Box(Vector3(50.0F, 0.5F, 50.0F)), Vector3(0.0F, 0.0F, 0.0F));
        // A thin wall crossing the character's path (character walks toward -Z): wide in X so it
        // cannot be walked around within the test's duration, thin in Z, tall in Y.
        (void)world.CreateStaticBody(ShapeDesc::Box(Vector3(5.0F, 2.0F, 0.2F)), Vector3(0.0F, 2.0F, -3.0F));

        const CharacterHandle character = world.CreateCharacter(Vector3(0.0F, 1.5F, 0.0F), 0.3F, 0.6F);

        // Settle onto the floor first.
        for (int i = 0; i < 60; ++i)
        {
            world.MoveCharacter(character, Vector3(0.0F, -1.0F, 0.0F), 1.0F / 60.0F);
            world.Step(1.0F / 60.0F);
        }
        Require(world.IsCharacterGrounded(character), "character must be grounded after settling onto the floor");
        const float groundedHeight = world.GetCharacterPosition(character).Y;

        // Walk toward the wall for long enough that an unobstructed character would have passed
        // through it (wall is 5 units away, well within reach at this speed/duration).
        for (int i = 0; i < 300; ++i)
        {
            world.MoveCharacter(character, Vector3(0.0F, -1.0F, -2.0F), 1.0F / 60.0F);
            world.Step(1.0F / 60.0F);
        }

        const Vector3 blockedPosition = world.GetCharacterPosition(character);
        Require(blockedPosition.Z > -2.9F, "the wall must stop the character before it reaches the wall's position");
        Require(std::abs(blockedPosition.Y - groundedHeight) < 0.3F,
               "the character must still be near ground height, not fallen through the floor");

        world.DestroyCharacter(character);
    }

    void TestVehicleDrivesForward()
    {
        PhysicsWorld world;
        (void)world.CreateStaticBody(ShapeDesc::Box(Vector3(100.0F, 0.5F, 100.0F)), Vector3(0.0F, 0.0F, 0.0F));

        const std::array<Vector3, 4> wheelPositions = {
            Vector3(1.0F, -0.3F, 1.8F), Vector3(-1.0F, -0.3F, 1.8F),
            Vector3(1.0F, -0.3F, -1.8F), Vector3(-1.0F, -0.3F, -1.8F),
        };
        const VehicleHandle vehicle = world.CreateFourWheelVehicle(
            Vector3(1.0F, 0.3F, 2.1F), 1200.0F, Vector3(0.0F, 1.2F, 0.0F), wheelPositions, 0.35F, 0.3F);

        // Let the vehicle settle onto its suspension before driving.
        StepFor(world, 1.5F);
        const Vector3 startPosition = world.GetVehiclePosition(vehicle);

        bool anyWheelContact = false;
        for (const VehicleWheelState& wheel : world.GetVehicleWheelStates(vehicle))
        {
            anyWheelContact = anyWheelContact || wheel.hasContact;
        }
        Require(anyWheelContact, "at least one wheel must be in contact with the ground after settling");

        for (int i = 0; i < 180; ++i)
        {
            world.SetVehicleInput(vehicle, /*forward=*/1.0F, /*steer=*/0.0F, /*brake=*/0.0F, /*handBrake=*/0.0F);
            world.Step(1.0F / 60.0F);
        }

        const Vector3 endPosition = world.GetVehiclePosition(vehicle);
        const float dz = endPosition.Z - startPosition.Z;
        Require(std::abs(dz) > 1.0F, "three seconds of forward throttle must move the vehicle a meaningful distance");

        world.DestroyVehicle(vehicle);
    }
}

int main()
{
    const std::pair<const char*, void (*)()> tests[] = {
        {"RaycastHitsStaticFloor", TestRaycastHitsStaticFloor},
        {"DynamicBodySettlesOnFloor", TestDynamicBodySettlesOnFloor},
        {"TriggerFiresEnterAndExit", TestTriggerFiresEnterAndExit},
        {"CharacterControllerIsBlockedByWallAndGrounded", TestCharacterControllerIsBlockedByWallAndGrounded},
        {"VehicleDrivesForward", TestVehicleDrivesForward},
    };

    int failures = 0;
    for (const auto& [name, testFn] : tests)
    {
        std::cout << "[RUN]  " << name << std::endl;
        try
        {
            testFn();
            std::cout << "[PASS] " << name << '\n';
        }
        catch (const std::exception& error)
        {
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
            ++failures;
        }
    }

    if (failures == 0)
    {
        std::cout << "All physics tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
