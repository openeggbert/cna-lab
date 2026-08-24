#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace CnaTamagotchi::Domain {

// A P1 home phase is a hand-written one-bit drawing with an explicit LCD
// origin. Most characters currently use the classic 16×10 cell at (8, 3), but
// the reference egg expands one row above that cell in one observed phase.
// Keeping the geometry with the drawing avoids faking a movement by translating
// one contemporary sprite, while allowing a verified P1 phase to use its own
// true bounds.
struct P1SpriteFrame final {
    static constexpr std::size_t MaximumRows = 12;

    int originX = 8;
    int originY = 3;
    std::size_t rowCount = 10;
    std::array<std::string_view, MaximumRows> rows;

    [[nodiscard]] constexpr std::span<const std::string_view> visibleRows() const noexcept
    {
        return std::span<const std::string_view>(rows).first(rowCount);
    }
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
