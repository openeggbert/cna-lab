#pragma once

#include "explore2d/Canvas.hpp"
#include "explore2d/Session.hpp"

namespace explore2d {

struct RendererTheme final {
    Rgba frame{210, 218, 220, 255};
    Rgba panel{18, 24, 29, 255};
    Rgba panelAlt{28, 36, 43, 255};
    Rgba text{228, 236, 238, 255};
    Rgba dimText{130, 148, 153, 255};
    Rgba accent{242, 196, 84, 255};
    Rgba danger{231, 92, 84, 255};
    Rgba player{236, 229, 210, 255};
    Rgba playerAccent{74, 154, 171, 255};
};

class AdventureRenderer final {
public:
    AdventureRenderer(const WorldDefinition& world, SessionConfig config = {}, RendererTheme theme = {});

    void render(const AdventureSession& session);
    [[nodiscard]] const Canvas& canvas() const noexcept { return canvas_; }

private:
    const WorldDefinition& world_;
    SessionConfig config_;
    RendererTheme theme_;
    Canvas canvas_;

    void drawVisual(const Visual& visual);
    void drawWorld(const AdventureSession& session);
    void drawPlayer(const AdventureSession& session);
    void drawHud(const AdventureSession& session);
    void drawChoice(const AdventureSession& session, std::string_view title);
    void drawMessage(const AdventureSession& session);
    void drawTerminal(const AdventureSession& session);
};

} // namespace explore2d
