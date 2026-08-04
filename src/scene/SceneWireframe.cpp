// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Scene/SceneWireframe.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "CNA/Editor/Scene/EditorCamera2D.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

namespace CNA::Editor
{
    namespace
    {
        /** @brief Returns the signed distance from the eye to @p point along the view direction. */
        float depthOf(const EditorCamera3D& camera, const EditorVector3& point)
        {
            return dot(subtract(point, camera.getEye()), camera.getForward());
        }

        /** @brief Returns the point @p fraction of the way from @p from to @p to. */
        EditorVector3 interpolate(const EditorVector3& from, const EditorVector3& to, float fraction)
        {
            return add(from, scale(subtract(to, from), fraction));
        }

        /** @brief Appends the twelve edges of @p bounds to @p out, clipped and projected. */
        std::size_t appendBox(std::vector<WireSegment>& out, const EditorCamera3D& camera,
                              const WorldBounds3D& bounds, const EditorColor& color, float thickness)
        {
            const EditorVector3 corners[8] = {
                {bounds.min.x, bounds.min.y, bounds.min.z}, {bounds.max.x, bounds.min.y, bounds.min.z},
                {bounds.max.x, bounds.min.y, bounds.max.z}, {bounds.min.x, bounds.min.y, bounds.max.z},
                {bounds.min.x, bounds.max.y, bounds.min.z}, {bounds.max.x, bounds.max.y, bounds.min.z},
                {bounds.max.x, bounds.max.y, bounds.max.z}, {bounds.min.x, bounds.max.y, bounds.max.z}};

            // Bottom face, top face, then the four uprights.
            static constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                                 {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

            std::size_t drawn = 0;
            for (const auto& edge : edges)
            {
                const std::optional<std::pair<EditorVector2, EditorVector2>> projected =
                    projectSegment(camera, corners[edge[0]], corners[edge[1]]);
                if (!projected) { continue; }
                out.push_back(WireSegment{projected->first, projected->second, color, thickness});
                ++drawn;
            }
            return drawn;
        }
    }

    std::optional<std::pair<EditorVector2, EditorVector2>> projectSegment(const EditorCamera3D& camera,
                                                                          const EditorVector3& from,
                                                                          const EditorVector3& to)
    {
        // A hair in front of the near plane, not on it: a point exactly on the plane divides by a
        // w of zero, and the resulting coordinate is an infinity that draws a line to nowhere.
        const float nearDistance = camera.getNearClipDistance() + 1e-4f;

        float fromDepth = depthOf(camera, from);
        float toDepth = depthOf(camera, to);

        if (fromDepth < nearDistance && toDepth < nearDistance) { return std::nullopt; }

        EditorVector3 clippedFrom = from;
        EditorVector3 clippedTo = to;

        if (fromDepth < nearDistance)
        {
            clippedFrom = interpolate(from, to, (nearDistance - fromDepth) / (toDepth - fromDepth));
        }
        else if (toDepth < nearDistance)
        {
            clippedTo = interpolate(to, from, (nearDistance - toDepth) / (fromDepth - toDepth));
        }

        const std::optional<EditorVector2> screenFrom = camera.worldToScreen(clippedFrom);
        const std::optional<EditorVector2> screenTo = camera.worldToScreen(clippedTo);
        if (!screenFrom || !screenTo) { return std::nullopt; }

        return std::make_pair(*screenFrom, *screenTo);
    }

    std::vector<WireSegment> buildGroundGrid(const EditorCamera3D& camera, const WireframeOptions& options)
    {
        std::vector<WireSegment> segments;
        if (options.gridHalfExtent <= 0) { return segments; }

        float spacing = options.gridSpacing;
        if (spacing <= 0.0f)
        {
            // The same decade stepping the 2D grid uses, fed the pixels-per-world-unit the camera
            // achieves at its pivot. One answer to "how far apart are the lines", so a 2D and a 3D
            // view of one scene do not disagree about what a grid square means.
            const float height = camera.getOrthographicHeight();
            const float pixelsPerUnit =
                height > 0.0f ? camera.getViewportSize().y / height : 1.0f;
            spacing = chooseGridSpacing(pixelsPerUnit, kGridTargetPixels);
        }
        if (spacing <= 0.0f) { return segments; }

        // Centred under the pivot and snapped to the spacing, so flying across a level does not
        // drag the grid's origin along and turn the lines into a shimmering mess.
        const float centerX = std::round(camera.getPivot().x / spacing) * spacing;
        const float centerZ = std::round(camera.getPivot().z / spacing) * spacing;

        const int extent = options.gridHalfExtent;
        const float half = static_cast<float>(extent) * spacing;

        for (int step = -extent; step <= extent; ++step)
        {
            const float offset = static_cast<float>(step) * spacing;
            const float x = centerX + offset;
            const float z = centerZ + offset;

            // The world axes win over the grid, and every tenth line over an ordinary one. Without
            // that a user cannot tell where the origin is, which is the one landmark a 3D view has.
            const bool xIsAxis = std::abs(x) < spacing * 0.5f;
            const bool zIsAxis = std::abs(z) < spacing * 0.5f;
            const bool isMajor = (step % 10) == 0;

            const EditorColor alongZ =
                xIsAxis ? WireColors::kAxisZ : (isMajor ? WireColors::kGridMajor : WireColors::kGrid);
            const EditorColor alongX =
                zIsAxis ? WireColors::kAxisX : (isMajor ? WireColors::kGridMajor : WireColors::kGrid);

            const std::optional<std::pair<EditorVector2, EditorVector2>> lineAlongZ = projectSegment(
                camera, EditorVector3{x, 0.0f, centerZ - half}, EditorVector3{x, 0.0f, centerZ + half});
            if (lineAlongZ)
            {
                segments.push_back(WireSegment{lineAlongZ->first, lineAlongZ->second, alongZ,
                                               xIsAxis ? 2.0f : 1.0f});
            }

            const std::optional<std::pair<EditorVector2, EditorVector2>> lineAlongX = projectSegment(
                camera, EditorVector3{centerX - half, 0.0f, z}, EditorVector3{centerX + half, 0.0f, z});
            if (lineAlongX)
            {
                segments.push_back(WireSegment{lineAlongX->first, lineAlongX->second, alongX,
                                               zIsAxis ? 2.0f : 1.0f});
            }
        }

        return segments;
    }

