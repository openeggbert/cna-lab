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
    // Simulate at minute boundaries. This is deliberately CPU-only: it keeps
    // the historical timers correct across sleep/wake edges without causing
    // the application to write a save for each boundary.
    for (int minute = 0; minute < requested; ++minute) {
        const int previousClockMinute = state.minutesSinceClockSet;
        const int previousLifeMinute = state.minutesSinceHatch;
        const bool elapsedWhileAwake = !state.asleep;
        ++state.minutesSinceClockSet;
        state.clockMinutesOfDay = (state.clockMinutesOfDay + 1) % (24 * 60);

        if (state.stage == ProgramStage::Egg
            && previousClockMinute < programme.lifecycle.hatchDelayMinutes
            && state.minutesSinceClockSet >= programme.lifecycle.hatchDelayMinutes) {
            hatch(programme, state);
            state.attentionReason = ProgramAttentionReason::Hunger;
            state.attentionDeadlineMinutes = programme.lifecycle.hatchDelayMinutes
                + programme.lifecycle.attentionWindowMinutes;
            report.hatched = true;
            continue;
        }

        if (state.stage == ProgramStage::Egg || state.stage == ProgramStage::End) {
            updateSleepSchedule(programme, state);
            continue;
        }

        const int currentLifeMinute = std::max(
            0, state.minutesSinceClockSet - programme.lifecycle.hatchDelayMinutes);
        state.minutesSinceHatch = currentLifeMinute;
        advanceNeedTimers(programme, state, elapsedWhileAwake);

        if (state.stage == ProgramStage::Baby) {
            updateBabyEvents(programme, state, previousLifeMinute, currentLifeMinute);

            if (currentLifeMinute >= programme.lifecycle.babyToChildMinutes) {
                if (const CreatureDefinition* const child = firstCharacterAtStage(
                        programme, ProgramStage::Child)) {
                    state.characterId = child->id;
                    state.stage = ProgramStage::Child;
                    state.weight = child->minimumWeight;
                    state.asleep = false;
                    state.lightOff = false;
                    // Baby care is a practice period in P1. It can produce
                    // calls, but no baby-stage care history affects Marutchi.
                    state.careMistakes = 0;
                    state.attentionReason = ProgramAttentionReason::None;
                    state.attentionDeadlineMinutes = -1;
                    state.nextAttentionEligibleMinutes = state.minutesSinceClockSet;
                    initialiseStageRuntime(state);
                    report.becameChild = true;
                }
            }
        }

        const int teenEvolutionMinute = programme.lifecycle.babyToChildMinutes
            + programme.lifecycle.childToTeenMinutes;
        if (state.stage == ProgramStage::Child && currentLifeMinute >= teenEvolutionMinute) {
            state.teenLineage = state.disciplineBars >= 3
                ? ProgramTeenLineage::TypeA : ProgramTeenLineage::TypeB;
            state.teenStartedWithNoDiscipline = state.disciplineBars == 0;
            report.becameTeen = evolve(programme, state);
        }

        const int adultEvolutionMinute = teenEvolutionMinute + programme.lifecycle.teenToAdultMinutes;
        if (state.stage == ProgramStage::Teen && currentLifeMinute >= adultEvolutionMinute) {
            report.becameAdult = evolve(programme, state);
        }

        expireAttention(programme, state);
        updateSleepSchedule(programme, state);
        if (state.stage == ProgramStage::Adult) {
            report.becameHiddenAdult = evolve(programme, state);
        }
        beginAttentionIfNeeded(programme, state);
    }
    return report;
}

