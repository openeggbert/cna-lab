// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/InspectorPanel.hpp"

#include <memory>
#include <vector>

#include "CNA/Editor/Assets/AssetCommands.hpp"
#include "CNA/Editor/EditorContext.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"
#include "CNA/Editor/Scene/SceneTransform.hpp"

namespace CNA::Editor
{
    void InspectorPanel::draw()
    {
        if (!ui_.beginPanel("Inspector", DockSide::Right)) { ui_.endPanel(); return; }

        if (context_.getSelectedAsset().isValid())
        {
            drawAssetInspector(context_.getSelectedAsset());
            ui_.endPanel();
            return;
        }

        const Uuid selectedId = context_.getPrimarySelection();
        const EditorEntity* entity = context_.getScene().findEntity(selectedId);
        if (entity == nullptr)
        {
            ui_.text("Nothing selected.");
            ui_.endPanel();
            return;
        }

        ui_.text("Entity: " + entity->getName());
        ui_.text("Id: " + entity->getId().toString());
        ui_.separator();

        // Removal is deferred past the loop. Executing it here would mutate the very vector being
        // iterated, and invalidate the component reference the loop body is holding.
        std::optional<std::size_t> removeIndex;

        const std::vector<EditorComponent>& components = entity->getComponents();
        for (std::size_t index = 0; index < components.size(); ++index)
        {
            const EditorComponent& component = components[index];
            const ComponentDescriptor* descriptor = context_.getComponentRegistry().find(component.getTypeId());
            if (descriptor == nullptr)
            {
                // An unregistered component still has to be visible, or the user has no way to
                // discover that a scene depends on a plugin that failed to load. It is removable:
                // dropping a component whose plugin is gone is a legitimate way to fix a scene.
                ui_.text(component.getTypeId() + "  (unregistered -- data preserved, not editable)");
                ui_.sameLine();
                if (ui_.button("Remove##" + std::to_string(index))) { removeIndex = index; }
                ui_.separator();
                continue;
            }

            ui_.text(descriptor->displayName);

            // A required component is what makes the entity what it is -- removing the transform
            // would leave it with no position -- so it gets no button rather than a dead one.
            if (!descriptor->required)
            {
                ui_.sameLine();
                // The "##index" suffix is what keeps two components of the same type from sharing
                // one button identity; ImGui derives a widget's id from its label.
                if (ui_.button("Remove##" + std::to_string(index))) { removeIndex = index; }
            }

            for (const PropertyDescriptor& property : descriptor->properties)
            {
                const PropertyValue value = component.getPropertyOrDefault(property.name, descriptor);
                const std::optional<PropertyEdit> edited =
                    drawPropertyRow(selectedId, component.getTypeId(), property, value);
                if (!edited) { continue; }

                // Merging means an inspector drag collapses into a single undo entry that returns
                // to the value the drag started from. A structural change -- adding, removing or
                // moving a list element -- is one action per press and gets its own entry, or
                // pressing Add three times would undo in one.
                context_.execute(std::make_unique<SetPropertyCommand>(
                                     context_.getScene(), selectedId, component.getTypeId(),
                                     property.name, edited->value),
                                 edited->structural ? MergePolicy::NewEntry
                                                    : MergePolicy::MergeWithPrevious);
            }
            ui_.separator();
        }

        drawAddComponentControl(*entity);
        ui_.endPanel();

        // Past every reference into the entity's component vector, so the mutation is safe.
        if (removeIndex)
        {
            auto command = std::make_unique<RemoveComponentCommand>(
                context_.getScene(), context_.getComponentRegistry(), selectedId, *removeIndex);
            if (command->isValid()) { context_.execute(std::move(command)); }
        }
    }

