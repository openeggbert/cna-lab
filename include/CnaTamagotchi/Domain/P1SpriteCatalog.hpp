#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace CnaTamagotchi::Domain {

// A P1 home phase is a hand-written one-bit drawing with an explicit LCD
// origin. Most characters currently use the classic 16×10 cell at (8, 3), but
// a reference phase may use its true observed height and origin. Keeping the
// geometry with the drawing avoids faking movement by translating one
// contemporary sprite.
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
    static constexpr std::size_t MaximumIdleFrameCount = 36;
    static constexpr float DefaultIdleFrameSeconds = 0.42F;

    // Frame count and timing belong to the observed sequence, not to the
    // renderer. Sequences range from the egg's two silhouettes to longer
    // moving character traces, and must wrap at their own active count.
    float idleFrameSeconds{DefaultIdleFrameSeconds};
    std::size_t idleFrameCount{1U};
    std::array<P1SpriteFrame, MaximumIdleFrameCount> idleFrames;

    [[nodiscard]] constexpr std::span<const P1SpriteFrame> visibleIdleFrames() const noexcept
    {
        return std::span<const P1SpriteFrame>(idleFrames).first(idleFrameCount);
    }

    [[nodiscard]] const P1SpriteFrame& idleFrame(const std::size_t index) const noexcept
    {
        return idleFrames[index % idleFrameCount];
    }
};

class P1SpriteCatalog final {
public:
    [[nodiscard]] static const P1Sprite& spriteForCharacter(std::string_view characterId) noexcept;
};

} // namespace CnaTamagotchi::Domain
