#include "CnaTamagotchi/Domain/ProgramSimulation.hpp"

#include <algorithm>

namespace CnaTamagotchi::Domain {

ProgramAdvanceReport ProgramSimulation::advance(const ProgramDefinition& programme,
                                                ProgramPetState& state,
                                                const int elapsedMinutes) const noexcept
{
    const int requested = std::max(elapsedMinutes, 0);
    if (requested == 0) {
        return ProgramAdvanceReport{.requestedMinutes = requested};
    }

    ProgramAdvanceReport report{.requestedMinutes = requested, .appliedMinutes = requested};
    const int previousClockMinute = state.minutesSinceClockSet;
    state.minutesSinceClockSet += requested;

    if (state.stage == ProgramStage::Egg
        && previousClockMinute < programme.lifecycle.hatchDelayMinutes
        && state.minutesSinceClockSet >= programme.lifecycle.hatchDelayMinutes) {
        hatch(programme, state);
        report.hatched = true;
    }

    if (state.stage != ProgramStage::Baby) {
        return report;
    }

    const int previousLifeMinute = state.minutesSinceHatch;
    const int currentLifeMinute = std::max(
        0, state.minutesSinceClockSet - programme.lifecycle.hatchDelayMinutes);
    state.minutesSinceHatch = currentLifeMinute;
    updateBabyEvents(programme, state, previousLifeMinute, currentLifeMinute);

    if (currentLifeMinute >= programme.lifecycle.babyToChildMinutes) {
        if (const CreatureDefinition* const child = firstCharacterAtStage(programme, ProgramStage::Child)) {
            state.characterId = child->id;
            state.stage = ProgramStage::Child;
            state.weight = child->minimumWeight;
            state.asleep = false;
            report.becameChild = true;
        }
    }
    return report;
}

bool ProgramSimulation::giveMedicine(ProgramPetState& state) const noexcept
{
    if (state.medicineDosesRemaining <= 0) {
        return false;
    }
    --state.medicineDosesRemaining;
    state.sick = state.medicineDosesRemaining > 0;
    return true;
}

const CreatureDefinition* ProgramSimulation::firstCharacterAtStage(
    const ProgramDefinition& programme, const ProgramStage stage) noexcept
{
    const auto found = std::find_if(programme.creatures.begin(), programme.creatures.end(),
        [stage](const CreatureDefinition& character) { return character.stage == stage; });
    return found == programme.creatures.end() ? nullptr : &*found;
}

void ProgramSimulation::hatch(const ProgramDefinition& programme, ProgramPetState& state) noexcept
{
    const CreatureDefinition* const baby = firstCharacterAtStage(programme, ProgramStage::Baby);
    if (baby == nullptr) {
        return;
    }
    state.characterId = baby->id;
    state.stage = ProgramStage::Baby;
    state.minutesSinceHatch = 0;
    state.age = 0;
    state.weight = baby->minimumWeight;
    state.hungerHearts = 0;
    state.happinessHearts = 0;
    state.disciplineBars = 0;
    state.medicineDosesRemaining = 0;
    state.asleep = false;
    state.sick = false;
}

void ProgramSimulation::updateBabyEvents(const ProgramDefinition& programme,
                                         ProgramPetState& state,
                                         const int previousLifeMinute,
                                         const int currentLifeMinute) noexcept
{
    const LifecycleDefinition& lifecycle = programme.lifecycle;
    if (!state.sick && previousLifeMinute < lifecycle.babyIllnessMinute
        && currentLifeMinute >= lifecycle.babyIllnessMinute) {
        state.sick = true;
        state.medicineDosesRemaining = lifecycle.babyIllnessMedicineDoses;
    }

    const int napEnd = lifecycle.babyNapStartMinute + lifecycle.babyNapDurationMinutes;
    state.asleep = currentLifeMinute >= lifecycle.babyNapStartMinute
        && currentLifeMinute < napEnd;
    if (previousLifeMinute < napEnd && currentLifeMinute >= napEnd) {
        ++state.age;
    }
}

} // namespace CnaTamagotchi::Domain
