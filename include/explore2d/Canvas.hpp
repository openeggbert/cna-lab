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

    void clear(Rgba color);
    void pixel(int x, int y, Rgba color);
    void fillRect(Rect rect, Rgba color);
    void strokeRect(Rect rect, Rgba color);
    void line(Vec2 from, Vec2 to, Rgba color);
    void text(int x, int y, std::string_view text, Rgba color, int scale = 1);
    void wrappedText(int x, int y, int width, std::string_view text, Rgba color, int scale = 1, int lineGap = 2);

    [[nodiscard]] int textWidth(std::string_view text, int scale = 1) const noexcept;
    [[nodiscard]] int lineHeight(int scale = 1) const noexcept { return 7 * scale; }

private:
    int width_{};
    int height_{};
    std::vector<std::uint8_t> pixels_;
};

} // namespace explore2d
