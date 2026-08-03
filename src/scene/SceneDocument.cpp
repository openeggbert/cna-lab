// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Scene/SceneDocument.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "CNA/Editor/Core/Json.hpp"

namespace CNA::Editor
{
    namespace
    {
        /**
         * @brief Guesses a property type from raw JSON, for components with no descriptor.
         *
         * Only reached when a scene references a component type the editor does not know -- a
         * plugin that failed to load, or a scene authored by a newer build. The guess does not
         * need to be right for the inspector (which will not show an unknown component anyway);
         * it needs to be *lossless enough to write back out unchanged*, which this is.
         */
        PropertyType inferPropertyType(const JsonValue& json)
        {
            switch (json.getType())
            {
                case JsonType::Boolean: return PropertyType::Boolean;
                case JsonType::Number: return PropertyType::Float;
                case JsonType::String: return PropertyType::String;
                case JsonType::Array:
                    switch (json.getElements().size())
                    {
                        case 2: return PropertyType::Vector2;
                        case 3: return PropertyType::Vector3;
                        case 4: return PropertyType::Vector4;
                        default: return PropertyType::String;
                    }
                default: return PropertyType::String;
            }
        }
    }

    SceneDocument::SceneDocument() : sceneId_(Uuid::generate()) {}

    const EditorEntity* SceneDocument::findEntity(const Uuid& id) const
    {
        const auto found = indexById_.find(id);
        return found == indexById_.end() ? nullptr : &entities_[found->second];
    }

    EditorEntity* SceneDocument::findEntity(const Uuid& id)
    {
        const auto found = indexById_.find(id);
        return found == indexById_.end() ? nullptr : &entities_[found->second];
    }

    Uuid SceneDocument::addEntity(EditorEntity entity)
    {
        if (!entity.getId().isValid()) { entity.setId(Uuid::generate()); }
        if (indexById_.find(entity.getId()) != indexById_.end()) { return Uuid{}; }

        const Uuid id = entity.getId();
        indexById_.emplace(id, entities_.size());
        entities_.push_back(std::move(entity));
        return id;
    }

