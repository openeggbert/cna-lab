#include "CnaTamagotchi/Domain/PetSimulation.hpp"

#include <iostream>

using namespace CnaTamagotchi::Domain;

namespace {

int failures = 0;

void expect(const bool condition, const char* const message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void testActionsRespectBounds()
{
    PetSimulation simulation;
    PetState pet{};
    pet.needs.hunger = 98;
    pet.needs.happiness = 95;
    pet.needs.energy = 5;

    simulation.applyAction(pet, PetAction::Meal);
    simulation.applyAction(pet, PetAction::Snack);
    simulation.applyAction(pet, PetAction::Play);

    expect(pet.needs.hunger <= 100, "feeding must not exceed maximum hunger");
    expect(pet.needs.happiness <= 100, "actions must not exceed maximum happiness");
    expect(pet.needs.energy >= 0, "play must not make energy negative");
    expect(pet.weight == 13, "meal and snack must update weight deterministically");
}

void testOfflineAdvanceClampsAndEvolves()
{
    PetSimulation simulation;
    PetState pet{};

    const SimulationReport report = simulation.advance(
        pet, PetSimulation::MaximumOfflineMinutes + 1);

    expect(report.wasClamped, "offline advance beyond the limit must be clamped");
    expect(report.appliedMinutes == PetSimulation::MaximumOfflineMinutes,
        "clamped advance must use the documented maximum");
    expect(pet.ageMinutes == PetSimulation::MaximumOfflineMinutes,
        "age must advance by the applied duration");
    expect(pet.lifeStage == LifeStage::Child,
        "a twelve-hour-old pet must reach the child stage");
}

void testClassicEggAndChildTiming()
{
    PetSimulation simulation;
    PetState pet{};

    static_cast<void>(simulation.advance(pet, 4));
    expect(pet.lifeStage == LifeStage::Egg, "egg must remain visible for the first four minutes");

    static_cast<void>(simulation.advance(pet, 1));
    expect(pet.lifeStage == LifeStage::Hatchling, "egg must hatch after five minutes");

    static_cast<void>(simulation.advance(pet, 60));
    expect(pet.lifeStage == LifeStage::Child, "baby must become a child at 65 minutes");
}

void testSleepRecoversEnergy()
{
    PetSimulation simulation;
    PetState awake{};
    PetState asleep{};
    awake.needs.energy = 50;
    asleep.needs.energy = 50;
    asleep.asleep = true;

    static_cast<void>(simulation.advance(awake, 60));
    static_cast<void>(simulation.advance(asleep, 60));

    expect(asleep.needs.energy > awake.needs.energy,
        "sleep must recover more energy than being awake");
}

} // namespace

int main()
{
    testActionsRespectBounds();
    testOfflineAdvanceClampsAndEvolves();
    testClassicEggAndChildTiming();
    testSleepRecoversEnergy();

    if (failures == 0) {
        std::cout << "PetSimulationTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
