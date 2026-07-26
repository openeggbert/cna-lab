#include "CnaTamagotchi/Domain/P1Program.hpp"

#include <array>

namespace CnaTamagotchi::Domain::Programs {
namespace {

constexpr std::array<FoodDefinition, 2> Foods{{
    FoodDefinition{"bread", "BREAD", 1, 0, 1},
    FoodDefinition{"candy", "CANDY", 0, 1, 2},
}};

constexpr std::array<CreatureDefinition, 11> Creatures{{
    CreatureDefinition{"babytchi", "Babytchi", ProgramStage::Baby},
    CreatureDefinition{"marutchi", "Marutchi", ProgramStage::Child},
    CreatureDefinition{"tamatchi", "Tamatchi", ProgramStage::Teen},
    CreatureDefinition{"kuchitamatchi", "Kuchitamatchi", ProgramStage::Teen},
    CreatureDefinition{"mametchi", "Mametchi", ProgramStage::Adult},
    CreatureDefinition{"ginjirotchi", "Ginjirotchi", ProgramStage::Adult},
    CreatureDefinition{"maskutchi", "Maskutchi", ProgramStage::Adult},
    CreatureDefinition{"kuchipatchi", "Kuchipatchi", ProgramStage::Adult},
    CreatureDefinition{"nyorotchi", "Nyorotchi", ProgramStage::Adult},
    CreatureDefinition{"tarakotchi", "Tarakotchi", ProgramStage::Adult},
    CreatureDefinition{"bill", "Bill", ProgramStage::Adult, true},
}};

constexpr ProgramDefinition Definition{
    .id = "international-p1-1997",
    .displayName = "International P1 (1997)",
    .display = DisplayDefinition{.checkerboardBackground = true},
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
};

} // namespace

const ProgramDefinition& internationalP1() noexcept
{
    return Definition;
}

} // namespace CnaTamagotchi::Domain::Programs
