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
    expect(pet.weight == 12, "meal, snack, and game must update weight deterministically");
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

void testAttentionWindowAndDiscipline()
{
    PetSimulation simulation;
    PetState pet{};
    pet.lifeStage = LifeStage::Hatchling;
    pet.ageMinutes = 14;
    pet.needs.hunger = 25;

    static_cast<void>(simulation.advance(pet, 1));
    expect(pet.attentionReason == AttentionReason::Hunger,
        "empty hunger must create an attention call");
    expect(pet.attentionDeadlineMinutes == 30,
        "attention call must give a fifteen-minute response window");

    static_cast<void>(simulation.advance(pet, 15));
    expect(pet.careMistakes == 1, "ignored hunger call must create a care mistake");
    expect(pet.attentionReason == AttentionReason::None,
        "expired attention call must clear before a later retry");

    pet.attentionReason = AttentionReason::Discipline;
    pet.attentionDeadlineMinutes = pet.ageMinutes + 15;
    const int disciplineBefore = pet.needs.discipline;
    simulation.applyAction(pet, PetAction::Discipline);
    expect(pet.needs.discipline == disciplineBefore + 25,
        "discipline must only reward a false attention call");
    expect(pet.attentionReason == AttentionReason::None,
        "correct discipline must resolve the false call");
}

void testWasteAndSleepLight()
{
    PetSimulation simulation;
    PetState pet{};
    pet.lifeStage = LifeStage::Hatchling;
    pet.ageMinutes = 119;

    static_cast<void>(simulation.advance(pet, 1));
    expect(pet.wasteCount == 1, "creature must create waste on its scheduled interval");
    simulation.applyAction(pet, PetAction::Clean);
    expect(pet.wasteCount == 0, "clean action must remove waste");

    PetState sleepingPet{};
    sleepingPet.lifeStage = LifeStage::Child;
    sleepingPet.clockMinutesOfDay = 20 * 60 - 1;
    static_cast<void>(simulation.advance(sleepingPet, 1));
    expect(sleepingPet.asleep, "child must fall asleep at the scheduled evening time");
    expect(sleepingPet.attentionReason == AttentionReason::SleepLight,
        "sleeping with the light on must request attention");
    simulation.applyAction(sleepingPet, PetAction::ToggleLight);
    expect(sleepingPet.lightOff, "light action must turn the light off while asleep");
    expect(sleepingPet.attentionReason == AttentionReason::None,
        "turning the light off must resolve its attention call");
}

} // namespace

int main()
{
    testActionsRespectBounds();
    testOfflineAdvanceClampsAndEvolves();
    testClassicEggAndChildTiming();
    testSleepRecoversEnergy();
    testAttentionWindowAndDiscipline();
    testWasteAndSleepLight();

    if (failures == 0) {
        std::cout << "PetSimulationTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
