#include "TamagotchiCna/Application/DeviceShellRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/Rectangle.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace TamagotchiCna::Application {
namespace {

Color asColor(const Presentation::ShellRgba colour) noexcept
{
    return Color(colour.red, colour.green, colour.blue, colour.alpha);
}

void drawRectangle(SpriteBatch& batch, Texture2D& texture, const Rectangle& rectangle,
                   const Color colour)
{
    batch.Draw(texture, rectangle, colour);
}

void drawEllipse(SpriteBatch& batch, Texture2D& texture, const int centreX, const int centreY,
                 const int radiusX, const int radiusY, const Color colour)
{
    for (int y = -radiusY; y <= radiusY; ++y) {
        const float normalizedY = static_cast<float>(y) / static_cast<float>(radiusY);
        const int halfWidth =
            static_cast<int>(std::sqrt(std::max(0.0F, 1.0F - normalizedY * normalizedY)) *
                             static_cast<float>(radiusX));
        drawRectangle(batch, texture,
                      Rectangle(centreX - halfWidth, centreY + y, halfWidth * 2 + 1, 1), colour);
    }
}

void drawEgg(SpriteBatch& batch, Texture2D& texture, const int centreX, const int centreY,
             const int radiusX, const int radiusY, const Color colour)
{
    for (int y = -radiusY; y <= radiusY; ++y) {
        const float normalizedY = static_cast<float>(y) / static_cast<float>(radiusY);
        const float ovalWidth = std::sqrt(std::max(0.0F, 1.0F - normalizedY * normalizedY));
        const float lowerHalf = (normalizedY + 1.0F) * 0.5F;
        const float taper = 0.82F + lowerHalf * 0.26F;
        const int halfWidth = static_cast<int>(ovalWidth * taper * static_cast<float>(radiusX));
        drawRectangle(batch, texture,
                      Rectangle(centreX - halfWidth, centreY + y, halfWidth * 2 + 1, 1), colour);
    }
}

void drawRing(SpriteBatch& batch, Texture2D& texture, const int centreX, const int centreY,
              const int radius, const Color outer, const Color inner)
{
    drawEllipse(batch, texture, centreX, centreY, radius, radius, outer);
    drawEllipse(batch, texture, centreX, centreY, radius - 3, radius - 3, inner);
}

} // namespace

void DeviceShellRenderer::drawBody(SpriteBatch& spriteBatch, Texture2D& pixelTexture,
                                   const Presentation::DeviceShellStyle& style,
                                   const Color background)
{
    const Color outline = asColor(style.outline);
    const Color body = asColor(style.body);
    const Color shadow = asColor(style.bodyShadow);
    const Color highlight = asColor(style.bodyHighlight);

    // A soft three-layer contact shadow grounds the device without recreating
    // the former hard-edged dark block beneath it.
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::CentreX,
                DeviceShellGeometry::FloorShadowCentreY, 174, 20,
                Color(3, 4, 4, 26));
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::CentreX,
                DeviceShellGeometry::FloorShadowCentreY - 2, 132, 14,
                Color(4, 5, 6, 38));
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::CentreX,
                DeviceShellGeometry::FloorShadowCentreY - 4, 88, 8,
                Color(5, 7, 7, 52));

    // Concentric shells form a continuous moulded rim.
    drawEgg(spriteBatch, pixelTexture, 270, 348, 220,
            DeviceShellGeometry::BodyRadiusY, outline);
    drawEgg(spriteBatch, pixelTexture, 270, 346, 212, 264, shadow);
    drawEgg(spriteBatch, pixelTexture, 268, 339, 202, 253, body);

    // Restrained moulded-plastic reflections add curvature while staying
    // within the egg silhouette. The translucent family receives an extra
    // internal glow instead of pretending to be a flat opaque fill.
    drawEllipse(spriteBatch, pixelTexture, 204, 292, 58, 145, highlight);
    if (style.translucent) {
        const Color innerGlow(style.bodyHighlight.red, style.bodyHighlight.green,
                              style.bodyHighlight.blue, 70U);
        drawEllipse(spriteBatch, pixelTexture, 322, 348, 30, 132, innerGlow);
    } else {
        const Color edgeGlow(style.bodyHighlight.red, style.bodyHighlight.green,
                             style.bodyHighlight.blue, 72U);
        drawEllipse(spriteBatch, pixelTexture, 337, 330, 16, 105, edgeGlow);
    }

    // A moulded keychain lug uses the same material and rim depth as the body.
    drawRing(spriteBatch, pixelTexture, 270, 72, 21, outline, background);
    drawRing(spriteBatch, pixelTexture, 270, 72, 15, shadow, background);
    drawRing(spriteBatch, pixelTexture, 270, 72, 11, body, background);
}

