#pragma once

#include "explore2d/Types.hpp"

#include <string>
#include <vector>

namespace explore2d {

// Fluent C++ scene builder over the fixed Explore2D/QBasic-like drawing model.
// Coordinates are shifted by origin(), making reusable procedural motifs easy.
class Drawing final {
public:
    Drawing& origin(Vec2 value) noexcept;
    [[nodiscard]] Vec2 origin() const noexcept { return origin_; }

    Drawing& pixel(Vec2 at, PaletteColor color);
    Drawing& rectangle(Rect rect, PaletteColor color, bool filled = true);
    Drawing& line(Vec2 from, Vec2 to, PaletteColor color);
    Drawing& circle(Vec2 center, float radius, PaletteColor color, bool filled = false);
    Drawing& ellipse(Vec2 center, Vec2 radii, PaletteColor color, bool filled = false);
    Drawing& arc(Vec2 center, Vec2 radii, float startRadians, float endRadians, PaletteColor color);
    Drawing& polyline(std::vector<Vec2> points, PaletteColor color, bool closed = false);
    Drawing& polygon(std::vector<Vec2> points, PaletteColor color, bool filled = true);
    Drawing& paint(Vec2 at, PaletteColor fill, PaletteColor boundary);
    Drawing& text(Vec2 at, LocalizedText value, PaletteColor color, int scale = 1);
    Drawing& image(Vec2 at, IndexedImage value, RasterOperation operation = RasterOperation::copy,
        PaletteColor transparentColor = PaletteColor::black);

    Drawing& add(Visual visual);
    Drawing& appendTo(std::vector<Visual>& destination);
    [[nodiscard]] const std::vector<Visual>& visuals() const noexcept { return visuals_; }
    [[nodiscard]] std::vector<Visual> release() && noexcept;

private:
    Vec2 origin_{};
    std::vector<Visual> visuals_;

    [[nodiscard]] Vec2 shifted(Vec2 point) const noexcept;
    [[nodiscard]] Rect shifted(Rect rect) const noexcept;
};

} // namespace explore2d
