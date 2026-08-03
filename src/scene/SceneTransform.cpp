// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Scene/SceneTransform.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

namespace CNA::Editor
{
    void WorldBounds2D::encapsulate(const EditorVector2& point)
    {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
    }

    WorldBounds2D WorldBounds2D::combine(const WorldBounds2D& a, const WorldBounds2D& b)
    {
        if (a.isEmpty()) { return b; }
        if (b.isEmpty()) { return a; }

        WorldBounds2D result = a;
        result.encapsulate(b.min);
        result.encapsulate(b.max);
        return result;
    }

    EditorQuaternion multiply(const EditorQuaternion& a, const EditorQuaternion& b)
    {
        return EditorQuaternion{
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
    }

    EditorVector3 rotate(const EditorQuaternion& rotation, const EditorVector3& vector)
    {
        // v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v). Cheaper than building a matrix and
        // avoids the normalisation a matrix path would want.
        const float x = rotation.x;
        const float y = rotation.y;
        const float z = rotation.z;
        const float w = rotation.w;

        const float tx = 2.0f * (y * vector.z - z * vector.y);
        const float ty = 2.0f * (z * vector.x - x * vector.z);
        const float tz = 2.0f * (x * vector.y - y * vector.x);

        return EditorVector3{
            vector.x + w * tx + (y * tz - z * ty),
            vector.y + w * ty + (z * tx - x * tz),
            vector.z + w * tz + (x * ty - y * tx)};
    }

    EditorQuaternion quaternionFromZRotation(float radians)
    {
        const float half = radians * 0.5f;
        return EditorQuaternion{0.0f, 0.0f, std::sin(half), std::cos(half)};
    }

    float zRotationOf(const EditorQuaternion& rotation)
    {
        return std::atan2(2.0f * (rotation.w * rotation.z + rotation.x * rotation.y),
                          1.0f - 2.0f * (rotation.y * rotation.y + rotation.z * rotation.z));
    }

    std::optional<WorldTransform> computeWorldTransform(const SceneDocument& scene, const Uuid& entityId)
    {
        if (scene.findEntity(entityId) == nullptr) { return std::nullopt; }

        // Walk up to the root collecting the chain, then compose downwards. Composing on the way
        // up would require inverting each step, and the chains here are a handful of links deep.
        std::vector<const EditorEntity*> chain;
        Uuid current = entityId;
        for (std::size_t step = 0; step <= scene.getEntityCount(); ++step)
        {
            const EditorEntity* entity = scene.findEntity(current);
            if (entity == nullptr) { break; }
            chain.push_back(entity);
            current = entity->getParentId();
            if (!current.isValid()) { break; }
        }

        WorldTransform world;
        for (auto iterator = chain.rbegin(); iterator != chain.rend(); ++iterator)
        {
            const EditorComponent* transform = (*iterator)->findComponent(BuiltinComponentIds::kTransform);

            EditorVector3 localPosition;
            EditorQuaternion localRotation;
            EditorVector3 localScale{1.0f, 1.0f, 1.0f};

            if (transform != nullptr)
            {
                localPosition = transform->getProperty("position").get<EditorVector3>(localPosition);
                localRotation = transform->getProperty("rotation").get<EditorQuaternion>(localRotation);
                localScale = transform->getProperty("scale").get<EditorVector3>(localScale);
            }

            // Standard TRS composition: the child's local offset is scaled and rotated by the
            // parent before being added to the parent's position.
            const EditorVector3 scaled{localPosition.x * world.scale.x,
                                       localPosition.y * world.scale.y,
                                       localPosition.z * world.scale.z};
            const EditorVector3 rotated = rotate(world.rotation, scaled);

            world.position = EditorVector3{world.position.x + rotated.x,
                                           world.position.y + rotated.y,
                                           world.position.z + rotated.z};
            world.rotation = multiply(world.rotation, localRotation);
            world.scale = EditorVector3{world.scale.x * localScale.x,
                                        world.scale.y * localScale.y,
                                        world.scale.z * localScale.z};
        }

        return world;
    }

    std::optional<WorldBounds2D> computeEntityBounds2D(const SceneDocument& scene,
                                                       const Uuid& entityId,
                                                       const SpriteSizeProvider& sizeProvider)
    {
        const EditorEntity* entity = scene.findEntity(entityId);
        if (entity == nullptr) { return std::nullopt; }

        const EditorComponent* sprite = entity->findComponent(BuiltinComponentIds::kSpriteRenderer);
        if (sprite == nullptr) { return std::nullopt; }

        const std::optional<WorldTransform> world = computeWorldTransform(scene, entityId);
        if (!world) { return std::nullopt; }

        // Size, in order of preference: the source rectangle when it selects a sub-region, then the
        // whole texture, then a fixed extent. The last matters -- a sprite whose texture failed to
        // import must still be clickable, or the entity cannot be selected and therefore cannot be
        // fixed.
        EditorVector2 size;
        const EditorRectangle source = sprite->getProperty("sourceRectangle").get<EditorRectangle>();
        if (!source.isEmpty())
        {
            size = EditorVector2{static_cast<float>(source.width), static_cast<float>(source.height)};
        }
        else
        {
            const Uuid textureId = sprite->getProperty("texture").get<PropertyValue::AssetReference>().id;
            if (sizeProvider && textureId.isValid()) { size = sizeProvider(textureId); }
        }
        if (size.x <= 0.0f || size.y <= 0.0f)
        {
            size = EditorVector2{kUnknownSpriteExtent, kUnknownSpriteExtent};
        }

        const EditorVector2 origin = sprite->getProperty("origin").get<EditorVector2>();

        // Corners in the sprite's own space, relative to its origin, then scaled, rotated and
        // translated into the world. Rotating first and taking the AABB afterwards is what makes a
        // rotated sprite's box actually cover it.
        const float left = -origin.x;
        const float top = -origin.y;
        const float right = left + size.x;
        const float bottom = top + size.y;

        const EditorVector2 corners[4] = {
            EditorVector2{left, top},
            EditorVector2{right, top},
            EditorVector2{right, bottom},
            EditorVector2{left, bottom},
        };

        WorldBounds2D bounds;
        bounds.min = EditorVector2{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        bounds.max = EditorVector2{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};

        for (const EditorVector2& corner : corners)
        {
            const EditorVector3 scaledCorner{corner.x * world->scale.x, corner.y * world->scale.y, 0.0f};
            const EditorVector3 rotatedCorner = rotate(world->rotation, scaledCorner);
            bounds.encapsulate(EditorVector2{world->position.x + rotatedCorner.x,
                                             world->position.y + rotatedCorner.y});
        }

        return bounds;
    }

    std::optional<WorldBounds2D> computeHierarchyBounds2D(const SceneDocument& scene,
                                                          const Uuid& entityId,
                                                          const SpriteSizeProvider& sizeProvider)
    {
        if (scene.findEntity(entityId) == nullptr) { return std::nullopt; }

        std::optional<WorldBounds2D> result = computeEntityBounds2D(scene, entityId, sizeProvider);

        for (const Uuid& childId : scene.getChildren(entityId))
        {
            const std::optional<WorldBounds2D> childBounds =
                computeHierarchyBounds2D(scene, childId, sizeProvider);
            if (!childBounds) { continue; }
            result = result ? WorldBounds2D::combine(*result, *childBounds) : childBounds;
        }

        return result;
    }
}
