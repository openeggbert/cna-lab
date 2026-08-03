// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Plugins/Plugin.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "CNA/Editor/Core/Json.hpp"

namespace CNA::Editor
{
    std::optional<PluginManifest> PluginManifest::loadFromFile(const std::string& path, std::string* errorMessage)
    {
        std::ifstream stream{path, std::ios::binary};
        if (!stream)
        {
            if (errorMessage != nullptr) { *errorMessage = "cannot open '" + path + "'"; }
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();

        const JsonParseResult parsed = Json::parse(buffer.str());
        if (!parsed.succeeded)
        {
            if (errorMessage != nullptr) { *errorMessage = parsed.errorMessage; }
            return std::nullopt;
        }

        PluginManifest manifest;
        manifest.id = parsed.value["id"].asString();
        if (manifest.id.empty())
        {
            if (errorMessage != nullptr) { *errorMessage = "manifest has no 'id'"; }
            return std::nullopt;
        }

        manifest.name = parsed.value["name"].asString(manifest.id);
        manifest.version = parsed.value["version"].asString("0.0.0");
        manifest.description = parsed.value["description"].asString();
        manifest.author = parsed.value["author"].asString();
        manifest.editorApiVersion = parsed.value["editorApiVersion"].asInt(0);
        manifest.library = parsed.value["library"].asString();

        for (const JsonValue& dependency : parsed.value["dependencies"].getElements())
        {
            manifest.dependencies.push_back(dependency.asString());
        }

        manifest.directory = std::filesystem::path{path}.parent_path().generic_string();
        return manifest;
    }

    std::vector<LoadedPlugin> PluginHost::discover(const std::string& directory)
    {
        plugins_.clear();

        std::error_code errorCode;
        if (!std::filesystem::is_directory(directory, errorCode)) { return plugins_; }

        std::vector<PluginManifest> manifests;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator{directory, errorCode})
        {
            if (!entry.is_directory(errorCode)) { continue; }

            const std::string manifestPath = (entry.path() / "plugin.json").generic_string();
            if (!std::filesystem::exists(manifestPath, errorCode)) { continue; }

            std::string manifestError;
            if (std::optional<PluginManifest> manifest = PluginManifest::loadFromFile(manifestPath, &manifestError))
            {
                manifests.push_back(std::move(*manifest));
            }
            else
            {
                LoadedPlugin rejected;
                rejected.manifest.directory = entry.path().generic_string();
                rejected.error = "malformed plugin.json: " + manifestError;
                plugins_.push_back(std::move(rejected));
            }
        }

        // Sort by id first so that the dependency pass below is deterministic; two plugins with no
        // relationship must always come out in the same order, or a build is not reproducible.
        std::sort(manifests.begin(), manifests.end(),
                  [](const PluginManifest& lhs, const PluginManifest& rhs) { return lhs.id < rhs.id; });

        // Kahn-style ordering. A plugin whose dependency is missing or circular is emitted at the
        // end and marked with an error rather than dropped, so the user can see why it is inactive.
        std::unordered_set<std::string> satisfied;
        std::vector<PluginManifest> pending = std::move(manifests);
        bool madeProgress = true;
        while (madeProgress && !pending.empty())
        {
            madeProgress = false;
            for (auto iterator = pending.begin(); iterator != pending.end();)
            {
                const bool ready = std::all_of(iterator->dependencies.begin(), iterator->dependencies.end(),
                                               [&](const std::string& dependency) {
                                                   return satisfied.count(dependency) > 0;
                                               });
                if (!ready) { ++iterator; continue; }

                LoadedPlugin entry;
                entry.manifest = *iterator;
                if (!entry.manifest.isCompatible())
                {
                    entry.error = "built against editor API version "
                                + std::to_string(entry.manifest.editorApiVersion) + ", this editor speaks "
                                + std::to_string(kEditorPluginApiVersion);
                }
                else if (entry.manifest.library.empty())
                {
                    entry.error = "manifest declares no 'library'";
                }
                else if (!std::filesystem::exists(
                             std::filesystem::path{entry.manifest.directory} / entry.manifest.library, errorCode))
                {
                    entry.error = "library '" + entry.manifest.library + "' is missing";
                }
                else
                {
                    // Phase 1 stops here: the plugin is valid and would load. Actual dlopen and
                    // EditorPlugin construction arrive with plan.md ED-411.
                    entry.loaded = false;
                    entry.error = "dynamic loading is not implemented yet (plan.md ED-411)";
                }

                satisfied.insert(entry.manifest.id);
                plugins_.push_back(std::move(entry));
                iterator = pending.erase(iterator);
                madeProgress = true;
            }
        }

        for (PluginManifest& manifest : pending)
        {
            LoadedPlugin entry;
            entry.manifest = std::move(manifest);
            entry.error = "unsatisfied or circular dependencies";
            plugins_.push_back(std::move(entry));
        }

        return plugins_;
    }
}
