// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/EditorContext.hpp"

#include <algorithm>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"

namespace CNA::Editor
{
    EditorContext::EditorContext()
    {
        registerBuiltinComponents(components_);
        registerBuiltinImporters(importers_);
    }

    bool EditorContext::openProject(const std::string& path)
    {
        const ProjectLoadResult loaded = project_.loadFromFile(path);
        if (!loaded.succeeded)
        {
            log(LogSeverity::Error, "Failed to open project: " + loaded.errorMessage);
            return false;
        }
        for (const std::string& warning : loaded.warnings) { log(LogSeverity::Warning, warning); }

        log(LogSeverity::Info, "Opened project '" + project_.getName() + "' ("
                                   + toString(project_.getKind()) + ") at " + project_.getRootPath());

        assets_.clear();
        assets_.setProjectRoot(project_.getRootPath());
        const AssetScanResult scan = assets_.scan(project_.getAssetDirectory());
        if (!scan.succeeded)
        {
            log(LogSeverity::Warning, "Asset scan failed: " + scan.errorMessage);
        }
        else
        {
            // Facts an importer can read out of the file itself -- a texture's dimensions today.
            // Only what changed is written back, so opening a project twice produces no diff.
            applyImporterFacts(assets_);

            for (const std::string& warning : scan.warnings) { log(LogSeverity::Warning, warning); }
            log(LogSeverity::Info, "Assets: " + std::to_string(scan.discoveredCount) + " found, "
                                       + std::to_string(scan.newCount) + " new, "
                                       + std::to_string(scan.movedCount) + " moved, "
                                       + std::to_string(scan.missingCount) + " missing");
        }

        // An XnaCompatible project has no scene model at all, so opening a startup scene for one
        // would be the editor imposing its own concepts on a project that opted out of them.
        if (project_.getKind() == ProjectKind::CnaNative && !project_.getStartupScene().empty())
        {
            openScene(project_.resolvePath(project_.getStartupScene()));
        }
        return true;
    }

    bool EditorContext::openScene(const std::string& path)
    {
        const SceneLoadResult loaded = scene_.loadFromFile(path, components_);
        if (!loaded.succeeded)
        {
            log(LogSeverity::Error, "Failed to open scene: " + loaded.errorMessage);
            return false;
        }
        for (const std::string& warning : loaded.warnings) { log(LogSeverity::Warning, warning); }

        scenePath_ = path;
        history_.clear();
        clearSelection();
        log(LogSeverity::Info, "Opened scene '" + scene_.getName() + "' with "
                                   + std::to_string(scene_.getEntityCount()) + " entities");
        return true;
    }

    bool EditorContext::saveScene(const std::string& path)
    {
        const std::string target = path.empty() ? scenePath_ : path;
        if (target.empty())
        {
            log(LogSeverity::Error, "Cannot save: the scene has no file path");
            return false;
        }

        std::string errorMessage;
        if (!scene_.saveToFile(target, &errorMessage))
        {
            log(LogSeverity::Error, "Failed to save scene: " + errorMessage);
            return false;
        }

        scenePath_ = target;
        history_.markSaved();
        log(LogSeverity::Info, "Saved scene to " + target);
        return true;
    }

    void EditorContext::newScene(std::string name)
    {
        scene_.clear();
        scene_.setName(std::move(name));
        scenePath_.clear();
        history_.clear();
        clearSelection();

        // A scene with no camera renders nothing, which reads as "the editor is broken" rather
        // than "you have not added a camera yet". Starting with one avoids that entirely.
        EditorEntity camera{Uuid::generate(), "Main Camera"};
        EditorComponent transform{BuiltinComponentIds::kTransform};
        if (const ComponentDescriptor* descriptor = components_.find(BuiltinComponentIds::kTransform))
        {
            transform.applyDefaults(*descriptor);
        }
        camera.addComponent(std::move(transform));

        EditorComponent cameraComponent{BuiltinComponentIds::kCamera};
        if (const ComponentDescriptor* descriptor = components_.find(BuiltinComponentIds::kCamera))
        {
            cameraComponent.applyDefaults(*descriptor);
        }
        camera.addComponent(std::move(cameraComponent));

        scene_.addEntity(std::move(camera));
        log(LogSeverity::Info, "Created scene '" + scene_.getName() + "'");
    }

    void EditorContext::execute(std::unique_ptr<EditorCommand> command, MergePolicy policy)
    {
        if (!command) { return; }
        const std::string description = command->getDescription();

        history_.execute(std::move(command), policy);

        // The observer is handed the *entry*, not the command that was just pushed -- a merged
        // command is folded into the previous entry and destroyed, and the entry is what now holds
        // the change. They target the same property either way, because a merge only happens when
        // the merge keys match.
        if (commandObserver_ && history_.getCursor() > 0)
        {
            if (const EditorCommand* entry = history_.getCommandAt(history_.getCursor() - 1))
            {
                commandObserver_(*entry);
            }
        }
        pruneSelection();
        log(LogSeverity::Trace, description);
    }

    Uuid EditorContext::getPrimarySelection() const
    {
        return selection_.empty() ? Uuid{} : selection_.front();
    }

    bool EditorContext::isSelected(const Uuid& id) const
    {
        return std::find(selection_.begin(), selection_.end(), id) != selection_.end();
    }

    void EditorContext::select(const Uuid& id)
    {
        // Selecting an entity is also a decision about what the inspector is showing.
        selectedAsset_ = Uuid{};

        selection_.clear();
        if (id.isValid()) { selection_.push_back(id); }
    }

    void EditorContext::selectAsset(const Uuid& id)
    {
        selectedAsset_ = id;
        if (selectedAsset_.isValid()) { selection_.clear(); }
    }

    void EditorContext::setSelection(std::vector<Uuid> ids)
    {
        selection_ = std::move(ids);
        pruneSelection();
    }

    void EditorContext::toggleSelection(const Uuid& id)
    {
        if (!id.isValid()) { return; }
        const auto found = std::find(selection_.begin(), selection_.end(), id);
        if (found == selection_.end()) { selection_.push_back(id); }
        else { selection_.erase(found); }
    }

    void EditorContext::clearSelection()
    {
        selection_.clear();
    }

    void EditorContext::pruneSelection()
    {
        selection_.erase(std::remove_if(selection_.begin(), selection_.end(),
                                        [this](const Uuid& id) { return scene_.findEntity(id) == nullptr; }),
                         selection_.end());
    }

    void EditorContext::log(LogSeverity severity, const std::string& message) const
    {
        if (logSink_) { logSink_(severity, message); }
    }
}
