#include "CnaTamagotchi/Domain/P1Program.hpp"

#include <array>

namespace CnaTamagotchi::Domain::Programs {
namespace {

constexpr std::array<FoodDefinition, 2> Foods{{
    FoodDefinition{"bread", "BREAD", 1, 0, 1},
    FoodDefinition{"candy", "CANDY", 0, 1, 2},
}};

constexpr std::array<CreatureDefinition, 11> Creatures{{
    CreatureDefinition{"babytchi", "Babytchi", ProgramStage::Baby, false, 5},
    CreatureDefinition{"marutchi", "Marutchi", ProgramStage::Child, false, 10, 20 * 60, 9 * 60},
    CreatureDefinition{"tamatchi", "Tamatchi", ProgramStage::Teen, false, 20, 21 * 60, 9 * 60},
    CreatureDefinition{"kuchitamatchi", "Kuchitamatchi", ProgramStage::Teen, false, 20, 21 * 60, 9 * 60},
    CreatureDefinition{"mametchi", "Mametchi", ProgramStage::Adult, false, 30, 22 * 60, 9 * 60},
    CreatureDefinition{"ginjirotchi", "Ginjirotchi", ProgramStage::Adult, false, 30, 22 * 60, 9 * 60},
    CreatureDefinition{"maskutchi", "Maskutchi", ProgramStage::Adult, false, 30, 23 * 60, 11 * 60},
    CreatureDefinition{"kuchipatchi", "Kuchipatchi", ProgramStage::Adult, false, 20, 22 * 60, 9 * 60},
    CreatureDefinition{"nyorotchi", "Nyorotchi", ProgramStage::Adult, false, 10, 22 * 60, 9 * 60},
    CreatureDefinition{"tarakotchi", "Tarakotchi", ProgramStage::Adult, false, 20, 22 * 60, 10 * 60},
    CreatureDefinition{"bill", "Bill", ProgramStage::Adult, true, 30, 22 * 60, 9 * 60},
}};

// International P1's published chart distinguishes the hidden A/B teen
// lineage from its visible character.  These ranges are intentionally data so
// the simulator stays programme-agnostic; Bill's later Maskutchi-only special
// branch is represented in the roster but awaits its separately timed trace.
constexpr std::array<EvolutionRule, 17> EvolutionRules{{
    EvolutionRule{"marutchi", "tamatchi", 0, 2},
    EvolutionRule{"marutchi", "kuchitamatchi", 3, -1},

    EvolutionRule{"tamatchi", "mametchi", 0, 2, 0, 0, ProgramTeenLineage::TypeA},
    EvolutionRule{"tamatchi", "ginjirotchi", 0, 2, 1, 1, ProgramTeenLineage::TypeA},
    EvolutionRule{"tamatchi", "maskutchi", 0, 2, 2, -1, ProgramTeenLineage::TypeA},
    EvolutionRule{"tamatchi", "kuchipatchi", 3, -1, 0, 1, ProgramTeenLineage::TypeA},
    EvolutionRule{"tamatchi", "nyorotchi", 3, -1, 2, 3, ProgramTeenLineage::TypeA},
    EvolutionRule{"tamatchi", "tarakotchi", 3, -1, 4, -1, ProgramTeenLineage::TypeA},
    EvolutionRule{"tamatchi", "maskutchi", 0, 3, 2, -1, ProgramTeenLineage::TypeB},
    EvolutionRule{"tamatchi", "nyorotchi", 4, -1, 3, 7, ProgramTeenLineage::TypeB},
    EvolutionRule{"tamatchi", "tarakotchi", 4, -1, 8, -1, ProgramTeenLineage::TypeB},

    EvolutionRule{"kuchitamatchi", "kuchipatchi", 3, -1, 0, 1, ProgramTeenLineage::TypeA},
    EvolutionRule{"kuchitamatchi", "nyorotchi", 3, -1, 2, 2, ProgramTeenLineage::TypeA},
    EvolutionRule{"kuchitamatchi", "tarakotchi", 3, -1, 3, -1, ProgramTeenLineage::TypeA},
    EvolutionRule{"kuchitamatchi", "maskutchi", 0, 3, 2, -1, ProgramTeenLineage::TypeB},
    EvolutionRule{"kuchitamatchi", "nyorotchi", 3, -1, 3, 5, ProgramTeenLineage::TypeB},
    EvolutionRule{"kuchitamatchi", "tarakotchi", 3, -1, 6, -1, ProgramTeenLineage::TypeB},
}};

constexpr ProgramDefinition Definition{
    .id = "international-p1-1997",
    .displayName = "International P1 (1997)",
    .display = DisplayDefinition{.checkerboardBackground = true},
    .lifecycle = LifecycleDefinition{
        .hatchDelayMinutes = 5,
        .babyToChildMinutes = 65,
        .childToTeenMinutes = 48 * 60,
        .teenToAdultMinutes = 72 * 60,
        .teenAge = 3,
        .adultAge = 6,
        .babyNapStartMinute = 40,
        .babyNapDurationMinutes = 5,
        .babyIllnessMinute = 33,
        .babyIllnessMedicineDoses = 2,
        .babyFirstWasteMinute = 15,
        .babySecondWasteMinute = 45,
    },
    .game = GameDefinition{
        .kind = ProgramGameKind::Character,
        .rounds = 5,
        .winsNeededForHappiness = 3,
        .happinessHeartDeltaOnWin = 1,
        .weightDeltaOnCompletion = -1,
    },
    .endScreen = ProgramEndScreen::AngelStars,
    .food = Foods,
    .creatures = Creatures,
    .evolutionRules = EvolutionRules,
};

} // namespace

const ProgramDefinition& internationalP1() noexcept
{
    return Definition;
}

} // namespace CnaTamagotchi::Domain::Programs
