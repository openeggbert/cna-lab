#include "explore2d/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <type_traits>

namespace explore2d {
namespace {

constexpr Rect inventoryPanel{508.0F, 108.0F, 113.0F, 149.0F};
constexpr Rect creditPanel{36.0F, 278.0F, 431.0F, 17.0F};
constexpr Rect actionPanel{70.0F, 306.0F, 364.0F, 17.0F};

[[nodiscard]] Vec2 shifted(const Vec2 point, const Vec2 offset) noexcept {
    return {point.x + offset.x, point.y + offset.y};
}

[[nodiscard]] Rect shifted(const Rect rect, const Vec2 offset) noexcept {
    return {rect.x + offset.x, rect.y + offset.y, rect.width, rect.height};
}

[[nodiscard]] std::vector<std::string> wrapBubbleText(const std::string_view value, const int maxChars) {
    std::istringstream input{std::string{value}};
    std::vector<std::string> lines;
    std::string word;
    std::string current;
    while (input >> word) {
        if (current.empty()) current = word;
        else if (static_cast<int>(current.size() + 1U + word.size()) <= maxChars) current += " " + word;
        else {
            lines.push_back(std::move(current));
            current = std::move(word);
        }
    }
    if (!current.empty()) lines.push_back(std::move(current));
    if (lines.empty()) lines.emplace_back();
    return lines;
}

} // namespace

AdventureRenderer::AdventureRenderer(const WorldDefinition& world, SessionConfig config, RendererTheme theme)
    : world_{world}, config_{config}, theme_{theme}, canvas_{ScreenMetrics::width, ScreenMetrics::height}
{
}

void AdventureRenderer::drawVisual(const Visual& visual, const Vec2 offset) {
    std::visit([this, offset](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, PixelVisual>) {
            const Vec2 at = shifted(value.at, offset);
            canvas_.pixel(static_cast<int>(at.x), static_cast<int>(at.y), value.color);
        } else if constexpr (std::is_same_v<T, RectVisual>) {
            if (value.filled) canvas_.fillRect(shifted(value.rect, offset), value.color);
            else canvas_.strokeRect(shifted(value.rect, offset), value.color);
        } else if constexpr (std::is_same_v<T, LineVisual>) {
            canvas_.line(shifted(value.from, offset), shifted(value.to, offset), value.color);
        } else if constexpr (std::is_same_v<T, CircleVisual>) {
            canvas_.circle(shifted(value.center, offset), value.radius, value.color, value.filled);
        } else if constexpr (std::is_same_v<T, EllipseVisual>) {
            canvas_.ellipse(shifted(value.center, offset), value.radii, value.color, value.filled);
        } else if constexpr (std::is_same_v<T, ArcVisual>) {
            canvas_.arc(shifted(value.center, offset), value.radii,
                value.startRadians, value.endRadians, value.color);
        } else if constexpr (std::is_same_v<T, PolylineVisual>) {
            std::vector<Vec2> points = value.points;
            for (Vec2& point : points) point = shifted(point, offset);
            canvas_.polyline(points, value.color, value.closed);
        } else if constexpr (std::is_same_v<T, PolygonVisual>) {
            std::vector<Vec2> points = value.points;
            for (Vec2& point : points) point = shifted(point, offset);
            canvas_.polygon(points, value.color, value.filled);
        } else if constexpr (std::is_same_v<T, PaintVisual>) {
            canvas_.paint(shifted(value.at, offset), value.fill, value.boundary);
        } else if constexpr (std::is_same_v<T, TextVisual>) {
            const Vec2 at = shifted(value.at, offset);
            canvas_.text(static_cast<int>(at.x), static_cast<int>(at.y), value.text, value.color, value.scale);
        } else if constexpr (std::is_same_v<T, ImageVisual>) {
            canvas_.blit(value.image, shifted(value.at, offset), value.operation, value.transparentColor);
        }
    }, visual);
}