    WireframeResult buildSceneWireframe(const SceneDocument& scene, const EditorCamera3D& camera,
                                        const std::vector<Uuid>& selection,
                                        const SpriteSizeProvider& sizeProvider,
                                        const WireframeOptions& options)
    {
        WireframeResult result;

        if (options.drawGrid) { result.segments = buildGroundGrid(camera, options); }

        if (!options.drawEntityBounds) { return result; }

        for (const EditorEntity& entity : scene.getEntities())
        {
            if (result.segments.size() >= options.maxSegments)
            {
                // Said out loud rather than stopped quietly: a wireframe that ran out of room
                // looks exactly like a scene missing half its entities.
                result.truncated = true;
                break;
            }

            if (!entity.isEnabled()) { continue; }

            const std::optional<WorldBounds3D> bounds =
                computeEntityBounds3D(scene, entity.getId(), sizeProvider);
            if (!bounds) { continue; }

            const bool selected =
                std::find(selection.begin(), selection.end(), entity.getId()) != selection.end();

            const std::size_t drawn =
                appendBox(result.segments, camera, *bounds,
                          selected ? WireColors::kSelected : WireColors::kEntity, selected ? 2.0f : 1.0f);
            if (drawn > 0) { ++result.entitiesDrawn; }
        }

        return result;
    }

    std::optional<float> intersectRayWithBounds(const WorldRay& ray, const WorldBounds3D& bounds)
    {
        // The slab test: clip the ray against each pair of parallel faces and keep the overlap.
        float entry = -std::numeric_limits<float>::max();
        float exit = std::numeric_limits<float>::max();

        const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
        const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
        const float low[3] = {bounds.min.x, bounds.min.y, bounds.min.z};
        const float high[3] = {bounds.max.x, bounds.max.y, bounds.max.z};

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::abs(direction[axis]) < 1e-8f)
            {
                // Parallel to this pair of faces: a miss unless the ray already lies between them.
                // A flat box -- which is what a sprite is -- is exactly this case on one axis, so
                // getting it wrong makes every sprite unpickable from the side.
                if (origin[axis] < low[axis] || origin[axis] > high[axis]) { return std::nullopt; }
                continue;
            }

            const float inverse = 1.0f / direction[axis];
            float near = (low[axis] - origin[axis]) * inverse;
            float far = (high[axis] - origin[axis]) * inverse;
            if (near > far) { std::swap(near, far); }

            entry = std::max(entry, near);
            exit = std::min(exit, far);
            if (entry > exit) { return std::nullopt; }
        }

        if (exit < 0.0f) { return std::nullopt; }

        // Zero when the ray starts inside the box, which is a hit at the eye rather than a miss.
        return std::max(entry, 0.0f);
    }

    Uuid pickEntityAt3D(const SceneDocument& scene, const EditorCamera3D& camera,
                        const EditorVector2& screenPoint, const SpriteSizeProvider& sizeProvider)
    {
        const WorldRay ray = camera.screenToRay(screenPoint);

        Uuid nearestId;
        float nearestDistance = std::numeric_limits<float>::max();

        for (const EditorEntity& entity : scene.getEntities())
        {
            if (!entity.isEnabled()) { continue; }

            const std::optional<WorldBounds3D> bounds =
                computeEntityBounds3D(scene, entity.getId(), sizeProvider);
            if (!bounds) { continue; }

            const std::optional<float> distance = intersectRayWithBounds(ray, *bounds);
            if (!distance || *distance >= nearestDistance) { continue; }

            nearestDistance = *distance;
            nearestId = entity.getId();
        }

        return nearestId;
    }
}
