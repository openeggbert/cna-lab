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

[[nodiscard]] std::string utf8Prefix(const std::string_view value, const std::size_t maxCharacters) {
    std::size_t offset = 0;
    std::size_t characters = 0;
    while (offset < value.size() && characters < maxCharacters) {
        const auto first = static_cast<unsigned char>(value[offset]);
        std::size_t bytes = 1;
        if ((first & 0xE0U) == 0xC0U) bytes = 2;
        else if ((first & 0xF0U) == 0xE0U) bytes = 3;
        else if ((first & 0xF8U) == 0xF0U) bytes = 4;
        if (offset + bytes > value.size()) bytes = 1;
        offset += bytes;
        ++characters;
    }
    return std::string{value.substr(0, offset)};
}

[[nodiscard]] std::vector<std::string> utf8Characters(const std::string_view value) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset < value.size()) {
        const std::size_t firstOffset = offset;
        const auto first = static_cast<unsigned char>(value[offset]);
        std::size_t bytes = 1;
        if ((first & 0xE0U) == 0xC0U) bytes = 2;
        else if ((first & 0xF0U) == 0xE0U) bytes = 3;
        else if ((first & 0xF8U) == 0xF0U) bytes = 4;
        if (offset + bytes > value.size()) bytes = 1;
        offset += bytes;
        result.emplace_back(value.substr(firstOffset, bytes));
    }
    return result;
}

[[nodiscard]] std::vector<std::string> wrapBubbleText(
    const std::string_view value,
    const int maxWidth,
    const Canvas& canvas)
{
    std::istringstream input{std::string{value}};
    std::vector<std::string> lines;
    std::string word;
    std::string current;
    while (input >> word) {
        if (current.empty()) current = word;
        else if (canvas.textWidth(current + " " + word, 1) <= maxWidth) current += " " + word;
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
    : world_{world}, config_{config}, theme_{theme}, canvas_{ScreenMetrics::width, ScreenMetrics::height},
      language_{world.localization.normalized(world.localization.defaultLanguage)}
{
}

std::string_view AdventureRenderer::localize(const LocalizedText& text) const noexcept {
    return text.resolve(language_);
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
            canvas_.text(static_cast<int>(at.x), static_cast<int>(at.y), localize(value.text), value.color, value.scale);
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
    const std::string logoTitle = utf8Prefix(localize(world_.title), 18);
    int x = 565 - canvas_.textWidth(logoTitle, 1) / 2;
    int colorIndex = 0;
    for (const std::string& character : utf8Characters(logoTitle)) {
        const PaletteColor color = colors.empty() ? theme_.accent : colors[static_cast<std::size_t>(colorIndex) % colors.size()];
        canvas_.text(x, 28, character, color, 1);
        x += 6;
        if (character != " ") ++colorIndex;
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
    const InterfaceTextDefinition& ui = world_.presentation.interfaceText;
    canvas_.text(516, 116, localize(world_.presentation.inventoryHeading), theme_.text, 1);
    canvas_.line({516.0F, 127.0F}, {612.0F, 127.0F}, theme_.panelPattern);
    int y = 136;
    if (session.inventory().empty()) {
        canvas_.text(516, y, localize(ui.inventoryEmpty), theme_.dimText, 1);
    } else {
        for (const std::string& itemId : session.inventory()) {
            if (const ItemDefinition* item = world_.item(itemId); item != nullptr) {
                canvas_.text(516, y, utf8Prefix(localize(item->label), 16), theme_.text, 1);
                y += 10;
                if (y > 244) break;
            }
        }
    }
    canvas_.text(44, 283, utf8Prefix(localize(session.currentRoom().label), 34), theme_.text, 1);
    const std::string_view credit = localize(world_.presentation.creditLine);
    const int creditWidth = canvas_.textWidth(credit, 1);
    canvas_.text(std::max(44, 459 - creditWidth), 283, credit, theme_.dimText, 1);

    constexpr std::array<Verb, 3> verbs{Verb::use, Verb::examine, Verb::take};
    constexpr std::array<int, 3> positions{83, 190, 330};
    const std::array<std::string, 3> labels{
        "1 - " + std::string{localize(ui.verbUse)},
        "2 - " + std::string{localize(ui.verbExamine)},
        "3 - " + std::string{localize(ui.verbTake)}};
    for (std::size_t index = 0; index < verbs.size(); ++index) {
        canvas_.text(positions[index], 311, labels[index],
            session.selectedVerb() == verbs[index] ? theme_.selected : theme_.accent, 1);
    }
}

void AdventureRenderer::drawChoice(const AdventureSession& session, const std::string_view title) {
    const InterfaceTextDefinition& ui = world_.presentation.interfaceText;
    canvas_.fillRect({509.0F, 109.0F, 111.0F, 147.0F}, theme_.panel);
    canvas_.text(516, 116, title, theme_.selected, 1);
    canvas_.line({516.0F, 127.0F}, {612.0F, 127.0F}, theme_.panelPattern);
    int rowY = 136;
    for (std::size_t i = 0; i < session.choices().size() && rowY <= 234; ++i) {
        const bool selected = i == session.selectionIndex();
        const std::string label = (selected ? ">" : " ") + utf8Prefix(session.choices()[i].label, 15);
        canvas_.text(512, rowY, label, selected ? theme_.selected : theme_.text, 1);
        rowY += 10;
    }
    canvas_.text(516, 245, localize(ui.confirmCancel), theme_.dimText, 1);
}

void AdventureRenderer::drawMap(const AdventureSession& session) {
    const InterfaceTextDefinition& ui = world_.presentation.interfaceText;
    canvas_.setClip(ScreenMetrics::sceneRect);
    canvas_.fillRect(ScreenMetrics::sceneRect, PaletteColor::blue);
    canvas_.strokeRect({24.0F, 23.0F, 459.0F, 229.0F}, theme_.text);
    const std::string_view mapTitle = localize(ui.travelMap);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(mapTitle, 2)) / 2, 35, mapTitle, theme_.selected, 2);
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
    const std::string_view help = localize(ui.travelHelp);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(help, 1)) / 2, 237, help, theme_.dimText, 1);
    canvas_.resetClip();
}

