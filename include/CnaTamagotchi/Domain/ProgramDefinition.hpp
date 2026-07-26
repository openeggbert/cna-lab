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

struct CreatureDefinition final {
    std::string_view id;
    std::string_view displayName;
    ProgramStage stage;
    bool hidden{false};
    int minimumWeight{0};
    int sleepStartMinute{-1};
    int wakeMinute{-1};
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
};

struct LifecycleDefinition final {
    int hatchDelayMinutes{0};
    int babyToChildMinutes{0};
    int teenAge{0};
    int adultAge{0};
    int babyNapStartMinute{-1};
    int babyNapDurationMinutes{0};
    int babyIllnessMinute{-1};
    int babyIllnessMedicineDoses{0};
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
};

} // namespace CnaTamagotchi::Domain