    std::optional<InspectorPanel::PropertyEdit> InspectorPanel::drawPropertyRow(
        const Uuid& entityId,
        const std::string& componentTypeId,
        const PropertyDescriptor& property,
        const PropertyValue& value)
    {
        const std::string label = property.displayName.empty() ? property.name : property.displayName;

        if (value.getType() == PropertyType::List)
        {
            return drawListRows(componentTypeId, property, value);
        }

        if (value.getType() != PropertyType::Quaternion)
        {
            PropertyValue edited = value;
            const bool changed = ui_.propertyField(label, edited, property.enumOptions, property.readOnly);

            // An asset slot is also a drop target for the browser, which is the only way to fill
            // one without copying a Uuid by hand.
            if (property.type == PropertyType::AssetReference && !property.readOnly)
            {
                if (const std::optional<PropertyValue> dropped = acceptAssetDrop(property))
                {
                    // A drop is one action, not a drag: it must not fold into whatever edit
                    // happened to precede it.
                    return PropertyEdit{*dropped, true};
                }
            }

            return changed ? std::optional<PropertyEdit>{PropertyEdit{edited, false}} : std::nullopt;
        }

        // Quaternions are shown as degrees, because four raw components are not something anyone
        // can author: rotating a sprite by 45 degrees should not require working out a quaternion.
        const EditorQuaternion stored = value.get<EditorQuaternion>();

        // Reuse what the user typed for as long as the stored value is still exactly the one it
        // produced. Recomputing every frame would let the other two angles jump to an equivalent
        // spelling mid-edit; comparing against our own output means an undo, a gizmo drag or a
        // reload is picked up immediately, because none of those produce that exact quaternion.
        const bool cacheApplies = eulerEdit_.matches(entityId, componentTypeId, property.name)
                               && eulerEdit_.produced == stored;

        PropertyValue shown{cacheApplies ? eulerEdit_.degrees : eulerDegreesOf(stored)};
        if (!ui_.propertyField(label + " (deg)", shown, {}, property.readOnly))
        {
            return std::nullopt;
        }

        const EditorVector3 degrees = shown.get<EditorVector3>();
        const EditorQuaternion produced = quaternionFromEulerDegrees(degrees);

        eulerEdit_ = EulerEdit{entityId, componentTypeId, property.name, degrees, produced};
        return PropertyEdit{PropertyValue{produced}, false};
    }

    std::optional<InspectorPanel::PropertyEdit> InspectorPanel::drawListRows(
        const std::string& componentTypeId,
        const PropertyDescriptor& property,
        const PropertyValue& value)
    {
        const std::string label = property.displayName.empty() ? property.name : property.displayName;
        const std::string id = "list-" + componentTypeId + "-" + property.name;

        PropertyValue::ListValue list = value.get<PropertyValue::ListValue>();

        const UiTreeNodeResult node =
            ui_.treeNode(id, label + "  (" + value.toDisplayString() + ")", false, false);
        if (!node.expanded) { return std::nullopt; }

        std::optional<PropertyEdit> edit;

        // Structural changes are recorded and applied after the loop. Erasing or swapping an
        // element while iterating would draw the rest of the rows against a list the user has not
        // seen, and the widget ids would shift underneath them mid-frame.
        std::optional<std::size_t> removeIndex;
        std::optional<std::size_t> moveUpIndex;

        for (std::size_t index = 0; index < list.items.size(); ++index)
        {
            const std::string rowId = "##" + id + "-" + std::to_string(index);

            PropertyValue item = list.items[index];
            if (ui_.propertyField(std::to_string(index) + rowId, item, property.enumOptions,
                                  property.readOnly))
            {
                list.items[index] = std::move(item);
                edit = PropertyEdit{PropertyValue{list}, false};
            }

            if (property.readOnly) { continue; }

            // Not drawn at all at the ends, rather than drawn and ignored. A button that does
            // nothing when pressed is a bug report waiting to be filed.
            if (index > 0)
            {
                ui_.sameLine();
                if (ui_.button("Up" + rowId)) { moveUpIndex = index; }
            }
            if (index + 1 < list.items.size())
            {
                ui_.sameLine();
                if (ui_.button("Down" + rowId)) { moveUpIndex = index + 1; }
            }

            ui_.sameLine();
            if (ui_.button("Remove" + rowId)) { removeIndex = index; }
        }

        if (!property.readOnly && ui_.button("Add##" + id))
        {
            // The declared element type, not the type of whatever is already in there: an empty
            // list has nothing to copy from, and that is exactly when Add is pressed.
            list.items.push_back(PropertyValue::defaultOf(property.elementType));
            edit = PropertyEdit{PropertyValue{list}, true};
        }

        ui_.treePop();

        if (removeIndex && *removeIndex < list.items.size())
        {
            list.items.erase(list.items.begin() + static_cast<std::ptrdiff_t>(*removeIndex));
            edit = PropertyEdit{PropertyValue{list}, true};
        }
        else if (moveUpIndex && *moveUpIndex > 0 && *moveUpIndex < list.items.size())
        {
            std::swap(list.items[*moveUpIndex], list.items[*moveUpIndex - 1]);
            edit = PropertyEdit{PropertyValue{list}, true};
        }

        return edit;
    }