bool ProgramSimulation::feed(const ProgramDefinition& programme, ProgramPetState& state,
                             const int foodIndex) const noexcept
{
    if (state.stage == ProgramStage::Egg || state.stage == ProgramStage::End
        || state.attentionReason == ProgramAttentionReason::Discipline
        || foodIndex < 0 || foodIndex >= static_cast<int>(programme.food.size())) {
        return false;
    }

    const FoodDefinition& food = programme.food[static_cast<std::size_t>(foodIndex)];
    const bool changesHunger = food.hungerHeartDelta > 0 && state.hungerHearts < 4;
    const bool changesHappiness = food.happinessHeartDelta > 0 && state.happinessHearts < 4;
    if (!changesHunger && !changesHappiness) {
        return false;
    }

    const bool restoredEmptyHunger = food.hungerHeartDelta > 0 && state.hungerHearts == 0;
    const bool restoredEmptyHappiness = food.happinessHeartDelta > 0 && state.happinessHearts == 0;
    state.hungerHearts = std::clamp(state.hungerHearts + food.hungerHeartDelta, 0, 4);
    state.happinessHearts = std::clamp(state.happinessHearts + food.happinessHeartDelta, 0, 4);
    // Babytchi can eat and play, but the original P1 never changes its
    // displayed five-ounce baby weight.
    if (state.stage != ProgramStage::Baby) {
        state.weight = std::max(0, state.weight + food.weightDelta);
    }
    if ((food.hungerHeartDelta > 0 && state.attentionReason == ProgramAttentionReason::Hunger)
        || (food.happinessHeartDelta > 0
            && state.attentionReason == ProgramAttentionReason::Happiness)) {
        state.attentionReason = ProgramAttentionReason::None;
        state.attentionDeadlineMinutes = -1;
    }
    if (restoredEmptyHunger || restoredEmptyHappiness) {
        state.nextAttentionEligibleMinutes = state.minutesSinceClockSet;
        beginAttentionIfNeeded(programme, state);
    }
    return true;
}

