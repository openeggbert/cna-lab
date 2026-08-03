// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Plugins/Plugin.hpp
 * @brief The plugin manifest, the C ABI entry point, and the services a plugin may use.
 *
 * A plugin is a manifest plus a shared library. The manifest is read *before* the library is
 * loaded, which is what lets the editor refuse an incompatible plugin without ever running its
 * code -- an ABI mismatch that reaches `dlopen` is a crash, not an error message.
 *
 * The entry point is deliberately `extern "C"`: a C++ symbol would bind the plugin to the exact
 * compiler and standard library the editor was built with, which no plugin author can be expected
 * to match. See ANALYSIS.md decision D-11.
 */

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace CNA::Editor
{
    class ComponentRegistry;
    class EditorContext;

    /** @brief The plugin API version this build exposes. Bumped on any breaking change. */
    inline constexpr int kEditorPluginApiVersion = 1;

    /** @brief A parsed `plugin.json` manifest. */
    struct PluginManifest
    {
        /** @brief Reverse-DNS identity, e.g. "org.openeggbert.mc3". Must be unique. */
        std::string id;

        std::string name;
        std::string version;
        std::string description;
        std::string author;

        /** @brief The kEditorPluginApiVersion the plugin was built against. */
        int editorApiVersion = 0;

        /** @brief Library file name, relative to the manifest, e.g. "libmc3-editor-plugin.so". */
        std::string library;

        /** @brief Ids of plugins that must load first. */
        std::vector<std::string> dependencies;

        /** @brief Absolute path of the directory the manifest was read from. */
        std::string directory;

        /**
         * @brief Parses a manifest file.
         * @param path Path to the `plugin.json`.
         * @param[out] errorMessage Set when parsing fails.
         * @return The manifest, or std::nullopt on failure.
         */
        static std::optional<PluginManifest> loadFromFile(const std::string& path, std::string* errorMessage);

        /** @brief Returns true when @p editorApiVersion matches this build exactly. */
        [[nodiscard]] bool isCompatible() const { return editorApiVersion == kEditorPluginApiVersion; }
    };

    /**
     * @brief What a loaded plugin can do.
     *
     * A plugin implements this and returns one from its entry point. The methods are called at
     * well-defined points, and a plugin that throws out of any of them is unloaded and reported
     * rather than allowed to destabilise the editor.
     */
    class EditorPlugin
    {
    public:
        virtual ~EditorPlugin() = default;

        /**
         * @brief Called once after loading, before any document is opened.
         *
         * The usual work is registering component descriptors, asset importers, panels and menu
         * commands against @p context.
         */
        virtual void initialize(EditorContext& context) = 0;

        /** @brief Called once before unloading. Must remove everything initialize() registered. */
        virtual void shutdown(EditorContext& context) = 0;

        /** @brief Returns the plugin's manifest id, for diagnostics. */
        [[nodiscard]] virtual std::string getId() const = 0;
    };

    /**
     * @brief The signature the plugin library must export as `cnaEditorCreatePlugin`.
     *
     * The returned object is owned by the caller and destroyed through
     * `cnaEditorDestroyPlugin`, so that allocation and deallocation both happen inside the
     * plugin's own runtime -- mixing allocators across a shared-library boundary is the classic
     * way to make a plugin system crash only in release builds.
     */
    using CreatePluginFunction = EditorPlugin* (*)(int editorApiVersion);

    /** @brief The signature the plugin library must export as `cnaEditorDestroyPlugin`. */
    using DestroyPluginFunction = void (*)(EditorPlugin*);

    /** @brief One entry in the plugin host's registry, whether or not the library loaded. */
    struct LoadedPlugin
    {
        PluginManifest manifest;

        /** @brief False when the plugin was rejected; @c error then says why. */
        bool loaded = false;
        std::string error;
    };

    /**
     * @brief Discovers, validates and (in a later phase) loads plugins.
     *
     * The Phase 1 implementation stops short of `dlopen`: it discovers manifests, validates the
     * API version and dependency order, and reports what *would* load. That is deliberate --
     * getting discovery and rejection right first means the eventual dynamic loading has nothing
     * left to get wrong except the loading itself. See plan.md ED-411.
     */
    class PluginHost
    {
    public:
        /**
         * @brief Scans @p directory for immediate subdirectories containing a `plugin.json`.
         * @return One entry per manifest found, in dependency order where it can be determined.
         */
        std::vector<LoadedPlugin> discover(const std::string& directory);

        /** @brief Returns everything discover() found on its most recent call. */
        [[nodiscard]] const std::vector<LoadedPlugin>& getPlugins() const { return plugins_; }

    private:
        std::vector<LoadedPlugin> plugins_;
    };
}
