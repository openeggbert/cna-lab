#pragma once

#include <cstdint>

namespace CnaTamagotchi::Domain {

enum class LifeStage : std::uint8_t {
    Egg,
    Hatchling,
    Child,
    Teen,
    Adult,
    Elder,
    Farewell,
};

enum class PetSpecies : std::uint8_t {
    Puffin,
    Mossling,
    Pebblet,
    Cometling,
};

enum class AttentionReason : std::uint8_t {
    None,
    Hunger,
    Happiness,
    SleepLight,
    Discipline,
};

// All needs use the inclusive [0, 100] range. Domain code owns their
// invariants so the future renderer can project them without clamping.
struct Needs final {
    int hunger{75};
    int happiness{75};
    int energy{75};
    int hygiene{75};
    int health{100};
    int affection{50};
    int discipline{50};
};

struct PetState final {
    PetSpecies species{PetSpecies::Puffin};
    LifeStage lifeStage{LifeStage::Egg};
    Needs needs{};
    int weight{10};
    int careMistakes{0};
    int ageMinutes{0};
    int clockMinutesOfDay{9 * 60};
    int wasteCount{0};
    int attentionDeadlineMinutes{-1};
    int nextAttentionEligibleMinutes{0};
    bool asleep{false};
    bool lightOff{false};
    bool sick{false};
    AttentionReason attentionReason{AttentionReason::None};
};

} // namespace CnaTamagotchi::Domain
