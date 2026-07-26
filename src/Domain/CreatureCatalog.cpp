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

constexpr CreatureSprite Budbit{{
    "    ##    ",
    "   ####   ",
    "    ##    ",
    "  ##  ##  ",
    " ######## ",
    "## ## ## #",
    "##########",
    "   ## ##  ",
    "  ##  ##  ",
    " ##    ## ",
}};

constexpr CreatureSprite Fernkin{{
    "    ##    ",
    "   ####   ",
    " ## ## ## ",
    "  ######  ",
    "##########",
    "## ## ## #",
    " ######## ",
    "  ##  ##  ",
    " ##    ## ",
    "##      ##",
}};

constexpr CreatureSprite Lilyloop{{
    " ## ## ## ",
    "### ## ###",
    " ######## ",
    "## ## ## #",
    "##########",
    "  ######  ",
    " ## ## ## ",
    "##  ##  ##",
    "   ## ##   ",
    "  ##  ##  ",
}};

constexpr CreatureSprite Thornhop{{
    "##  ##  ##",
    " ###  ### ",
    "  ######  ",
    " ######## ",
    "## ## ## #",
    "##########",
    " ## ######",
    "  ##  ##  ",
    " ##    ## ",
    "##      ##",
}};

constexpr CreatureSprite Reedhare{{
    " ##    ## ",
    "###    ###",
    " ######## ",
    "## ## ## #",
    "##########",
    "  ######  ",
    " ## ## ## ",
    " ## ####  ",
    "##  ## ## ",
    "   ##  ## ",
}};

constexpr CreatureSprite Cloverowl{{
    " ## ## ## ",
    "##########",
    " ## ## ## ",
    "## ## ## #",
    "##########",
    "## #### ##",
    "##########",
    "  ##  ##  ",
    " ######## ",
    "##      ##",
}};

constexpr CreatureSprite Bloomtail{{
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

constexpr CreatureSprite Sedgehog{{
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

constexpr CreatureSprite Nectarmoth{{
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

constexpr CreatureSprite Rootslug{{
    "     ##   ",
    "   ###### ",
    " ######## ",
    "## ## ## #",
    "##########",
    "##########",
    " #######  ",
    "## ###### ",
    " #########",
    "   #######",
}};

constexpr CreatureSprite Starbloom{{
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
    if (state.lifeStage == LifeStage::Farewell) {
        return CreatureForm::Farewell;
    }

    if (state.species == PetSpecies::Mossling) {
        switch (state.lifeStage) {
        case LifeStage::Egg:
            return CreatureForm::Egg;
        case LifeStage::Hatchling:
            return CreatureForm::Budbit;
        case LifeStage::Child:
            return CreatureForm::Fernkin;
        case LifeStage::Teen:
            return careQuality(state) >= 60 ? CreatureForm::Lilyloop : CreatureForm::Thornhop;
        case LifeStage::Adult:
        case LifeStage::Elder: {
            const int quality = careQuality(state);
            if (state.ageMinutes >= 12 * 24 * 60 && quality >= 85) {
                return CreatureForm::Starbloom;
            }
            if (quality >= 85) return CreatureForm::Reedhare;
            if (quality >= 70) return CreatureForm::Cloverowl;
            if (quality >= 55) return CreatureForm::Bloomtail;
            if (quality >= 40) return CreatureForm::Sedgehog;
            if (quality >= 25) return CreatureForm::Nectarmoth;
            return CreatureForm::Rootslug;
        }
        case LifeStage::Farewell:
            return CreatureForm::Farewell;
        }
    }

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
    case LifeStage::Farewell: return CreatureForm::Farewell;
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
    case CreatureForm::Budbit: return Budbit;
    case CreatureForm::Fernkin: return Fernkin;
    case CreatureForm::Lilyloop: return Lilyloop;
    case CreatureForm::Thornhop: return Thornhop;
    case CreatureForm::Reedhare: return Reedhare;
    case CreatureForm::Cloverowl: return Cloverowl;
    case CreatureForm::Bloomtail: return Bloomtail;
    case CreatureForm::Sedgehog: return Sedgehog;
    case CreatureForm::Nectarmoth: return Nectarmoth;
    case CreatureForm::Rootslug: return Rootslug;
    case CreatureForm::Starbloom: return Starbloom;
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
    case CreatureForm::Budbit: return "Budbit";
    case CreatureForm::Fernkin: return "Fernkin";
    case CreatureForm::Lilyloop: return "Lilyloop";
    case CreatureForm::Thornhop: return "Thornhop";
    case CreatureForm::Reedhare: return "Reedhare";
    case CreatureForm::Cloverowl: return "Cloverowl";
    case CreatureForm::Bloomtail: return "Bloomtail";
    case CreatureForm::Sedgehog: return "Sedgehog";
    case CreatureForm::Nectarmoth: return "Nectarmoth";
    case CreatureForm::Rootslug: return "Rootslug";
    case CreatureForm::Starbloom: return "Starbloom";
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