void AdventureRenderer::drawFrame() {
    canvas_.fillRect(inventoryPanel, theme_.panel);
    canvas_.fillRect(creditPanel, theme_.panel);
    canvas_.fillRect(actionPanel, theme_.panel);
    for (int x = 2; x < ScreenMetrics::width - 2; x += 8) {
        canvas_.line({static_cast<float>(x), 334.0F}, {static_cast<float>(x + 3), 347.0F}, theme_.panelPattern);
        canvas_.line({static_cast<float>(x + 3), 347.0F}, {static_cast<float>(x + 7), 334.0F}, theme_.panelPattern);
    }
    for (int y = 2; y < 332; y += 8) {
        canvas_.pixel(2, y, theme_.panelPattern);
        canvas_.pixel(630, y + 3, theme_.panelPattern);
    }
    canvas_.strokeRect({1.0F, 1.0F, 630.0F, 331.0F}, theme_.frame);
    canvas_.strokeRect({7.0F, 7.0F, 495.0F, 265.0F}, theme_.frame);
    canvas_.strokeRect(inventoryPanel, theme_.frame);
    canvas_.strokeRect(creditPanel, theme_.frame);
    canvas_.strokeRect(actionPanel, theme_.frame);
}

void AdventureRenderer::drawLogo() {
    const auto& colors = world_.presentation.title.titleColors;
    const std::string logoTitle = world_.title.substr(0, 18);
    int x = 565 - canvas_.textWidth(logoTitle, 1) / 2;
    int colorIndex = 0;
    for (const char character : logoTitle) {
        const PaletteColor color = colors.empty() ? theme_.accent : colors[static_cast<std::size_t>(colorIndex) % colors.size()];
        canvas_.text(x, 28, std::string(1, character), color, 1);
        x += 6;
        if (character != ' ') ++colorIndex;
    }
    canvas_.line({522.0F, 42.0F}, {607.0F, 42.0F}, theme_.panelPattern);
    canvas_.text(526, 50, "EXPLORE2D", theme_.dimText, 1);
    canvas_.text(521, 65, "ADVENTURE", theme_.dimText, 1);
    canvas_.text(538, 82, "ENGINE", theme_.dimText, 1);
}

void AdventureRenderer::drawWorld(const AdventureSession& session) {
    const RoomDefinition& room = session.currentRoom();
    canvas_.setClip(ScreenMetrics::sceneRect);
    canvas_.fillRect(ScreenMetrics::sceneRect, room.background);
    for (const Visual& visual : room.decorations) drawVisual(visual, ScreenMetrics::sceneOrigin);
    for (const HotspotDefinition& hotspot : room.hotspots) {
        if (!session.hotspotVisible(hotspot)) continue;
        for (const Visual& visual : hotspot.visuals) drawVisual(visual, ScreenMetrics::sceneOrigin);
    }
    drawAnimations(session);
    canvas_.resetClip();
}

void AdventureRenderer::drawAnimations(const AdventureSession& session) {
    for (const SceneAnimationDefinition& animation : session.currentRoom().animations) {
        if (animation.frames.empty() || !session.allConditionsSatisfied(animation.visibleWhen)) continue;
        float elapsed = session.sceneElapsedSeconds();
        if (!animation.autoplay) {
            const std::optional<float> activeElapsed = session.animationElapsed(animation.id);
            if (!activeElapsed.has_value()) continue;
            elapsed = *activeElapsed;
        }
        int totalTicks = 0;
        for (const AnimationFrame& frame : animation.frames) totalTicks += std::max(1, frame.durationTicks);
        float tick = elapsed * qbasicTimerTicksPerSecond;
        if (animation.loop && totalTicks > 0) tick = std::fmod(tick, static_cast<float>(totalTicks));
        else if (tick >= static_cast<float>(totalTicks)) continue;
        const AnimationFrame* selected = &animation.frames.back();
        for (const AnimationFrame& frame : animation.frames) {
            if (tick < static_cast<float>(std::max(1, frame.durationTicks))) {
                selected = &frame;
                break;
            }
            tick -= static_cast<float>(std::max(1, frame.durationTicks));
        }
        for (const Visual& visual : selected->visuals) drawVisual(visual, ScreenMetrics::sceneOrigin);
    }
}

