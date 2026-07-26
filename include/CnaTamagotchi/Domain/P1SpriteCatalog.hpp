#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace CnaTamagotchi::Domain {

// A home-screen character occupies a 16×10 cell inside the 32×16 P1 LCD.
// A character is never animated by merely moving one static picture around:
// every idle phase has its own one-bit drawing, anchored at the same origin.
// This retains the P1 LCD's wide, low proportion and lets the visual catalogue
// evolve independently from the programme simulation.
struct P1SpriteFrame final {
    std::array<std::string_view, 10> rows;
};

struct P1Sprite final {
    static constexpr std::size_t IdleFrameCount = 3;

    std::array<P1SpriteFrame, IdleFrameCount> idleFrames;

    [[nodiscard]] const P1SpriteFrame& idleFrame(const std::size_t index) const noexcept
    {
        return idleFrames[index % idleFrames.size()];
    }
};

class P1SpriteCatalog final {
public:
    [[nodiscard]] static const P1Sprite& spriteForCharacter(std::string_view characterId) noexcept;
};

} // namespace CnaTamagotchi::Domain
