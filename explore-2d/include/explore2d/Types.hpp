#pragma once

#include "explore2d/Localization.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace explore2d {

struct Vec2 final {
    float x{};
    float y{};

    [[nodiscard]] friend constexpr bool operator==(const Vec2&, const Vec2&) noexcept = default;
};

struct Rect final {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] constexpr float left() const noexcept { return x; }
    [[nodiscard]] constexpr float right() const noexcept { return x + width; }
    [[nodiscard]] constexpr float top() const noexcept { return y; }
    [[nodiscard]] constexpr float bottom() const noexcept { return y + height; }

    [[nodiscard]] constexpr bool contains(const Vec2 p) const noexcept {
        return p.x >= left() && p.x <= right() && p.y >= top() && p.y <= bottom();
    }

    [[nodiscard]] constexpr bool intersects(const Rect other) const noexcept {
        return left() < other.right() && right() > other.left() &&
            top() < other.bottom() && bottom() > other.top();
    }

    [[nodiscard]] friend constexpr bool operator==(const Rect&, const Rect&) noexcept = default;
};

struct Rgba final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    [[nodiscard]] friend constexpr bool operator==(const Rgba&, const Rgba&) noexcept = default;
};

// The fixed 16-colour EGA palette used by the original QBasic SCREEN 9 mode.
// Games deliberately choose palette indices instead of arbitrary RGB values.
enum class PaletteColor : std::uint8_t {
    black,
    blue,
    green,
    cyan,
    red,
    magenta,
    brown,
    lightGray,
    darkGray,
    brightBlue,
    brightGreen,
    brightCyan,
    brightRed,
    brightMagenta,
    brightYellow,
    white,
};

[[nodiscard]] constexpr Rgba paletteRgba(const PaletteColor color) noexcept {
    switch (color) {
    case PaletteColor::black: return {0, 0, 0, 255};
    case PaletteColor::blue: return {0, 0, 170, 255};
    case PaletteColor::green: return {0, 170, 0, 255};
    case PaletteColor::cyan: return {0, 170, 170, 255};
    case PaletteColor::red: return {170, 0, 0, 255};
    case PaletteColor::magenta: return {170, 0, 170, 255};
    case PaletteColor::brown: return {170, 85, 0, 255};
    case PaletteColor::lightGray: return {170, 170, 170, 255};
    case PaletteColor::darkGray: return {85, 85, 85, 255};
    case PaletteColor::brightBlue: return {85, 85, 255, 255};
    case PaletteColor::brightGreen: return {85, 255, 85, 255};
    case PaletteColor::brightCyan: return {85, 255, 255, 255};
    case PaletteColor::brightRed: return {255, 85, 85, 255};
    case PaletteColor::brightMagenta: return {255, 85, 255, 255};
    case PaletteColor::brightYellow: return {255, 255, 85, 255};
    case PaletteColor::white: return {255, 255, 255, 255};
    }
    return {0, 0, 0, 255};
}

struct ScreenMetrics final {
    static constexpr int width = 640;
    static constexpr int height = 350;
    static constexpr Vec2 sceneOrigin{8.0F, 8.0F};
    static constexpr Rect worldBounds{0.0F, 0.0F, 492.0F, 262.0F};
    static constexpr Rect sceneRect{8.0F, 8.0F, 492.0F, 262.0F};
};

enum class Direction : std::uint8_t { left, right, up, down };
enum class Facing : std::uint8_t { left, right };
enum class PlayerPose : std::uint8_t { standing, jumping, taking };
enum class Verb : std::uint8_t { use, examine, take, context };
enum class HotspotKind : std::uint8_t { scenery, item, character, mechanism, hazard };
enum class SessionMode : std::uint8_t { world, choice, map, message, dead, won };
enum class MessageStyle : std::uint8_t { inspect, speech, system, warning };
enum class MessageSpeaker : std::uint8_t { automatic, player, target };

enum class ConditionType : std::uint8_t {
    flagSet,
    flagClear,
    hasItem,
    lacksItem,
    counterAtLeast,
    counterEquals,
    roomVisited,
};

struct Condition final {
    ConditionType type{ConditionType::flagSet};
    std::string key;
    int value{};

