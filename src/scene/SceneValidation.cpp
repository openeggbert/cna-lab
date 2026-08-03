// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Scene/SceneValidation.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

namespace CNA::Editor
{
    namespace
    {
        /** @brief Below this, a scale factor collapses the sprite to nothing on any display. */
        constexpr float kZeroScaleEpsilon = 1e-6f;

        SceneIssue makeIssue(SceneIssue::Severity severity,
                             std::string ruleId,
                             const EditorEntity& entity,
                             std::string componentTypeId,
                             std::string message)
        {
            SceneIssue issue;
            issue.severity = severity;
            issue.ruleId = std::move(ruleId);
            issue.entityId = entity.getId();
            issue.entityName = entity.getName();
            issue.componentTypeId = std::move(componentTypeId);
            issue.message = std::move(message);
            return issue;
        }

        /**
         * @brief True when @p entity and every one of its ancestors is enabled.
         *
         * A camera under a disabled parent is not going to render, so counting it as a competing
         * primary camera would report a conflict the user has already resolved -- by switching the
         * other one off, which is exactly how a person switches cameras.
         */
        bool isEffectivelyEnabled(const SceneDocument& scene, const EditorEntity& entity)
        {
            const EditorEntity* current = &entity;
            // Bounded by the entity count: SceneDocument forbids cycles, but a bound costs nothing
            // and turns a corrupted document into a wrong answer rather than a hang.
            for (std::size_t guard = 0; guard <= scene.getEntityCount(); ++guard)
            {
                if (!current->isEnabled()) { return false; }
                if (!current->getParentId().isValid()) { return true; }

                current = scene.findEntity(current->getParentId());
                if (current == nullptr) { return true; }
            }
            return true;
        }

        /** @brief True when the camera component is the one the game is meant to start with. */
        bool isPrimaryCamera(const EditorComponent& camera, const ComponentDescriptor* descriptor)
        {
            return camera.getPropertyOrDefault("isPrimary", descriptor).get<bool>(false);
        }

        void checkCameras(const SceneDocument& scene,
                          const ComponentRegistry& registry,
                          std::vector<SceneIssue>& issues)
        {
            const ComponentDescriptor* descriptor = registry.find(BuiltinComponentIds::kCamera);

            std::vector<const EditorEntity*> cameras;
            std::vector<const EditorEntity*> primaries;

            for (const EditorEntity& entity : scene.getEntities())
            {
                const EditorComponent* camera = entity.findComponent(BuiltinComponentIds::kCamera);
                if (camera == nullptr) { continue; }
                if (!isEffectivelyEnabled(scene, entity)) { continue; }

                cameras.push_back(&entity);
                if (isPrimaryCamera(*camera, descriptor)) { primaries.push_back(&entity); }
            }

            if (cameras.empty()) { return; }

            if (primaries.empty())
            {
                // Scene-wide rather than pinned to an entity: there is no offending entity to
                // select, and blaming an arbitrary camera for the absence of a decision would send
                // the user to the wrong row.
                SceneIssue issue;
                issue.severity = SceneIssue::Severity::Warning;
                issue.ruleId = "no-primary-camera";
                issue.message = "No camera is marked primary. The game has nothing to start with.";
                issues.push_back(std::move(issue));
                return;
            }

            if (primaries.size() == 1) { return; }

            // One issue per offending camera, so every row selects a real entity. A single
            // scene-wide issue would name them and select none of them.
            for (const EditorEntity* entity : primaries)
            {
                issues.push_back(makeIssue(
                    SceneIssue::Severity::Error, "duplicate-primary-camera", *entity,
                    BuiltinComponentIds::kCamera,
                    "Marked primary, and so are " + std::to_string(primaries.size() - 1) +
                        " other camera(s). Which one the game starts with is arbitrary."));
            }
        }

        void checkCameraPlanes(const EditorEntity& entity,
                               const EditorComponent& camera,
                               const ComponentDescriptor* descriptor,
                               std::vector<SceneIssue>& issues)
        {
            const float nearPlane = camera.getPropertyOrDefault("nearPlane", descriptor).get<float>(0.0f);
            const float farPlane = camera.getPropertyOrDefault("farPlane", descriptor).get<float>(0.0f);

            if (nearPlane < farPlane) { return; }

            issues.push_back(makeIssue(
                SceneIssue::Severity::Error, "camera-planes-inverted", entity,
                BuiltinComponentIds::kCamera,
                "Near plane (" + std::to_string(nearPlane) + ") is not in front of the far plane (" +
                    std::to_string(farPlane) + "). The projection matrix is degenerate."));
        }

        void checkTransform(const EditorEntity& entity,
                            const EditorComponent& transform,
                            const ComponentDescriptor* descriptor,
                            std::vector<SceneIssue>& issues)
        {
            const EditorVector3 scale =
                transform.getPropertyOrDefault("scale", descriptor).get<EditorVector3>(EditorVector3{1.0f, 1.0f, 1.0f});

            const bool collapsed = std::fabs(scale.x) < kZeroScaleEpsilon ||
                                   std::fabs(scale.y) < kZeroScaleEpsilon ||
                                   std::fabs(scale.z) < kZeroScaleEpsilon;
            if (!collapsed) { return; }

            // A warning, not an error: hiding something by scaling it to zero is legal, and the
            // scene still runs. It is reported because the usual cause is a scale that was never
            // set rather than one that was, and because the symptom -- nothing drawn -- looks
            // identical to a missing texture.
            issues.push_back(makeIssue(
                SceneIssue::Severity::Warning, "zero-scale", entity, BuiltinComponentIds::kTransform,
                "Scale is zero on at least one axis, so this entity and its children draw nothing. "
                "Disabling the entity says the same thing more clearly."));
        }

