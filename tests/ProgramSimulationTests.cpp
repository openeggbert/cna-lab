#include "CnaTamagotchi/Domain/P1Program.hpp"
#include "CnaTamagotchi/Domain/ProgramSimulation.hpp"

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

void testP1EggHatchesFromProgrammeData()
{
    ProgramPetState pet{};
    ProgramSimulation simulation;
    const ProgramDefinition& p1 = Programs::internationalP1();

    const ProgramAdvanceReport beforeHatch = simulation.advance(p1, pet, 4);
    expect(!beforeHatch.hatched && pet.stage == ProgramStage::Egg,
        "P1 egg must remain an egg for the first four clock minutes");

    const ProgramAdvanceReport hatch = simulation.advance(p1, pet, 1);
    expect(hatch.hatched && pet.stage == ProgramStage::Baby && pet.characterId == "babytchi",
        "P1 programme data must hatch Babytchi at five minutes");
    expect(pet.weight == 5 && pet.hungerHearts == 0 && pet.happinessHearts == 0,
        "a newly hatched Babytchi must begin at its captured P1 state");
}

void testP1BabyTraceUsesSharedProgrammeSimulator()
{
    ProgramPetState pet{};
    ProgramSimulation simulation;
    const ProgramDefinition& p1 = Programs::internationalP1();
    static_cast<void>(simulation.advance(p1, pet, 5));

    static_cast<void>(simulation.advance(p1, pet, 33));
    expect(pet.sick && pet.medicineDosesRemaining == 2,
        "Babytchi must become ill at the captured P1 minute and require two doses");
    expect(simulation.giveMedicine(pet) && pet.sick && pet.medicineDosesRemaining == 1,
        "first P1 medicine dose must leave Babytchi sick");
    expect(simulation.giveMedicine(pet) && !pet.sick && pet.medicineDosesRemaining == 0,
        "second P1 medicine dose must cure Babytchi");

    static_cast<void>(simulation.advance(p1, pet, 7));
    expect(pet.minutesSinceHatch == 40 && pet.asleep,
        "Babytchi must start its captured five-minute nap at minute forty");
    static_cast<void>(simulation.advance(p1, pet, 5));
    expect(!pet.asleep && pet.age == 1,
        "Babytchi must wake and gain an age year after its captured nap");

    const ProgramAdvanceReport evolution = simulation.advance(p1, pet, 20);
    expect(evolution.becameChild && pet.stage == ProgramStage::Child && pet.characterId == "marutchi",
        "shared simulator must evolve Babytchi to Marutchi at minute sixty-five");
    expect(pet.weight == 10, "Marutchi must receive its P1 minimum weight from programme data");
}

} // namespace

int main()
{
    testP1EggHatchesFromProgrammeData();
    testP1BabyTraceUsesSharedProgrammeSimulator();

    if (failures == 0) {
        std::cout << "ProgramSimulationTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
