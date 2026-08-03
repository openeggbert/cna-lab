// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Viewport/CnaUiRenderer.hpp
 * @brief Draws UiDrawData through CNA's public graphics API.
 *
 * This is the answer to ANALYSIS.md question Q-01, and the proof of decision D-01: the editor's
 * own user interface is rendered using only the API a CNA *game* has. No `CNA::Internal::*`, no
 * per-backend code, no shader authored here. One implementation serves every backend CNA supports
 * for the editor UI, because everything it uses is backend-independent by construction:
 *
 * | ImGui needs                            | CNA public API used                                    |
 * |----------------------------------------|--------------------------------------------------------|
 * | Textured, vertex-coloured triangles    | `GraphicsDevice::DrawUserIndexedPrimitives` with        |
 * |                                        | `VertexPositionColorTexture` and 16-bit indices         |
 * | Pixel-space orthographic projection    | `BasicEffect` (`Projection`, `VertexColorEnabled`,      |
 * |                                        | `TextureEnabled`, `LightingEnabled = false`)            |
 * | Font atlas upload and incremental grow | `Texture2D(device, w, h)` + `Texture2D::SetData`        |
 * | Per-command clipping                   | `RasterizerState::ScissorTestEnable` +                  |
 * |                                        | `GraphicsDevice::ScissorRectangle`                      |
 * | Straight-alpha blending                | `BlendState::NonPremultiplied`                          |
 * | No depth testing                       | `DepthStencilState::None`                               |
 * | Bilinear clamped sampling              | `SamplerState::LinearClamp`                             |
 *
 * The one thing that is *not* free is the vertex repack: ImGui's layout differs from
 * `VertexPositionColorTexture`. That conversion already happens on the UI side while filling
 * UiDrawData, so this class consumes a layout it can hand almost straight to CNA.
 */

#include <memory>
#include <string>

#include "CNA/Editor/Ui/UiDrawData.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
}

namespace CNA::Editor
{
    /** @brief Per-frame counters, for the profiler panel and for tests. */
    struct UiRenderStats
    {
        std::size_t drawCalls = 0;
        std::size_t triangles = 0;
        std::size_t texturesCreated = 0;
        std::size_t texturesUpdated = 0;
        std::size_t texturesDestroyed = 0;

        /** @brief Commands skipped because their clip rectangle selected no pixels. */
        std::size_t clippedAway = 0;
    };

    /**
     * @brief Renders an immediate-mode UI with CNA.
     *
     * The GraphicsDevice is borrowed, never owned: the application owns the window and the device,
     * and this class only draws into whatever it is handed.
     */
    class CnaUiRenderer
    {
    public:
        CnaUiRenderer();
        ~CnaUiRenderer();

        CnaUiRenderer(const CnaUiRenderer&) = delete;
        CnaUiRenderer& operator=(const CnaUiRenderer&) = delete;

        /**
         * @brief Binds the renderer to a device.
         * @param device The device to draw with; must outlive this renderer.
         */
        void initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        /** @brief Releases every texture. Safe to call more than once. */
        void shutdown();

        /**
         * @brief Honours @p drawData's texture requests, then draws its geometry.
         *
         * Restores the device's blend, depth, rasterizer and scissor state before returning, so a
         * caller that draws a scene after the UI is not silently affected by the UI's own state.
         *
         * @return Counters for the frame just drawn.
         */
        UiRenderStats render(const UiDrawData& drawData);

        /** @brief Returns the stats from the most recent render(). */
        [[nodiscard]] const UiRenderStats& getLastStats() const { return lastStats_; }

        /** @brief Returns the number of textures currently held. */
        [[nodiscard]] std::size_t getTextureCount() const;

        /** @brief Returns the name of the CNA backend this build was compiled against. */
        [[nodiscard]] static std::string getBackendName();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        UiRenderStats lastStats_;
    };
}
