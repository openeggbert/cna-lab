#pragma once

#include "CnaTamagotchi/Presentation/DeviceShell.hpp"

#include <array>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace CnaTamagotchi::Application {

struct ShellPoint final {
    int x;
    int y;
};

struct DeviceShellGeometry final {
    static constexpr int CentreX = 270;
    static constexpr int CentreY = 348;
    static constexpr int ResetX = 408;
    static constexpr int ResetY = 542;
    static constexpr int ResetRadius = 10;
    static constexpr int ButtonHitRadius = 29;
    static constexpr std::array<ShellPoint, 3> Buttons{{
        {202, 555},
        {270, 567},
        {338, 555},
    }};
};

// Reusable CNA presentation renderer for the physical device. It draws no LCD
// pixels and owns no simulation state; the application can therefore switch a
// shell without changing the P1 framebuffer or consuming an A/B/C action.
class DeviceShellRenderer final {
  public:
    static void drawBody(Microsoft::Xna::Framework::Graphics::SpriteBatch& spriteBatch,
                         Microsoft::Xna::Framework::Graphics::Texture2D& pixelTexture,
                         const Presentation::DeviceShellStyle& style,
                         Microsoft::Xna::Framework::Color background);

    static void drawControls(Microsoft::Xna::Framework::Graphics::SpriteBatch& spriteBatch,
                             Microsoft::Xna::Framework::Graphics::Texture2D& pixelTexture,
                             const Presentation::DeviceShellStyle& style);
};

} // namespace CnaTamagotchi::Application
