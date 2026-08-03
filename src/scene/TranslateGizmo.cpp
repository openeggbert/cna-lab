// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Scene/TranslateGizmo.hpp"

#include <algorithm>
#include <cmath>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

namespace CNA::Editor
{
    namespace
    {
        /** @brief Returns the distance from @p point to the segment @p a -- @p b. */
        float distanceToSegment(const EditorVector2& point, const EditorVector2& a, const EditorVector2& b)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float lengthSquared = dx * dx + dy * dy;
            if (lengthSquared <= 0.0f)
            {
                return std::hypot(point.x - a.x, point.y - a.y);
            }

            // Projection parameter, clamped so the nearest point stays on the segment rather than
            // on its infinite line -- otherwise the arms would be grabbable far past their tips.
            const float t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared,
                                       0.0f, 1.0f);
            return std::hypot(point.x - (a.x + dx * t), point.y - (a.y + dy * t));
        }
    }

    std::optional<TranslateGizmoLayout> computeTranslateGizmoLayout(const SceneDocument& scene,
                                                                    const EditorCamera2D& camera,
                                                                    const Uuid& entityId)
    {
        const EditorEntity* entity = scene.findEntity(entityId);
        if (entity == nullptr) { return std::nullopt; }
        if (entity->findComponent(BuiltinComponentIds::kTransform) == nullptr) { return std::nullopt; }

        const std::optional<WorldTransform> world = computeWorldTransform(scene, entityId);
        if (!world) { return std::nullopt; }

        TranslateGizmoLayout layout;
        layout.origin = camera.worldToScreen(EditorVector2{world->position.x, world->position.y});
        return layout;
    }

    GizmoHandle hitTestTranslateGizmo(const TranslateGizmoLayout& layout, const EditorVector2& screenPoint)
    {
        // The centre is tested first and wins where it overlaps the arms: it is the smaller target,
        // and it is what a user aiming at the middle expects. The arms stay reachable along the
        // rest of their length.
        if (std::fabs(screenPoint.x - layout.origin.x) <= layout.centerExtent
            && std::fabs(screenPoint.y - layout.origin.y) <= layout.centerExtent)
        {
            return GizmoHandle::Both;
        }

        if (distanceToSegment(screenPoint, layout.origin, layout.getXTip()) <= layout.grabTolerance)
        {
            return GizmoHandle::XAxis;
        }
        if (distanceToSegment(screenPoint, layout.origin, layout.getYTip()) <= layout.grabTolerance)
        {
            return GizmoHandle::YAxis;
        }

        return GizmoHandle::None;
    }

    EditorVector2 worldDeltaToLocal(const SceneDocument& scene,
                                    const Uuid& entityId,
                                    const EditorVector2& worldDelta)
    {
        const EditorEntity* entity = scene.findEntity(entityId);
        if (entity == nullptr) { return worldDelta; }

        const Uuid parentId = entity->getParentId();
        if (!parentId.isValid()) { return worldDelta; }

        const std::optional<WorldTransform> parent = computeWorldTransform(scene, parentId);
        if (!parent) { return worldDelta; }

        // Undo the parent's rotation, then its scale -- the inverse of how a local offset becomes a
        // world one in computeWorldTransform. Skipping this gives a gizmo that is exact on root
        // entities and drifts on every child of a rotated or scaled parent.
        const EditorQuaternion inverseRotation{-parent->rotation.x, -parent->rotation.y,
                                               -parent->rotation.z, parent->rotation.w};
        const EditorVector3 unrotated = rotate(inverseRotation, EditorVector3{worldDelta.x, worldDelta.y, 0.0f});

        const float scaleX = parent->scale.x != 0.0f ? parent->scale.x : 1.0f;
        const float scaleY = parent->scale.y != 0.0f ? parent->scale.y : 1.0f;
        return EditorVector2{unrotated.x / scaleX, unrotated.y / scaleY};
    }

    bool TranslateGizmoDrag::begin(const SceneDocument& scene,
                                   const EditorCamera2D& camera,
                                   const Uuid& entityId,
                                   GizmoHandle handle,
                                   const EditorVector2& screenPoint)
    {
        end();
        if (handle == GizmoHandle::None) { return false; }

        const EditorEntity* entity = scene.findEntity(entityId);
        if (entity == nullptr) { return false; }

        const EditorComponent* transform = entity->findComponent(BuiltinComponentIds::kTransform);
        if (transform == nullptr) { return false; }

        handle_ = handle;
        entityId_ = entityId;
        grabWorld_ = camera.screenToWorld(screenPoint);
        startLocalPosition_ = transform->getProperty("position").get<EditorVector3>();
        return true;
    }

    std::optional<EditorVector3> TranslateGizmoDrag::update(const SceneDocument& scene,
                                                            const EditorCamera2D& camera,
                                                            const EditorVector2& screenPoint) const
    {
        if (!isActive()) { return std::nullopt; }

        // Measured from where the drag *began*, never accumulated frame to frame. Accumulating is
        // the classic source of gizmo drift: rounding compounds and the object separates from the
        // cursor over a long drag.
        const EditorVector2 nowWorld = camera.screenToWorld(screenPoint);
        EditorVector2 worldDelta{nowWorld.x - grabWorld_.x, nowWorld.y - grabWorld_.y};

        if (handle_ == GizmoHandle::XAxis) { worldDelta.y = 0.0f; }
        if (handle_ == GizmoHandle::YAxis) { worldDelta.x = 0.0f; }

        const EditorVector2 localDelta = worldDeltaToLocal(scene, entityId_, worldDelta);
        return EditorVector3{startLocalPosition_.x + localDelta.x,
                             startLocalPosition_.y + localDelta.y,
                             startLocalPosition_.z};
    }
}
