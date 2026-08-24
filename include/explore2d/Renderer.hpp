#pragma once

#include "explore2d/Canvas.hpp"
#include "explore2d/Session.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace explore2d {

struct RendererTheme final {
    PaletteColor frame{PaletteColor::brightYellow};
    PaletteColor panel{PaletteColor::black};
    PaletteColor panelPattern{PaletteColor::red};
    PaletteColor text{PaletteColor::white};
    PaletteColor dimText{PaletteColor::lightGray};
    PaletteColor accent{PaletteColor::brightCyan};
    PaletteColor selected{PaletteColor::brightMagenta};
    PaletteColor danger{PaletteColor::brightRed};
    PaletteColor playerSkin{PaletteColor::brightYellow};
    PaletteColor playerShirt{PaletteColor::brightRed};
    PaletteColor playerPants{PaletteColor::brightBlue};
};

class AdventureRenderer final {
public:
    AdventureRenderer(const WorldDefinition& world, SessionConfig config = {}, RendererTheme theme = {});

    void render(const AdventureSession& session);
    void renderTitle(std::size_t selectedItem = 0, std::string_view language = {});
    void renderPause(const AdventureSession& session, std::size_t selectedItem = 0);
    void renderSettings(std::size_t selectedItem, std::string_view language,
        const AdventureSession* background = nullptr);
    void renderHelp(const AdventureSession& session);
    [[nodiscard]] const Canvas& canvas() const noexcept { return canvas_; }

private:
    const WorldDefinition& world_;
    SessionConfig config_;
    RendererTheme theme_;
    Canvas canvas_;
    std::string language_;

    [[nodiscard]] std::string_view localize(const LocalizedText& text) const noexcept;
    void drawVisual(const Visual& visual, Vec2 offset = {});
    void drawFrame();
    void drawLogo();
    void drawWorld(const AdventureSession& session);
    void drawAnimations(const AdventureSession& session);
    void drawPlayer(const AdventureSession& session);
    void drawHud(const AdventureSession& session);
    void drawChoice(const AdventureSession& session, std::string_view title);
    void drawMap(const AdventureSession& session);
    void drawMessage(const AdventureSession& session);
    void drawTerminal(const AdventureSession& session);
    void drawOverlayMenu(std::string_view heading, const std::vector<std::string>& labels,
        std::size_t selectedItem, std::string_view help);
};

} // namespace explore2d
