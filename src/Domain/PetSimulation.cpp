#include "CnaTamagotchi/Domain/PetSimulation.hpp"

#include <algorithm>

namespace CnaTamagotchi::Domain {
namespace {

constexpr int MinNeed = 0;
constexpr int MaxNeed = 100;
constexpr int HeartValue = 25;
constexpr int AttentionWindowMinutes = 15;

int clampNeed(const int value) noexcept
{
    return std::clamp(value, MinNeed, MaxNeed);
}

int heartLossInterval(const LifeStage stage) noexcept
{
    switch (stage) {
    case LifeStage::Hatchling: return 15;
    case LifeStage::Child: return 30;
    case LifeStage::Teen: return 35;
    case LifeStage::Adult: return 40;
    case LifeStage::Elder: return 30;
    case LifeStage::Egg:
    case LifeStage::Farewell:
        return 0;
    }

    return 0;
}

} // namespace

SimulationReport PetSimulation::advance(PetState& state, const int elapsedMinutes) const noexcept
{
    const int requested = std::max(elapsedMinutes, 0);
    const int applied = std::min(requested, MaximumOfflineMinutes);

    for (int minute = 0; minute < applied; ++minute) {
        advanceOneMinute(state);
    }

    enforceInvariants(state);
    return SimulationReport{
        .requestedMinutes = requested,
        .appliedMinutes = applied,
        .wasClamped = requested != applied,
    };
}

void PetSimulation::applyAction(PetState& state, const PetAction action) const noexcept
{
    switch (action) {
    case PetAction::Meal:
        state.needs.hunger += HeartValue;
        ++state.weight;
        if (state.attentionReason == AttentionReason::Hunger) {
            clearAttention(state);
        }
        break;
    case PetAction::Snack:
        state.needs.happiness += HeartValue;
        state.weight += 2;
        if (state.attentionReason == AttentionReason::Happiness) {
            clearAttention(state);
        }
        break;
    case PetAction::Clean:
        state.needs.hygiene = MaxNeed;
        state.wasteCount = 0;
        break;
    case PetAction::Medicine:
        if (state.sick) {
            state.needs.health += 35;
            state.sick = state.needs.health < 40;
        } else {
            state.needs.affection -= 1;
        }
        break;
    case PetAction::Play:
        if (!state.asleep) {
            state.needs.happiness += HeartValue;
            state.needs.energy -= 10;
            state.weight = std::max(0, state.weight - 1);
            if (state.attentionReason == AttentionReason::Happiness) {
                clearAttention(state);
            }
        }
        break;
    case PetAction::ToggleSleep:
        state.asleep = !state.asleep;
        if (!state.asleep) {
            state.lightOff = false;
        }
        break;
    case PetAction::ToggleLight:
        if (state.asleep) {
            state.lightOff = !state.lightOff;
            if (state.lightOff && state.attentionReason == AttentionReason::SleepLight) {
                clearAttention(state);
            }
        }
        break;
    case PetAction::Discipline:
        if (state.attentionReason == AttentionReason::Discipline) {
            state.needs.discipline += HeartValue;
            clearAttention(state);
        }
        break;
    }

    enforceInvariants(state);
    updateAttention(state);
}

void PetSimulation::advanceOneMinute(PetState& state) noexcept
{
    ++state.ageMinutes;
    state.clockMinutesOfDay = (state.clockMinutesOfDay + 1) % (24 * 60);

    updateLifeStage(state);
    updateSleepSchedule(state);

    const int lossInterval = heartLossInterval(state.lifeStage);
    if (lossInterval > 0 && state.ageMinutes % lossInterval == 0) {
        state.needs.hunger -= HeartValue;
        state.needs.happiness -= HeartValue;
        --state.needs.hygiene;
        state.needs.energy += state.asleep ? 2 : -1;
    }

    if (lossInterval > 0 && state.ageMinutes % 120 == 0) {
        ++state.wasteCount;
    }

    if (state.ageMinutes % 60 == 0) {
        const bool neglected = state.needs.hunger == 0 || state.needs.happiness == 0
            || state.needs.hygiene < 20 || state.needs.energy < 10 || state.wasteCount >= 2;
        if (neglected) {
            state.needs.health -= 5;
            ++state.careMistakes;
        } else if (!state.sick) {
            ++state.needs.health;
        }

        if (state.needs.health <= 15) {
            state.sick = true;
        }
    }

    enforceInvariants(state);
    updateAttention(state);
}

void PetSimulation::updateLifeStage(PetState& state) noexcept
{
    if (state.lifeStage == LifeStage::Farewell || state.lifeStage == LifeStage::Elder) {
        return;
    }

    if (state.ageMinutes >= 30 * 24 * 60) {
        state.lifeStage = LifeStage::Elder;
    } else if (state.ageMinutes >= 6 * 24 * 60) {
        state.lifeStage = LifeStage::Adult;
    } else if (state.ageMinutes >= 3 * 24 * 60) {
        state.lifeStage = LifeStage::Teen;
    } else if (state.ageMinutes >= 65) {
        state.lifeStage = LifeStage::Child;
    } else if (state.ageMinutes >= 5) {
        state.lifeStage = LifeStage::Hatchling;
    }
}

void PetSimulation::updateSleepSchedule(PetState& state) noexcept
{
    if (state.lifeStage == LifeStage::Egg || state.lifeStage == LifeStage::Hatchling
        || state.lifeStage == LifeStage::Farewell) {
        return;
    }

    constexpr int SleepStart = 20 * 60;
    constexpr int WakeTime = 8 * 60;
    const bool shouldSleep = state.clockMinutesOfDay >= SleepStart
        || state.clockMinutesOfDay < WakeTime;
    if (state.asleep != shouldSleep) {
        state.asleep = shouldSleep;
        if (!state.asleep) {
            state.lightOff = false;
        }
    }
}

void PetSimulation::updateAttention(PetState& state) noexcept
{
    if (state.attentionReason != AttentionReason::None) {
        if (state.ageMinutes >= state.attentionDeadlineMinutes) {
            if (state.attentionReason != AttentionReason::Discipline) {
                ++state.careMistakes;
            }
            clearAttention(state);
            state.nextAttentionEligibleMinutes = state.ageMinutes + AttentionWindowMinutes;
        }
        return;
    }

    if (state.ageMinutes < state.nextAttentionEligibleMinutes) {
        return;
    }

    AttentionReason reason = AttentionReason::None;
    if (state.needs.hunger == 0) {
        reason = AttentionReason::Hunger;
    } else if (state.needs.happiness == 0) {
        reason = AttentionReason::Happiness;
    } else if (state.asleep && !state.lightOff) {
        reason = AttentionReason::SleepLight;
    } else if (state.lifeStage != LifeStage::Egg && state.lifeStage != LifeStage::Farewell
               && state.ageMinutes > 0 && state.ageMinutes % (6 * 60) == 0) {
        reason = AttentionReason::Discipline;
    }

    if (reason != AttentionReason::None) {
        state.attentionReason = reason;
        state.attentionDeadlineMinutes = state.ageMinutes + AttentionWindowMinutes;
    }
}

void PetSimulation::clearAttention(PetState& state) noexcept
{
    state.attentionReason = AttentionReason::None;
    state.attentionDeadlineMinutes = -1;
}

void PetSimulation::enforceInvariants(PetState& state) noexcept
{
    state.needs.hunger = clampNeed(state.needs.hunger);
    state.needs.happiness = clampNeed(state.needs.happiness);
    state.needs.energy = clampNeed(state.needs.energy);
    state.needs.hygiene = clampNeed(state.needs.hygiene);
    state.needs.health = clampNeed(state.needs.health);
    state.needs.affection = clampNeed(state.needs.affection);
    state.needs.discipline = clampNeed(state.needs.discipline);
    state.weight = std::max(state.weight, 0);
    state.careMistakes = std::max(state.careMistakes, 0);
    state.ageMinutes = std::max(state.ageMinutes, 0);
    state.clockMinutesOfDay = std::clamp(state.clockMinutesOfDay, 0, 24 * 60 - 1);
    state.wasteCount = std::max(state.wasteCount, 0);
    state.nextAttentionEligibleMinutes = std::max(state.nextAttentionEligibleMinutes, 0);
    if (state.attentionReason == AttentionReason::None) {
        state.attentionDeadlineMinutes = -1;
    } else {
        state.attentionDeadlineMinutes = std::max(state.attentionDeadlineMinutes, state.ageMinutes);
    }
}

} // namespace CnaTamagotchi::Domain
