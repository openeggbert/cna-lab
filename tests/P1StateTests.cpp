#include "CnaTamagotchi/Domain/P1State.hpp"

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

void testInternationalP1RosterHasOnlyTheChosenProgramme()
{
    constexpr std::array<P1Character, 6> regularAdults{{
        P1Character::Mametchi,
        P1Character::Ginjirotchi,
        P1Character::Maskutchi,
        P1Character::Kuchipatchi,
        P1Character::Nyorotchi,
        P1Character::Tarakotchi,
    }};

    for (const P1Character character : regularAdults) {
        expect(stageFor(character) == P1Stage::Adult,
            "each international P1 regular adult must resolve to adult stage");
        expect(isP1RegularAdult(character),
            "each international P1 regular adult must be classified as regular");
    }
    expect(stageFor(P1Character::Bill) == P1Stage::Adult,
        "Bill must remain the international P1 hidden adult");
    expect(!isP1RegularAdult(P1Character::Bill),
        "Bill must not be confused with a regular adult outcome");
}

void testGrowthStagesAreUnambiguous()
{
    expect(stageFor(P1Character::Egg) == P1Stage::Egg, "egg stage must be explicit");
    expect(stageFor(P1Character::Babytchi) == P1Stage::Baby, "Babytchi must be baby stage");
    expect(stageFor(P1Character::Marutchi) == P1Stage::Child, "Marutchi must be child stage");
    expect(stageFor(P1Character::Tamatchi) == P1Stage::Teen, "Tamatchi must be teen stage");
    expect(stageFor(P1Character::Kuchitamatchi) == P1Stage::Teen,
        "Kuchitamatchi must be teen stage");
    expect(stageFor(P1Character::Angel) == P1Stage::End, "angel must be an end display state");
}

void testP1VisibleMetersRemainDiscrete()
{
    P1VisibleState state{};
    expect(isValidP1VisibleState(state), "default P1 egg state must be valid");

    state.hungerHearts = 5;
    expect(!isValidP1VisibleState(state), "P1 hunger must never exceed four hearts");
    state.hungerHearts = 4;
    state.disciplineBars = -1;
    expect(!isValidP1VisibleState(state), "P1 discipline must never be negative");
    state.disciplineBars = 0;
    state.stage = P1Stage::Adult;
    expect(!isValidP1VisibleState(state),
        "the displayed stage must always agree with the current P1 character");
}

} // namespace

int main()
{
    testInternationalP1RosterHasOnlyTheChosenProgramme();
    testGrowthStagesAreUnambiguous();
    testP1VisibleMetersRemainDiscrete();

    if (failures == 0) {
        std::cout << "P1StateTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