    [[nodiscard]] static Condition flag(std::string key);
    [[nodiscard]] static Condition notFlag(std::string key);
    [[nodiscard]] static Condition has(std::string item);
    [[nodiscard]] static Condition lacks(std::string item);
    [[nodiscard]] static Condition counterAtLeast(std::string key, int value);
    [[nodiscard]] static Condition counterEquals(std::string key, int value);
    [[nodiscard]] static Condition visited(std::string room);
};

enum class MutationType : std::uint8_t {
    setFlag,
    clearFlag,
    addItem,
    removeItem,
    setCounter,
    addCounter,
    unlockTravel,
    moveToRoom,
    playAnimation,
    killPlayer,
    winGame,
};

struct Mutation final {
    MutationType type{MutationType::setFlag};
    std::string key;
    int value{};
    LocalizedText text;

    [[nodiscard]] static Mutation setFlag(std::string key);
    [[nodiscard]] static Mutation clearFlag(std::string key);
    [[nodiscard]] static Mutation addItem(std::string item);
    [[nodiscard]] static Mutation removeItem(std::string item);
    [[nodiscard]] static Mutation setCounter(std::string key, int value);
    [[nodiscard]] static Mutation addCounter(std::string key, int delta);
    [[nodiscard]] static Mutation unlockTravel(std::string room);
    [[nodiscard]] static Mutation moveTo(std::string room);
    [[nodiscard]] static Mutation playAnimation(std::string animation);
    [[nodiscard]] static Mutation kill(LocalizedText message);
    [[nodiscard]] static Mutation win(LocalizedText message);
};

struct PixelVisual final { Vec2 at; PaletteColor color{PaletteColor::white}; };
struct RectVisual final { Rect rect; PaletteColor color{PaletteColor::white}; bool filled{true}; };
struct LineVisual final { Vec2 from; Vec2 to; PaletteColor color{PaletteColor::white}; };
struct CircleVisual final { Vec2 center; float radius{}; PaletteColor color{PaletteColor::white}; bool filled{}; };
struct EllipseVisual final { Vec2 center; Vec2 radii; PaletteColor color{PaletteColor::white}; bool filled{}; };
struct ArcVisual final {
    Vec2 center;
    Vec2 radii;
    float startRadians{};
    float endRadians{};
    PaletteColor color{PaletteColor::white};
};
struct PolylineVisual final {
    std::vector<Vec2> points;
    PaletteColor color{PaletteColor::white};
    bool closed{};
};
struct PolygonVisual final {
    std::vector<Vec2> points;
    PaletteColor color{PaletteColor::white};
    bool filled{true};
};
struct PaintVisual final {
    Vec2 at;
    PaletteColor fill{PaletteColor::white};
    PaletteColor boundary{PaletteColor::white};
};
struct TextVisual final { Vec2 at; LocalizedText text; PaletteColor color{PaletteColor::white}; int scale{1}; };

enum class RasterOperation : std::uint8_t { copy, preset, bitAnd, bitOr, bitXor, transparent };

struct IndexedImage final {
    int width{};
    int height{};
    std::vector<PaletteColor> pixels;
};

struct ImageVisual final {
    Vec2 at;
    IndexedImage image;
    RasterOperation operation{RasterOperation::copy};
    PaletteColor transparentColor{PaletteColor::black};
};

using Visual = std::variant<PixelVisual, RectVisual, LineVisual, CircleVisual, EllipseVisual,
    ArcVisual, PolylineVisual, PolygonVisual, PaintVisual, TextVisual, ImageVisual>;

struct Message final {
    LocalizedText text;
    MessageStyle style{MessageStyle::inspect};
    MessageSpeaker speaker{MessageSpeaker::automatic};
};

struct PlayerState final {
    Vec2 position{};
    Facing facing{Facing::right};
    PlayerPose pose{PlayerPose::standing};
    float verticalVelocity{};
    bool grounded{true};
};

inline constexpr float qbasicTimerTicksPerSecond = 18.2065F;

struct SessionConfig final {
    Vec2 playerSize{14.0F, 28.0F};
    float walkStep{10.0F};
    float jumpVelocity{-205.0F};
    float gravity{620.0F};
    float terminalVelocity{410.0F};
    bool turnBeforeWalk{true};
};

[[nodiscard]] constexpr std::string_view verbName(const Verb verb) noexcept {
    switch (verb) {
    case Verb::use: return "USE";
    case Verb::examine: return "EXAMINE";
    case Verb::take: return "TAKE";
    case Verb::context: return "ACT";
    }
    return "?";
}

} // namespace explore2d
