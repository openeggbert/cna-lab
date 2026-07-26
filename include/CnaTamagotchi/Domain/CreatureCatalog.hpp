#pragma once

#include "CnaTamagotchi/Domain/PetState.hpp"

#include <array>
#include <string_view>

namespace CnaTamagotchi::Domain {

// Every form in the first original creature line. Egg is a display state;
// all remaining entries are living creature forms.
enum class CreatureForm {
    Egg,
    Pipple,
    Sproutlet,
    Flitwing,
    Tumblepuff,
    Skywhistle,
    Mossmuzzle,
    Ripplefin,
    Pebbleback,
    Bramblepaw,
    Duskroot,
    Moonmote,
};

struct CreatureSprite final {
    std::array<std::string_view, 10> rows;
};

// Resolves an original, care-driven character form and exposes the matching
// 1-bit LCD sprite. It remains independent of CNA and rendering.
class CreatureCatalog final {
public:
    [[nodiscard]] static CreatureForm formFor(const PetState& state) noexcept;
    [[nodiscard]] static const CreatureSprite& spriteFor(CreatureForm form) noexcept;
    [[nodiscard]] static std::string_view nameFor(CreatureForm form) noexcept;

private:
    [[nodiscard]] static int careQuality(const PetState& state) noexcept;
};

} // namespace CnaTamagotchi::Domain
