// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Scene/SceneWireframe.hpp
 * @brief What the 3D viewport draws, expressed as screen-space line segments (plan.md ED-400).
 *
 * The same split the gizmos use, and for the same reason: the *decisions* -- where the ground grid
 * goes, how far it extends, which entity gets a box, what is clipped against the near plane -- are
 * CNA-free and unit-tested in CI, and the renderer's job shrinks to calling `drawLine` in a loop.
 * A 3D viewport whose geometry could only be checked by looking at it would be a 3D viewport
 * nobody could check.
 *
 * This is deliberately *not* a 3D renderer. Until ED-402 brings a model pipeline there is no mesh
 * to draw, and a wireframe over the scene's bounds answers the question a 3D camera exists to ask:
 * where is everything, actually, in relation to everything else. Sprites are not drawn as textured
 * quads here because `SpriteBatch` cannot draw an arbitrary quad -- only a rotated rectangle -- and
 * a sprite seen from an angle is a trapezoid.
 */

#include <cstddef>
#include <optional>
#include <vector>

#include "CNA/Editor/Scene/EditorCamera3D.hpp"
#include "CNA/Editor/Scene/EditorIcons.hpp"

namespace CNA::Editor
{
    class SceneDocument;

    /** @brief One line to draw, in viewport pixels. */
    struct WireSegment
    {
        EditorVector2 from;
        EditorVector2 to;
        EditorColor color;
        float thickness = 1.0f;
    };

    /** @brief The colours the 3D viewport draws with, kept beside the geometry that uses them. */
    namespace WireColors
    {
        /** @brief The ground grid. Dim enough to read the scene over. */
        inline constexpr EditorColor kGrid{70, 70, 78, 255};

        /** @brief Every tenth line, so distances stay readable when the grid is dense. */
        inline constexpr EditorColor kGridMajor{104, 104, 116, 255};

        /** @brief The world X axis. Red, as every 3D tool since the first one. */
        inline constexpr EditorColor kAxisX{196, 84, 84, 255};

        /** @brief The world Y axis. Green, matching the gizmo's Y arm. */
        inline constexpr EditorColor kAxisY{92, 170, 92, 255};

        /** @brief An entity's bounding box. */
        inline constexpr EditorColor kEntity{130, 138, 150, 255};

        /** @brief A selected entity's bounding box. Matches the 2D viewport's selection colour. */
        inline constexpr EditorColor kSelected{255, 190, 60, 255};
    }

    /** @brief What to include in a wireframe. */
    struct WireframeOptions
    {
        /** @brief Draw the scene-plane grid at world Z = 0. */
        bool drawGrid = true;

        /** @brief Draw a box per entity. */
        bool drawEntityBounds = true;

        /**
         * @brief World units between grid lines, or 0 to choose one from the camera's distance.
         *
         * Choosing rather than fixing, for the reason `chooseGridSpacing` exists: a fixed spacing
         * is a solid block when zoomed out and invisible when zoomed in.
         */
        float gridSpacing = 0.0f;

        /** @brief How many cells the grid extends from its centre, in each direction. */
        int gridHalfExtent = 24;

        /**
         * @brief Ceiling on the segments produced, so a large scene cannot stall a frame.
         *
         * Reached rather than approached silently: `WireframeResult::truncated` says so, and the
         * viewport reports it, because a wireframe that quietly stopped halfway through a scene
         * looks exactly like a scene with half its entities missing.
         */
        std::size_t maxSegments = 20000;
    };

    /** @brief The segments to draw, and what had to be left out to produce them. */
    struct WireframeResult
    {
        std::vector<WireSegment> segments;

        /** @brief Entities whose box contributed at least one visible segment. */
        std::size_t entitiesDrawn = 0;

        /** @brief True when `maxSegments` stopped the build before the scene was exhausted. */
        bool truncated = false;
    };

    /**
     * @brief Projects the segment @p from -> @p to, clipping it against the near plane.
     *
     * @return The screen-space endpoints, or std::nullopt when the segment is entirely behind the
     *         camera. A segment with one endpoint behind is *shortened* rather than dropped: a
     *         grid line running under the camera is mostly visible, and dropping it whole leaves a
     *         wedge of missing floor exactly where the user is looking.
     */
    [[nodiscard]] std::optional<std::pair<EditorVector2, EditorVector2>> projectSegment(
        const EditorCamera3D& camera, const EditorVector3& from, const EditorVector3& to);

    /**
     * @brief Returns the grid alone: the XY plane at world Z = 0, centred on the camera's pivot.
     *
     * The *scene's* plane, not a ground plane under it. Everything this editor can currently place
     * lives in XY -- sprites, tilemaps, the 2D camera's whole world -- so a grid on XZ would be a
     * floor beneath a scene that has no floor, and an unrotated 3D camera would look along it
     * edge-on and show nothing. On this plane, a 3D camera at yaw and pitch zero shows exactly what
     * the 2D viewport shows, which is what makes the two views recognisably the same scene.
     *
     * When ED-402 brings models and scenes stop being flat, a ground plane becomes the useful one
     * and this is where that choice would live.
     */
    [[nodiscard]] std::vector<WireSegment> buildSceneGrid(const EditorCamera3D& camera,
                                                          const WireframeOptions& options = {});

    /**
     * @brief Returns the screen-space badge for @p kind, centred on @p screenPoint.
     *
     * Drawn in pixels rather than in the world, exactly as the 2D viewport's icons are and for the
     * same reason: a camera has no size, so a badge scaled by distance would vanish at the far end
     * of a level and swallow the screen at the near end. Ten entities that draw nothing are ten
     * identical cubes without this -- and "which of these is the camera" is the first question a
     * 3D view of such a scene is asked.
     */
    [[nodiscard]] std::vector<WireSegment> buildIconBadge(EditorIconKind kind,
                                                          const EditorVector2& screenPoint,
                                                          const EditorColor& color);

    /**
     * @brief Returns everything the 3D viewport draws for @p scene.
     *
     * @param selection Entities drawn in the selection colour, and drawn thicker so a selected box
     *        inside a cluster of others can still be told apart.
     * @param sizeProvider Supplies sprite dimensions, exactly as the 2D picking path does.
     */
    [[nodiscard]] WireframeResult buildSceneWireframe(const SceneDocument& scene,
                                                      const EditorCamera3D& camera,
                                                      const std::vector<Uuid>& selection,
                                                      const SpriteSizeProvider& sizeProvider,
                                                      const WireframeOptions& options = {});

    /**
     * @brief Returns the entity whose box is nearest the eye along the ray through @p screenPoint.
     *
     * The 3D counterpart of `pickEntityAt`, and the same trade: a ray against bounds rather than
     * GPU picking, so it needs no render target, no read-back, and works headless. "Nearest"
     * rather than "topmost", because depth is a real quantity here and layer order is not.
     *
     * @return The entity hit, or the nil Uuid when the ray missed everything.
     */
    [[nodiscard]] Uuid pickEntityAt3D(const SceneDocument& scene, const EditorCamera3D& camera,
                                      const EditorVector2& screenPoint,
                                      const SpriteSizeProvider& sizeProvider);

    /**
     * @brief Returns the distance along @p ray at which it enters @p bounds, if it does.
     *
     * The slab test. Exposed because picking is not its only caller -- framing a click and
     * dropping an asset into a 3D view both need to know where a ray meets a box.
     */
    [[nodiscard]] std::optional<float> intersectRayWithBounds(const WorldRay& ray,
                                                              const WorldBounds3D& bounds);
}
