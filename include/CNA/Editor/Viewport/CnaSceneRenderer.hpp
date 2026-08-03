// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Viewport/CnaSceneRenderer.hpp
 * @brief Draws a scene document into an offscreen texture, through CNA's public API.
 *
 * The counterpart to CnaUiRenderer: that one draws the editor's *chrome*, this one draws the
 * *content*. Both go through `Microsoft::Xna::Framework::*` only.
 *
 * Rendering to a `RenderTarget2D` rather than straight to the back buffer is what lets the result
 * appear as an image inside a docked ImGui panel — the viewport is a panel like any other, and the
 * user can move, resize and tab it. It also means the scene is drawn at exactly the panel's size,
 * so nothing is stretched.
 *
 * The passes are kept separate and ordered, per ANALYSIS.md's adoption of the original design:
 * grid, then the game's own content, then the editor's overlay. Editor artefacts — grid lines,
 * selection outlines, icons, gizmos — are never entities in the scene, so a build can never ship
 * with them.
 */

#include <memory>
#include <string>
#include <vector>

#include "CNA/Editor/Assets/AssetDatabase.hpp"
#include "CNA/Editor/Scene/EditorCamera2D.hpp"
#include "CNA/Editor/Ui/UiDrawData.hpp"
#include "CNA/Editor/Viewport/EditorViewport.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
}

namespace CNA::Editor
{
    class SceneDocument;
    class CnaUiRenderer;

    /** @brief Per-frame counters for the scene pass. */
    struct SceneRenderStats
    {
        std::size_t spritesDrawn = 0;
        std::size_t spritesSkipped = 0;
        std::size_t gridLines = 0;
        std::size_t texturesLoaded = 0;

        /** @brief Icons drawn for entities the viewport cannot render, such as cameras and lights. */
        std::size_t iconsDrawn = 0;

        /** @brief Sprites whose texture asset could not be loaded, drawn as a placeholder. */
        std::size_t missingTextures = 0;
    };

    /**
     * @brief Renders a scene into an offscreen target sized to the viewport panel.
     *
     * Textures are loaded lazily, once per asset id, and kept until reset(). A texture that fails
     * to load is remembered as failed so a broken asset costs one attempt rather than one per
     * frame — an editor that retries a missing file sixty times a second is an editor that stalls.
     */
    class CnaSceneRenderer
    {
    public:
        CnaSceneRenderer();
        ~CnaSceneRenderer();

        CnaSceneRenderer(const CnaSceneRenderer&) = delete;
        CnaSceneRenderer& operator=(const CnaSceneRenderer&) = delete;

        /** @brief Binds the renderer to a device and an asset database. */
        void initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const AssetDatabase& assets);

        /** @brief Releases the render target and every loaded texture. */
        void shutdown();

        /**
         * @brief Draws @p scene at @p camera into an offscreen target of the given pixel size.
         *
         * Restores the previous render target before returning, so the caller's own drawing is
         * unaffected.
         *
         * @param selection Entities to outline. Drawn in the overlay pass, never as scene content.
         * @param gizmoMode Which manipulator to draw on the first selected entity, if any.
         */
        SceneRenderStats render(const SceneDocument& scene,
                                const EditorCamera2D& camera,
                                int width,
                                int height,
                                const std::vector<Uuid>& selection,
                                GizmoMode gizmoMode = GizmoMode::None);

        /**
         * @brief Registers the rendered target with @p uiRenderer and returns its UI texture id.
         *
         * The id is what the viewport panel passes to `EditorUi::image()`. Zero when nothing has
         * been rendered yet.
         */
        UiTextureId shareWithUi(CnaUiRenderer& uiRenderer);

        /** @brief Returns the texel size of @p assetId, or (0, 0) when it cannot be resolved. */
        [[nodiscard]] EditorVector2 getSpriteSize(const Uuid& assetId) const;

        /** @brief Returns a SpriteSizeProvider bound to this renderer, for picking and framing. */
        [[nodiscard]] SpriteSizeProvider makeSizeProvider() const;

        /** @brief Returns the counters from the most recent render(). */
        [[nodiscard]] const SceneRenderStats& getLastStats() const { return lastStats_; }

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        SceneRenderStats lastStats_;
    };

    class EditorViewport;

    /**
     * @brief Creates the CNA-backed scene viewport.
     *
     * @param device The graphics device; must outlive the viewport.
     * @param assets Where sprite textures are resolved from.
     * @param uiRenderer The UI renderer the rendered target is shared through, so the viewport
     *        panel can display it as an ordinary image.
     */
    std::unique_ptr<EditorViewport> createCnaEditorViewport(
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
        const AssetDatabase& assets,
        CnaUiRenderer& uiRenderer);
}
