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
    state.clockMinutesOfDay = (state.clockMinutesOfDay + requested) % (24 * 60);

    if (state.stage == ProgramStage::Egg
        && previousClockMinute < programme.lifecycle.hatchDelayMinutes
        && state.minutesSinceClockSet >= programme.lifecycle.hatchDelayMinutes) {
        hatch(programme, state);
        report.hatched = true;
    }

    if (state.stage != ProgramStage::Baby) {
        updateSleepSchedule(programme, state);
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
            state.lightOff = false;
            report.becameChild = true;
        }
    }
    updateSleepSchedule(programme, state);
    return report;
}

bool ProgramSimulation::feed(const ProgramDefinition& programme, ProgramPetState& state,
                             const int foodIndex) const noexcept
{
    if (state.stage == ProgramStage::Egg || state.stage == ProgramStage::End
        || foodIndex < 0 || foodIndex >= static_cast<int>(programme.food.size())) {
        return false;
    }

    const FoodDefinition& food = programme.food[static_cast<std::size_t>(foodIndex)];
    const bool changesHunger = food.hungerHeartDelta > 0 && state.hungerHearts < 4;
    const bool changesHappiness = food.happinessHeartDelta > 0 && state.happinessHearts < 4;
    if (!changesHunger && !changesHappiness) {
        return false;
    }

    state.hungerHearts = std::clamp(state.hungerHearts + food.hungerHeartDelta, 0, 4);
    state.happinessHearts = std::clamp(state.happinessHearts + food.happinessHeartDelta, 0, 4);
    state.weight = std::max(0, state.weight + food.weightDelta);
    if ((food.hungerHeartDelta > 0 && state.attentionReason == ProgramAttentionReason::Hunger)
        || (food.happinessHeartDelta > 0
            && state.attentionReason == ProgramAttentionReason::Happiness)) {
        state.attentionReason = ProgramAttentionReason::None;
        state.attentionDeadlineMinutes = -1;
    }
    return true;
}

bool ProgramSimulation::completeGame(const ProgramDefinition& programme, ProgramPetState& state,
                                     const int wins) const noexcept
{
    if (state.stage == ProgramStage::Egg || state.stage == ProgramStage::End || state.asleep) {
        return false;
    }

    state.weight = std::max(0, state.weight + programme.game.weightDeltaOnCompletion);
    if (wins >= programme.game.winsNeededForHappiness) {
        state.happinessHearts = std::clamp(
            state.happinessHearts + programme.game.happinessHeartDeltaOnWin, 0, 4);
        if (state.attentionReason == ProgramAttentionReason::Happiness) {
            state.attentionReason = ProgramAttentionReason::None;
            state.attentionDeadlineMinutes = -1;
        }
    }
    return true;
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

bool ProgramSimulation::toggleLight(ProgramPetState& state) const noexcept
{
    if (!state.asleep) {
        return false;
    }
    state.lightOff = !state.lightOff;
    if (state.lightOff && state.attentionReason == ProgramAttentionReason::SleepLight) {
        state.attentionReason = ProgramAttentionReason::None;
        state.attentionDeadlineMinutes = -1;
    }
    return true;
}

bool ProgramSimulation::cleanWaste(ProgramPetState& state) const noexcept
{
    if (state.wasteCount == 0) {
        return false;
    }
    state.wasteCount = 0;
    return true;
}

bool ProgramSimulation::discipline(ProgramPetState& state) const noexcept
{
    if (state.attentionReason != ProgramAttentionReason::Discipline) {
        return false;
    }
    state.disciplineBars = std::min(state.disciplineBars + 1, 4);
    state.attentionReason = ProgramAttentionReason::None;
    state.attentionDeadlineMinutes = -1;
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
    state.wasteCount = 0;
    state.careMistakes = 0;
    state.attentionDeadlineMinutes = -1;
    state.nextAttentionEligibleMinutes = 0;
    state.asleep = false;
    state.lightOff = false;
    state.sick = false;
    state.attentionReason = ProgramAttentionReason::None;
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
    const bool passedFirstWaste = previousLifeMinute < lifecycle.babyFirstWasteMinute
        && currentLifeMinute >= lifecycle.babyFirstWasteMinute;
    const bool passedSecondWaste = previousLifeMinute < lifecycle.babySecondWasteMinute
        && currentLifeMinute >= lifecycle.babySecondWasteMinute;
    state.wasteCount += static_cast<int>(passedFirstWaste) + static_cast<int>(passedSecondWaste);
    state.asleep = currentLifeMinute >= lifecycle.babyNapStartMinute
        && currentLifeMinute < napEnd;
    if (previousLifeMinute < napEnd && currentLifeMinute >= napEnd) {
        ++state.age;
        state.lightOff = false;
    }
}

void ProgramSimulation::updateSleepSchedule(const ProgramDefinition& programme,
                                            ProgramPetState& state) noexcept
{
    if (state.stage == ProgramStage::Egg || state.stage == ProgramStage::Baby
        || state.stage == ProgramStage::End) {
        return;
    }

    const auto character = std::find_if(programme.creatures.begin(), programme.creatures.end(),
        [&state](const CreatureDefinition& definition) { return definition.id == state.characterId; });
    if (character == programme.creatures.end() || character->sleepStartMinute < 0
        || character->wakeMinute < 0) {
        return;
    }

    const bool crossesMidnight = character->sleepStartMinute > character->wakeMinute;
    const bool shouldSleep = crossesMidnight
        ? state.clockMinutesOfDay >= character->sleepStartMinute
            || state.clockMinutesOfDay < character->wakeMinute
        : state.clockMinutesOfDay >= character->sleepStartMinute
            && state.clockMinutesOfDay < character->wakeMinute;
    if (state.asleep && !shouldSleep) {
        state.lightOff = false;
    }
    state.asleep = shouldSleep;
}

} // namespace CnaTamagotchi::Domain
