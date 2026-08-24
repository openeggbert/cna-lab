#include "explore2d/Renderer.hpp"

#include <algorithm>
#include <string>
#include <type_traits>

namespace explore2d {

AdventureRenderer::AdventureRenderer(const WorldDefinition& world, SessionConfig config, RendererTheme theme)
    : world_{world}, config_{config}, theme_{theme}, canvas_{config.logicalWidth, config.logicalHeight}
{
}

void AdventureRenderer::drawVisual(const Visual& visual) {
    std::visit([this](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, RectVisual>) {
            if (value.filled) canvas_.fillRect(value.rect, value.color);
            else canvas_.strokeRect(value.rect, value.color);
        } else if constexpr (std::is_same_v<T, LineVisual>) {
            canvas_.line(value.from, value.to, value.color);
        } else if constexpr (std::is_same_v<T, TextVisual>) {
            canvas_.text(static_cast<int>(value.at.x), static_cast<int>(value.at.y), value.text, value.color, value.scale);
        }
    }, visual);
}

void AdventureRenderer::drawWorld(const AdventureSession& session) {
    const RoomDefinition& room = session.currentRoom();
    canvas_.fillRect(config_.worldViewport, room.background);
    for (const Visual& visual : room.decorations) drawVisual(visual);
    for (const HotspotDefinition& hotspot : room.hotspots) {
        if (!session.hotspotVisible(hotspot)) continue;
        for (const Visual& visual : hotspot.visuals) drawVisual(visual);
    }
    canvas_.strokeRect(config_.worldViewport, theme_.frame);
}

void AdventureRenderer::drawPlayer(const AdventureSession& session) {
    const PlayerState& p = session.player();
    const Rect body{p.position.x + 3.0F, p.position.y + 8.0F, config_.playerSize.x - 6.0F, config_.playerSize.y - 8.0F};
    canvas_.fillRect(body, theme_.player);
    canvas_.fillRect({p.position.x + 4.0F, p.position.y + 1.0F, config_.playerSize.x - 8.0F, 8.0F}, theme_.playerAccent);
    const float dir = p.facing == Facing::right ? 1.0F : -1.0F;
    const float faceX = p.facing == Facing::right ? p.position.x + config_.playerSize.x - 2.0F : p.position.x + 1.0F;
    canvas_.line({faceX, p.position.y + 12.0F}, {faceX + dir * 5.0F, p.position.y + 12.0F}, theme_.accent);
}

void AdventureRenderer::drawHud(const AdventureSession& session) {
    const int worldW = static_cast<int>(config_.worldViewport.width);
    const int worldH = static_cast<int>(config_.worldViewport.height);
    const int panelX = worldW;
    const int panelW = config_.logicalWidth - panelX;
    canvas_.fillRect({static_cast<float>(panelX), 0.0F, static_cast<float>(panelW), static_cast<float>(worldH)}, theme_.panel);
    canvas_.fillRect({0.0F, static_cast<float>(worldH), static_cast<float>(config_.logicalWidth), static_cast<float>(config_.logicalHeight - worldH)}, theme_.panelAlt);
    canvas_.strokeRect({static_cast<float>(panelX), 0.0F, static_cast<float>(panelW), static_cast<float>(worldH)}, theme_.frame);
    canvas_.strokeRect({0.0F, static_cast<float>(worldH), static_cast<float>(config_.logicalWidth), static_cast<float>(config_.logicalHeight - worldH)}, theme_.frame);

    canvas_.text(panelX + 9, 10, "EXPLORE2D", theme_.accent, 1);
    canvas_.text(panelX + 9, 27, "INVENTORY", theme_.text, 1);
    int y = 45;
    if (session.inventory().empty()) {
        canvas_.text(panelX + 9, y, "(EMPTY)", theme_.dimText, 1);
    } else {
        for (const std::string& itemId : session.inventory()) {
            if (const ItemDefinition* item = world_.item(itemId); item != nullptr) {
                canvas_.text(panelX + 9, y, item->label, theme_.text, 1);
                y += 12;
                if (y > worldH - 24) break;
            }
        }
    }

    canvas_.text(9, worldH + 10, session.currentRoom().label, theme_.text, 1);
    if (const HotspotDefinition* nearby = session.nearbyHotspot(); nearby != nullptr) {
        canvas_.text(9, worldH + 25, "NEAR: " + nearby->label, theme_.accent, 1);
    } else {
        canvas_.text(9, worldH + 25, "NEAR: -", theme_.dimText, 1);
    }

    constexpr Verb verbs[] = {Verb::use, Verb::examine, Verb::take};
    int vx = 9;
    for (const Verb verb : verbs) {
        const bool active = session.selectedVerb() == verb;
        const std::string label = std::string{active ? ">" : " "} + std::string{verbName(verb)};
        canvas_.text(vx, worldH + 45, label, active ? theme_.accent : theme_.text, 1);
        vx += 112;
    }
    canvas_.text(365, worldH + 45, "M MAP  S SAVE  L LOAD  Q QUIT", theme_.dimText, 1);
}

