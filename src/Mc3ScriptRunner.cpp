// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Mc3ScriptRunner.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <array>
#include <stdexcept>

using namespace MeshCraft::Mc3;

namespace MeshWorld {

namespace {

// Lua-facing placement API, bound as the global `def` while the script
// runs. Holds non-owning pointers -- both outlive the script execution
// (target/doc are caller-owned references passed straight through
// Mc3ScriptRunner::run()'s own stack frame).
struct PlacementApi {
    Mc3Object*         target;
    const Mc3Document* doc;

    // Shared by place()/place_at(): creates a new Instance child of
    // `target` referencing `definitionRef` at `position`. `definitionRef`
    // must already exist in doc->definitions (post-import-resolution) --
    // never silently creates a dangling reference.
    void place_instance(const std::string& childId, const std::string& definitionRef,
                         std::array<float, 3> position) const {
        if (!doc->definitions.count(definitionRef))
            throw std::runtime_error(
                "unknown definition (not resolved/imported): " + definitionRef);

        auto instance                = std::make_shared<Mc3Object>();
        instance->type               = ObjectType::Instance;
        instance->name               = childId;
        instance->id                 = childId;
        instance->definition         = definitionRef;
        instance->transform.position = position;
        target->children.push_back(instance);
    }

    void place(const std::string& childId, const std::string& definitionRef,
               const std::string& socketName) const {
        if (!target->assetMetadata.has_value())
            throw std::runtime_error("target has no assetMetadata (no sockets declared)");

        const auto socket_it = target->assetMetadata->sockets.find(socketName);
        if (socket_it == target->assetMetadata->sockets.end())
            throw std::runtime_error("unknown socket: " + socketName);

        place_instance(childId, definitionRef, socket_it->second);
    }

    // R112 facade_module consumption -- unlike place(), takes raw
    // coordinates instead of a pre-authored named socket, so a script can
    // COMPUTE positions itself (e.g. tiling N facade bay modules along a
    // wall span, where N and each position depend on the wall's own
    // length -- not something a fixed, hand-authored socket list can
    // express).
    void place_at(const std::string& childId, const std::string& definitionRef,
                   float x, float y, float z) const {
        place_instance(childId, definitionRef, {x, y, z});
    }

    bool has_socket(const std::string& socketName) const {
        return target->assetMetadata.has_value() &&
               target->assetMetadata->sockets.count(socketName) > 0;
    }
};

} // namespace

std::string Mc3ScriptRunner::run(Mc3Object& target, const Mc3Document& doc) {
    if (target.scriptId.empty()) return "";

    const auto script_it = doc.scripts.find(target.scriptId);
    if (script_it == doc.scripts.end())
        return "unknown script id: " + target.scriptId;
    if (!script_it->second.hasSource())
        return ""; // an empty script body is a legitimate no-op, not an error

    try {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

        // Sandbox: same discipline as LuaRuntime.cpp -- no filesystem/
        // process/module access.
        for (const char* g : {"io", "os", "debug", "package",
                               "dofile", "loadfile", "load", "collectgarbage"}) {
            lua[g] = sol::nil;
        }
        lua.set_function("require", [](const std::string& name) -> sol::object {
            throw std::runtime_error("require is not allowed in MeshWorld sandbox: " + name);
        });

        lua.new_usertype<PlacementApi>("Mc3ScriptPlacementApi",
            sol::no_constructor,
            "place",      &PlacementApi::place,
            "place_at",   &PlacementApi::place_at,
            "has_socket", &PlacementApi::has_socket);

        PlacementApi api{&target, &doc};
        lua["def"] = &api;

        auto result = lua.safe_script(script_it->second.source, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            return std::string("Lua error: ") + err.what();
        }
        return "";
    } catch (const std::exception& e) {
        return std::string("Mc3ScriptRunner error: ") + e.what();
    }
}

} // namespace MeshWorld