        void checkSprite(const EditorEntity& entity,
                         const EditorComponent& sprite,
                         std::vector<SceneIssue>& issues)
        {
            const PropertyValue texture = sprite.getProperty("texture");
            if (texture.getType() == PropertyType::AssetReference &&
                texture.get<PropertyValue::AssetReference>().id.isValid())
            {
                return;
            }

            // Deliberately separate from the missing-reference report: that one answers "this id
            // will not load", this one answers "there is no id at all". An empty slot is a normal
            // state while building a scene, so it is only ever a warning.
            issues.push_back(makeIssue(
                SceneIssue::Severity::Warning, "sprite-without-texture", entity,
                BuiltinComponentIds::kSpriteRenderer,
                "Sprite Renderer has no texture, so it draws nothing."));
        }

        void checkComponentSet(const EditorEntity& entity,
                               const ComponentRegistry& registry,
                               std::vector<SceneIssue>& issues)
        {
            std::vector<std::string> reportedDuplicates;

            for (const EditorComponent& component : entity.getComponents())
            {
                const ComponentDescriptor* descriptor = registry.find(component.getTypeId());
                if (descriptor == nullptr)
                {
                    issues.push_back(makeIssue(
                        SceneIssue::Severity::Warning, "unknown-component-type", entity,
                        component.getTypeId(),
                        "Component type \"" + component.getTypeId() +
                            "\" is not registered. Its data is preserved, but nothing can edit or "
                            "run it -- a plugin is probably missing."));
                    continue;
                }

                if (!descriptor->unique) { continue; }

                const std::size_t count = static_cast<std::size_t>(
                    std::count_if(entity.getComponents().begin(), entity.getComponents().end(),
                                  [&](const EditorComponent& other)
                                  { return other.getTypeId() == component.getTypeId(); }));
                if (count < 2) { continue; }

                if (std::find(reportedDuplicates.begin(), reportedDuplicates.end(), component.getTypeId()) !=
                    reportedDuplicates.end())
                {
                    continue;
                }
                reportedDuplicates.push_back(component.getTypeId());

                issues.push_back(makeIssue(
                    SceneIssue::Severity::Error, "duplicate-component", entity, component.getTypeId(),
                    "Has " + std::to_string(count) + " " + descriptor->displayName +
                        " components, but the type allows one. Which one wins is arbitrary."));
            }

            // Required means "cannot be removed", which is only enforceable on components an
            // entity already has. A scene file written by hand, or by an older build, can still
            // arrive without one -- and an entity with no Transform has no position at all.
            for (const std::string& typeId : registry.getTypeIds())
            {
                const ComponentDescriptor* descriptor = registry.find(typeId);
                if (descriptor == nullptr || !descriptor->required) { continue; }
                if (entity.findComponent(typeId) != nullptr) { continue; }

                issues.push_back(makeIssue(
                    SceneIssue::Severity::Error, "missing-required-component", entity, typeId,
                    "Has no " + descriptor->displayName +
                        " component, which every entity is required to have."));
            }
        }

        void checkEmptyEntity(const EditorEntity& entity,
                              const ComponentRegistry& registry,
                              const std::unordered_set<Uuid>& parents,
                              std::vector<SceneIssue>& issues)
        {
            // An entity with children is a group, and a group that carries nothing but a transform
            // is the normal way to move a set of things together.
            if (parents.count(entity.getId()) != 0) { return; }

            for (const EditorComponent& component : entity.getComponents())
            {
                const ComponentDescriptor* descriptor = registry.find(component.getTypeId());

                // An unknown type counts as doing something: the editor cannot see what it does,
                // which is not the same as knowing that it does nothing.
                if (descriptor == nullptr || !descriptor->required) { return; }
            }

            issues.push_back(makeIssue(
                SceneIssue::Severity::Warning, "empty-entity", entity, {},
                "Has no components beyond the required ones and no children, so it does nothing."));
        }
    }

    const char* toString(SceneIssue::Severity severity)
    {
        switch (severity)
        {
            case SceneIssue::Severity::Warning: return "warning";
            case SceneIssue::Severity::Error: return "error";
        }
        return "issue";
    }

    std::vector<SceneIssue> validateScene(const SceneDocument& scene, const ComponentRegistry& registry)
    {
        std::vector<SceneIssue> issues;

        checkCameras(scene, registry, issues);

        // Derived once rather than per entity: getChildren() is a scan, and asking it for every
        // entity would turn the report into O(n^2) on exactly the large scenes that need it most.
        std::unordered_set<Uuid> parents;
        for (const EditorEntity& entity : scene.getEntities())
        {
            if (entity.getParentId().isValid()) { parents.insert(entity.getParentId()); }
        }

        for (const EditorEntity& entity : scene.getEntities())
        {
            checkComponentSet(entity, registry, issues);

            if (const EditorComponent* transform = entity.findComponent(BuiltinComponentIds::kTransform))
            {
                checkTransform(entity, *transform, registry.find(BuiltinComponentIds::kTransform), issues);
            }

            if (const EditorComponent* sprite = entity.findComponent(BuiltinComponentIds::kSpriteRenderer))
            {
                checkSprite(entity, *sprite, issues);
            }

            if (const EditorComponent* camera = entity.findComponent(BuiltinComponentIds::kCamera))
            {
                checkCameraPlanes(entity, *camera, registry.find(BuiltinComponentIds::kCamera), issues);
            }

            checkEmptyEntity(entity, registry, parents, issues);
        }

        return issues;
    }

    std::size_t countIssues(const std::vector<SceneIssue>& issues, SceneIssue::Severity severity)
    {
        return static_cast<std::size_t>(
            std::count_if(issues.begin(), issues.end(),
                          [severity](const SceneIssue& issue) { return issue.severity == severity; }));
    }
}
