#pragma once

#include <array>
#include <string_view>

namespace CnaTamagotchi::Domain {

// One 10×10 one-bit character cell for the 32×16 P1 game area. These are
// deliberately separate from programme rules so later captured animation
// frames can replace a sprite without changing the simulator.
struct P1Sprite final {
    std::array<std::string_view, 10> rows;
};

class P1SpriteCatalog final {
public:
    [[nodiscard]] static const P1Sprite& spriteForCharacter(std::string_view characterId) noexcept;
};

} // namespace CnaTamagotchi::Domain
