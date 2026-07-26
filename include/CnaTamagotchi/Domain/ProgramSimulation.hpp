#pragma once

#include "CnaTamagotchi/Domain/ProgramDefinition.hpp"

#include <string>

namespace CnaTamagotchi::Domain {

enum class ProgramAttentionReason : std::uint8_t {
    None,
    Hunger,
    Happiness,
    SleepLight,
    Discipline,
};

// Runtime state intentionally contains no P1/P2 branch. Character ids select
// an entry in the active ProgramDefinition and are owned so they cross the
// save/load boundary without retaining a pointer into a temporary document.
struct ProgramPetState final {
    std::string characterId{"egg"};
    ProgramStage stage{ProgramStage::Egg};
    int minutesSinceClockSet{0};
    int minutesSinceHatch{0};
    int age{0};
    int weight{0};
    int hungerHearts{4};
    int happinessHearts{4};
    int disciplineBars{0};
    int medicineDosesRemaining{0};
    int clockMinutesOfDay{9 * 60};
    int wasteCount{0};
    int careMistakes{0};
    int disciplineMistakes{0};
    ProgramTeenLineage teenLineage{ProgramTeenLineage::None};
    bool teenStartedWithNoDiscipline{false};
    int attentionDeadlineMinutes{-1};
    int nextAttentionEligibleMinutes{0};
    bool asleep{false};
    bool lightOff{false};
    bool sick{false};
    ProgramAttentionReason attentionReason{ProgramAttentionReason::None};
};

struct ProgramAdvanceReport final {
    int requestedMinutes{0};
    int appliedMinutes{0};
    bool hatched{false};
    bool becameChild{false};
    bool becameTeen{false};
    bool becameAdult{false};
};

// Shared programme-driven lifecycle engine. Its first implemented slice is
// deliberately the fully captured egg/Baby lifecycle; subsequent P1 stages
// and any P2 programme use the same data boundary.
class ProgramSimulation final {
public:
    [[nodiscard]] ProgramAdvanceReport advance(const ProgramDefinition& programme,
                                               ProgramPetState& state,
                                               int elapsedMinutes) const noexcept;
    [[nodiscard]] bool feed(const ProgramDefinition& programme, ProgramPetState& state,
                            int foodIndex) const noexcept;
    [[nodiscard]] bool completeGame(const ProgramDefinition& programme, ProgramPetState& state,
                                    int wins) const noexcept;
    [[nodiscard]] bool giveMedicine(ProgramPetState& state) const noexcept;
    [[nodiscard]] bool toggleLight(ProgramPetState& state) const noexcept;
    [[nodiscard]] bool cleanWaste(ProgramPetState& state) const noexcept;
    [[nodiscard]] bool discipline(ProgramPetState& state) const noexcept;

private:
    [[nodiscard]] static const CreatureDefinition* firstCharacterAtStage(
        const ProgramDefinition& programme, ProgramStage stage) noexcept;
    [[nodiscard]] static const CreatureDefinition* characterById(
        const ProgramDefinition& programme, std::string_view id) noexcept;
    [[nodiscard]] static const EvolutionRule* matchingEvolutionRule(
        const ProgramDefinition& programme, const ProgramPetState& state) noexcept;
    static bool evolve(const ProgramDefinition& programme, ProgramPetState& state) noexcept;
    static void hatch(const ProgramDefinition& programme, ProgramPetState& state) noexcept;
    static void updateBabyEvents(const ProgramDefinition& programme,
                                 ProgramPetState& state,
                                 int previousLifeMinute,
                                 int currentLifeMinute) noexcept;
    static void updateSleepSchedule(const ProgramDefinition& programme,
                                    ProgramPetState& state) noexcept;
};

} // namespace CnaTamagotchi::Domain