void DeviceShellRenderer::drawControls(SpriteBatch& spriteBatch, Texture2D& pixelTexture,
                                       const Presentation::DeviceShellStyle& style,
                                       const DeviceShellControlState& controlState)
{
    const Color outline = asColor(style.outline);
    const Color bodyShadow = asColor(style.bodyShadow);
    const Color button = asColor(style.button);
    const Color buttonShadow = asColor(style.buttonShadow);
    const Color buttonHighlight = asColor(style.buttonHighlight);

    const Color pressedButton(
        (static_cast<int>(style.button.red) * 3 + style.buttonShadow.red) / 4,
        (static_cast<int>(style.button.green) * 3 + style.buttonShadow.green) / 4,
        (static_cast<int>(style.button.blue) * 3 + style.buttonShadow.blue) / 4,
        255);
    const Color pressedHighlight(style.buttonHighlight.red, style.buttonHighlight.green,
                                 style.buttonHighlight.blue, 105U);

    for (std::size_t index = 0; index < DeviceShellGeometry::Buttons.size(); ++index) {
        const ShellPoint position = DeviceShellGeometry::Buttons[index];
        const bool pressed = controlState.buttons[index];
        const int capY = position.y + (pressed ? DeviceShellGeometry::PressedButtonTravel : 0);
        drawEllipse(spriteBatch, pixelTexture, position.x, position.y + 5, 25, 25, outline);
        drawEllipse(spriteBatch, pixelTexture, position.x, position.y + 3, 22, 22, buttonShadow);
        drawEllipse(spriteBatch, pixelTexture, position.x, capY, pressed ? 19 : 20,
                    pressed ? 19 : 20, pressed ? pressedButton : button);
        drawEllipse(spriteBatch, pixelTexture, position.x - (pressed ? 4 : 5),
                    capY - (pressed ? 3 : 5), pressed ? 6 : 8, pressed ? 6 : 8,
                    pressed ? pressedHighlight : buttonHighlight);
        if (!pressed) {
            drawEllipse(spriteBatch, pixelTexture, position.x - 7, position.y - 7, 3, 3,
                        Color(255, 255, 255, 135));
        }
    }

    // The recessed reset pinhole remains separate from the three P1 controls.
    if (controlState.resetPressed) {
        const Color resetHalo(style.bodyHighlight.red, style.bodyHighlight.green,
                              style.bodyHighlight.blue, 190U);
        drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::ResetX,
                    DeviceShellGeometry::ResetY,
                    DeviceShellGeometry::ResetRadius + 5,
                    DeviceShellGeometry::ResetRadius + 5, resetHalo);
    }
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::ResetX, DeviceShellGeometry::ResetY,
                DeviceShellGeometry::ResetRadius + 1, DeviceShellGeometry::ResetRadius + 1,
                outline);
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::ResetX, DeviceShellGeometry::ResetY,
                DeviceShellGeometry::ResetRadius - 2, DeviceShellGeometry::ResetRadius - 2,
                bodyShadow);
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::ResetX, DeviceShellGeometry::ResetY,
                controlState.resetPressed ? 2 : 3, controlState.resetPressed ? 2 : 3,
                Color(22, 28, 30, 255));
}

} // namespace TamagotchiCna::Application