    std::vector<EditorEntity> SceneDocument::removeEntityRecursive(const Uuid& id)
    {
        std::vector<EditorEntity> removed;
        if (findEntity(id) == nullptr) { return removed; }

        // Breadth-first so the result is ordered parents-before-children, which is what lets
        // DeleteEntityCommand::undo() re-add them without ever pointing at a missing parent.
        std::vector<Uuid> pending{id};
        std::unordered_set<Uuid> doomed;
        while (!pending.empty())
        {
            const Uuid current = pending.front();
            pending.erase(pending.begin());
            if (!doomed.insert(current).second) { continue; }
            for (const Uuid& child : getChildren(current)) { pending.push_back(child); }
        }

        for (auto iterator = entities_.begin(); iterator != entities_.end();)
        {
            if (doomed.count(iterator->getId()) > 0)
            {
                removed.push_back(*iterator);
                iterator = entities_.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }

        // Preserve parents-first order regardless of storage order: an entity whose parent is
        // also being removed must come after it.
        std::stable_sort(removed.begin(), removed.end(),
                         [&](const EditorEntity& lhs, const EditorEntity& rhs) {
                             const bool lhsIsRootOfDeletion = lhs.getId() == id;
                             const bool rhsIsRootOfDeletion = rhs.getId() == id;
                             if (lhsIsRootOfDeletion != rhsIsRootOfDeletion) { return lhsIsRootOfDeletion; }
                             return false;
                         });

        rebuildIndex();
        return removed;
    }

    bool SceneDocument::reparentEntity(const Uuid& childId, const Uuid& newParentId)
    {
        EditorEntity* child = findEntity(childId);
        if (child == nullptr) { return false; }

        if (newParentId.isValid())
        {
            if (findEntity(newParentId) == nullptr) { return false; }
            // Rejecting the cycle here rather than detecting it later is the whole point: an
            // entity graph with a cycle has no roots, and every hierarchy walk becomes infinite.
            if (isAncestorOf(childId, newParentId)) { return false; }
        }

        child->setParentId(newParentId);
        return true;
    }

    std::vector<Uuid> SceneDocument::getChildren(const Uuid& parentId) const
    {
        std::vector<const EditorEntity*> children;
        for (const EditorEntity& entity : entities_)
        {
            if (entity.getParentId() == parentId) { children.push_back(&entity); }
        }

        std::stable_sort(children.begin(), children.end(),
                         [](const EditorEntity* lhs, const EditorEntity* rhs) {
                             if (lhs->getSortOrder() != rhs->getSortOrder())
                             {
                                 return lhs->getSortOrder() < rhs->getSortOrder();
                             }
                             return lhs->getName() < rhs->getName();
                         });

        std::vector<Uuid> ids;
        ids.reserve(children.size());
        for (const EditorEntity* entity : children) { ids.push_back(entity->getId()); }
        return ids;
    }

    std::vector<Uuid> SceneDocument::getRootEntities() const
    {
        return getChildren(Uuid{});
    }

    bool SceneDocument::isAncestorOf(const Uuid& ancestorId, const Uuid& descendantId) const
    {
        Uuid current = descendantId;
        // Bounded by the entity count so that an already-corrupt graph (a cycle introduced by a
        // hand-edited file) cannot hang the editor here.
        for (std::size_t step = 0; step <= entities_.size(); ++step)
        {
            if (!current.isValid()) { return false; }
            if (current == ancestorId) { return true; }
            const EditorEntity* entity = findEntity(current);
            if (entity == nullptr) { return false; }
            current = entity->getParentId();
        }
        return false;
    }

    JsonValue SceneDocument::toJson() const
    {
        JsonValue root = JsonValue::makeObject();
        root.set("formatVersion", JsonValue{kFormatVersion});
        root.set("sceneId", JsonValue{sceneId_.toString()});
        root.set("name", JsonValue{name_});

        JsonValue entitiesJson = JsonValue::makeArray();
        for (const EditorEntity& entity : entities_)
        {
            JsonValue entityJson = JsonValue::makeObject();
            entityJson.set("id", JsonValue{entity.getId().toString()});
            entityJson.set("name", JsonValue{entity.getName()});
            if (entity.getParentId().isValid())
            {
                entityJson.set("parent", JsonValue{entity.getParentId().toString()});
            }
            if (!entity.isEnabled()) { entityJson.set("enabled", JsonValue{false}); }
            if (entity.getSortOrder() != 0) { entityJson.set("sortOrder", JsonValue{entity.getSortOrder()}); }

            JsonValue componentsJson = JsonValue::makeObject();
            for (const EditorComponent& component : entity.getComponents())
            {
                JsonValue componentJson = JsonValue::makeObject();
                for (const auto& [name, value] : component.getProperties())
                {
                    componentJson.set(name, value.toJson());
                }
                componentsJson.set(component.getTypeId(), std::move(componentJson));
            }
            entityJson.set("components", std::move(componentsJson));

            if (!entity.getEditorState().empty())
            {
                JsonValue editorStateJson = JsonValue::makeObject();
                for (const auto& [name, value] : entity.getEditorState())
                {
                    editorStateJson.set(name, value.toJson());
                }
                entityJson.set("editorState", std::move(editorStateJson));
            }

            entitiesJson.append(std::move(entityJson));
        }
        root.set("entities", std::move(entitiesJson));
        return root;
    }

    SceneLoadResult SceneDocument::loadFromJson(const JsonValue& json, const ComponentRegistry& registry)
    {
        SceneLoadResult result;

        if (!json.isObject())
        {
            result.errorMessage = "scene root is not a JSON object";
            return result;
        }

        const int formatVersion = json["formatVersion"].asInt(0);
        if (formatVersion <= 0)
        {
            result.errorMessage = "scene has no usable 'formatVersion'";
            return result;
        }
        if (formatVersion > kFormatVersion)
        {
            result.errorMessage = "scene formatVersion " + std::to_string(formatVersion)
                                + " is newer than this build supports (" + std::to_string(kFormatVersion) + ")";
            return result;
        }

        clear();

        const Uuid sceneId = Uuid::parse(json["sceneId"].asString());
        sceneId_ = sceneId.isValid() ? sceneId : Uuid::generate();
        if (!sceneId.isValid()) { result.warnings.push_back("scene had no valid 'sceneId'; a new one was generated"); }
        name_ = json["name"].asString("Untitled");

        for (const JsonValue& entityJson : json["entities"].getElements())
        {
            EditorEntity entity;
            const Uuid id = Uuid::parse(entityJson["id"].asString());
            entity.setId(id.isValid() ? id : Uuid::generate());
            if (!id.isValid())
            {
                result.warnings.push_back("entity '" + entityJson["name"].asString("<unnamed>")
                                          + "' had no valid id; a new one was generated");
            }
            entity.setName(entityJson["name"].asString("Entity"));
            entity.setParentId(Uuid::parse(entityJson["parent"].asString()));
            entity.setEnabled(entityJson.contains("enabled") ? entityJson["enabled"].asBoolean(true) : true);
            entity.setSortOrder(entityJson["sortOrder"].asInt(0));

            for (const auto& [typeId, componentJson] : entityJson["components"].getMembers())
            {
                EditorComponent component{typeId};
                const ComponentDescriptor* descriptor = registry.find(typeId);
                if (descriptor == nullptr)
                {
                    result.warnings.push_back("entity '" + entity.getName() + "' uses unregistered component type '"
                                              + typeId + "'; its data is preserved but not editable");
                }

                for (const auto& [propertyName, propertyJson] : componentJson.getMembers())
                {
                    const PropertyDescriptor* property =
                        descriptor != nullptr ? descriptor->findProperty(propertyName) : nullptr;
                    const PropertyType type =
                        property != nullptr ? property->type : inferPropertyType(propertyJson);
                    component.setProperty(propertyName, PropertyValue::fromJson(propertyJson, type));
                }

                if (descriptor != nullptr) { component.applyDefaults(*descriptor); }
                entity.addComponent(std::move(component));
            }

            for (const auto& [name, valueJson] : entityJson["editorState"].getMembers())
            {
                entity.setEditorState(name, PropertyValue::fromJson(valueJson, inferPropertyType(valueJson)));
            }

            if (!addEntity(std::move(entity)).isValid())
            {
                result.warnings.push_back("duplicate entity id in scene; the later entity was dropped");
            }
        }

        // Dangling and cyclic parent links are repaired rather than rejected, so that a scene
        // whose parent entity was deleted by a bad merge still opens and can be fixed by hand.
        for (EditorEntity& entity : entities_)
        {
            const Uuid parentId = entity.getParentId();
            if (!parentId.isValid()) { continue; }
            if (findEntity(parentId) == nullptr)
            {
                result.warnings.push_back("entity '" + entity.getName() + "' referenced a missing parent; "
                                          "it was made a root entity");
                entity.setParentId(Uuid{});
            }
        }
        for (EditorEntity& entity : entities_)
        {
            if (entity.getParentId().isValid() && isAncestorOf(entity.getId(), entity.getParentId()))
            {
                result.warnings.push_back("entity '" + entity.getName() + "' was part of a parent cycle; "
                                          "it was made a root entity");
                entity.setParentId(Uuid{});
            }
        }

        result.succeeded = true;
        return result;
    }

    SceneLoadResult SceneDocument::loadFromFile(const std::string& path, const ComponentRegistry& registry)
    {
        SceneLoadResult result;

        std::ifstream stream{path, std::ios::binary};
        if (!stream)
        {
            result.errorMessage = "cannot open '" + path + "'";
            return result;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        const std::string text = buffer.str();

        const JsonParseResult parsed = Json::parse(text);
        if (!parsed.succeeded)
        {
            result.errorMessage = "'" + path + "': " + parsed.errorMessage + " at offset "
                                + std::to_string(parsed.errorOffset);
            return result;
        }

        return loadFromJson(parsed.value, registry);
    }

    bool SceneDocument::saveToFile(const std::string& path, std::string* errorMessage) const
    {
        std::error_code errorCode;
        const std::filesystem::path filePath{path};
        if (filePath.has_parent_path())
        {
            std::filesystem::create_directories(filePath.parent_path(), errorCode);
            if (errorCode)
            {
                if (errorMessage != nullptr) { *errorMessage = "cannot create directory: " + errorCode.message(); }
                return false;
            }
        }

        std::ofstream stream{path, std::ios::binary | std::ios::trunc};
        if (!stream)
        {
            if (errorMessage != nullptr) { *errorMessage = "cannot open '" + path + "' for writing"; }
            return false;
        }

        stream << Json::write(toJson(), true);
        if (!stream)
        {
            if (errorMessage != nullptr) { *errorMessage = "write to '" + path + "' failed"; }
            return false;
        }
        return true;
    }

    void SceneDocument::clear()
    {
        entities_.clear();
        indexById_.clear();
        sceneId_ = Uuid::generate();
    }

    void SceneDocument::rebuildIndex()
    {
        indexById_.clear();
        for (std::size_t index = 0; index < entities_.size(); ++index)
        {
            indexById_[entities_[index].getId()] = index;
        }
    }
}
