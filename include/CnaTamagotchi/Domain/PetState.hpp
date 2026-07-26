#pragma once

#include <cstdint>

namespace CnaTamagotchi::Domain {

// Intentionally small placeholder for the forthcoming deterministic domain.
// The next milestone will add needs, events, actions, and evolution rules.
enum class LifeStage : std::uint8_t {
    Egg,
    Hatchling,
    Child,
    Teen,
    Adult,
    Elder,
    Farewell,
};

struct PetState {
    LifeStage lifeStage{LifeStage::Egg};
};

} // namespace CnaTamagotchi::Domain
