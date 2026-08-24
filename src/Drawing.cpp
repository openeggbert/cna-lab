#include "explore2d/Drawing.hpp"

#include <utility>

namespace explore2d {

Drawing& Drawing::origin(const Vec2 value) noexcept {
    origin_ = value;
    return *this;
}

Vec2 Drawing::shifted(const Vec2 point) const noexcept {
    return {point.x + origin_.x, point.y + origin_.y};
}

Rect Drawing::shifted(const Rect rect) const noexcept {
    return {rect.x + origin_.x, rect.y + origin_.y, rect.width, rect.height};
}

Drawing& Drawing::pixel(const Vec2 at, const PaletteColor color) {
    visuals_.emplace_back(PixelVisual{shifted(at), color});
    return *this;
}

Drawing& Drawing::rectangle(const Rect rect, const PaletteColor color, const bool filled) {
    visuals_.emplace_back(RectVisual{shifted(rect), color, filled});
    return *this;
}

Drawing& Drawing::line(const Vec2 from, const Vec2 to, const PaletteColor color) {
    visuals_.emplace_back(LineVisual{shifted(from), shifted(to), color});
    return *this;
}

Drawing& Drawing::circle(const Vec2 center, const float radius, const PaletteColor color, const bool filled) {
    visuals_.emplace_back(CircleVisual{shifted(center), radius, color, filled});
    return *this;
}

Drawing& Drawing::ellipse(const Vec2 center, const Vec2 radii, const PaletteColor color, const bool filled) {
    visuals_.emplace_back(EllipseVisual{shifted(center), radii, color, filled});
    return *this;
}

Drawing& Drawing::arc(
    const Vec2 center,
    const Vec2 radii,
    const float startRadians,
    const float endRadians,
    const PaletteColor color)
{
    visuals_.emplace_back(ArcVisual{shifted(center), radii, startRadians, endRadians, color});
    return *this;
}

Drawing& Drawing::polyline(std::vector<Vec2> points, const PaletteColor color, const bool closed) {
    for (Vec2& point : points) point = shifted(point);
    visuals_.emplace_back(PolylineVisual{std::move(points), color, closed});
    return *this;
}

Drawing& Drawing::polygon(std::vector<Vec2> points, const PaletteColor color, const bool filled) {
    for (Vec2& point : points) point = shifted(point);
    visuals_.emplace_back(PolygonVisual{std::move(points), color, filled});
    return *this;
}

Drawing& Drawing::paint(const Vec2 at, const PaletteColor fill, const PaletteColor boundary) {
    visuals_.emplace_back(PaintVisual{shifted(at), fill, boundary});
    return *this;
}

Drawing& Drawing::text(Vec2 at, LocalizedText value, const PaletteColor color, const int scale) {
    visuals_.emplace_back(TextVisual{shifted(at), std::move(value), color, scale});
    return *this;
}

Drawing& Drawing::image(
    const Vec2 at,
    IndexedImage value,
    const RasterOperation operation,
    const PaletteColor transparentColor)
{
    visuals_.emplace_back(ImageVisual{shifted(at), std::move(value), operation, transparentColor});
    return *this;
}

Drawing& Drawing::add(Visual visual) {
    visuals_.push_back(std::move(visual));
    return *this;
}

Drawing& Drawing::appendTo(std::vector<Visual>& destination) {
    destination.insert(destination.end(), visuals_.begin(), visuals_.end());
    return *this;
}

std::vector<Visual> Drawing::release() && noexcept {
    return std::move(visuals_);
}

} // namespace explore2d
