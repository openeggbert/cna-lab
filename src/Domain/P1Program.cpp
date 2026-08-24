#include "CnaTamagotchi/Domain/P1Program.hpp"

#include <array>

namespace CnaTamagotchi::Domain::Programs {
namespace {

constexpr std::array<FoodDefinition, 2> Foods{{
    FoodDefinition{"bread", "BREAD", 1, 0, 1},
    FoodDefinition{"candy", "CANDY", 0, 1, 2},
}};

constexpr std::array<CreatureDefinition, 11> Creatures{{
    CreatureDefinition{"babytchi", "Babytchi", ProgramStage::Baby, false, 5,
        5, -1, -1, 3, 4, -1, 1, 2},
    CreatureDefinition{"marutchi", "Marutchi", ProgramStage::Child, false, 10,
        99, 20 * 60, 9 * 60, 50, 60, 6, 1, 2},
    CreatureDefinition{"tamatchi", "Tamatchi", ProgramStage::Teen, false, 20,
        99, 21 * 60, 9 * 60, 75, 85, 6, 1, 2},
    CreatureDefinition{"kuchitamatchi", "Kuchitamatchi", ProgramStage::Teen, false, 20,
        99, 21 * 60, 9 * 60, 75, 85, 6, 1, 2},
    CreatureDefinition{"mametchi", "Mametchi", ProgramStage::Adult, false, 30,
        99, 22 * 60, 9 * 60, 81, 91, -1, 1, 2},
    CreatureDefinition{"ginjirotchi", "Ginjirotchi", ProgramStage::Adult, false, 30,
        99, 22 * 60, 9 * 60, 81, 91, 7, 1, 2},
    CreatureDefinition{"maskutchi", "Maskutchi", ProgramStage::Adult, false, 30,
        99, 23 * 60, 11 * 60, 55, 65, 7, 10, 32},
    CreatureDefinition{"kuchipatchi", "Kuchipatchi", ProgramStage::Adult, false, 20,
        99, 22 * 60, 9 * 60, 60, 70, -1, 22, 32},
    CreatureDefinition{"nyorotchi", "Nyorotchi", ProgramStage::Adult, false, 10,
        99, 22 * 60, 9 * 60, 60, 70, 7, 1, 2},
    CreatureDefinition{"tarakotchi", "Tarakotchi", ProgramStage::Adult, false, 20,
        99, 22 * 60, 10 * 60, 45, 50, 7, 1, 2},
    CreatureDefinition{"bill", "Bill", ProgramStage::Adult, true, 30,
        99, 22 * 60, 9 * 60, 81, 91, -1, 1, 2},
}};

// International P1's classic chart distinguishes the hidden A/B teen lineage
// from its visible character.  Unlike a modern rerelease, it evaluates the
// four visible discipline bars (0%, 25%, 50%, 75%, 100%), not a count of
// missed discipline calls. These ranges are intentionally data so the
// simulator stays programme-agnostic, including Bill's later, qualified
// Maskutchi-only form change.
constexpr std::array<EvolutionRule, 20> EvolutionRules{{
    EvolutionRule{.sourceCharacterId = "marutchi", .targetCharacterId = "tamatchi",
        .minimumCareMistakes = 0, .maximumCareMistakes = 1,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "marutchi", .targetCharacterId = "tamatchi",
        .minimumCareMistakes = 0, .maximumCareMistakes = 1,
        .requiredTeenLineage = ProgramTeenLineage::TypeB, .targetDisciplineBars = 0},
    EvolutionRule{.sourceCharacterId = "marutchi", .targetCharacterId = "kuchitamatchi",
        .minimumCareMistakes = 2, .requiredTeenLineage = ProgramTeenLineage::TypeA,
        .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "marutchi", .targetCharacterId = "kuchitamatchi",
        .minimumCareMistakes = 2, .requiredTeenLineage = ProgramTeenLineage::TypeB,
        .targetDisciplineBars = 0},

    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "mametchi",
        .maximumCareMistakes = 2, .minimumDisciplineBars = 4, .maximumDisciplineBars = 4,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 4},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "ginjirotchi",
        .maximumCareMistakes = 2, .minimumDisciplineBars = 3, .maximumDisciplineBars = 3,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "maskutchi",
        .maximumCareMistakes = 2, .maximumDisciplineBars = 2,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 0},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "kuchipatchi",
        .minimumCareMistakes = 3, .minimumDisciplineBars = 4, .maximumDisciplineBars = 4,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 4},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "nyorotchi",
        .minimumCareMistakes = 3, .minimumDisciplineBars = 3, .maximumDisciplineBars = 3,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "tarakotchi",
        .minimumCareMistakes = 3, .maximumDisciplineBars = 2,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 0},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "ginjirotchi",
        .maximumCareMistakes = 2, .minimumDisciplineBars = 4, .maximumDisciplineBars = 4,
        .requiredTeenLineage = ProgramTeenLineage::TypeB, .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "maskutchi",
        .maximumCareMistakes = 2, .maximumDisciplineBars = 3,
        .requiredTeenLineage = ProgramTeenLineage::TypeB, .targetDisciplineBars = 0},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "nyorotchi",
        .minimumCareMistakes = 3, .minimumDisciplineBars = 4, .maximumDisciplineBars = 4,
        .requiredTeenLineage = ProgramTeenLineage::TypeB, .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "tamatchi", .targetCharacterId = "tarakotchi",
        .minimumCareMistakes = 3, .maximumDisciplineBars = 3,
        .requiredTeenLineage = ProgramTeenLineage::TypeB, .targetDisciplineBars = 0},

    EvolutionRule{.sourceCharacterId = "kuchitamatchi", .targetCharacterId = "kuchipatchi",
        .minimumDisciplineBars = 4, .maximumDisciplineBars = 4,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 4},
    EvolutionRule{.sourceCharacterId = "kuchitamatchi", .targetCharacterId = "nyorotchi",
        .minimumDisciplineBars = 3, .maximumDisciplineBars = 3,
        .requiredTeenLineage = ProgramTeenLineage::TypeA, .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "kuchitamatchi", .targetCharacterId = "tarakotchi",
        .maximumDisciplineBars = 2, .requiredTeenLineage = ProgramTeenLineage::TypeA,
        .targetDisciplineBars = 0},
    EvolutionRule{.sourceCharacterId = "kuchitamatchi", .targetCharacterId = "nyorotchi",
        .minimumDisciplineBars = 4, .maximumDisciplineBars = 4,
        .requiredTeenLineage = ProgramTeenLineage::TypeB, .targetDisciplineBars = 2},
    EvolutionRule{.sourceCharacterId = "kuchitamatchi", .targetCharacterId = "tarakotchi",
        .maximumDisciplineBars = 3, .requiredTeenLineage = ProgramTeenLineage::TypeB,
        .targetDisciplineBars = 0},

    EvolutionRule{.sourceCharacterId = "maskutchi", .targetCharacterId = "bill",
        .requiredTeenLineage = ProgramTeenLineage::TypeB, .targetDisciplineBars = 4,
        .minimumAge = 10, .requiresTeenStartedWithNoDiscipline = true},
}};

constexpr ProgramDefinition Definition{
    .id = "international-p1-1997",
    .displayName = "International P1 (1997)",
    .display = DisplayDefinition{
        .checkerboardBackground = true,
        .iconSelectionTimeoutSeconds = 10.0F,
        .menuTimeoutSeconds = 10.0F},
    .lifecycle = LifecycleDefinition{
        .hatchDelayMinutes = 5,
        .attentionWindowMinutes = 15,
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
        .rounds = 5,
        .winsNeededForHappiness = 3,
        .happinessHeartDeltaOnWin = 1,
        .weightDeltaOnCompletion = -1,
    },
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
