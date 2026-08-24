#include "CnaTamagotchi/Application/DeviceShellRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/Rectangle.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CnaTamagotchi::Application {
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

    // Concentric shells form a continuous moulded rim; there is intentionally
    // no detached floor shadow below the device.
    drawEgg(spriteBatch, pixelTexture, 270, 348, 220, 272, outline);
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
                                       const Presentation::DeviceShellStyle& style)
{
    const Color outline = asColor(style.outline);
    const Color bodyShadow = asColor(style.bodyShadow);
    const Color button = asColor(style.button);
    const Color buttonShadow = asColor(style.buttonShadow);
    const Color buttonHighlight = asColor(style.buttonHighlight);

    for (const ShellPoint position : DeviceShellGeometry::Buttons) {
        drawEllipse(spriteBatch, pixelTexture, position.x, position.y + 5, 25, 25, outline);
        drawEllipse(spriteBatch, pixelTexture, position.x, position.y + 3, 22, 22, buttonShadow);
        drawEllipse(spriteBatch, pixelTexture, position.x, position.y, 20, 20, button);
        drawEllipse(spriteBatch, pixelTexture, position.x - 5, position.y - 5, 8, 8,
                    buttonHighlight);
        drawEllipse(spriteBatch, pixelTexture, position.x - 7, position.y - 7, 3, 3,
                    Color(255, 255, 255, 135));
    }

    // The recessed reset pinhole remains separate from the three P1 controls.
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::ResetX, DeviceShellGeometry::ResetY,
                DeviceShellGeometry::ResetRadius + 1, DeviceShellGeometry::ResetRadius + 1,
                outline);
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::ResetX, DeviceShellGeometry::ResetY,
                DeviceShellGeometry::ResetRadius - 2, DeviceShellGeometry::ResetRadius - 2,
                bodyShadow);
    drawEllipse(spriteBatch, pixelTexture, DeviceShellGeometry::ResetX, DeviceShellGeometry::ResetY,
                3, 3, Color(22, 28, 30, 255));
}

} // namespace CnaTamagotchi::Application
