#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace CnaTamagotchi::Domain {

// These types describe a virtual-pet programme. The simulator consumes this
// data rather than branching on a P1/P2 enum, so a later P2 package can reuse
// the same state machine, persistence boundary, renderer, and input adapter.
enum class ProgramStage : std::uint8_t {
    Egg,
    Baby,
    Child,
    Teen,
    Adult,
    End,
};

enum class ProgramGameKind : std::uint8_t {
    Character,
    Number,
};

enum class ProgramEndScreen : std::uint8_t {
    AngelStars,
    Ufo,
};

// P1 records a hidden A/B lineage when the child becomes a teen.  This is a
// programme fact, not a UI species choice: later adult rules may constrain the
// lineage in addition to accumulated care and the visible discipline meter.
enum class ProgramTeenLineage : std::uint8_t {
    None,
    TypeA,
    TypeB,
};

struct CreatureDefinition final {
    std::string_view id;
    std::string_view displayName;
    ProgramStage stage;
    bool hidden{false};
    int minimumWeight{0};
    int maximumWeight{99};
    int sleepStartMinute{-1};
    int wakeMinute{-1};
    int hungerHeartLossMinutes{-1};
    int happinessHeartLossMinutes{-1};
    // Number of real hunger/happiness heart decrements between false
    // Discipline calls. A negative value means that this form never makes
    // them; the run-time quota still stops calls once its visible meter could
    // have been filled.
    int disciplineCallAfterNeedDecrements{-1};
    // Character-game chance expressed exactly as a fraction so the program
    // data does not depend on a host floating-point random implementation.
    int characterGameWinNumerator{1};
    int characterGameWinDenominator{2};
};

struct FoodDefinition final {
    std::string_view id;
    std::string_view lcdLabel;
    int hungerHeartDelta{0};
    int happinessHeartDelta{0};
    int weightDelta{0};
};

struct GameDefinition final {
    ProgramGameKind kind;
    int rounds{0};
    int winsNeededForHappiness{0};
    int happinessHeartDeltaOnWin{0};
    int weightDeltaOnCompletion{0};
};

struct DisplayDefinition final {
    bool checkerboardBackground{false};
    int logicalWidth{32};
    int logicalHeight{16};
    float iconSelectionTimeoutSeconds{0.0F};
};

struct LifecycleDefinition final {
    int hatchDelayMinutes{0};
    int attentionWindowMinutes{15};
    int babyToChildMinutes{0};
    int childToTeenMinutes{0};
    int teenToAdultMinutes{0};
    int teenAge{0};
    int adultAge{0};
    int babyNapStartMinute{-1};
    int babyNapDurationMinutes{0};
    int babyIllnessMinute{-1};
    int babyIllnessMedicineDoses{0};
    int babyFirstWasteMinute{-1};
    int babySecondWasteMinute{-1};
};

// A target rule is data, rather than a P1-specific if/else chain. Negative
// maximum values are unbounded. The same schema lets another first-generation
// programme provide an independent evolution chart without copying the engine.
struct EvolutionRule final {
    std::string_view sourceCharacterId;
    std::string_view targetCharacterId;
    int minimumCareMistakes{0};
    int maximumCareMistakes{-1};
    int minimumDisciplineBars{0};
    int maximumDisciplineBars{-1};
    // Kept as an independent criterion so a later programme whose documented
    // chart uses missed discipline calls can still use the shared resolver.
    // International P1 leaves this range unconstrained and uses its four
    // visible discipline bars instead.
    int minimumDisciplineMistakes{0};
    int maximumDisciplineMistakes{-1};
    ProgramTeenLineage requiredTeenLineage{ProgramTeenLineage::None};
    // A programme can initialise the visible four-segment discipline meter
    // when it changes form. -1 preserves the prior value; 0..4 replaces it.
    // This keeps form-specific display state in the programme data instead
    // of embedding a P1 exception in the shared evolution engine.
    int targetDisciplineBars{-1};
    // Most rules are stage transitions; a programme can also define a later
    // form change that becomes eligible at a displayed age.
    int minimumAge{0};
    bool requiresTeenStartedWithNoDiscipline{false};
};

struct ProgramDefinition final {
    std::string_view id;
    std::string_view displayName;
    DisplayDefinition display;
    LifecycleDefinition lifecycle;
    GameDefinition game;
    ProgramEndScreen endScreen;
    std::span<const FoodDefinition> food;
    std::span<const CreatureDefinition> creatures;
    std::span<const EvolutionRule> evolutionRules;
};

} // namespace CnaTamagotchi::Domain
