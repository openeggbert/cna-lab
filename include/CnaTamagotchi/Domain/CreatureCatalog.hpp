#pragma once

#include "CnaTamagotchi/Domain/PetState.hpp"

#include <array>
#include <string_view>

namespace CnaTamagotchi::Domain {

// Every form in the two original creature lines. Egg and Farewell are display
// states; the remaining entries are living creature forms.
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
    Budbit,
    Fernkin,
    Lilyloop,
    Thornhop,
    Reedhare,
    Cloverowl,
    Bloomtail,
    Sedgehog,
    Nectarmoth,
    Rootslug,
    Starbloom,
    Farewell,
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