void AdventureRenderer::drawMessage(const AdventureSession& session) {
    if (!session.activeMessage().has_value()) return;
    const Message& message = *session.activeMessage();
    const std::string_view messageText = localize(message.text);
    const std::vector<std::string> lines = wrapBubbleText(messageText, 204, canvas_);
    int widest = 0;
    for (const std::string& lineValue : lines) widest = std::max(widest, canvas_.textWidth(lineValue, 1));
    const int width = std::clamp(widest + 16, 112, 220);
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
    const std::string_view advance = localize(world_.presentation.interfaceText.messageAdvance);
    canvas_.text(x + width - canvas_.textWidth(advance, 1) - 8, y + height - 9, advance, theme_.dimText, 1);
}

void AdventureRenderer::drawTerminal(const AdventureSession& session) {
    const bool won = session.mode() == SessionMode::won;
    const InterfaceTextDefinition& ui = world_.presentation.interfaceText;
    canvas_.fillRect(ScreenMetrics::sceneRect, PaletteColor::black);
    canvas_.strokeRect({38.0F, 44.0F, 434.0F, 185.0F}, won ? theme_.accent : theme_.danger);
    const std::string_view heading = localize(won ? ui.missionComplete : ui.missionFailed);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(heading, 3)) / 2, 69, heading,
        won ? theme_.accent : theme_.danger, 3);
    canvas_.wrappedText(62, 113, 388, session.terminalMessage(), theme_.text, 1, 4);
    const std::string_view restart = localize(ui.restartPrompt);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(restart, 1)) / 2, 205, restart, theme_.selected, 1);
}

void AdventureRenderer::renderTitle(const std::size_t selectedItem, const std::string_view language) {
    language_ = world_.localization.normalized(language);
    const TitleScreenDefinition& title = world_.presentation.title;
    canvas_.clear(title.background);
    canvas_.strokeRect({5.0F, 5.0F, 630.0F, 340.0F}, title.border);
    canvas_.strokeRect({9.0F, 9.0F, 622.0F, 332.0F}, theme_.panelPattern);
    for (const Visual& visual : title.artwork) drawVisual(visual);

    const std::string_view resolvedTitle = localize(world_.title);
    const int baseTitleWidth = std::max(1, canvas_.textWidth(resolvedTitle, 1));
    const int titleScale = std::clamp(560 / baseTitleWidth, 1, 4);
    const int titleWidth = canvas_.textWidth(resolvedTitle, titleScale);
    int x = (ScreenMetrics::width - titleWidth) / 2;
    int colorIndex = 0;
    for (const std::string& character : utf8Characters(resolvedTitle)) {
        const auto& colors = title.titleColors;
        const PaletteColor color = colors.empty() ? theme_.accent : colors[static_cast<std::size_t>(colorIndex) % colors.size()];
        canvas_.text(x, 26, character, color, titleScale);
        x += 6 * titleScale;
        if (character != " ") ++colorIndex;
    }
    const std::string_view subtitle = localize(title.subtitle);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(subtitle, 1)) / 2, 62, subtitle, theme_.text, 1);

    constexpr int menuX = 218;
    constexpr int menuY = 235;
    constexpr int menuW = 204;
    constexpr int menuH = 84;
    canvas_.fillRect({menuX, menuY, menuW, menuH}, PaletteColor::blue);
    canvas_.strokeRect({menuX, menuY, menuW, menuH}, title.border);
    const std::array<std::string, 4> labels{
        std::string{localize(title.startLabel)},
        std::string{localize(title.loadLabel)},
        std::string{localize(title.settingsLabel)},
        std::string{localize(title.quitLabel)}};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const std::string prefix = index == selectedItem ? "> " : "  ";
        canvas_.text(menuX + 16, menuY + 10 + static_cast<int>(index) * 17,
            prefix + labels[index], index == selectedItem ? theme_.selected : theme_.text, 1);
    }
    const std::string_view byline = localize(title.byline);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(byline, 1)) / 2, 328, byline, theme_.dimText, 1);
}