    std::optional<PropertyValue> InspectorPanel::acceptAssetDrop(const PropertyDescriptor& property)
    {
        const std::optional<std::string> dropped = ui_.acceptDrop(kAssetDragType);
        if (!dropped) { return std::nullopt; }

        const Uuid assetId = Uuid::parse(*dropped);
        const AssetRecord* record = context_.getAssets().find(assetId);
        if (record == nullptr)
        {
            context_.log(LogSeverity::Warning, "Dropped asset is no longer in the database.");
            return std::nullopt;
        }

        // A slot that declares what it takes refuses everything else, and says which is which.
        // Silently accepting a sound into a texture slot would produce a scene that loads and a
        // sprite that never appears, with nothing anywhere to explain it.
        if (!property.assetType.empty() && property.assetType != toString(record->type))
        {
            context_.log(LogSeverity::Warning,
                         "'" + record->sourcePath + "' is a " + std::string{toString(record->type)}
                             + "; this field takes a " + property.assetType + ".");
            return std::nullopt;
        }

        return PropertyValue{PropertyValue::AssetReference{assetId}};
    }

    void InspectorPanel::drawAssetInspector(const Uuid& assetId)
    {
        const AssetRecord* record = context_.getAssets().find(assetId);
        if (record == nullptr)
        {
            ui_.text("This asset is no longer in the database.");
            return;
        }

        ui_.text("Asset: " + record->sourcePath);
        ui_.text("Id: " + record->id.toString());
        ui_.text(std::string{"Type: "} + toString(record->type));
        ui_.separator();

        const ComponentDescriptor* descriptor = context_.getImporterRegistry().find(record->importerId);
        if (descriptor == nullptr)
        {
            // An importer with no declared settings is not a fault -- most have nothing worth
            // choosing. Saying so beats an empty form the user waits for something to appear in.
            ui_.text(record->importerId.empty() ? "No importer for this file type."
                                                : record->importerId + " has no settings.");
            return;
        }

        ui_.text(descriptor->displayName);

        for (const PropertyDescriptor& property : descriptor->properties)
        {
            // The stored setting when the sidecar carries one, the declared default otherwise.
            // Writing every default into the sidecar on first sight would make each asset's diff
            // noise, so absent stays absent until the user actually chooses something.
            const JsonValue& stored = record->importerSettings[property.name];
            PropertyValue value = stored.isNull() ? property.defaultValue
                                                  : PropertyValue::fromJson(stored, property.type);

            const std::string label = property.displayName.empty() ? property.name : property.displayName;
            if (!ui_.propertyField(label, value, property.enumOptions, property.readOnly)) { continue; }

            auto command = std::make_unique<SetImporterSettingCommand>(
                context_.getAssets(), assetId, property.name, value);
            if (!command->isValid()) { continue; }

            // Merging, like every other inspector field: dragging a slider is one undo entry that
            // returns to the value the drag started from.
            context_.execute(std::move(command), MergePolicy::MergeWithPrevious);
        }
    }

    void InspectorPanel::drawAddComponentControl(const EditorEntity& entity)
    {
        std::vector<std::string> labels;
        std::vector<std::string> typeIds;

        for (const std::string& typeId : context_.getComponentRegistry().getTypeIds())
        {
            const ComponentDescriptor* descriptor = context_.getComponentRegistry().find(typeId);
            if (descriptor == nullptr) { continue; }

            // A unique component the entity already has cannot be added again, so listing it would
            // be listing an entry that does nothing -- AddComponentCommand would refuse it anyway.
            if (descriptor->unique && entity.findComponent(typeId) != nullptr) { continue; }

            labels.push_back(descriptor->category.empty()
                                 ? descriptor->displayName
                                 : descriptor->category + " / " + descriptor->displayName);
            typeIds.push_back(typeId);
        }

        if (labels.empty())
        {
            ui_.text("Every component type is already on this entity.");
            return;
        }

        // The choice is remembered as a *type id*, not as an index: the list shortens the moment a
        // unique component is added, and an index would then silently point at a different type.
        std::size_t chosen = 0;
        for (std::size_t index = 0; index < typeIds.size(); ++index)
        {
            if (typeIds[index] == addComponentChoice_) { chosen = index; break; }
        }
        addComponentChoice_ = typeIds[chosen];

        ui_.setNextItemWidth(190.0f);

        PropertyValue value{PropertyValue::EnumValue{labels[chosen]}};
        if (ui_.propertyField("##addComponentType", value, labels))
        {
            const std::string picked = value.get<PropertyValue::EnumValue>().name;
            for (std::size_t index = 0; index < labels.size(); ++index)
            {
                if (labels[index] == picked) { addComponentChoice_ = typeIds[index]; break; }
            }
        }

        ui_.sameLine();

        if (ui_.button("Add Component"))
        {
            auto command = std::make_unique<AddComponentCommand>(
                context_.getScene(), context_.getComponentRegistry(), entity.getId(), addComponentChoice_);
            if (command->isValid()) { context_.execute(std::move(command)); }
        }
    }
}
