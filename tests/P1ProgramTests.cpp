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

} // namespace

int main()
{
    testP1ProgrammeDefinesItsOwnContent();
    testP1FoodAndRosterExcludeP2Content();

    if (failures == 0) {
        std::cout << "P1ProgramTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
