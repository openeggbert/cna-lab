#pragma once

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

enum class Direction : std::uint8_t { left, right, up, down };
enum class Facing : std::uint8_t { left, right };
enum class Verb : std::uint8_t { use, examine, take, context };
enum class HotspotKind : std::uint8_t { scenery, item, character, mechanism, hazard };
enum class SessionMode : std::uint8_t { world, choice, map, message, dead, won };
enum class MessageStyle : std::uint8_t { inspect, speech, system, warning };

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
    killPlayer,
    winGame,
};

struct Mutation final {
    MutationType type{MutationType::setFlag};
    std::string key;
    int value{};

    [[nodiscard]] static Mutation setFlag(std::string key);
    [[nodiscard]] static Mutation clearFlag(std::string key);
    [[nodiscard]] static Mutation addItem(std::string item);
    [[nodiscard]] static Mutation removeItem(std::string item);
    [[nodiscard]] static Mutation setCounter(std::string key, int value);
    [[nodiscard]] static Mutation addCounter(std::string key, int delta);
    [[nodiscard]] static Mutation unlockTravel(std::string room);
    [[nodiscard]] static Mutation moveTo(std::string room);
    [[nodiscard]] static Mutation kill(std::string message);
    [[nodiscard]] static Mutation win(std::string message);
};

struct RectVisual final { Rect rect; Rgba color; bool filled{true}; };
struct LineVisual final { Vec2 from; Vec2 to; Rgba color; };
struct TextVisual final { Vec2 at; std::string text; Rgba color; int scale{1}; };
using Visual = std::variant<RectVisual, LineVisual, TextVisual>;

struct Message final {
    std::string text;
    MessageStyle style{MessageStyle::inspect};
};

struct PlayerState final {
    Vec2 position{};
    Facing facing{Facing::right};
    float verticalVelocity{};
    bool grounded{true};
};

struct SessionConfig final {
    int logicalWidth{640};
    int logicalHeight{360};
    Rect worldViewport{0.0F, 0.0F, 512.0F, 288.0F};
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
