// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/ProjectCommands.hpp
 * @brief Undoable changes to the open `.cnaproject`.
 *
 * The project is a document like the scene and the asset database, so a change to it goes through
 * a command (ANALYSIS.md decision D-06). An editor where some edits undo and others quietly do not
 * is worse than one where nothing does.
 *
 * These live above both `cna-editor-project` and `cna-editor-scene` rather than inside either:
 * changing the layer list has to update the component registry as well as the project, and neither
 * of those modules may depend on the other.
 */

#include <string>
#include <vector>

#include "CNA/Editor/Core/ComponentDescriptor.hpp"
#include "CNA/Editor/Core/EditorCommand.hpp"
#include "CNA/Editor/Project/Project.hpp"

namespace CNA::Editor
{
    /**
     * @brief Replaces the project's render layers.
     *
     * Also re-registers `CNA.Layer` so the inspector offers the new names, and writes the project
     * file -- the same shape as `SetImporterSettingCommand`, which persists a sidecar rather than
     * waiting for a save the user has no reason to expect.
     *
     * Entities already on a layer that has been renamed away are **not** rewritten. Which of the
     * remaining layers they belonged to is the user's decision, not the command's; scene validation
     * reports them instead (`unknown-enum-value`).
     */
    class SetProjectLayersCommand final : public EditorCommand
    {
    public:
        SetProjectLayersCommand(Project& project, ComponentRegistry& registry, std::vector<std::string> layers);

        /** @brief Returns false when the new list is empty or identical to the current one. */
        [[nodiscard]] bool isValid() const { return valid_; }

        void execute() override;
        void undo() override;
        [[nodiscard]] std::string getDescription() const override;

        /** @brief Returns whether the last apply() managed to write the project file. */
        [[nodiscard]] bool wasSavedToDisk() const { return savedToDisk_; }

    private:
        void apply(const std::vector<std::string>& layers);

        Project* project_;
        ComponentRegistry* registry_;
        std::vector<std::string> newLayers_;
        std::vector<std::string> oldLayers_;
        bool valid_ = false;
        bool savedToDisk_ = false;
    };
}
