// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "LuaGeneratorRegistry.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace MeshWorld {

// ---------------------------------------------------------------------------
// Derive a fallback ID from a file path relative to the generators/lua/ root.
// e.g. .../generators/lua/object/chair.lua → "lua.object.chair"
// ---------------------------------------------------------------------------
static std::string id_from_path(const std::filesystem::path& file,
                                 const std::filesystem::path& root) {
    auto rel = std::filesystem::relative(file, root);
    std::string id = "lua";
    for (auto it = rel.begin(); it != rel.end(); ++it) {
        std::string part = it->string();
        // strip .lua extension from the last component
        if (part.size() > 4 && part.substr(part.size() - 4) == ".lua")
            part = part.substr(0, part.size() - 4);
        id += '.' + part;
    }
    return id;
}

// ---------------------------------------------------------------------------
// Execute a Lua module in a minimal state to extract M.id.
// Returns M.id string on success, empty string if not available.
// ---------------------------------------------------------------------------
static std::string extract_module_id(const std::string& source) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table,
                       sol::lib::math);
    // Silence io/os — generators may reference them at top level
    lua["io"] = sol::nil;
    lua["os"] = sol::nil;

    auto result = lua.safe_script(source, sol::script_pass_on_error);
    if (!result.valid()) return {};

    sol::optional<sol::table> tbl = result;
    if (!tbl) return {};

    sol::optional<std::string> mid = (*tbl)["id"];
    return mid.value_or("");
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void LuaGeneratorRegistry::load_from_dir(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) return;

    for (auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".lua") continue;

        // Read source
        std::ifstream ifs(entry.path());
        if (!ifs.is_open()) continue;
        std::string source((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());

        // Determine ID
        std::string id = extract_module_id(source);
        if (id.empty())
            id = id_from_path(entry.path(), dir);

        sources_[id] = std::move(source);
    }
}

void LuaGeneratorRegistry::register_source(const std::string& id,
                                            const std::string& source) {
    sources_[id] = source;
}

void LuaGeneratorRegistry::unregister_source(const std::string& id) {
    sources_.erase(id);
}

const std::string& LuaGeneratorRegistry::get(const std::string& id) const {
    auto it = sources_.find(id);
    if (it == sources_.end())
        throw std::out_of_range("LuaGeneratorRegistry: unknown generator id: " + id);
    return it->second;
}

bool LuaGeneratorRegistry::has(const std::string& id) const {
    return sources_.count(id) > 0;
}

std::vector<std::string> LuaGeneratorRegistry::list() const {
    std::vector<std::string> ids;
    ids.reserve(sources_.size());
    for (auto& kv : sources_) ids.push_back(kv.first);
    return ids;
}

LuaGeneratorRegistry& LuaGeneratorRegistry::instance() {
    static LuaGeneratorRegistry reg;
    return reg;
}

} // namespace MeshWorld
