// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/HierarchyPanel.hpp"

#include <memory>
#include <optional>
#include <vector>

#include "CNA/Editor/EditorContext.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"

namespace CNA::Editor
{
    void HierarchyPanel::draw()
    {
        if (!ui_.beginPanel("Scene Hierarchy", DockSide::Left)) { ui_.endPanel(); return; }

        ui_.text(context_.getScene().getName() + " ("
                 + std::to_string(context_.getScene().getEntityCount()) + " entities)");
        ui_.separator();

        if (ui_.button("Add Entity"))
        {
            EditorEntity entity{Uuid::generate(), "Entity"};
            EditorComponent transform{BuiltinComponentIds::kTransform};
            if (const ComponentDescriptor* descriptor =
                    context_.getComponentRegistry().find(BuiltinComponentIds::kTransform))
            {
                transform.applyDefaults(*descriptor);
            }
            entity.addComponent(std::move(transform));

            auto command = std::make_unique<CreateEntityCommand>(context_.getScene(), std::move(entity));
            const Uuid createdId = command->getEntityId();
            context_.execute(std::move(command));
            context_.select(createdId);
        }

        for (const Uuid& rootId : context_.getScene().getRootEntities()) { drawNode(rootId); }

        ui_.endPanel();

        // Applied after the whole tree is drawn, never inside the recursion: a reparent reorders
        // the very lists drawNode() is walking, and a delete invalidates them outright.
        applyPendingAction();
    }

    void HierarchyPanel::drawNode(const Uuid& entityId)
    {
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr) { return; }

        const std::vector<Uuid> children = context_.getScene().getChildren(entityId);

        if (renamingEntity_ == entityId)
        {
            // A row being renamed is a text field, not a tree node, so nothing is pushed and
            // nothing is popped. Its children still draw, so the tree does not appear to lose a
            // subtree for as long as the field is open.
            drawRenameField(entityId);
            for (const Uuid& childId : children) { drawNode(childId); }
            return;
        }

        const UiTreeNodeResult node =
            ui_.treeNode(entityId, entity->getName(), context_.isSelected(entityId), children.empty());

        // Whether the node pushed onto the tree stack is settled here and read nowhere else.
        // Pairing the pop with renamingEntity_ instead would be wrong in both directions: starting
        // a rename below would skip a pop that is owed, and committing one would add a pop that is
        // not -- and ImGui's tree stack does not survive either mistake.
        const bool pushed = node.expanded;

        // The drag source and the drop target are both the node just drawn: dragging one entity
        // onto another is how a hierarchy is rearranged, and both roles belong to every node.
        ui_.setDragSource(kEntityDragType, entityId.toString(), entity->getName());

        if (const std::optional<std::string> dropped = ui_.acceptDrop(kEntityDragType))
        {
            pending_.kind = Action::Reparent;
            pending_.entityId = Uuid::parse(*dropped);
            pending_.parentId = entityId;
        }

        drawContextMenu(entityId);

        // Double-click first: a double-click also reports a click, and starting a rename must win
        // over merely reselecting what is already selected.
        if (node.doubleClicked) { beginRename(entityId); }
        else if (node.clicked)
        {
            // Ctrl extends the selection rather than replacing it, which is what every editor does
            // and what makes "these three, not that one" reachable at all.
            if (ui_.getModifiers().control) { context_.toggleSelection(entityId); }
            else { context_.select(entityId); }
        }

        if (!pushed) { return; }

        for (const Uuid& childId : children) { drawNode(childId); }
        ui_.treePop();
    }

    void HierarchyPanel::drawContextMenu(const Uuid& entityId)
    {
        if (!ui_.beginContextMenu("entity-" + entityId.toString())) { return; }

        // Right-clicking a node acts on that node, so it becomes the selection first. Acting on
        // something other than what was right-clicked is the classic context-menu bug.
        if (!context_.isSelected(entityId)) { context_.select(entityId); }

        if (ui_.menuItem("Rename", "F2")) { beginRename(entityId); }
        if (ui_.menuItem("Duplicate", "Ctrl+D")) { actions_.duplicateSelection(); }

        const bool hasParent = context_.getScene().findEntity(entityId) != nullptr
                            && context_.getScene().findEntity(entityId)->getParentId().isValid();
        if (ui_.menuItem("Move to Root", {}, hasParent))
        {
            pending_.kind = Action::Reparent;
            pending_.entityId = entityId;
            pending_.parentId = Uuid{};
        }

        ui_.separator();
        if (ui_.menuItem("Delete", "Del")) { pending_.kind = Action::Delete; }

        ui_.endContextMenu();
    }

    void HierarchyPanel::beginRename(const Uuid& entityId)
    {
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr) { return; }

        renamingEntity_ = entityId;
        renameBuffer_ = entity->getName();
        renameNeedsFocus_ = true;
        context_.select(entityId);
    }

    void HierarchyPanel::drawRenameField(const Uuid& entityId)
    {
        const UiTextFieldResult result =
            ui_.inputText("##rename-" + entityId.toString(), renameBuffer_, renameNeedsFocus_);

        // Only the first frame asks for focus. Asking every frame would make the field impossible
        // to leave, because it would take the keyboard back the instant anything else claimed it.
        renameNeedsFocus_ = false;

        if (!result.committed) { return; }

        renamingEntity_ = Uuid{};

        // An empty name is a slip, not an instruction: an unnamed row in the hierarchy is
        // unusable, and the old name is still right there to keep.
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr || renameBuffer_.empty() || renameBuffer_ == entity->getName()) { return; }

        context_.execute(std::make_unique<RenameEntityCommand>(context_.getScene(), entityId, renameBuffer_));
    }

    void HierarchyPanel::applyPendingAction()
    {
        const PendingAction action = pending_;
        pending_ = PendingAction{};

        switch (action.kind)
        {
            case Action::None:
                return;

            case Action::Delete:
                actions_.deleteSelection();
                return;

            case Action::Reparent: {
                if (!action.entityId.isValid()) { return; }
                if (action.entityId == action.parentId) { return; }

                // Dropping a parent onto its own descendant would make a cycle. SceneDocument
                // rejects it, but a command that does nothing still lands in the undo stack, and
                // an entry the user cannot see the effect of is worse than no entry.
                if (action.parentId.isValid()
                    && context_.getScene().isAncestorOf(action.entityId, action.parentId))
                {
                    context_.log(LogSeverity::Warning,
                                 "Cannot move an entity under one of its own children.");
                    return;
                }

                const EditorEntity* entity = context_.getScene().findEntity(action.entityId);
                if (entity == nullptr || entity->getParentId() == action.parentId) { return; }

                context_.execute(std::make_unique<ReparentEntityCommand>(
                    context_.getScene(), action.entityId, action.parentId));
                return;
            }
        }
    }
}