void AdventureRenderer::drawPlayer(const AdventureSession& session) {
    const PlayerState& p = session.player();
    const float x = p.position.x + ScreenMetrics::sceneOrigin.x;
    const float y = p.position.y + ScreenMetrics::sceneOrigin.y;
    canvas_.setClip(ScreenMetrics::sceneRect);
    if (p.pose == PlayerPose::taking) {
        const float direction = p.facing == Facing::right ? 1.0F : -1.0F;
        const float headX = x + 7.0F + direction * 3.0F;
        canvas_.circle({headX, y + 13.0F}, 4.0F, theme_.playerSkin, true);
        canvas_.pixel(static_cast<int>(headX + direction * 2.0F), static_cast<int>(y + 13.0F), PaletteColor::black);
        canvas_.fillRect({x + 3.0F, y + 16.0F, 8.0F, 7.0F}, theme_.playerShirt);
        canvas_.line({x + 7.0F, y + 18.0F}, {x + 7.0F + direction * 8.0F, y + 27.0F}, theme_.playerSkin);
        canvas_.line({x + 4.0F, y + 23.0F}, {x + 1.0F, y + 27.0F}, theme_.playerPants);
        canvas_.line({x + 9.0F, y + 23.0F}, {x + 13.0F, y + 27.0F}, theme_.playerPants);
        canvas_.line({x, y + 27.0F}, {x + 4.0F, y + 27.0F}, PaletteColor::black);
        canvas_.line({x + 10.0F, y + 27.0F}, {x + 14.0F, y + 27.0F}, PaletteColor::black);
        canvas_.resetClip();
        return;
    }
    canvas_.circle({x + 7.0F, y + 5.0F}, 4.0F, theme_.playerSkin, true);
    canvas_.pixel(static_cast<int>(x + (p.facing == Facing::right ? 9.0F : 4.0F)),
        static_cast<int>(y + 4.0F), PaletteColor::black);
    canvas_.fillRect({x + 3.0F, y + 9.0F, 8.0F, 11.0F}, theme_.playerShirt);
    canvas_.line({x + 3.0F, y + 11.0F}, {x, y + 17.0F}, theme_.playerSkin);
    canvas_.line({x + 10.0F, y + 11.0F}, {x + 13.0F, y + 17.0F}, theme_.playerSkin);
    canvas_.fillRect({x + 3.0F, y + 20.0F, 3.0F, 7.0F}, theme_.playerPants);
    canvas_.fillRect({x + 8.0F, y + 20.0F, 3.0F, 7.0F}, theme_.playerPants);
    canvas_.line({x + 2.0F, y + 27.0F}, {x + 6.0F, y + 27.0F}, PaletteColor::black);
    canvas_.line({x + 8.0F, y + 27.0F}, {x + 12.0F, y + 27.0F}, PaletteColor::black);
    canvas_.resetClip();
}

void AdventureRenderer::drawHud(const AdventureSession& session) {
    drawLogo();
    canvas_.text(516, 116, world_.presentation.inventoryHeading, theme_.text, 1);
    canvas_.line({516.0F, 127.0F}, {612.0F, 127.0F}, theme_.panelPattern);
    int y = 136;
    if (session.inventory().empty()) {
        canvas_.text(516, y, "(NOTHING)", theme_.dimText, 1);
    } else {
        for (const std::string& itemId : session.inventory()) {
            if (const ItemDefinition* item = world_.item(itemId); item != nullptr) {
                canvas_.text(516, y, item->label.substr(0, 16), theme_.text, 1);
                y += 10;
                if (y > 244) break;
            }
        }
    }
    canvas_.text(44, 283, session.currentRoom().label.substr(0, 34), theme_.text, 1);
    const int creditWidth = canvas_.textWidth(world_.presentation.creditLine, 1);
    canvas_.text(std::max(44, 459 - creditWidth), 283, world_.presentation.creditLine, theme_.dimText, 1);

    constexpr std::array<Verb, 3> verbs{Verb::use, Verb::examine, Verb::take};
    constexpr std::array<std::string_view, 3> labels{"1 - USE", "2 - EXAMINE", "3 - TAKE"};
    constexpr std::array<int, 3> positions{83, 190, 330};
    for (std::size_t index = 0; index < verbs.size(); ++index) {
        canvas_.text(positions[index], 311, labels[index],
            session.selectedVerb() == verbs[index] ? theme_.selected : theme_.accent, 1);
    }
}

