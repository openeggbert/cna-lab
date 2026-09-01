// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldPack — packs generators/lua/, data/taxonomy/, and builtin materials
// into a single meshworld_content.sqlite file.
//
// Usage: MeshWorldPack [output.sqlite] [project_root]
//   output.sqlite  defaults to meshworld_content.sqlite
//   project_root   defaults to current directory

#include "SqliteContentPack.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "TaxonomyRegistry.hpp"
#include "ContainmentRuleRegistry.hpp"
#include "BuiltinMaterials.hpp"
#include "MaterialRegistry.hpp"
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    fs::path out_path   = (argc > 1) ? argv[1] : "meshworld_content.sqlite";
    fs::path root       = (argc > 2) ? argv[2] : fs::current_path();

    std::printf("MeshWorldPack\n");
    std::printf("  Root   : %s\n", root.string().c_str());
    std::printf("  Output : %s\n", out_path.string().c_str());

    try {
        MeshWorld::SqliteContentPack pack(out_path);

        // ── Lua generators ────────────────────────────────────────────────
        MeshWorld::LuaGeneratorRegistry lua_reg;
        fs::path lua_dir = root / "generators" / "lua";
        if (fs::exists(lua_dir)) {
            lua_reg.load_from_dir(lua_dir);
            for (const auto& id : lua_reg.list()) {
                MeshWorld::LuaGeneratorRow row;
                row.id     = id;
                row.source = lua_reg.get(id);
                pack.upsert_lua_generator(row);
            }
            std::printf("  Packed %zu Lua generators\n", lua_reg.list().size());
        } else {
            std::fprintf(stderr, "Warning: lua dir not found: %s\n", lua_dir.string().c_str());
        }

        // ── Taxonomy nodes ────────────────────────────────────────────────
        MeshWorld::TaxonomyRegistry tax_reg;
        fs::path tax_json = root / "data" / "taxonomy" / "taxonomy.json";
        if (fs::exists(tax_json)) {
            tax_reg.load(tax_json);
            for (const auto& node : tax_reg.all()) {
                MeshWorld::TaxonomyNodeRow row;
                row.id   = node.id;
                row.kind = node.kind;
                row.name = node.name;
                pack.upsert_taxonomy_node(row);
            }
            std::printf("  Packed %zu taxonomy nodes\n", tax_reg.all().size());
        }

        // ── Containment rules ─────────────────────────────────────────────
        MeshWorld::ContainmentRuleRegistry cont_reg;
        fs::path cont_json = root / "data" / "taxonomy" / "containment.json";
        if (fs::exists(cont_json)) {
            cont_reg.load(cont_json);
            for (const auto& parent : [&]() {
                    // collect all unique parents
                    std::vector<std::string> parents;
                    for (const auto& node : tax_reg.all())
                        parents.push_back(node.id);
                    return parents;
                }()) {
                for (const auto& rule : cont_reg.children_of(parent)) {
                    MeshWorld::ContainmentRuleRow row;
                    row.parent      = rule.parent;
                    row.child       = rule.child;
                    row.probability = rule.probability;
                    row.min_count   = rule.min_count;
                    row.max_count   = rule.max_count;
                    row.lod_max     = rule.lod_max;
                    pack.upsert_containment_rule(row);
                }
            }
        }

        // ── Builtin materials ─────────────────────────────────────────────
        MeshWorld::register_builtin_materials();
        auto materials = MeshWorld::MaterialRegistry::instance().all();
        for (const auto& m : materials) {
            MeshWorld::MaterialRow row;
            row.id           = m.id;
            row.r            = m.r;
            row.g            = m.g;
            row.b            = m.b;
            row.roughness    = m.roughness;
            row.metallic     = m.metallic;
            row.spdx_license = m.license.spdx_license;
            row.author       = m.license.author;
            pack.upsert_material(row);
        }
        std::printf("  Packed %zu materials\n", materials.size());

        std::printf("Done. Content pack: %s\n", out_path.string().c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
