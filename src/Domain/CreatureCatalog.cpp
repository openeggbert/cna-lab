#include "CnaTamagotchi/Domain/CreatureCatalog.hpp"

#include <algorithm>

namespace CnaTamagotchi::Domain {
namespace {

constexpr CreatureSprite Egg{{
    "   ####   ",
    "  ######  ",
    " ######## ",
    "##########",
    "##########",
    " ######## ",
    "  ######  ",
    "   ####   ",
    "          ",
    "          ",
}};

constexpr CreatureSprite Pipple{{
    "    ##    ",
    "  ######  ",
    " ######## ",
    "## ## ## #",
    "##########",
    "## #### ##",
    "##########",
    "  ##  ##  ",
    " ##    ## ",
    "##      ##",
}};

constexpr CreatureSprite Sproutlet{{
    "    ##    ",
    "   ####   ",
    "  ##  ##  ",
    " ######## ",
    "## ## ## #",
    "##########",
    " ## ## ## ",
    "  ##  ##  ",
    " ##    ## ",
    "##      ##",
}};

constexpr CreatureSprite Flitwing{{
    " ##    ## ",
    "###    ###",
    " ######## ",
    "## ## ## #",
    "##########",
    "  ######  ",
    " ## ## ## ",
    "##  ##  ##",
    "   ## ##   ",
    "  ##  ##  ",
}};

constexpr CreatureSprite Tumblepuff{{
    "  ##  ##  ",
    " ######## ",
    "##########",
    "## ## ## #",
    "##########",
    "##########",
    " ## ######",
    "  ##  ##  ",
    " ##    ## ",
    "##      ##",
}};

constexpr CreatureSprite Skywhistle{{
    " ##    ## ",
    "###    ###",
    " ######## ",
    "## ## ## #",
    "##########",
    "  ######  ",
    "  ## ##   ",
    " ## ####  ",
    "##  ## ## ",
    "   ##  ## ",
}};

constexpr CreatureSprite Mossmuzzle{{
    " ##    ## ",
    "###    ###",
    " ######## ",
    "## ## ## #",
    "##########",
    "## #### ##",
    "##########",
    "  ##  ##  ",
    " ######## ",
    "##      ##",
}};

constexpr CreatureSprite Ripplefin{{
    "    ##    ",
    "  ######  ",
    " ######## ",
    "## ## ## #",
    "##########",
    "##########",
    " ## ####  ",
    "##  ######",
    " ####  ## ",
    "##      ##",
}};

constexpr CreatureSprite Pebbleback{{
    "  ######  ",
    " ######## ",
    "## #### ##",
    "##########",
    "## ## ## #",
    "##########",
    "  ####### ",
    " ## ## ## ",
    "##  ##  ##",
    "  ##  ##  ",
}};

constexpr CreatureSprite Bramblepaw{{
    " ##    ## ",
    "###    ###",
    "##########",
    "## ## ## #",
    "##########",
    "##########",
    " ## ######",
    "## ## ## #",
    "  ##  ##  ",
    " ##    ## ",
}};

constexpr CreatureSprite Duskroot{{
    "##      ##",
    "###    ###",
    " ######## ",
    "## ## ## #",
    "##########",
    "## #### ##",
    "##########",
    "##  ##  ##",
    " ## ## ## ",
    "##      ##",
}};

constexpr CreatureSprite Moonmote{{
    "   ## ##  ",
    " ## ######",
    "##########",
    "## ## ## #",
    "##########",
    "  ######  ",
    " ######## ",
    "## ## ## #",
    "  ##  ##  ",
    " ##    ## ",
}};

constexpr CreatureSprite Farewell{{
    "    ##    ",
    "   ####   ",
    "  ##  ##  ",
    " ######## ",
    "   ####   ",
    "  ##  ##  ",
    " ##    ## ",
    "          ",
    "    ##    ",
    "   ####   ",
}};

} // namespace

CreatureForm CreatureCatalog::formFor(const PetState& state) noexcept
{
    switch (state.lifeStage) {
    case LifeStage::Egg:
        return CreatureForm::Egg;
    case LifeStage::Hatchling:
        return CreatureForm::Pipple;
    case LifeStage::Child:
        return CreatureForm::Sproutlet;
    case LifeStage::Teen:
        return careQuality(state) >= 60 ? CreatureForm::Flitwing : CreatureForm::Tumblepuff;
    case LifeStage::Adult:
    case LifeStage::Elder: {
        const int quality = careQuality(state);
        if (state.ageMinutes >= 12 * 24 * 60 && quality >= 85) {
            return CreatureForm::Moonmote;
        }
        if (quality >= 85) return CreatureForm::Skywhistle;
        if (quality >= 70) return CreatureForm::Mossmuzzle;
        if (quality >= 55) return CreatureForm::Ripplefin;
        if (quality >= 40) return CreatureForm::Pebbleback;
        if (quality >= 25) return CreatureForm::Bramblepaw;
        return CreatureForm::Duskroot;
    }
    case LifeStage::Farewell:
        return CreatureForm::Farewell;
    }

    return CreatureForm::Egg;
}

const CreatureSprite& CreatureCatalog::spriteFor(const CreatureForm form) noexcept
{
    switch (form) {
    case CreatureForm::Egg: return Egg;
    case CreatureForm::Pipple: return Pipple;
    case CreatureForm::Sproutlet: return Sproutlet;
    case CreatureForm::Flitwing: return Flitwing;
    case CreatureForm::Tumblepuff: return Tumblepuff;
    case CreatureForm::Skywhistle: return Skywhistle;
    case CreatureForm::Mossmuzzle: return Mossmuzzle;
    case CreatureForm::Ripplefin: return Ripplefin;
    case CreatureForm::Pebbleback: return Pebbleback;
    case CreatureForm::Bramblepaw: return Bramblepaw;
    case CreatureForm::Duskroot: return Duskroot;
    case CreatureForm::Moonmote: return Moonmote;
    case CreatureForm::Farewell: return Farewell;
    }

    return Egg;
}

std::string_view CreatureCatalog::nameFor(const CreatureForm form) noexcept
{
    switch (form) {
    case CreatureForm::Egg: return "Egg";
    case CreatureForm::Pipple: return "Pipple";
    case CreatureForm::Sproutlet: return "Sproutlet";
    case CreatureForm::Flitwing: return "Flitwing";
    case CreatureForm::Tumblepuff: return "Tumblepuff";
    case CreatureForm::Skywhistle: return "Skywhistle";
    case CreatureForm::Mossmuzzle: return "Mossmuzzle";
    case CreatureForm::Ripplefin: return "Ripplefin";
    case CreatureForm::Pebbleback: return "Pebbleback";
    case CreatureForm::Bramblepaw: return "Bramblepaw";
    case CreatureForm::Duskroot: return "Duskroot";
    case CreatureForm::Moonmote: return "Moonmote";
    case CreatureForm::Farewell: return "Farewell";
    }

    return "Egg";
}

int CreatureCatalog::careQuality(const PetState& state) noexcept
{
    const int weightPenalty = std::max(0, state.weight - 15) * 3;
    const int illnessPenalty = state.sick ? 20 : 0;
    const int score = 100 - state.careMistakes * 15
        + (state.needs.discipline - 50) / 2 - weightPenalty - illnessPenalty;
    return std::clamp(score, 0, 100);
}

} // namespace CnaTamagotchi::Domain
