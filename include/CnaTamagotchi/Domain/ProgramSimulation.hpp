#pragma once

#include "CnaTamagotchi/Domain/ProgramDefinition.hpp"

#include <string_view>

namespace CnaTamagotchi::Domain {

// Runtime state intentionally contains no P1/P2 branch. Character ids point
// at a selected ProgramDefinition while running; persistence will store the
// same ids as owned strings during the save-format migration.
struct ProgramPetState final {
    std::string_view characterId{"egg"};
    ProgramStage stage{ProgramStage::Egg};
    int minutesSinceClockSet{0};
    int minutesSinceHatch{0};
    int age{0};
    int weight{0};
    int hungerHearts{4};
    int happinessHearts{4};
    int disciplineBars{0};
    int medicineDosesRemaining{0};
    bool asleep{false};
    bool sick{false};
};

struct ProgramAdvanceReport final {
    int requestedMinutes{0};
    int appliedMinutes{0};
    bool hatched{false};
    bool becameChild{false};
};

// Shared programme-driven lifecycle engine. Its first implemented slice is
// deliberately the fully captured egg/Baby lifecycle; subsequent P1 stages
// and any P2 programme use the same data boundary.
class ProgramSimulation final {
public:
    [[nodiscard]] ProgramAdvanceReport advance(const ProgramDefinition& programme,
                                               ProgramPetState& state,
                                               int elapsedMinutes) const noexcept;
    [[nodiscard]] bool giveMedicine(ProgramPetState& state) const noexcept;

private:
    [[nodiscard]] static const CreatureDefinition* firstCharacterAtStage(
        const ProgramDefinition& programme, ProgramStage stage) noexcept;
    static void hatch(const ProgramDefinition& programme, ProgramPetState& state) noexcept;
    static void updateBabyEvents(const ProgramDefinition& programme,
                                 ProgramPetState& state,
                                 int previousLifeMinute,
                                 int currentLifeMinute) noexcept;
};

} // namespace CnaTamagotchi::Domain
