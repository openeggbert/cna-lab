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
 * Rendering is split into ordered passes rather than one draw call, because the editor's own
 * overlay must never become part of the game's scene. Selection outlines, grids, gizmos and the
 * icons for cameras and lights are editor artefacts: putting them into the scene graph means a
 * build eventually ships with them, and means the game's own render order has to accommodate
 * objects it does not own.
 */

#include <memory>
#include <string>
#include <vector>

#include "CNA/Editor/Core/EditorMath.hpp"
#include "CNA/Editor/Core/Uuid.hpp"

namespace CNA::Editor
{
    class SceneDocument;

    /** @brief Which axes the viewport navigates and which gizmos it offers. */
    enum class ViewportMode
    {
        /** @brief Orthographic, XY plane, no rotation gizmo by default. */
        TwoDimensional,
        ThreeDimensional
    };

    /** @brief The manipulator currently bound to the mouse. */
    enum class GizmoMode
    {
        None,
        Translate,
        Rotate,
        Scale
    };

    /** @brief The editor's own camera. Never part of the scene document. */
    struct ViewportCamera
    {
        EditorVector3 position{0.0f, 0.0f, 100.0f};
        EditorQuaternion rotation;

        /** @brief Visible world height in 2D mode. */
        float orthographicSize = 600.0f;

        /** @brief Vertical field of view in degrees, used in 3D mode. */
        float fieldOfView = 45.0f;

        float nearPlane = 0.1f;
        float farPlane = 10000.0f;
    };

    /** @brief What a click at a given point hit. */
    struct PickResult
    {
        /** @brief The entity under the cursor, or the nil Uuid when nothing was hit. */
        Uuid entityId;

        /** @brief Distance along the pick ray. Zero in 2D mode. */
        float distance = 0.0f;
    };

    /**
     * @brief The scene preview.
     *
     * The interface is abstract so the panel layer can be built and tested against
     * NullEditorViewport, and so a CNA-backed implementation can be swapped in without any panel
     * changing. Callers drive the passes in order; an implementation may make any of them a no-op.
     */
    class EditorViewport
    {
    public:
        virtual ~EditorViewport() = default;

        /** @brief Returns a short name for the implementation, e.g. "cna-easygl" or "null". */
        [[nodiscard]] virtual const char* getBackendName() const = 0;

        /** @brief Resizes the render surface. */
        virtual void resize(int width, int height) = 0;

        /** @brief Draws the game's own content. */
        virtual void renderScene(const SceneDocument& scene) = 0;

        /** @brief Draws the reference grid, beneath everything else. */
        virtual void renderGrid() = 0;

        /** @brief Outlines the selected entities. */
        virtual void renderSelectionOutline(const std::vector<Uuid>& selection) = 0;

        /** @brief Draws icons for entities with no visible geometry: cameras, lights, audio sources. */
        virtual void renderIcons(const SceneDocument& scene) = 0;

        /** @brief Draws the active manipulator for the selection. */
        virtual void renderGizmos(const std::vector<Uuid>& selection, GizmoMode mode) = 0;

        /**
         * @brief Returns the entity at viewport pixel (@p x, @p y).
         *
         * Phase 1 implementations ray-cast against entity bounds, which needs no render target and
         * no GPU read-back. GPU picking through an id buffer is a later optimisation for scenes
         * where the ray cast becomes the bottleneck -- see plan.md ED-320.
         */
        [[nodiscard]] virtual PickResult pick(const SceneDocument& scene, int x, int y) const = 0;

        [[nodiscard]] virtual ViewportCamera& getCamera() = 0;
        [[nodiscard]] virtual ViewportMode getMode() const = 0;
        virtual void setMode(ViewportMode mode) = 0;
    };

    /**
     * @brief A viewport that renders nothing.
     *
     * Used by `--headless`, by every unit test, and as the fallback when the editor is built
     * without CNA. Picking always misses, which is correct: with no rendering there is no
     * geometry to hit.
     */
    class NullEditorViewport final : public EditorViewport
    {
    public:
        [[nodiscard]] const char* getBackendName() const override { return "null"; }

        void resize(int width, int height) override { width_ = width; height_ = height; }

        void renderScene(const SceneDocument& scene) override { (void)scene; ++sceneRenderCount_; }
        void renderGrid() override {}
        void renderSelectionOutline(const std::vector<Uuid>& selection) override { (void)selection; }
        void renderIcons(const SceneDocument& scene) override { (void)scene; }
        void renderGizmos(const std::vector<Uuid>& selection, GizmoMode mode) override
        {
            (void)selection;
            (void)mode;
        }

        [[nodiscard]] PickResult pick(const SceneDocument& scene, int x, int y) const override
        {
            (void)scene;
            (void)x;
            (void)y;
            return PickResult{};
        }

        [[nodiscard]] ViewportCamera& getCamera() override { return camera_; }
        [[nodiscard]] ViewportMode getMode() const override { return mode_; }
        void setMode(ViewportMode mode) override { mode_ = mode; }

        /** @brief Returns how many times renderScene() has been called. */
        [[nodiscard]] std::uint64_t getSceneRenderCount() const { return sceneRenderCount_; }

        [[nodiscard]] int getWidth() const { return width_; }
        [[nodiscard]] int getHeight() const { return height_; }

    private:
        ViewportCamera camera_;
        ViewportMode mode_ = ViewportMode::TwoDimensional;
        int width_ = 0;
        int height_ = 0;
        std::uint64_t sceneRenderCount_ = 0;
    };
}
