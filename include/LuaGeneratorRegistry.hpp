// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace MeshWorld {

// Maps Lua generator IDs (e.g. "lua.zone.park", "lua.object.chair.simple")
// to their Lua source code.
//
// ID resolution:
//   load_from_dir() executes each .lua file in a minimal sandbox to read M.id.
//   If M.id is present, that becomes the registry key.
//   Otherwise the key is derived from the file path:
//     generators/lua/object/chair.lua → "lua.object.chair"
//
// Thread safety: register/load operations are not thread-safe; call them
// once at startup before any concurrent get() calls.
class LuaGeneratorRegistry {
public:
    // Load all .lua files recursively from dir.
    // Each file is executed in a minimal Lua state to extract M.id.
    void load_from_dir(const std::filesystem::path& dir);

    // Register source code directly under a given ID.
    void register_source(const std::string& id, const std::string& source);

    // Remove a previously registered ID (no-op if absent). Mainly for tests
    // that seed instance() (a process-wide singleton) and must restore it
    // afterward so other tests see it in its default, empty-of-that-id state.
    void unregister_source(const std::string& id);

    // Returns source for id. Throws std::out_of_range if not found.
    const std::string& get(const std::string& id) const;

    // Returns true if id is registered.
    bool has(const std::string& id) const;

    // Returns all registered IDs (unordered).
    std::vector<std::string> list() const;

    // Process-wide singleton. Use for global generator lookup.
    static LuaGeneratorRegistry& instance();

private:
    std::unordered_map<std::string, std::string> sources_; // id → source
};

} // namespace MeshWorld
