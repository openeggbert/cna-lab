#include "explore2d/Canvas.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <deque>
#include <stdexcept>
#include <string>

namespace explore2d {
namespace {

using Glyph = std::array<std::uint8_t, 7>;

[[nodiscard]] Glyph glyph(const char input) noexcept {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(input)));
    switch (c) {
    case 'A': return {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
    case 'B': return {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
    case 'C': return {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
    case 'D': return {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
    case 'E': return {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
    case 'F': return {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
    case 'G': return {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F};
    case 'H': return {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
    case 'I': return {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
    case 'J': return {0x07,0x02,0x02,0x02,0x12,0x12,0x0C};
    case 'K': return {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
    case 'L': return {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
    case 'M': return {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
    case 'N': return {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
    case 'O': return {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
    case 'P': return {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
    case 'Q': return {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
    case 'R': return {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
    case 'S': return {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
    case 'T': return {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
    case 'U': return {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
    case 'V': return {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
    case 'W': return {0x11,0x11,0x11,0x15,0x15,0x15,0x0A};
    case 'X': return {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
    case 'Y': return {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
    case 'Z': return {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};
    case '0': return {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
    case '1': return {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
    case '2': return {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
    case '3': return {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E};
    case '4': return {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
    case '5': return {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E};
    case '6': return {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E};
    case '7': return {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
    case '8': return {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
    case '9': return {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E};
    case '.': return {0,0,0,0,0,0x0C,0x0C};
    case ',': return {0,0,0,0,0x0C,0x0C,0x08};
    case ':': return {0,0x0C,0x0C,0,0x0C,0x0C,0};
    case ';': return {0,0x0C,0x0C,0,0x0C,0x0C,0x08};
    case '!': return {0x04,0x04,0x04,0x04,0x04,0,0x04};
    case '?': return {0x0E,0x11,0x01,0x02,0x04,0,0x04};
    case '-': return {0,0,0,0x1F,0,0,0};
    case '+': return {0,0x04,0x04,0x1F,0x04,0x04,0};
    case '/': return {0x01,0x02,0x02,0x04,0x08,0x08,0x10};
    case '\\': return {0x10,0x08,0x08,0x04,0x02,0x02,0x01};
    case '[': return {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E};
    case ']': return {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E};
    case '(': return {0x02,0x04,0x08,0x08,0x08,0x04,0x02};
    case ')': return {0x08,0x04,0x02,0x02,0x02,0x04,0x08};
    case '>': return {0x08,0x04,0x02,0x01,0x02,0x04,0x08};
    case '<': return {0x01,0x02,0x04,0x08,0x04,0x02,0x01};
    case '=': return {0,0x1F,0,0x1F,0,0,0};
    case '_': return {0,0,0,0,0,0,0x1F};
    case '"': return {0x0A,0x0A,0x0A,0,0,0,0};
    case '\'': return {0x04,0x04,0x08,0,0,0,0};
    case ' ': return {0,0,0,0,0,0,0};
    default: return {0x1F,0x11,0x02,0x04,0x04,0,0x04};
    }
}

} // namespace

Canvas::Canvas(const int width, const int height)
    : width_{width}, height_{height}
{
    if (width <= 0 || height <= 0) throw std::invalid_argument{"Canvas dimensions must be positive"};
    pixels_.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4U);
    indices_.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), PaletteColor::black);
    resetClip();
    clear(PaletteColor::black);
}

void Canvas::pixel(const int x, const int y, const PaletteColor color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_ ||
        x < static_cast<int>(clip_.left()) || x >= static_cast<int>(clip_.right()) ||
        y < static_cast<int>(clip_.top()) || y >= static_cast<int>(clip_.bottom())) return;
    const auto paletteIndex = static_cast<std::size_t>(y * width_ + x);
    const auto index = paletteIndex * 4U;
    const Rgba rgba = paletteRgba(color);
    indices_[paletteIndex] = color;
    pixels_[index] = rgba.r;
    pixels_[index + 1] = rgba.g;
    pixels_[index + 2] = rgba.b;
    pixels_[index + 3] = rgba.a;
}

void Canvas::clear(const PaletteColor color) {
    const Rect oldClip = clip_;
    resetClip();
    fillRect({0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_)}, color);
    clip_ = oldClip;
}

void Canvas::fillRect(const Rect rect, const PaletteColor color) {
    const int x0 = std::max({0, static_cast<int>(std::floor(rect.x)), static_cast<int>(clip_.left())});
    const int y0 = std::max({0, static_cast<int>(std::floor(rect.y)), static_cast<int>(clip_.top())});
    const int x1 = std::min({width_, static_cast<int>(std::ceil(rect.right())), static_cast<int>(clip_.right())});
    const int y1 = std::min({height_, static_cast<int>(std::ceil(rect.bottom())), static_cast<int>(clip_.bottom())});
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) pixel(x, y, color);
}

void Canvas::strokeRect(const Rect rect, const PaletteColor color) {
    line({rect.x, rect.y}, {rect.x + rect.width - 1.0F, rect.y}, color);
    line({rect.x, rect.y}, {rect.x, rect.y + rect.height - 1.0F}, color);
    line({rect.x + rect.width - 1.0F, rect.y}, {rect.x + rect.width - 1.0F, rect.y + rect.height - 1.0F}, color);
    line({rect.x, rect.y + rect.height - 1.0F}, {rect.x + rect.width - 1.0F, rect.y + rect.height - 1.0F}, color);
}

void Canvas::line(const Vec2 from, const Vec2 to, const PaletteColor color) {
    int x0 = static_cast<int>(std::lround(from.x));
    int y0 = static_cast<int>(std::lround(from.y));
    const int x1 = static_cast<int>(std::lround(to.x));
    const int y1 = static_cast<int>(std::lround(to.y));
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * error;
        if (e2 >= dy) { error += dy; x0 += sx; }
        if (e2 <= dx) { error += dx; y0 += sy; }
    }
}

void Canvas::ellipse(const Vec2 center, const Vec2 radii, const PaletteColor color, const bool filled) {
    const int rx = std::max(1, static_cast<int>(std::lround(std::abs(radii.x))));
    const int ry = std::max(1, static_cast<int>(std::lround(std::abs(radii.y))));
    const int cx = static_cast<int>(std::lround(center.x));
    const int cy = static_cast<int>(std::lround(center.y));
    if (filled) {
        for (int y = -ry; y <= ry; ++y) {
            const float normalized = 1.0F - static_cast<float>(y * y) / static_cast<float>(ry * ry);
            const int extent = static_cast<int>(std::lround(static_cast<float>(rx) * std::sqrt(std::max(0.0F, normalized))));
            line({static_cast<float>(cx - extent), static_cast<float>(cy + y)},
                {static_cast<float>(cx + extent), static_cast<float>(cy + y)}, color);
        }
        return;
    }
    const int steps = std::max(24, static_cast<int>(std::ceil(6.4F * static_cast<float>(std::max(rx, ry)))));
    constexpr float tau = 6.2831853071795864769F;
    Vec2 previous{static_cast<float>(cx + rx), static_cast<float>(cy)};
    for (int step = 1; step <= steps; ++step) {
        const float angle = tau * static_cast<float>(step) / static_cast<float>(steps);
        const Vec2 next{static_cast<float>(cx) + static_cast<float>(rx) * std::cos(angle),
            static_cast<float>(cy) + static_cast<float>(ry) * std::sin(angle)};
        line(previous, next, color);
        previous = next;
    }
}

void Canvas::circle(const Vec2 center, const float radius, const PaletteColor color, const bool filled) {
    ellipse(center, {radius, radius}, color, filled);
}

PaletteColor Canvas::colorAt(const int x, const int y) const noexcept {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return PaletteColor::black;
    return indices_[static_cast<std::size_t>(y * width_ + x)];
}

void Canvas::paint(const Vec2 at, const PaletteColor fill, const PaletteColor boundary) {
    const int startX = static_cast<int>(std::lround(at.x));
    const int startY = static_cast<int>(std::lround(at.y));
    if (startX < static_cast<int>(clip_.left()) || startX >= static_cast<int>(clip_.right()) ||
        startY < static_cast<int>(clip_.top()) || startY >= static_cast<int>(clip_.bottom()) ||
        colorAt(startX, startY) == boundary || colorAt(startX, startY) == fill) return;

    std::deque<std::pair<int, int>> pending;
    pending.emplace_back(startX, startY);
    while (!pending.empty()) {
        const auto [x, y] = pending.front();
        pending.pop_front();
        if (x < static_cast<int>(clip_.left()) || x >= static_cast<int>(clip_.right()) ||
            y < static_cast<int>(clip_.top()) || y >= static_cast<int>(clip_.bottom()) ||
            colorAt(x, y) == boundary || colorAt(x, y) == fill) continue;
        pixel(x, y, fill);
        pending.emplace_back(x - 1, y);
        pending.emplace_back(x + 1, y);
        pending.emplace_back(x, y - 1);
        pending.emplace_back(x, y + 1);
    }
}

void Canvas::setClip(const Rect clip) noexcept {
    const float left = std::clamp(clip.left(), 0.0F, static_cast<float>(width_));
    const float top = std::clamp(clip.top(), 0.0F, static_cast<float>(height_));
    const float right = std::clamp(clip.right(), left, static_cast<float>(width_));
    const float bottom = std::clamp(clip.bottom(), top, static_cast<float>(height_));
    clip_ = {left, top, right - left, bottom - top};
}

void Canvas::resetClip() noexcept {
    clip_ = {0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_)};
}

void Canvas::text(int x, int y, const std::string_view value, const PaletteColor color, const int scaleValue) {
    const int scale = std::max(1, scaleValue);
    const int startX = x;
    for (const char c : value) {
        if (c == '\n') { x = startX; y += 9 * scale; continue; }
        const Glyph g = glyph(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((g[static_cast<std::size_t>(row)] & (1U << (4 - col))) == 0) continue;
                fillRect({static_cast<float>(x + col * scale), static_cast<float>(y + row * scale),
                    static_cast<float>(scale), static_cast<float>(scale)}, color);
            }
        }
        x += 6 * scale;
    }
}

int Canvas::textWidth(const std::string_view value, const int scaleValue) const noexcept {
    const int scale = std::max(1, scaleValue);
    int current = 0;
    int widest = 0;
    for (const char c : value) {
        if (c == '\n') { widest = std::max(widest, current); current = 0; }
        else current += 6 * scale;
    }
    return std::max(widest, current);
}

void Canvas::wrappedText(
    const int x,
    int y,
    const int width,
    const std::string_view value,
    const PaletteColor color,
    const int scale,
    const int lineGap)
{
    std::string lineValue;
    std::string word;
    const int maxChars = std::max(1, width / (6 * std::max(1, scale)));
    auto flushLine = [&]() {
        text(x, y, lineValue, color, scale);
        y += 7 * std::max(1, scale) + lineGap;
        lineValue.clear();
    };
    auto appendWord = [&]() {
        if (word.empty()) return;
        if (lineValue.empty()) lineValue = word;
        else if (static_cast<int>(lineValue.size() + 1 + word.size()) <= maxChars) lineValue += " " + word;
        else { flushLine(); lineValue = word; }
        word.clear();
    };
    for (const char c : value) {
        if (c == '\n') { appendWord(); flushLine(); }
        else if (std::isspace(static_cast<unsigned char>(c))) appendWord();
        else word.push_back(c);
    }
    appendWord();
    if (!lineValue.empty()) flushLine();
}

} // namespace explore2d