void AdventureRenderer::drawChoice(const AdventureSession& session, const std::string_view title) {
    canvas_.fillRect({509.0F, 109.0F, 111.0F, 147.0F}, theme_.panel);
    canvas_.text(516, 116, title, theme_.selected, 1);
    canvas_.line({516.0F, 127.0F}, {612.0F, 127.0F}, theme_.panelPattern);
    int rowY = 136;
    for (std::size_t i = 0; i < session.choices().size() && rowY <= 234; ++i) {
        const bool selected = i == session.selectionIndex();
        const std::string label = (selected ? ">" : " ") + session.choices()[i].label.substr(0, 15);
        canvas_.text(512, rowY, label, selected ? theme_.selected : theme_.text, 1);
        rowY += 10;
    }
    canvas_.text(516, 245, "ENTER / ESC", theme_.dimText, 1);
}

void AdventureRenderer::drawMap(const AdventureSession& session) {
    canvas_.setClip(ScreenMetrics::sceneRect);
    canvas_.fillRect(ScreenMetrics::sceneRect, PaletteColor::blue);
    canvas_.strokeRect({24.0F, 23.0F, 459.0F, 229.0F}, theme_.text);
    canvas_.text(185, 35, "TRAVEL MAP", theme_.selected, 2);
    canvas_.line({55.0F, 64.0F}, {450.0F, 64.0F}, theme_.accent);
    int y = 86;
    for (std::size_t i = 0; i < session.choices().size() && y < 230; ++i) {
        const bool selected = i == session.selectionIndex();
        canvas_.circle({80.0F, static_cast<float>(y + 3)}, 4.0F,
            selected ? theme_.selected : theme_.accent, true);
        canvas_.line({84.0F, static_cast<float>(y + 3)}, {115.0F, static_cast<float>(y + 3)}, theme_.dimText);
        canvas_.text(122, y, session.choices()[i].label, selected ? theme_.selected : theme_.text, 1);
        y += 20;
    }
    canvas_.text(171, 237, "ARROWS + ENTER   ESC BACK", theme_.dimText, 1);
    canvas_.resetClip();
}

void AdventureRenderer::drawMessage(const AdventureSession& session) {
    if (!session.activeMessage().has_value()) return;
    const Message& message = *session.activeMessage();
    const std::vector<std::string> lines = wrapBubbleText(message.text, 34);
    std::size_t widest = 0;
    for (const std::string& lineValue : lines) widest = std::max(widest, lineValue.size());
    const int width = std::clamp(static_cast<int>(widest) * 6 + 16, 112, 220);
    const int height = static_cast<int>(lines.size()) * 10 + 20;
    const Vec2 localAnchor = session.messageAnchor();
    const Vec2 anchor{localAnchor.x + ScreenMetrics::sceneOrigin.x,
        localAnchor.y + ScreenMetrics::sceneOrigin.y};
    const int minX = static_cast<int>(ScreenMetrics::sceneRect.left()) + 6;
    const int maxX = static_cast<int>(ScreenMetrics::sceneRect.right()) - width - 6;
    const int x = std::clamp(static_cast<int>(std::lround(anchor.x)) - width / 2, minX, maxX);
    const int minY = static_cast<int>(ScreenMetrics::sceneRect.top()) + 6;
    const int maxY = static_cast<int>(ScreenMetrics::sceneRect.bottom()) - height - 6;
    int y = static_cast<int>(std::lround(anchor.y)) - height - 13;
    bool tailBelow = true;
    if (y < minY) {
        y = static_cast<int>(std::lround(anchor.y)) + 13;
        tailBelow = false;
    }
    y = std::clamp(y, minY, std::max(minY, maxY));

    const float tailX = std::clamp(anchor.x, static_cast<float>(x + 12), static_cast<float>(x + width - 12));
    std::vector<Vec2> tail;
    if (tailBelow) {
        tail = {{tailX - 6.0F, static_cast<float>(y + height - 1)},
            {tailX + 6.0F, static_cast<float>(y + height - 1)}, anchor};
    } else {
        tail = {{tailX - 6.0F, static_cast<float>(y)},
            {tailX + 6.0F, static_cast<float>(y)}, anchor};
    }
    canvas_.polygon(tail, PaletteColor::blue, true);
    canvas_.fillRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)},
        PaletteColor::blue);
    canvas_.polyline(tail, message.style == MessageStyle::warning ? theme_.danger : theme_.text, true);
    canvas_.strokeRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)},
        message.style == MessageStyle::warning ? theme_.danger : theme_.text);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        canvas_.text(x + 8, y + 7 + static_cast<int>(index) * 10, lines[index], theme_.text, 1);
    }
    canvas_.text(x + width - 40, y + height - 9, "ENTER", theme_.dimText, 1);
}

