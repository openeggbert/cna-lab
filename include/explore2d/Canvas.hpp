#pragma once

#include "explore2d/Types.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace explore2d {

class Canvas final {
public:
    Canvas(int width, int height);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return pixels_; }

    void clear(PaletteColor color);
    void pixel(int x, int y, PaletteColor color);
    void fillRect(Rect rect, PaletteColor color);
    void strokeRect(Rect rect, PaletteColor color);
    void line(Vec2 from, Vec2 to, PaletteColor color);
    void circle(Vec2 center, float radius, PaletteColor color, bool filled = false);
    void ellipse(Vec2 center, Vec2 radii, PaletteColor color, bool filled = false);
    void paint(Vec2 at, PaletteColor fill, PaletteColor boundary);
    void text(int x, int y, std::string_view text, PaletteColor color, int scale = 1);
    void wrappedText(int x, int y, int width, std::string_view text, PaletteColor color, int scale = 1, int lineGap = 2);

    void setClip(Rect clip) noexcept;
    void resetClip() noexcept;
    [[nodiscard]] PaletteColor colorAt(int x, int y) const noexcept;

    [[nodiscard]] int textWidth(std::string_view text, int scale = 1) const noexcept;
    [[nodiscard]] int lineHeight(int scale = 1) const noexcept { return 7 * scale; }

private:
    int width_{};
    int height_{};
    std::vector<std::uint8_t> pixels_;
    std::vector<PaletteColor> indices_;
    Rect clip_{};
};

} // namespace explore2d
