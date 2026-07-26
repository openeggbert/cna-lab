#include "CnaTamagotchi/Domain/P1Program.hpp"
#include "CnaTamagotchi/Domain/ProgramSimulation.hpp"

#include <array>
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

    expect(simulation.feed(p1, pet, 0) && pet.hungerHearts == 1 && pet.weight == 5,
        "P1 Bread must add one hunger heart while Babytchi keeps its five-ounce weight");
    expect(simulation.feed(p1, pet, 1) && pet.happinessHearts == 1 && pet.weight == 5,
        "P1 Candy must add happiness while Babytchi keeps its five-ounce weight");
    expect(simulation.completeGame(p1, pet, 3) && pet.happinessHearts == 2 && pet.weight == 5,
        "a three-win P1 Character game must not change Babytchi's fixed baby weight");

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

ProgramPetState p1TeenWith(const int careMistakes, const int disciplineBars,
                           ProgramSimulation& simulation, const ProgramDefinition& p1)
{
    ProgramPetState pet{};
    static_cast<void>(simulation.advance(p1, pet, 5));
    pet.careMistakes = careMistakes;
    pet.disciplineBars = disciplineBars;
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

    ProgramPetState typeA = p1TeenWith(0, 3, simulation, p1);
    expect(typeA.characterId == "tamatchi" && typeA.teenLineage == ProgramTeenLineage::TypeA,
        "one-or-fewer-care-mistake P1 child state with 75%-100% discipline must select type-A Tamatchi");
    ProgramPetState typeB = p1TeenWith(0, 2, simulation, p1);
    expect(typeB.characterId == "tamatchi" && typeB.teenLineage == ProgramTeenLineage::TypeB,
        "0%-50% child discipline must retain Tamatchi but record the type-B lineage");

    ProgramPetState kuchitamatchi = p1TeenWith(2, 3, simulation, p1);
    expect(kuchitamatchi.characterId == "kuchitamatchi"
            && kuchitamatchi.teenLineage == ProgramTeenLineage::TypeA,
        "two care mistakes must select type-A Kuchitamatchi from Marutchi");

    struct AdultCase final {
        int childCareMistakes;
        int childDisciplineBars;
        int adultCareMistakes;
        int adultDisciplineBars;
        const char* expectedCharacterId;
    };
    constexpr std::array<AdultCase, 15> adultCases{{
        {0, 3, 0, 4, "mametchi"},
        {0, 3, 0, 3, "ginjirotchi"},
        {0, 3, 0, 0, "maskutchi"},
        {0, 3, 3, 4, "kuchipatchi"},
        {0, 3, 3, 3, "nyorotchi"},
        {0, 3, 3, 0, "tarakotchi"},
        {0, 2, 0, 4, "ginjirotchi"},
        {0, 2, 0, 0, "maskutchi"},
        {0, 2, 3, 4, "nyorotchi"},
        {0, 2, 3, 0, "tarakotchi"},
        {2, 3, 0, 4, "kuchipatchi"},
        {2, 3, 0, 3, "nyorotchi"},
        {2, 3, 0, 0, "tarakotchi"},
        {2, 2, 0, 4, "nyorotchi"},
        {2, 2, 0, 0, "tarakotchi"},
    }};

    for (const AdultCase& testCase : adultCases) {
        ProgramPetState pet = p1TeenWith(testCase.childCareMistakes,
            testCase.childDisciplineBars, simulation, p1);
        // The meter can change during the teen stage, while the version set at
        // its evolution remains hidden state. This is why both values belong
        // in a resolver trace.
        pet.careMistakes = testCase.adultCareMistakes;
        pet.disciplineBars = testCase.adultDisciplineBars;
        const ProgramAdvanceReport evolution = simulation.advance(
            p1, pet, p1.lifecycle.teenToAdultMinutes);
        expect(evolution.becameAdult && pet.characterId == testCase.expectedCharacterId
                && pet.age == p1.lifecycle.adultAge,
            "each classic P1 adult rule must select its documented data target at age six");
    }
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