bool ProgramSimulation::completeGame(const ProgramDefinition& programme, ProgramPetState& state,
                                     const int wins) const noexcept
{
    if (state.stage == ProgramStage::Egg || state.stage == ProgramStage::End || state.asleep) {
        return false;
    }
    if (state.attentionReason == ProgramAttentionReason::Discipline) {
        return false;
    }

    if (state.stage != ProgramStage::Baby) {
        state.weight = std::max(0, state.weight + programme.game.weightDeltaOnCompletion);
    }
    const bool restoredEmptyHappiness = state.happinessHearts == 0
        && wins >= programme.game.winsNeededForHappiness;
    if (wins >= programme.game.winsNeededForHappiness) {
        state.happinessHearts = std::clamp(
            state.happinessHearts + programme.game.happinessHeartDeltaOnWin, 0, 4);
        if (state.attentionReason == ProgramAttentionReason::Happiness) {
            state.attentionReason = ProgramAttentionReason::None;
            state.attentionDeadlineMinutes = -1;
        }
        if (restoredEmptyHappiness) {
            state.nextAttentionEligibleMinutes = state.minutesSinceClockSet;
            beginAttentionIfNeeded(programme, state);
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

bool ProgramSimulation::setLightOff(ProgramPetState& state, const bool lightOff) const noexcept
{
    if (!state.asleep || state.lightOff == lightOff) {
        return false;
    }
    state.lightOff = lightOff;
    if (state.lightOff && state.attentionReason == ProgramAttentionReason::SleepLight) {
        state.attentionReason = ProgramAttentionReason::None;
        state.attentionDeadlineMinutes = -1;
        state.nextAttentionEligibleMinutes = state.minutesSinceClockSet;
    }
    return true;
}

bool ProgramSimulation::toggleLight(ProgramPetState& state) const noexcept
{
    return setLightOff(state, !state.lightOff);
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
    state.nextAttentionEligibleMinutes = state.minutesSinceClockSet;
    return true;
}

const CreatureDefinition* ProgramSimulation::firstCharacterAtStage(
    const ProgramDefinition& programme, const ProgramStage stage) noexcept
{
    const auto found = std::find_if(programme.creatures.begin(), programme.creatures.end(),
        [stage](const CreatureDefinition& character) { return character.stage == stage; });
    return found == programme.creatures.end() ? nullptr : &*found;
}

const CreatureDefinition* ProgramSimulation::characterById(
    const ProgramDefinition& programme, const std::string_view id) noexcept
{
    const auto found = std::find_if(programme.creatures.begin(), programme.creatures.end(),
        [id](const CreatureDefinition& character) { return character.id == id; });
    return found == programme.creatures.end() ? nullptr : &*found;
}

const EvolutionRule* ProgramSimulation::matchingEvolutionRule(
    const ProgramDefinition& programme, const ProgramPetState& state) noexcept
{
    const auto within = [](const int value, const int minimum, const int maximum) {
        return value >= minimum && (maximum < 0 || value <= maximum);
    };
    const auto found = std::find_if(programme.evolutionRules.begin(), programme.evolutionRules.end(),
        [&state, &within](const EvolutionRule& rule) {
            return rule.sourceCharacterId == state.characterId
                && within(state.careMistakes, rule.minimumCareMistakes, rule.maximumCareMistakes)
                && within(state.disciplineBars,
                    rule.minimumDisciplineBars, rule.maximumDisciplineBars)
                && within(state.disciplineMistakes,
                    rule.minimumDisciplineMistakes, rule.maximumDisciplineMistakes)
                && state.age >= rule.minimumAge
                && (rule.requiredTeenLineage == ProgramTeenLineage::None
                    || rule.requiredTeenLineage == state.teenLineage)
                && (!rule.requiresTeenStartedWithNoDiscipline
                    || state.teenStartedWithNoDiscipline);
        });
    return found == programme.evolutionRules.end() ? nullptr : &*found;
}

bool ProgramSimulation::evolve(const ProgramDefinition& programme, ProgramPetState& state) noexcept
{
    const EvolutionRule* const rule = matchingEvolutionRule(programme, state);
    if (rule == nullptr) {
        return false;
    }
    const CreatureDefinition* const target = characterById(programme, rule->targetCharacterId);
    if (target == nullptr) {
        return false;
    }

    state.characterId = target->id;
    state.stage = target->stage;
    state.weight = target->minimumWeight;
    if (rule->targetDisciplineBars >= 0) {
        state.disciplineBars = rule->targetDisciplineBars;
    }
    initialiseStageRuntime(state);
    state.asleep = false;
    state.lightOff = false;
    return true;
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
    state.disciplineMistakes = 0;
    state.teenLineage = ProgramTeenLineage::None;
    state.teenStartedWithNoDiscipline = false;
    initialiseStageRuntime(state);
    state.attentionDeadlineMinutes = -1;
    state.nextAttentionEligibleMinutes = 0;
    state.asleep = false;
    state.lightOff = false;
    state.sick = false;
    state.attentionReason = ProgramAttentionReason::None;
}

void ProgramSimulation::initialiseStageRuntime(ProgramPetState& state) noexcept
{
    state.stageAwakeMinutes = 0;
    state.hungerLossElapsedMinutes = 0;
    state.happinessLossElapsedMinutes = 0;
    state.needHeartDecrementsSinceDisciplineCall = 0;
    state.disciplineCallQuota = std::max(0, 4 - state.disciplineBars);
    state.disciplineCallsIssued = 0;
    state.pendingDisciplineCall = false;
}

void ProgramSimulation::advanceNeedTimers(const ProgramDefinition& programme,
                                          ProgramPetState& state,
                                          const bool elapsedWhileAwake) noexcept
{
    if (!elapsedWhileAwake || state.stage == ProgramStage::Egg || state.stage == ProgramStage::End) {
        return;
    }
    const CreatureDefinition* const character = characterById(programme, state.characterId);
    if (character == nullptr) {
        return;
    }

    ++state.stageAwakeMinutes;
    const auto loseHeart = [](int& elapsed, const int interval, int& hearts) {
        if (interval < 1) {
            return false;
        }
        ++elapsed;
        if (elapsed < interval) {
            return false;
        }
        elapsed = 0;
        if (hearts == 0) {
            return false;
        }
        --hearts;
        return true;
    };

    const bool lostHunger = loseHeart(state.hungerLossElapsedMinutes,
        character->hungerHeartLossMinutes, state.hungerHearts);
    const bool lostHappiness = loseHeart(state.happinessLossElapsedMinutes,
        character->happinessHeartLossMinutes, state.happinessHearts);
    const int decrements = static_cast<int>(lostHunger) + static_cast<int>(lostHappiness);
    if (decrements == 0 || character->disciplineCallAfterNeedDecrements < 1
        || state.disciplineCallsIssued >= state.disciplineCallQuota) {
        return;
    }

    state.needHeartDecrementsSinceDisciplineCall += decrements;
    if (state.needHeartDecrementsSinceDisciplineCall
        >= character->disciplineCallAfterNeedDecrements) {
        state.needHeartDecrementsSinceDisciplineCall
            -= character->disciplineCallAfterNeedDecrements;
        ++state.disciplineCallsIssued;
        state.pendingDisciplineCall = true;
    }
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

void ProgramSimulation::expireAttention(const ProgramDefinition& programme,
                                        ProgramPetState& state) noexcept
{
    if (state.attentionReason == ProgramAttentionReason::None
        || state.attentionDeadlineMinutes < 0
        || state.minutesSinceClockSet < state.attentionDeadlineMinutes) {
        return;
    }

    // Classic 1996–97 P1 deliberately does not turn a missed false discipline
    // call into a care mistake. A real hunger, happiness, or lights-off call
    // does. The negative eligibility sentinel prevents repeated mistakes while
    // the same empty meter remains unattended.
    const bool wasDisciplineCall = state.attentionReason == ProgramAttentionReason::Discipline;
    if (!wasDisciplineCall) {
        ++state.careMistakes;
    }
    state.attentionReason = ProgramAttentionReason::None;
    state.attentionDeadlineMinutes = -1;
    state.nextAttentionEligibleMinutes = wasDisciplineCall
        ? state.minutesSinceClockSet : -1;
}

void ProgramSimulation::beginAttentionIfNeeded(const ProgramDefinition& programme,
                                               ProgramPetState& state) noexcept
{
    if (state.stage == ProgramStage::Egg || state.stage == ProgramStage::End
        || state.attentionReason != ProgramAttentionReason::None
        || state.nextAttentionEligibleMinutes < 0
        || state.minutesSinceClockSet < state.nextAttentionEligibleMinutes) {
        return;
    }

    ProgramAttentionReason reason = ProgramAttentionReason::None;
    if (state.asleep && !state.lightOff) {
        reason = ProgramAttentionReason::SleepLight;
    } else if (state.hungerHearts == 0) {
        reason = ProgramAttentionReason::Hunger;
    } else if (state.happinessHearts == 0) {
        reason = ProgramAttentionReason::Happiness;
    } else if (state.pendingDisciplineCall && !state.asleep) {
        reason = ProgramAttentionReason::Discipline;
        state.pendingDisciplineCall = false;
    }
    if (reason == ProgramAttentionReason::None) {
        return;
    }

    state.attentionReason = reason;
    state.attentionDeadlineMinutes = state.minutesSinceClockSet
        + programme.lifecycle.attentionWindowMinutes;
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
    const bool wasAsleep = state.asleep;
    if (wasAsleep && !shouldSleep) {
        ++state.age;
        state.lightOff = false;
        if (state.attentionReason == ProgramAttentionReason::SleepLight) {
            state.attentionReason = ProgramAttentionReason::None;
            state.attentionDeadlineMinutes = -1;
        }
        state.nextAttentionEligibleMinutes = state.minutesSinceClockSet;
    }
    state.asleep = shouldSleep;
    if (!wasAsleep && shouldSleep) {
        state.lightOff = false;
        state.nextAttentionEligibleMinutes = state.minutesSinceClockSet;
    }
}

} // namespace CnaTamagotchi::Domain
