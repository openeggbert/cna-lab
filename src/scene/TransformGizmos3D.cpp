// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Scene/TransformGizmos3D.hpp"

#include <algorithm>
#include <cmath>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

namespace CNA::Editor
{
    namespace
    {
        /** @brief The arms' colours: X red, Y green, Z blue, as every 3D tool since the first. */
        constexpr std::array<EditorColor, 3> kAxisColors{
            EditorColor{214, 92, 92, 255}, EditorColor{110, 200, 110, 255}, EditorColor{96, 132, 220, 255}};

        /** @brief The colour of the arm being dragged or hovered. */
        constexpr EditorColor kActiveColor{255, 210, 70, 255};

        /** @brief Returns the distance from @p point to the segment @p from -> @p to, in pixels. */
        float distanceToSegment(const EditorVector2& point, const EditorVector2& from,
                                const EditorVector2& to)
        {
            const float dx = to.x - from.x;
            const float dy = to.y - from.y;
            const float lengthSquared = dx * dx + dy * dy;
            if (lengthSquared <= 0.0f) { return std::hypot(point.x - from.x, point.y - from.y); }

            float t = ((point.x - from.x) * dx + (point.y - from.y) * dy) / lengthSquared;
            t = std::max(0.0f, std::min(1.0f, t));

            return std::hypot(point.x - (from.x + dx * t), point.y - (from.y + dy * t));
        }

        /** @brief Returns how many world units one viewport pixel spans at @p point's depth. */
        float worldUnitsPerPixelAt(const EditorCamera3D& camera, const EditorVector3& point)
        {
            const EditorVector2 viewport = camera.getViewportSize();
            if (viewport.y <= 0.0f) { return 1.0f; }

            if (camera.getProjection() == CameraProjection::Orthographic)
            {
                return camera.getOrthographicHeight() / viewport.y;
            }

            // A perspective gizmo has to be sized at the *entity's* depth, not the pivot's: an
            // entity twice as far away is drawn half the size, and an arm sized for the pivot
            // would be a manipulator that grows and shrinks as the user selects things.
            const float depth = dot(subtract(point, camera.getEye()), camera.getForward());
            const float usable = std::max(depth, camera.getNearPlane());
            return 2.0f * usable * std::tan(camera.getFieldOfView() * 0.5f) / viewport.y;
        }

        /** @brief Returns @p index's axis enumerator. */
        GizmoAxis3D axisAt(std::size_t index)
        {
            switch (index)
            {
                case 0: return GizmoAxis3D::X;
                case 1: return GizmoAxis3D::Y;
                default: return GizmoAxis3D::Z;
            }
        }
    }

    const char* toString(GizmoAxis3D axis)
    {
        switch (axis)
        {
            case GizmoAxis3D::None: return "None";
            case GizmoAxis3D::X: return "X";
            case GizmoAxis3D::Y: return "Y";
            case GizmoAxis3D::Z: return "Z";
        }
        return "None";
    }

    std::optional<TranslateGizmo3DLayout> computeTranslateGizmo3DLayout(const SceneDocument& scene,
                                                                        const EditorCamera3D& camera,
                                                                        const Uuid& entityId,
                                                                        GizmoSpace space)
    {
        const std::optional<WorldTransform> world = computeWorldTransform(scene, entityId);
        if (!world) { return std::nullopt; }

        TranslateGizmo3DLayout layout;
        layout.origin = world->position;

        if (space == GizmoSpace::Local)
        {
            layout.axes = {rotate(world->rotation, EditorVector3{1.0f, 0.0f, 0.0f}),
                           rotate(world->rotation, EditorVector3{0.0f, 1.0f, 0.0f}),
                           rotate(world->rotation, EditorVector3{0.0f, 0.0f, 1.0f})};
        }

        // Sized in pixels and converted to world units, so the manipulator is the same size on
        // screen wherever the entity is -- the property that makes it grabbable at any zoom.
        layout.armLength = kGizmo3DScreenLength * worldUnitsPerPixelAt(camera, layout.origin);

        const std::optional<EditorVector2> screenOrigin = camera.worldToScreen(layout.origin);
        if (!screenOrigin)
        {
            // Behind the eye: there is nothing to draw and nothing to grab. Returning a layout
            // with a plausible-looking origin would put a manipulator on screen for an entity the
            // user cannot see.
            return std::nullopt;
        }
        layout.screenOrigin = *screenOrigin;

        for (std::size_t index = 0; index < 3; ++index)
        {
            const EditorVector3 tip = add(layout.origin, scale(layout.axes[index], layout.armLength));
            const std::optional<EditorVector2> screenTip = camera.worldToScreen(tip);

            layout.armVisible[index] = screenTip.has_value();
            layout.screenTips[index] = screenTip.value_or(layout.screenOrigin);
        }

        return layout;
    }

