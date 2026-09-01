// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace MeshCraft::Mc3 { class Mc3Object; class Mc3Document; }

namespace MeshWorld {

// Singleton that holds pre-built Mc3Object definition subtrees for every
// instance ID referenced by C++ chunk generators.
//
// Call load_all() once at startup (after register_builtin_materials())
// before any WorldRenderer::render() call. After that the library is
// read-only and safe to read from any thread.
class ObjectDefinitionLibrary {
public:
    static ObjectDefinitionLibrary& instance();

    // Build and register all standard object definitions.
    void load_all();

    bool has(const std::string& id) const;

    // Returns nullptr if id is not registered.
    std::shared_ptr<MeshCraft::Mc3::Mc3Object> get(const std::string& id) const;

    void register_definition(std::string id,
                              std::shared_ptr<MeshCraft::Mc3::Mc3Object> obj);

private:
    ObjectDefinitionLibrary() = default;
    std::unordered_map<std::string, std::shared_ptr<MeshCraft::Mc3::Mc3Object>> defs_;
};

// R114 (city showcase) -- resolves every unresolved <instance ref="..."/>
// in `doc` against ObjectDefinitionLibrary::instance(), injecting a real
// <definitions> entry for each. Composer-registered ids (house.gable.
// wide_01 and friends) resolve here too -- register_composer_assets()'s
// own helpers always register into ObjectDefinitionLibrary directly,
// AssetRegistry is an ADDITIONAL, separate query index on top, not a
// replacement. This is exactly what WorldRenderer's own (previously
// renderer-only, now shared) inject_definitions() already did for the
// live app. Batch export tools (MeshWorldGLB) operate on
// already-exported chunk XML files with NO live ChunkPipeline/WorldRenderer
// involved, so instance refs are otherwise left dangling -- confirmed via a
// real conversion run: every house/vehicle/tree/street-furniture instance
// (everything BuildingComposer places via w.instance()) showed up as
// "unknown definition", silently dropped from the resulting .glb, while
// literal box/plane primitives (roads, crosswalks, traffic lights) still
// exported fine. A no-op for any id already defined in `doc` or genuinely
// unresolvable anywhere (skipped, not an error -- same fallback discipline
// every other lookup in this codebase already follows).
void resolve_instance_definitions(MeshCraft::Mc3::Mc3Document& doc);

} // namespace MeshWorld
