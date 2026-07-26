#pragma once

#include "CnaTamagotchi/Domain/PetState.hpp"

namespace CnaTamagotchi::Domain {

enum class PetAction {
    Meal,
    Snack,
    Clean,
    Medicine,
    Play,
    ToggleSleep,
    ToggleLight,
    Discipline,
};

struct SimulationReport final {
    int requestedMinutes{0};
    int appliedMinutes{0};
    bool wasClamped{false};
};

// Deterministic, framework-free pet rules. Persistence will supply the
// elapsed minutes calculated from UTC timestamps; this class deliberately
// neither reads a clock nor accesses files.
class PetSimulation final {
public:
    static constexpr int MaximumOfflineMinutes = 12 * 60;

    [[nodiscard]] SimulationReport advance(PetState& state, int elapsedMinutes) const noexcept;
    void applyAction(PetState& state, PetAction action) const noexcept;

private:
    static void advanceOneMinute(PetState& state) noexcept;
    static void updateLifeStage(PetState& state) noexcept;
    static void updateSleepSchedule(PetState& state) noexcept;
    static void updateAttention(PetState& state) noexcept;
    static void clearAttention(PetState& state) noexcept;
    static void enforceInvariants(PetState& state) noexcept;
};

} // namespace CnaTamagotchi::Domain
