#include "CnaTamagotchi/Domain/PetSimulation.hpp"

#include <algorithm>

namespace CnaTamagotchi::Domain {
namespace {

constexpr int MinNeed = 0;
constexpr int MaxNeed = 100;

int clampNeed(const int value) noexcept
{
    return std::clamp(value, MinNeed, MaxNeed);
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
        state.needs.hunger += 28;
        state.needs.happiness += 2;
        ++state.weight;
        break;
    case PetAction::Snack:
        state.needs.hunger += 8;
        state.needs.happiness += 20;
        state.needs.energy += 2;
        state.weight += 2;
        break;
    case PetAction::Clean:
        state.needs.hygiene = MaxNeed;
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
            state.needs.happiness += 20;
            state.needs.energy -= 15;
            state.needs.hunger -= 5;
        }
        break;
    case PetAction::ToggleSleep:
        state.asleep = !state.asleep;
        break;
    case PetAction::Discipline:
        state.needs.discipline += 12;
        state.needs.affection -= 3;
        break;
    }

    enforceInvariants(state);
}

void PetSimulation::advanceOneMinute(PetState& state) noexcept
{
    ++state.ageMinutes;

    // Needs move in deliberate 15-minute steps, so an interactive session
    // and an offline catch-up of equal duration always produce the same state.
    if (state.ageMinutes % 15 == 0) {
        --state.needs.hunger;
        --state.needs.happiness;
        --state.needs.hygiene;
        state.needs.energy += state.asleep ? 2 : -1;
    }

    if (state.ageMinutes % 60 == 0) {
        const bool neglected = state.needs.hunger < 20
            || state.needs.hygiene < 20 || state.needs.energy < 10;
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

    updateLifeStage(state);
    enforceInvariants(state);
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
}

} // namespace CnaTamagotchi::Domain
