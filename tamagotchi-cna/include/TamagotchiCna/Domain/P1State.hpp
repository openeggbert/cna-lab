#pragma once

#include "TamagotchiCna/Domain/ProgramDefinition.hpp"

namespace TamagotchiCna::Domain {

// The fixed roster of the English-language international Tamagotchi P1
// programme. Egg and Angel are display states, not raisable characters.
enum class P1Character : std::uint8_t {
    Egg,
    Babytchi,
    Marutchi,
    Tamatchi,
    Kuchitamatchi,
    Mametchi,
    Ginjirotchi,
    Maskutchi,
    Kuchipatchi,
    Nyorotchi,
    Tarakotchi,
    Bill,
    Angel,
};

using P1Stage = ProgramStage;

// This replaces the prototype's broad 0..100 needs when the active
// simulation is migrated. The extra runtime fields live in the simulation
// state; this value object deliberately preserves only P1-visible meters.
struct P1VisibleState final {
    P1Character character{P1Character::Egg};
    P1Stage stage{P1Stage::Egg};
    int hungerHearts{4};
    int happinessHearts{4};
    int disciplineBars{0};
    int age{0};
    int weight{0};
};

[[nodiscard]] constexpr P1Stage stageFor(const P1Character character) noexcept
{
    switch (character) {
    case P1Character::Egg: return P1Stage::Egg;
    case P1Character::Babytchi: return P1Stage::Baby;
    case P1Character::Marutchi: return P1Stage::Child;
    case P1Character::Tamatchi:
    case P1Character::Kuchitamatchi:
        return P1Stage::Teen;
    case P1Character::Mametchi:
    case P1Character::Ginjirotchi:
    case P1Character::Maskutchi:
    case P1Character::Kuchipatchi:
    case P1Character::Nyorotchi:
    case P1Character::Tarakotchi:
    case P1Character::Bill:
        return P1Stage::Adult;
    case P1Character::Angel: return P1Stage::End;
    }

    return P1Stage::Egg;
}

[[nodiscard]] constexpr bool isP1RegularAdult(const P1Character character) noexcept
{
    return character >= P1Character::Mametchi && character <= P1Character::Tarakotchi;
}

[[nodiscard]] constexpr bool isValidP1VisibleState(const P1VisibleState& state) noexcept
{
    return state.stage == stageFor(state.character) && state.hungerHearts >= 0
        && state.hungerHearts <= 4 && state.happinessHearts >= 0
        && state.happinessHearts <= 4 && state.disciplineBars >= 0
        && state.disciplineBars <= 4 && state.age >= 0 && state.weight >= 0;
}

} // namespace TamagotchiCna::Domain
