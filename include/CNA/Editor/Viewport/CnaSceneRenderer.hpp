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
#include "CNA/Editor/Core/ComponentDescriptor.hpp"
#include "CNA/Editor/Scene/EditorCamera2D.hpp"
#include "CNA/Editor/Ui/UiDrawData.hpp"
#include "CNA/Editor/Viewport/EditorViewport.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
    class Texture2D;
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

        /** @brief Non-empty tilemap cells drawn this frame, after culling to the viewport. */
        std::size_t tilesDrawn = 0;
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

        /**
         * @brief Binds the renderer to a device, an asset database and a component registry.
         *
         * The registry supplies declared defaults. A tilemap authored by hand may omit its tile
         * size, and reading zero there would draw nothing with no explanation -- the descriptor is
         * the only thing that knows what the field means when the file does not say.
         */
        void initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const AssetDatabase& assets,
                        const ComponentRegistry& components);

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
         * @param gizmoSpace Which frame that manipulator's arms point along.
         */
        SceneRenderStats render(const SceneDocument& scene,
                                const EditorCamera2D& camera,
                                int width,
                                int height,
                                const std::vector<Uuid>& selection,
                                GizmoMode gizmoMode = GizmoMode::None,
                                GizmoSpace gizmoSpace = GizmoSpace::World,
                                const AnimationPreview& preview = {});

        /**
         * @brief Draws @p scene the way the *game* sees it: into the current target, no overlays.
         *
         * What `cna-player` uses. Two differences from render(), and both are the point:
         *
         * - **No editor artefacts.** No grid, no icons, no selection outline, no gizmo. The
         *   separation was already enforced by the pass structure, so this is a matter of not
         *   running two of the three passes rather than of filtering anything out.
         * - **The current render target**, normally the back buffer, rather than an offscreen one.
         *   The player's window shows the game and nothing else, so there is nothing to compose it
         *   with -- and drawing straight to the back buffer is what makes
         *   `GraphicsDevice::GetBackBufferData` a screenshot of the game (plan.md ED-510).
         *
         * The caller clears; the game's own camera decides the colour, and this class has no
         * opinion about it.
         */
        SceneRenderStats renderGameView(const SceneDocument& scene,
                                        const EditorCamera2D& camera,
                                        int width,
                                        int height);

        /**
         * @brief Registers the rendered target with @p uiRenderer and returns its UI texture id.
         *
         * The id is what the viewport panel passes to `EditorUi::image()`. Zero when nothing has
         * been rendered yet.
         */
        UiTextureId shareWithUi(CnaUiRenderer& uiRenderer);

        /**
         * @brief Drops the cached texture for @p assetId, or every texture when @p assetId is nil.
         *
         * Also clears the failed-load memory for it: a texture that failed because the file was
         * missing must get another chance once the file comes back, or restoring it would appear
         * to do nothing.
         */
        void invalidateTexture(const Uuid& assetId);

        /**
         * @brief Returns the texture for @p assetId, loading it if it is not already in.
         *
         * The same cache the scene pass uses, so a thumbnail costs nothing once the sprite that
         * uses it has been drawn -- and a project of a hundred textures does not load them twice.
         * Returns nullptr when the asset is unknown or its file will not open.
         */
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D* getOrLoadTexture(const Uuid& assetId);

        /** @brief Returns the texel size of @p assetId, or (0, 0) when it cannot be resolved. */
        [[nodiscard]] EditorVector2 getSpriteSize(const Uuid& assetId) const;

        /** @brief Returns a SpriteSizeProvider bound to this renderer, for picking and framing. */
        [[nodiscard]] SpriteSizeProvider makeSizeProvider() const;

        /** @brief Returns the counters from the most recent render(). */
        [[nodiscard]] const SceneRenderStats& getLastStats() const { return lastStats_; }

    private:
        /**
         * @brief The one implementation behind render() and renderGameView().
         *
         * Both draw the same content pass, and a second copy of it would be a second place for
         * "what the game looks like" to drift from what the editor shows -- which is the single
         * property the editor exists to guarantee.
         */
        SceneRenderStats renderPasses(const SceneDocument& scene,
                                      const EditorCamera2D& camera,
                                      int width,
                                      int height,
                                      const std::vector<Uuid>& selection,
                                      GizmoMode gizmoMode,
                                      GizmoSpace gizmoSpace,
                                      const AnimationPreview& preview,
                                      bool editorOverlays,
                                      bool offscreen);

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
     * @param components Supplies declared property defaults for the components it draws.
     * @param uiRenderer The UI renderer the rendered target is shared through, so the viewport
     *        panel can display it as an ordinary image.
     */
    std::unique_ptr<EditorViewport> createCnaEditorViewport(
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
        const AssetDatabase& assets,
        const ComponentRegistry& components,
        CnaUiRenderer& uiRenderer);
}