void AdventureRenderer::render(const AdventureSession& session) {
    language_ = session.language();
    canvas_.clear(theme_.panel);
    drawWorld(session);
    drawPlayer(session);
    drawFrame();
    drawHud(session);
    switch (session.mode()) {
    case SessionMode::choice:
        drawChoice(session, localize(session.selectedVerb() == Verb::use
            ? world_.presentation.interfaceText.useWhat
            : world_.presentation.interfaceText.verbExamine));
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

void AdventureRenderer::drawOverlayMenu(
    const std::string_view heading,
    const std::vector<std::string>& labels,
    const std::size_t selectedItem,
    const std::string_view help)
{
    constexpr int panelX = 154;
    constexpr int panelY = 78;
    constexpr int panelW = 332;
    constexpr int panelH = 194;
    canvas_.fillRect({panelX, panelY, panelW, panelH}, PaletteColor::blue);
    canvas_.strokeRect({panelX, panelY, panelW, panelH}, theme_.frame);
    canvas_.strokeRect({panelX + 4, panelY + 4, panelW - 8, panelH - 8}, theme_.panelPattern);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(heading, 2)) / 2,
        panelY + 20, heading, theme_.selected, 2);
    canvas_.line({panelX + 28.0F, panelY + 48.0F}, {panelX + panelW - 28.0F, panelY + 48.0F}, theme_.accent);
    int y = panelY + 70;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const std::string prefix = index == selectedItem ? "> " : "  ";
        canvas_.text(panelX + 38, y, prefix + labels[index],
            index == selectedItem ? theme_.selected : theme_.text, 1);
        y += 22;
    }
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(help, 1)) / 2,
        panelY + panelH - 22, help, theme_.dimText, 1);
}

void AdventureRenderer::renderPause(const AdventureSession& session, const std::size_t selectedItem) {
    render(session);
    const InterfaceTextDefinition& ui = world_.presentation.interfaceText;
    drawOverlayMenu(localize(ui.paused),
        {std::string{localize(ui.resume)}, std::string{localize(ui.settings)},
            std::string{localize(ui.returnToTitle)}},
        selectedItem, localize(ui.confirmCancel));
}

void AdventureRenderer::renderSettings(
    const std::size_t selectedItem,
    const std::string_view language,
    const AdventureSession* const background)
{
    language_ = world_.localization.normalized(language);
    if (background != nullptr) render(*background);
    else renderTitle(0, language_);
    const InterfaceTextDefinition& ui = world_.presentation.interfaceText;
    const LanguageDefinition* definition = world_.localization.language(language_);
    const std::string languageName = definition == nullptr
        ? std::string{language_}
        : std::string{definition->label.resolve(language_)};
    drawOverlayMenu(localize(ui.settings),
        {std::string{localize(ui.language)} + ": < " + languageName + " >",
            std::string{localize(ui.back)}},
        selectedItem, localize(ui.settingsHelp));
}

void AdventureRenderer::renderHelp(const AdventureSession& session) {
    render(session);
    const InterfaceTextDefinition& ui = world_.presentation.interfaceText;
    const HintDefinition* hint = session.currentHint();
    const std::string_view hintText = hint == nullptr ? localize(ui.noHint) : localize(hint->text);

    constexpr int panelX = 104;
    constexpr int panelY = 78;
    constexpr int panelW = 432;
    constexpr int panelH = 194;
    canvas_.fillRect({panelX, panelY, panelW, panelH}, PaletteColor::blue);
    canvas_.strokeRect({panelX, panelY, panelW, panelH}, theme_.frame);
    canvas_.strokeRect({panelX + 4, panelY + 4, panelW - 8, panelH - 8}, theme_.panelPattern);

    const std::string_view heading = localize(ui.help);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(heading, 2)) / 2,
        panelY + 18, heading, theme_.selected, 2);
    const std::string_view nextStep = localize(ui.nextStep);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(nextStep, 1)) / 2,
        panelY + 48, nextStep, theme_.accent, 1);
    canvas_.line({panelX + 24.0F, panelY + 63.0F},
        {panelX + panelW - 24.0F, panelY + 63.0F}, theme_.accent);
    canvas_.wrappedText(panelX + 26, panelY + 82, panelW - 52,
        hintText, theme_.text, 1, 5);

    const std::string_view close = localize(ui.closeHelp);
    canvas_.text((ScreenMetrics::width - canvas_.textWidth(close, 1)) / 2,
        panelY + panelH - 22, close, theme_.dimText, 1);
}

} // namespace explore2d