void AdventureRenderer::drawChoice(const AdventureSession& session, const std::string_view title) {
    const int width = 340;
    const int height = std::min(210, 52 + static_cast<int>(session.choices().size()) * 17);
    const int x = (static_cast<int>(config_.worldViewport.width) - width) / 2;
    const int y = (static_cast<int>(config_.worldViewport.height) - height) / 2;
    canvas_.fillRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)}, theme_.panel);
    canvas_.strokeRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)}, theme_.frame);
    canvas_.text(x + 12, y + 12, title, theme_.accent, 1);
    int rowY = y + 34;
    for (std::size_t i = 0; i < session.choices().size() && rowY < y + height - 12; ++i) {
        const bool selected = i == session.selectionIndex();
        canvas_.text(x + 12, rowY, (selected ? "> " : "  ") + session.choices()[i].label,
            selected ? theme_.accent : theme_.text, 1);
        rowY += 17;
    }
}

void AdventureRenderer::drawMessage(const AdventureSession& session) {
    if (!session.activeMessage().has_value()) return;
    const Message& message = *session.activeMessage();
    const int x = 42;
    const int y = 82;
    const int width = 428;
    const int height = 126;
    canvas_.fillRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)}, theme_.panel);
    canvas_.strokeRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)},
        message.style == MessageStyle::warning ? theme_.danger : theme_.accent);
    std::string heading = "OBSERVATION";
    if (message.style == MessageStyle::speech) heading = "DIALOGUE";
    else if (message.style == MessageStyle::system) heading = "SYSTEM";
    else if (message.style == MessageStyle::warning) heading = "WARNING";
    canvas_.text(x + 12, y + 12, heading, theme_.accent, 1);
    canvas_.wrappedText(x + 12, y + 34, width - 24, message.text, theme_.text, 1, 3);
    canvas_.text(x + 12, y + height - 17, "ENTER / ESC", theme_.dimText, 1);
}

void AdventureRenderer::drawTerminal(const AdventureSession& session) {
    const bool won = session.mode() == SessionMode::won;
    const int x = 58;
    const int y = 76;
    const int width = 396;
    const int height = 140;
    canvas_.fillRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)}, theme_.panel);
    canvas_.strokeRect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)}, won ? theme_.accent : theme_.danger);
    canvas_.text(x + 14, y + 14, won ? "MISSION COMPLETE" : "MISSION FAILED", won ? theme_.accent : theme_.danger, 2);
    canvas_.wrappedText(x + 14, y + 49, width - 28, session.terminalMessage(), theme_.text, 1, 3);
    canvas_.text(x + 14, y + height - 18, "ENTER TO RESTART", theme_.dimText, 1);
}

void AdventureRenderer::render(const AdventureSession& session) {
    canvas_.clear(theme_.panel);
    drawWorld(session);
    drawPlayer(session);
    drawHud(session);
    switch (session.mode()) {
    case SessionMode::choice:
        drawChoice(session, session.selectedVerb() == Verb::use ? "CHOOSE ITEM" : "EXAMINE");
        break;
    case SessionMode::map:
        drawChoice(session, "TRAVEL MAP");
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
