// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Viewport/EditorViewport.hpp
 * @brief The scene preview surface, and the seam where CNA is allowed to appear.
 *
 * This is the **only** module in the editor that may link CNA. Everything else -- the document
 * model, the undo stack, the asset database, the panels -- is CNA-free, which is what lets the
 * editor build and its tests run with no CNA checkout, no window and no GPU (ANALYSIS.md
 * decision D-03).
 *
 * The interface is deliberately coarse: one `render()` call that returns a texture id the panel
 * can display. An earlier draft exposed the passes individually (`renderGrid`, `renderScene`,
 * `renderSelectionOutline`, …) and that turned out to be the wrong seam -- the *ordering* of those
 * passes is a property of the renderer, not a decision for the panel, and every implementation
 * would have had to be trusted to call them in the right order. The ordering still matters and is
 * still enforced; it is simply enforced in one place now, inside the implementation.
 *
 * Note what is absent: the camera lives in `cna-editor-scene` as `EditorCamera2D`, not here.
 * Picking, framing and grid spacing all need it, and none of those should require a CNA build.
 */

#include <cstdint>
#include <memory>
#include <vector>

#include "CNA/Editor/Core/EditorMath.hpp"
#include "CNA/Editor/Core/Uuid.hpp"
#include "CNA/Editor/Scene/EditorCamera2D.hpp"
#include "CNA/Editor/Scene/SpriteAnimation.hpp"
#include "CNA/Editor/Ui/UiDrawData.hpp"

namespace CNA::Editor
{
    class SceneDocument;

    /** @brief The manipulator currently bound to the mouse. */
    enum class GizmoMode
    {
        None,
        Translate,
        Rotate,
        Scale
    };

    /** @brief Counters from the most recent viewport render, for the profiler and for tests. */
    struct ViewportStats
    {
        std::size_t spritesDrawn = 0;
        std::size_t spritesSkipped = 0;
        std::size_t gridLines = 0;
        std::size_t missingTextures = 0;
    };

    /**
     * @brief The scene preview.
     *
     * Abstract so the panel layer can be built and tested against NullEditorViewport, and so a
     * CNA-backed implementation can be swapped in without any panel changing.
     */
    class EditorViewport
    {
    public:
        virtual ~EditorViewport() = default;

        /** @brief Returns a short name for the implementation, e.g. "cna-easygl" or "null". */
        [[nodiscard]] virtual const char* getBackendName() const = 0;

        /**
         * @brief Draws @p scene into an offscreen surface of @p width by @p height pixels.
         *
         * Passes run in a fixed order inside the implementation: grid, then the game's own content,
         * then the editor's overlay. Editor artefacts are never entities in the scene, so a build
         * can never ship with them.
         *
         * @param preview Which animation frame to draw for the entity being previewed. Passed in
         *        rather than read from the document, because playback is editor state and must not
         *        travel in a scene -- the same reason the selection is passed rather than stored.
         * @return A UI texture id the viewport panel can display, or zero when nothing was drawn.
         */
        virtual UiTextureId render(const SceneDocument& scene,
                                   int width,
                                   int height,
                                   const std::vector<Uuid>& selection,
                                   GizmoMode gizmoMode,
                                   const AnimationPreview& preview = {}) = 0;

        /**
         * @brief Returns the texel size of @p assetId, or (0, 0) when it cannot be resolved.
         *
         * Picking and framing need it, and only the viewport has loaded the textures.
         */
        [[nodiscard]] virtual EditorVector2 getSpriteSize(const Uuid& assetId) const = 0;

        /** @brief Returns a SpriteSizeProvider bound to this viewport. */
        [[nodiscard]] SpriteSizeProvider makeSizeProvider() const
        {
            return [this](const Uuid& assetId) { return getSpriteSize(assetId); };
        }

        /** @brief Returns the editor camera. */
        [[nodiscard]] virtual EditorCamera2D& getCamera() = 0;
        [[nodiscard]] virtual const EditorCamera2D& getCamera() const = 0;

        /**
         * @brief Returns true when render()'s texture must be sampled bottom-up.
         *
         * A render target's texture origin is not the same on every graphics API: OpenGL-family
         * backends put it bottom-left, Direct3D-family top-left. CNA does not normalise this for a
         * render target used as a sampled texture, so the viewport has to say which convention its
         * result follows. See docs/SPIKE-IMGUI-CNA.md gap G-03.
         */
        [[nodiscard]] virtual bool isRenderTextureFlippedVertically() const { return false; }

        /**
         * @brief Drops any cached GPU resource for @p assetId, so it is loaded afresh.
         *
         * Textures are cached for the life of the viewport, which is right until somebody edits
         * one in another program. Without this the editor goes on showing art that no longer
         * exists, and the only fix is to restart it.
         *
         * A nil id means "everything", which is what a project-wide rescan wants.
         */
        virtual void invalidateAsset(const Uuid& assetId) { (void)assetId; }

        /**
         * @brief Returns a UI texture id previewing @p assetId, or zero when it has none.
         *
         * Only image assets have one. The id stays the same across frames for the same asset, so
         * the panel can ask every frame without the UI seeing a different texture each time.
         */
        virtual UiTextureId getAssetThumbnail(const Uuid& assetId)
        {
            (void)assetId;
            return kUiTextureNone;
        }

        /** @brief Returns the counters from the most recent render(). */
        [[nodiscard]] virtual ViewportStats getLastStats() const = 0;
    };

    /**
     * @brief A viewport that renders nothing but still does all the geometry.
     *
     * Used by `--headless`, by every unit test, and as the fallback when the editor is built
     * without CNA. It maintains a real camera and honours resizes, so the panel layer, the picking
     * path and the framing logic are all exercised without a GPU; only the pixels are missing.
     */
    class NullEditorViewport final : public EditorViewport
    {
    public:
        [[nodiscard]] const char* getBackendName() const override { return "null"; }

        UiTextureId render(const SceneDocument& scene,
                           int width,
                           int height,
                           const std::vector<Uuid>& selection,
                           GizmoMode gizmoMode,
                           const AnimationPreview& preview = {}) override;

        [[nodiscard]] EditorVector2 getSpriteSize(const Uuid& assetId) const override
        {
            (void)assetId;
            return EditorVector2{};
        }

        [[nodiscard]] EditorCamera2D& getCamera() override { return camera_; }
        [[nodiscard]] const EditorCamera2D& getCamera() const override { return camera_; }

        [[nodiscard]] ViewportStats getLastStats() const override { return stats_; }

        /** @brief Returns how many times render() has been called. */
        [[nodiscard]] std::uint64_t getRenderCount() const { return renderCount_; }

        [[nodiscard]] int getWidth() const { return width_; }
        [[nodiscard]] int getHeight() const { return height_; }

    private:
        EditorCamera2D camera_;
        ViewportStats stats_;
        std::uint64_t renderCount_ = 0;
        int width_ = 0;
        int height_ = 0;
    };
}
