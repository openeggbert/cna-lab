#include "People/Objects/ObjectModel.hpp"

#include <array>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace People::Objects;
using namespace People::World;

namespace
{
    int failures = 0;

    void Check(const bool condition, const std::string& message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    void CheckThrows(const std::function<void()>& operation, const std::string& message)
    {
        try
        {
            operation();
            Check(false, message);
        }
        catch (const std::exception&)
        {
        }
    }

    ObjectDefinition MakeSlottedObject()
    {
        ObjectDefinition definition{
            "people.slot.test", "Slotted Object", ObjectCategory::Miscellaneous,
            10, {{0, 0}}, {}, 0x0F, {}, {}
        };
        definition.interactionSlots.push_back({
            "primary", {2, 1}, SlotFacing::North, SlotPosture::Seated, 2,
            {{0, 1}, {1, 0}}
        });
        return definition;
    }

    void TestDefinitionValidation()
    {
        LotGrid lot(5, 5);
        ObjectWorld world(lot);
        Check(world.RegisterDefinition(MakeSlottedObject()),
              "valid interaction slot enters object definition");

        ObjectDefinition invalid = MakeSlottedObject();
        invalid.id = "people.slot.empty_id";
        invalid.interactionSlots[0].id.clear();
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "empty slot ID is rejected");

        invalid = MakeSlottedObject();
        invalid.id = "people.slot.duplicate";
        invalid.interactionSlots.push_back(invalid.interactionSlots[0]);
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "duplicate slot ID is rejected per object");

        invalid = MakeSlottedObject();
        invalid.id = "people.slot.zero_capacity";
        invalid.interactionSlots[0].capacity = 0;
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "zero-capacity slot is rejected");

        invalid = MakeSlottedObject();
        invalid.id = "people.slot.bad_facing";
        invalid.interactionSlots[0].facing = static_cast<SlotFacing>(4);
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "invalid slot facing is rejected");

        invalid = MakeSlottedObject();
        invalid.id = "people.slot.bad_posture";
        invalid.interactionSlots[0].posture = static_cast<SlotPosture>(3);
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "invalid slot posture is rejected");

        invalid = MakeSlottedObject();
        invalid.id = "people.slot.duplicate_clearance";
        invalid.interactionSlots[0].clearance.push_back({0, 1});
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "duplicate slot-clearance offset is rejected");
    }

    void TestFourRotationsResolveWorldTarget()
    {
        const std::array<TileCoordinate, 4> approaches{{
            {9, 8, 0}, {6, 9, 0}, {5, 6, 0}, {8, 5, 0}
        }};
        const std::array<std::vector<TileCoordinate>, 4> clearances{{
            {{9, 9, 0}, {10, 8, 0}},
            {{5, 9, 0}, {6, 10, 0}},
            {{5, 5, 0}, {4, 6, 0}},
            {{9, 5, 0}, {8, 4, 0}}
        }};

        for (int index = 0; index < 4; ++index)
        {
            LotGrid lot(15, 15);
            ObjectWorld world(lot);
            (void)world.RegisterDefinition(MakeSlottedObject());
            const auto rotation = static_cast<ObjectRotation>(index);
            Check(world.Place({100, "people.slot.test", {7, 7, 0}, rotation}).IsValid(),
                  "rotated slotted object places");

            const SlotResolutionResult result = world.ResolveInteractionSlot(100, "primary");
            Check(result.IsValid(), "known object slot resolves");
            Check(result.slot.objectId == 100 && result.slot.slotId == "primary",
                  "resolved slot retains stable object and slot identity");
            Check(result.slot.approachTile == approaches[static_cast<std::size_t>(index)],
                  "approach offset rotates into expected world tile");
            Check(result.slot.facing == static_cast<SlotFacing>(index),
                  "required facing rotates with object orientation");
            Check(result.slot.posture == SlotPosture::Seated && result.slot.capacity == 2,
                  "posture and capacity remain authored simulation values");
            Check(result.slot.clearanceTiles == clearances[static_cast<std::size_t>(index)],
                  "slot-relative clearance rotates around world approach tile");
        }
    }

    void TestResolutionFailuresAndFacingRoundTrip()
    {
        LotGrid lot(5, 5);
        ObjectWorld world(lot);
        (void)world.RegisterDefinition(MakeSlottedObject());
        (void)world.Place({1, "people.slot.test", {1, 1, 0}, ObjectRotation::North});
        Check(world.ResolveInteractionSlot(999, "primary").failure
                  == SlotResolutionFailure::UnknownInstance,
              "unknown object instance has exact resolution failure");
        Check(world.ResolveInteractionSlot(1, "missing").failure
                  == SlotResolutionFailure::UnknownSlot,
              "unknown authored slot has exact resolution failure");

        SlotFacing facing = SlotFacing::North;
        for (int turn = 0; turn < 4; ++turn)
            facing = ObjectWorld::RotateSlotFacing(facing, ObjectRotation::East);
        Check(facing == SlotFacing::North, "four clockwise slot-facing turns round trip");
        CheckThrows([&] {
            (void)ObjectWorld::RotateSlotFacing(
                static_cast<SlotFacing>(4), ObjectRotation::North);
        }, "invalid direct slot facing rotation is rejected");
    }
}

int main()
{
    TestDefinitionValidation();
    TestFourRotationsResolveWorldTarget();
    TestResolutionFailuresAndFacingRoundTrip();

    if (failures != 0)
    {
        std::cerr << failures << " People interaction-slot test(s) failed\n";
        return 1;
    }
    std::cout << "All People interaction-slot tests passed\n";
    return 0;
}
