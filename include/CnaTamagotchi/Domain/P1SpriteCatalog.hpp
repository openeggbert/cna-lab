#pragma once

#include <array>
#include <string_view>

namespace CnaTamagotchi::Domain {

// One 16×8 one-bit character cell for the 32×16 P1 game area. This uses the
// original display's wide, low sprite proportion instead of stretching a pet
// into a modern portrait. Frames remain separate from programme rules so a
// later animation catalogue can replace them without changing the simulator.
struct P1Sprite final {
    std::array<std::string_view, 8> rows;
};

class P1SpriteCatalog final {
public:
    [[nodiscard]] static const P1Sprite& spriteForCharacter(std::string_view characterId) noexcept;
};

} // namespace CnaTamagotchi::Domain