    GizmoAxis3D hitTestTranslateGizmo3D(const TranslateGizmo3DLayout& layout,
                                        const EditorVector2& screenPoint)
    {
        GizmoAxis3D best = GizmoAxis3D::None;
        float bestDistance = layout.grabTolerance;

        for (std::size_t index = 0; index < 3; ++index)
        {
            if (!layout.armVisible[index]) { continue; }

            const float distance =
                distanceToSegment(screenPoint, layout.screenOrigin, layout.screenTips[index]);

            // Strictly nearer, so that where two arms overlap -- which happens exactly when one
            // points almost at the camera -- the one the cursor is actually closest to wins,
            // rather than whichever comes first in a fixed order.
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = axisAt(index);
            }
        }

        return best;
    }

    std::vector<WireSegment> buildTranslateGizmo3DSegments(const TranslateGizmo3DLayout& layout,
                                                           GizmoAxis3D active)
    {
        std::vector<WireSegment> segments;
        segments.reserve(3);

        for (std::size_t index = 0; index < 3; ++index)
        {
            if (!layout.armVisible[index]) { continue; }

            const bool highlighted = active == axisAt(index);
            segments.push_back(WireSegment{layout.screenOrigin, layout.screenTips[index],
                                           highlighted ? kActiveColor : kAxisColors[index],
                                           highlighted ? 3.0f : 2.0f});
        }

        return segments;
    }

    EditorVector3 worldDeltaToLocal3D(const SceneDocument& scene, const Uuid& entityId,
                                      const EditorVector3& worldDelta)
    {
        const EditorEntity* entity = scene.findEntity(entityId);
        if (entity == nullptr || !entity->getParentId().isValid()) { return worldDelta; }

        const std::optional<WorldTransform> parent = computeWorldTransform(scene, entity->getParentId());
        if (!parent) { return worldDelta; }

        // Undo the parent's rotation, then its scale. A zero scale on an axis is left alone rather
        // than divided by: the entity cannot be moved along an axis its parent has flattened, and
        // an infinity there would put it somewhere no undo could find.
        const EditorQuaternion inverse{-parent->rotation.x, -parent->rotation.y, -parent->rotation.z,
                                       parent->rotation.w};
        EditorVector3 local = rotate(inverse, worldDelta);

        if (parent->scale.x != 0.0f) { local.x /= parent->scale.x; }
        if (parent->scale.y != 0.0f) { local.y /= parent->scale.y; }
        if (parent->scale.z != 0.0f) { local.z /= parent->scale.z; }
        return local;
    }

    std::optional<float> closestPointOnAxis(const WorldRay& ray, const EditorVector3& origin,
                                            const EditorVector3& axis)
    {
        const float axisDotRay = dot(axis, ray.direction);
        const float denominator = 1.0f - axisDotRay * axisDotRay;

        // Parallel, or near enough that the answer is dominated by rounding: an arm pointing
        // straight at the camera, where a pixel of movement would otherwise fling the entity
        // across the level. Refusing is the only honest answer.
        if (denominator < 1e-4f) { return std::nullopt; }

        // The standard two-line closest-approach solution, with both directions unit length:
        // t = (b*e - d) / (1 - b*b), where b is the angle between them, d is the axis component of
        // the offset between their origins and e the ray's.
        const EditorVector3 toOrigin = subtract(origin, ray.origin);
        const float d = dot(axis, toOrigin);
        const float e = dot(ray.direction, toOrigin);
        return (axisDotRay * e - d) / denominator;
    }

    bool TranslateGizmo3DDrag::begin(const SceneDocument& scene, const EditorCamera3D& camera,
                                     const TranslateGizmo3DLayout& layout, const Uuid& entityId,
                                     const EditorVector2& cursor)
    {
        end();

        const GizmoAxis3D grabbed = hitTestTranslateGizmo3D(layout, cursor);
        if (grabbed == GizmoAxis3D::None) { return false; }

        const EditorEntity* entity = scene.findEntity(entityId);
        if (entity == nullptr) { return false; }

        const EditorComponent* transform = entity->findComponent(BuiltinComponentIds::kTransform);
        if (transform == nullptr) { return false; }

        const std::size_t index = grabbed == GizmoAxis3D::X ? 0 : (grabbed == GizmoAxis3D::Y ? 1 : 2);
        const EditorVector3 direction = layout.axes[index];

        const std::optional<float> parameter =
            closestPointOnAxis(camera.screenToRay(cursor), layout.origin, direction);
        if (!parameter) { return false; }

        axis_ = grabbed;
        entityId_ = entityId;
        direction_ = direction;
        startWorld_ = layout.origin;
        startLocal_ = transform->getProperty("position").get<EditorVector3>();
        startParameter_ = *parameter;
        return true;
    }

    void MultiTranslate3D::begin(const SceneDocument& scene, const std::vector<Uuid>& entityIds)
    {
        end();

        for (const Uuid& entityId : findSelectionRoots(scene, entityIds))
        {
            const EditorEntity* entity = scene.findEntity(entityId);
            if (entity == nullptr) { continue; }

            const EditorComponent* transform = entity->findComponent(BuiltinComponentIds::kTransform);
            if (transform == nullptr) { continue; }

            entries_.push_back(Entry{entityId, transform->getProperty("position").get<EditorVector3>()});
        }
    }

    std::vector<EntityTransformEdit> MultiTranslate3D::translate(const SceneDocument& scene,
                                                                 const EditorVector3& worldDelta) const
    {
        std::vector<EntityTransformEdit> edits;
        edits.reserve(entries_.size());

        for (const Entry& entry : entries_)
        {
            // Converted per entity, because the same world delta is a different local one under
            // each parent -- two siblings under differently rotated rigs move together in the
            // world and store different numbers.
            EntityTransformEdit edit;
            edit.entityId = entry.entityId;
            edit.position = add(entry.startLocal, worldDeltaToLocal3D(scene, entry.entityId, worldDelta));
            edits.push_back(edit);
        }

        return edits;
    }

    std::optional<EditorVector3> TranslateGizmo3DDrag::getWorldDelta(const EditorCamera3D& camera,
                                                                     const EditorVector2& cursor,
                                                                     const GizmoSnap& snap) const
    {
        if (!isActive()) { return std::nullopt; }

        const std::optional<float> parameter =
            closestPointOnAxis(camera.screenToRay(cursor), startWorld_, direction_);
        if (!parameter) { return std::nullopt; }

        EditorVector3 world = add(startWorld_, scale(direction_, *parameter - startParameter_));

        if (snap.translate > 0.0f)
        {
            // The *result* is snapped, not the movement, so a dragged entity lands on the grid
            // rather than a grid-sized distance from wherever it started. Along the axis only:
            // rounding a coordinate the drag never touched would move the entity on an axis the
            // user deliberately constrained out.
            const float along = dot(world, direction_);
            const float rounded = std::round(along / snap.translate) * snap.translate;
            world = add(world, scale(direction_, rounded - along));
        }

        const EditorVector3 delta = subtract(world, startWorld_);
        if (delta == EditorVector3{}) { return std::nullopt; }
        return delta;
    }

    std::optional<EditorVector3> TranslateGizmo3DDrag::update(const SceneDocument& scene,
                                                              const EditorCamera3D& camera,
                                                              const EditorVector2& cursor,
                                                              const GizmoSnap& snap)
    {
        // The single-entity answer, computed from the same gesture a whole selection uses, so one
        // entity and twenty cannot disagree about how far the cursor went.
        const std::optional<EditorVector3> delta = getWorldDelta(camera, cursor, snap);
        if (!delta) { return std::nullopt; }

        return add(startLocal_, worldDeltaToLocal3D(scene, entityId_, *delta));
    }

}
