#include "People/Simulation/ResidentModel.hpp"

#include <iostream>
#include <optional>
#include <string>

using namespace People::Simulation;
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

    ResidentState MakeResident()
    {
        return {2001, 1, "Mara Vale", {2, 3, 0}, std::nullopt, std::nullopt};
    }

    void TestIdentityAndLocationValidation()
    {
        LotGrid lot(5, 5, 2);
        ResidentRegistry registry(lot);
        Check(registry.Add(MakeResident()).IsValid(), "valid resident enters active lot");
        Check(registry.Residents().size() == 1, "registry exposes one stable resident");
        const ResidentState* mara = registry.Find(2001);
        Check(mara != nullptr && mara->householdId == 1 && mara->displayName == "Mara Vale",
              "resident retains stable identity and household reference");
        Check(mara != nullptr && mara->tile == TileCoordinate{2, 3, 0},
              "resident retains renderer-independent logical tile");

        ResidentState invalid = MakeResident();
        invalid.id = 0;
        Check(registry.Add(invalid).failure == ResidentFailure::InvalidResidentId,
              "zero resident ID is rejected");
        invalid = MakeResident();
        invalid.householdId = 0;
        Check(registry.Add(invalid).failure == ResidentFailure::InvalidHouseholdId,
              "zero household ID is rejected");
        invalid = MakeResident();
        invalid.displayName.clear();
        Check(registry.Add(invalid).failure == ResidentFailure::EmptyDisplayName,
              "empty display name is rejected");
        invalid = MakeResident();
        invalid.tile = {5, 0, 0};
        Check(registry.Add(invalid).failure == ResidentFailure::OutsideLot,
              "outside logical tile is rejected");
        Check(registry.Add(MakeResident()).failure == ResidentFailure::DuplicateResidentId,
              "duplicate stable resident ID is rejected");
    }

    void TestMovementAndActionReferences()
    {
        LotGrid lot(5, 5);
        ResidentRegistry registry(lot);
        (void)registry.Add(MakeResident());

        Check(registry.SetTile(2001, {4, 1, 0}).IsValid(),
              "logical position mutation succeeds in bounds");
        Check(registry.Find(2001)->tile == TileCoordinate{4, 1, 0},
              "logical position mutation is observable without render state");
        Check(registry.SetTile(2001, {-1, 1, 0}).failure == ResidentFailure::OutsideLot,
              "position mutation cannot leave the lot");

        Check(registry.SetMovementRequest(2001, MovementRequestId{3001}).IsValid(),
              "resident can reference one movement request");
        Check(registry.SetActiveAction(2001, ActionId{4001}).IsValid(),
              "resident can reference one active action");
        Check(registry.Find(2001)->movementRequest == std::optional<MovementRequestId>(3001)
                  && registry.Find(2001)->activeAction == std::optional<ActionId>(4001),
              "movement and action references are explicit inspectable handles");
        Check(registry.SetMovementRequest(2001, MovementRequestId{0}).failure
                  == ResidentFailure::InvalidMovementRequestId,
              "zero movement reference is rejected");
        Check(registry.SetActiveAction(2001, ActionId{0}).failure
                  == ResidentFailure::InvalidActionId,
              "zero action reference is rejected");
        Check(registry.SetMovementRequest(2001, std::nullopt).IsValid()
                  && registry.SetActiveAction(2001, std::nullopt).IsValid(),
              "movement and action references clear explicitly");
        Check(registry.SetTile(9999, {1, 1, 0}).failure == ResidentFailure::UnknownResident,
              "unknown resident mutations fail explicitly");
    }

    void TestDeletionLifecycleReportsCleanup()
    {
        LotGrid lot(5, 5);
        ResidentRegistry registry(lot);
        ResidentState resident = MakeResident();
        resident.movementRequest = 3007;
        resident.activeAction = 4009;
        Check(registry.Add(resident).IsValid(), "resident with valid live handles is accepted");

        const ResidentRemovalResult removed = registry.Remove(2001);
        Check(removed.IsValid() && removed.residentId == 2001 && removed.householdId == 1,
              "deletion returns stable identity and household for cleanup");
        Check(removed.movementRequest == std::optional<MovementRequestId>(3007)
                  && removed.activeAction == std::optional<ActionId>(4009),
              "deletion exposes movement/action handles before erasing identity");
        Check(registry.Find(2001) == nullptr && registry.Residents().empty(),
              "deleted resident is absent from registry");
        Check(registry.Remove(2001).failure == ResidentFailure::UnknownResident,
              "repeated deletion is an explicit unknown-resident result");
    }
}

int main()
{
    TestIdentityAndLocationValidation();
    TestMovementAndActionReferences();
    TestDeletionLifecycleReportsCleanup();

    if (failures != 0)
    {
        std::cerr << failures << " People resident-model test(s) failed\n";
        return 1;
    }
    std::cout << "All People resident-model tests passed\n";
    return 0;
}
