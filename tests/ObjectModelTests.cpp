#include "People/Objects/ObjectModel.hpp"

#include <array>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

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

    ObjectDefinition MakeChair()
    {
        return {
            "people.chair.test", "Test Chair", ObjectCategory::Seating, 75,
            {{0, 0}}, {{0, 1}}, 0x0F, {}
        };
    }

    void TestCatalogValidation()
    {
        LotGrid lot(5, 5);
        ObjectWorld world(lot);
        Check(world.RegisterDefinition(MakeChair()), "valid definition enters catalog");
        Check(!world.RegisterDefinition(MakeChair()), "duplicate definition ID is rejected");
        Check(world.Catalog().Size() == 1, "catalog size remains definition-driven");
        const ObjectDefinition* chair = world.Catalog().Find("people.chair.test");
        Check(chair != nullptr && chair->price == 75,
              "catalog lookup returns immutable definition data");

        ObjectDefinition invalid = MakeChair();
        invalid.id.clear();
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "empty definition ID is rejected");
        invalid = MakeChair();
        invalid.footprint.push_back({0, 0});
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "duplicate footprint cell is rejected");
        invalid = MakeChair();
        invalid.clearance = {{0, 0}};
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "clearance overlapping footprint is rejected");
    }

    void TestFourRotations()
    {
        const FootprintOffset start{2, 1};
        const std::array<FootprintOffset, 4> expected{{
            {2, 1}, {-1, 2}, {-2, -1}, {1, -2}
        }};
        for (int index = 0; index < 4; ++index)
        {
            const auto rotation = static_cast<ObjectRotation>(index);
            Check(ObjectWorld::RotateOffset(start, rotation) == expected[static_cast<std::size_t>(index)],
                  "footprint offset rotates around unchanged anchor");
            Check(ObjectWorld::RotationBit(rotation) == static_cast<std::uint8_t>(1U << index),
                  "rotation maps to stable allowed bit");
        }

        FootprintOffset value = start;
        for (int count = 0; count < 4; ++count)
            value = ObjectWorld::RotateOffset(value, ObjectRotation::East);
        Check(value == start, "four clockwise turns restore footprint offset");
    }

    void TestPlacementAndOccupancy()
    {
        LotGrid lot(5, 5);
        ObjectWorld world(lot);
        (void)world.RegisterDefinition(MakeChair());
        const PlacementResult placed = world.Place({101, "people.chair.test", {2, 2, 0},
                                                     ObjectRotation::North});
        Check(placed.IsValid(), "valid instance placement succeeds");
        Check(world.Find(101) != nullptr && world.Find(101)->definitionId == "people.chair.test",
              "persistent instance retains stable ID and definition reference");
        Check(world.OccupiedBy({2, 2, 0}) == std::optional<ObjectInstanceId>(101),
              "footprint cell records occupant");
        Check(!world.OccupiedBy({2, 3, 0}).has_value(),
              "clearance cell is validated but not occupied");

        PlacementResult lateBlocker = world.Place({104, "people.chair.test", {2, 3, 0},
                                                    ObjectRotation::South});
        Check(lateBlocker.failure == PlacementFailure::ClearanceBlocked
                  && lateBlocker.conflictingInstance == std::optional<ObjectInstanceId>(101),
              "a later footprint cannot consume an existing object's clearance");

        PlacementResult conflict = world.Place({102, "people.chair.test", {2, 2, 0},
                                                 ObjectRotation::South});
        Check(conflict.failure == PlacementFailure::Occupied
                  && conflict.conflictingInstance == std::optional<ObjectInstanceId>(101),
              "occupied placement identifies conflicting instance");
        PlacementResult blocked = world.Place({103, "people.chair.test", {2, 1, 0},
                                                ObjectRotation::North});
        Check(blocked.failure == PlacementFailure::ClearanceBlocked
                  && blocked.conflictingInstance == std::optional<ObjectInstanceId>(101),
              "clearance placement identifies blocking instance");

        Check(world.Place({105, "people.chair.test", {1, 3, 0},
                           ObjectRotation::West}).IsValid(),
              "nonexclusive clearance claims may overlap");
        Check(world.Remove(101), "object removal succeeds");
        Check(!world.OccupiedBy({2, 2, 0}).has_value(), "removal releases occupancy");
        lateBlocker = world.Place({106, "people.chair.test", {2, 3, 0},
                                   ObjectRotation::South});
        Check(lateBlocker.failure == PlacementFailure::ClearanceBlocked
                  && lateBlocker.conflictingInstance == std::optional<ObjectInstanceId>(105),
              "removal preserves another object's overlapping clearance claim");
        Check(world.Remove(105), "second object removal succeeds");
        Check(world.Place({107, "people.chair.test", {2, 3, 0},
                           ObjectRotation::South}).IsValid(),
              "removing the final claim releases shared clearance");
        Check(!world.Remove(101), "second removal is an explicit no-op");
    }

    void TestPlacementFailuresAndRotationMask()
    {
        LotGrid lot(3, 3);
        ObjectWorld world(lot);
        ObjectDefinition bed{
            "people.bed.test", "Test Bed", ObjectCategory::Beds, 400,
            {{0, 0}, {0, 1}}, {{0, 2}},
            static_cast<std::uint8_t>(
                ObjectWorld::RotationBit(ObjectRotation::North)
                | ObjectWorld::RotationBit(ObjectRotation::South)),
            {}
        };
        (void)world.RegisterDefinition(std::move(bed));

        Check(world.ValidatePlacement("missing", {1, 1, 0}, ObjectRotation::North).failure
                  == PlacementFailure::UnknownDefinition,
              "unknown definition fails explicitly");
        Check(world.ValidatePlacement("people.bed.test", {1, 1, 0}, ObjectRotation::East).failure
                  == PlacementFailure::RotationNotAllowed,
              "disallowed rotation fails explicitly");
        Check(world.ValidatePlacement("people.bed.test", {1, 2, 0}, ObjectRotation::North).failure
                  == PlacementFailure::OutsideLot,
              "footprint beyond lot fails bounds");
        Check(world.ValidatePlacement("people.bed.test", {1, 0, 0}, ObjectRotation::South).failure
                  == PlacementFailure::OutsideLot,
              "negative rotated footprint fails bounds");
        Check(world.ValidatePlacement("people.bed.test", {1, 0, 0}, ObjectRotation::North).failure
                  == PlacementFailure::None,
              "in-bounds multi-cell footprint and clearance validate");
        Check(world.Place({0, "people.bed.test", {1, 0, 0}, ObjectRotation::North}).failure
                  == PlacementFailure::InvalidInstanceId,
              "zero stable instance ID is rejected");
        Check(world.Place({200, "people.bed.test", {1, 0, 0}, ObjectRotation::North}).IsValid(),
              "first explicit stable ID places");
        Check(world.Place({200, "people.bed.test", {0, 0, 0}, ObjectRotation::North}).failure
                  == PlacementFailure::DuplicateInstanceId,
              "duplicate stable instance ID is rejected");
    }
}

int main()
{
    TestCatalogValidation();
    TestFourRotations();
    TestPlacementAndOccupancy();
    TestPlacementFailuresAndRotationMask();

    if (failures != 0)
    {
        std::cerr << failures << " People object-model test(s) failed\n";
        return 1;
    }
    std::cout << "All People object-model tests passed\n";
    return 0;
}
