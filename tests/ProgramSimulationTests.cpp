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

void testP1FoodGameWasteAndSleepUseProgrammeData()
{
    ProgramPetState pet{};
    ProgramSimulation simulation;
    const ProgramDefinition& p1 = Programs::internationalP1();
    static_cast<void>(simulation.advance(p1, pet, 5));

    expect(simulation.feed(p1, pet, 0) && pet.hungerHearts == 1 && pet.weight == 6,
        "P1 Bread must add one hunger heart and one ounce from programme data");
    expect(simulation.feed(p1, pet, 1) && pet.happinessHearts == 1 && pet.weight == 8,
        "P1 Candy must add one happiness heart and two ounces from programme data");
    expect(simulation.completeGame(p1, pet, 3) && pet.happinessHearts == 2 && pet.weight == 7,
        "a three-win P1 Character game must add happiness and remove one ounce");

    static_cast<void>(simulation.advance(p1, pet, 15));
    expect(pet.wasteCount == 1, "Babytchi's captured first waste event must be programme data");
    expect(simulation.cleanWaste(pet) && pet.wasteCount == 0,
        "the P1 Toilet action must remove recorded waste without a prototype hygiene meter");

    static_cast<void>(simulation.advance(p1, pet, 50));
    expect(pet.stage == ProgramStage::Child && pet.characterId == "marutchi",
        "the trace must reach Marutchi before its P1 sleep schedule is evaluated");
    pet.clockMinutesOfDay = 19 * 60 + 59;
    static_cast<void>(simulation.advance(p1, pet, 1));
    expect(pet.asleep, "Marutchi must follow its captured 20:00 P1 sleep schedule");
    expect(simulation.toggleLight(pet) && pet.lightOff,
        "the P1 Light action must apply only while the character is asleep");
}

ProgramPetState p1TeenWith(const int careMistakes, const int disciplineMistakes,
                           ProgramSimulation& simulation, const ProgramDefinition& p1)
{
    ProgramPetState pet{};
    static_cast<void>(simulation.advance(p1, pet, 5));
    pet.careMistakes = careMistakes;
    pet.disciplineMistakes = disciplineMistakes;
    const ProgramAdvanceReport evolution = simulation.advance(
        p1, pet, p1.lifecycle.babyToChildMinutes + p1.lifecycle.childToTeenMinutes);
    expect(evolution.becameChild && evolution.becameTeen && pet.stage == ProgramStage::Teen,
        "P1's captured child duration must produce a data-selected teen");
    expect(pet.age == p1.lifecycle.teenAge,
        "the P1 teen transition must retain the captured age-three display value");
    return pet;
}

void testP1EvolutionRulesRemainProgrammeData()
{
    ProgramSimulation simulation;
    const ProgramDefinition& p1 = Programs::internationalP1();
    expect(p1.evolutionRules.size() == 17,
        "the P1 programme must expose its teen and adult chart as data rules");

    ProgramPetState mametchi = p1TeenWith(0, 0, simulation, p1);
    expect(mametchi.characterId == "tamatchi" && mametchi.teenLineage == ProgramTeenLineage::TypeA,
        "low-care P1 child state must select type-A Tamatchi");
    const ProgramAdvanceReport mametchiEvolution = simulation.advance(
        p1, mametchi, p1.lifecycle.teenToAdultMinutes);
    expect(mametchiEvolution.becameAdult && mametchi.characterId == "mametchi"
            && mametchi.age == p1.lifecycle.adultAge,
        "type-A Tamatchi with zero discipline mistakes must become Mametchi at age six");

    ProgramPetState ginjirotchi = p1TeenWith(0, 1, simulation, p1);
    static_cast<void>(simulation.advance(p1, ginjirotchi, p1.lifecycle.teenToAdultMinutes));
    expect(ginjirotchi.characterId == "ginjirotchi",
        "type-A Tamatchi with one discipline mistake must become Ginjirotchi");

    ProgramPetState maskutchi = p1TeenWith(0, 2, simulation, p1);
    static_cast<void>(simulation.advance(p1, maskutchi, p1.lifecycle.teenToAdultMinutes));
    expect(maskutchi.characterId == "maskutchi",
        "type-A Tamatchi with two discipline mistakes must become Maskutchi");

    ProgramPetState kuchipatchi = p1TeenWith(3, 0, simulation, p1);
    expect(kuchipatchi.characterId == "kuchitamatchi",
        "three care mistakes must select Kuchitamatchi from Marutchi");
    static_cast<void>(simulation.advance(p1, kuchipatchi, p1.lifecycle.teenToAdultMinutes));
    expect(kuchipatchi.characterId == "kuchipatchi",
        "type-A Kuchitamatchi with low discipline mistakes must become Kuchipatchi");

    ProgramPetState typeB = p1TeenWith(0, 3, simulation, p1);
    expect(typeB.characterId == "tamatchi" && typeB.teenLineage == ProgramTeenLineage::TypeB,
        "three discipline mistakes must retain Tamatchi but record the type-B lineage");
    static_cast<void>(simulation.advance(p1, typeB, p1.lifecycle.teenToAdultMinutes));
    expect(typeB.characterId == "maskutchi",
        "the type-B Tamatchi rule must select Maskutchi from programme data");
}

} // namespace

int main()
{
    testP1EggHatchesFromProgrammeData();
    testP1BabyTraceUsesSharedProgrammeSimulator();
    testP1FoodGameWasteAndSleepUseProgrammeData();
    testP1EvolutionRulesRemainProgrammeData();

    if (failures == 0) {
        std::cout << "ProgramSimulationTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