void AdventureRenderer::drawTerminal(const AdventureSession& session) {
    const bool won = session.mode() == SessionMode::won;
    canvas_.fillRect(ScreenMetrics::sceneRect, PaletteColor::black);
    canvas_.strokeRect({38.0F, 44.0F, 434.0F, 185.0F}, won ? theme_.accent : theme_.danger);
    canvas_.text(won ? 105 : 117, 69, won ? "MISSION COMPLETE" : "MISSION FAILED",
        won ? theme_.accent : theme_.danger, 3);
    canvas_.wrappedText(62, 113, 388, session.terminalMessage(), theme_.text, 1, 4);
    canvas_.text(174, 205, "ENTER TO RESTART", theme_.selected, 1);
}

void AdventureRenderer::renderTitle(const std::size_t selectedItem) {
    const TitleScreenDefinition& title = world_.presentation.title;
    canvas_.clear(title.background);
    canvas_.strokeRect({5.0F, 5.0F, 630.0F, 340.0F}, title.border);
    canvas_.strokeRect({9.0F, 9.0F, 622.0F, 332.0F}, theme_.panelPattern);
    for (const Visual& visual : title.artwork) drawVisual(visual);

    const int baseTitleWidth = std::max(1, canvas_.textWidth(world_.title, 1));
    const int titleScale = std::clamp(560 / baseTitleWidth, 1, 4);
    const int titleWidth = canvas_.textWidth(world_.title, titleScale);
    int x = (ScreenMetrics::width - titleWidth) / 2;
    int colorIndex = 0;
    for (const char character : world_.title) {
        const auto& colors = title.titleColors;
        const PaletteColor color = colors.empty() ? theme_.accent : colors[static_cast<std::size_t>(colorIndex) % colors.size()];
        canvas_.text(x, 26, std::string(1, character), color, titleScale);
        x += 6 * titleScale;
        if (character != ' ') ++colorIndex;
    }
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(title.subtitle, 1)) / 2, 62, title.subtitle, theme_.text, 1);

    constexpr int menuX = 238;
    constexpr int menuY = 246;
    constexpr int menuW = 164;
    constexpr int menuH = 70;
    canvas_.fillRect({menuX, menuY, menuW, menuH}, PaletteColor::blue);
    canvas_.strokeRect({menuX, menuY, menuW, menuH}, title.border);
    const std::array<std::string, 3> labels{title.startLabel, title.loadLabel, title.quitLabel};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const std::string prefix = index == selectedItem ? "> " : "  ";
        canvas_.text(menuX + 16, menuY + 12 + static_cast<int>(index) * 17,
            prefix + labels[index], index == selectedItem ? theme_.selected : theme_.text, 1);
    }
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(title.byline, 1)) / 2, 328, title.byline, theme_.dimText, 1);
}

void AdventureRenderer::render(const AdventureSession& session) {
    canvas_.clear(theme_.panel);
    drawWorld(session);
    drawPlayer(session);
    drawFrame();
    drawHud(session);
    switch (session.mode()) {
    case SessionMode::choice:
        drawChoice(session, session.selectedVerb() == Verb::use ? "USE WHAT?" : "EXAMINE");
        break;
    case SessionMode::map:
        drawMap(session);
        break;
    case SessionMode::message:
        drawMessage(session);
        break;
    case SessionMode::dead:
    case SessionMode::won:
        drawTerminal(session);
        break;
    case SessionMode::world:
        break;
    }
}

} // namespace explore2d
