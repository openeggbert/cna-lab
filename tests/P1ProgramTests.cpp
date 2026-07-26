#include "CnaTamagotchi/Domain/P1Program.hpp"

#include <algorithm>
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

void testP1ProgrammeDefinesItsOwnContent()
{
    const ProgramDefinition& p1 = Programs::internationalP1();
    expect(p1.id == "international-p1-1997", "programme id must lock the selected variant");
    expect(p1.display.checkerboardBackground, "P1 must select its checkerboard LCD field");
    expect(p1.display.logicalWidth == 32 && p1.display.logicalHeight == 16,
        "programme display data must retain the target LCD dimensions");
    expect(p1.game.kind == ProgramGameKind::Character, "P1 must select Character, not Number");
    expect(p1.game.rounds == 5 && p1.game.winsNeededForHappiness == 3,
        "P1 Character game must define its five-round win threshold in data");
    expect(p1.endScreen == ProgramEndScreen::AngelStars,
        "P1 must select the angel-and-stars end display");
}

void testP1FoodAndRosterExcludeP2Content()
{
    const ProgramDefinition& p1 = Programs::internationalP1();
    expect(p1.food.size() == 2U, "P1 must have two food choices");
    expect(p1.food[0].lcdLabel == "BREAD" && p1.food[1].lcdLabel == "CANDY",
        "P1 food labels must come from programme data");
    expect(p1.creatures.size() == 11U, "P1 must define its eleven raisable characters");
    const auto bill = std::find_if(p1.creatures.begin(), p1.creatures.end(),
        [](const CreatureDefinition& creature) { return creature.id == "bill"; });
    expect(bill != p1.creatures.end() && bill->hidden,
        "Bill must be represented as the hidden international P1 character");
    const auto mimitchi = std::find_if(p1.creatures.begin(), p1.creatures.end(),
        [](const CreatureDefinition& creature) { return creature.id == "mimitchi"; });
    expect(mimitchi == p1.creatures.end(), "P2 characters must not leak into P1 data");
}

void testP1LifecycleAndCharacterSchedulesAreProgrammeData()
{
    const ProgramDefinition& p1 = Programs::internationalP1();
    expect(p1.lifecycle.hatchDelayMinutes == 5 && p1.lifecycle.babyToChildMinutes == 65,
        "P1 hatch and baby-to-child timing must be kept in programme data");
    expect(p1.lifecycle.attentionWindowMinutes == 15,
        "the P1 fifteen-minute Attention window must be programme data");
    expect(p1.lifecycle.teenAge == 3 && p1.lifecycle.adultAge == 6,
        "P1 evolution ages must be kept in programme data");
    expect(p1.lifecycle.babyNapStartMinute == 40 && p1.lifecycle.babyNapDurationMinutes == 5,
        "P1 Babytchi nap must be data rather than a simulator special case");
    expect(p1.lifecycle.babyIllnessMinute == 33 && p1.lifecycle.babyIllnessMedicineDoses == 2,
        "P1 Babytchi illness trace must be data rather than a simulator special case");
    expect(p1.lifecycle.babyFirstWasteMinute == 15 && p1.lifecycle.babySecondWasteMinute == 45,
        "P1 Babytchi waste events must be data rather than a simulator special case");

    const auto mametchi = std::find_if(p1.creatures.begin(), p1.creatures.end(),
        [](const CreatureDefinition& creature) { return creature.id == "mametchi"; });
    expect(mametchi != p1.creatures.end() && mametchi->minimumWeight == 30
            && mametchi->sleepStartMinute == 22 * 60 && mametchi->wakeMinute == 9 * 60,
        "P1 character minimum weight and sleep schedule must be programme data");

    const auto marutchi = std::find_if(p1.creatures.begin(), p1.creatures.end(),
        [](const CreatureDefinition& creature) { return creature.id == "marutchi"; });
    expect(marutchi != p1.creatures.end() && marutchi->hungerHeartLossMinutes == 50
            && marutchi->happinessHeartLossMinutes == 60
            && marutchi->disciplineCallAfterNeedDecrements == 6,
        "P1 Marutchi's need-decay and false-call cadence must be programme data");
}

} // namespace

int main()
{
    testP1ProgrammeDefinesItsOwnContent();
    testP1FoodAndRosterExcludeP2Content();
    testP1LifecycleAndCharacterSchedulesAreProgrammeData();

    if (failures == 0) {
        std::cout << "P1ProgramTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
