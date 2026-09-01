// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>

namespace MeshCraft::Mc3 {
class Mc3Object;
class Mc3Document;
} // namespace MeshCraft::Mc3

namespace MeshWorld {

// R103/R104 v1 -- executes a definition's own embedded Lua script
// (Mc3Object::scriptId -> Mc3Document::scripts, mesh-craft/mc3) against
// that definition, placing imported R101/R102 definitions at its own
// Mc3AssetMetadata.sockets (mesh_world_revival.md §6/§7's "build/compose
// time" placement).
//
// This is MeshWorld's OWN execution engine, not mesh-craft's. R104's own
// text asks for the Lua binding to live in MeshCraft itself ("so any
// mc3.xml consumer gets the same scripting capability") -- deliberately
// deferred for v1: adding a Lua/sol2 dependency to a shared cross-project
// library (also consumed by CNA/NOXNA/mc3togltf) is a materially bigger
// commitment than a single MeshWorld-side runner, and MeshWorld already
// has a working, sandboxed sol2 setup (LuaRuntime.cpp) to build on. Same
// sandboxing discipline as LuaRuntime: only base/math/string/table Lua
// libs, no io/os/debug/package/require.
class Mc3ScriptRunner {
public:
    // Runs `target.scriptId`'s script (looked up in doc.scripts) against
    // `target`, mutating `target.children` in place by appending placed
    // Instance objects. `doc` must already have had
    // MeshCraft::Mc3::Mc3ImportResolver::resolveAndMergeInto() called on
    // it so imported definitions are resolvable via doc.definitions.
    //
    // The script sees one global, `def`, exposing:
    //   def:place(childId, definitionRef, socketName) -- creates a new
    //     Instance child named childId, referencing definitionRef (must
    //     already exist in doc.definitions), positioned at socketName
    //     (must exist in target's own assetMetadata.sockets).
    //   def:place_at(childId, definitionRef, x, y, z) -- same as place(),
    //     but at raw coordinates instead of a named socket, so a script
    //     can COMPUTE positions itself (e.g. tiling N facade modules
    //     along a wall span, where N depends on the wall's own length).
    //   def:has_socket(socketName) -> bool
    //
    // Returns "" on success (including the no-op case: target.scriptId
    // empty, or the script's own source empty) or an error description
    // on failure (unknown script id, Lua syntax/runtime error, unknown
    // socket, unknown/unresolved definitionRef) -- never throws.
    std::string run(MeshCraft::Mc3::Mc3Object& target,
                     const MeshCraft::Mc3::Mc3Document& doc);
};

} // namespace MeshWorld
