// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "ContentPackLoader.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "TaxonomyRegistry.hpp"
#include "ContainmentRuleRegistry.hpp"
#include "SqliteContentPack.hpp"

namespace MeshWorld {

void ContentPackLoader::load_from_disk(const std::filesystem::path& root) {
    LuaGeneratorRegistry::instance().load_from_dir(root / "generators" / "lua");
    TaxonomyRegistry::instance().load(root / "data" / "taxonomy" / "taxonomy.json");
    ContainmentRuleRegistry::instance().load(root / "data" / "taxonomy" / "containment.json");
}

void ContentPackLoader::load_from_pack(const std::filesystem::path& pack_path) {
    SqliteContentPack pack(pack_path);

    auto& lua_reg = LuaGeneratorRegistry::instance();
    for (const auto& row : pack.all_lua_generators())
        lua_reg.register_source(row.id, row.source);

    auto& tax_reg = TaxonomyRegistry::instance();
    for (const auto& row : pack.all_taxonomy_nodes()) {
        TaxonomyNode node;
        node.id   = row.id;
        node.kind = row.kind;
        node.name = row.name;
        tax_reg.load_node(std::move(node));
    }

    auto& cont_reg = ContainmentRuleRegistry::instance();
    for (const auto& row : pack.all_containment_rules()) {
        ContainmentRule rule;
        rule.parent      = row.parent;
        rule.child       = row.child;
        rule.probability = row.probability;
        rule.min_count   = row.min_count;
        rule.max_count   = row.max_count;
        rule.lod_max     = row.lod_max;
        cont_reg.load_rule(std::move(rule));
    }
}

void ContentPackLoader::load_auto(const std::filesystem::path& root,
                                  const std::filesystem::path& pack_path) {
    if (std::filesystem::exists(pack_path))
        load_from_pack(pack_path);
    else
        load_from_disk(root);
}

} // namespace MeshWorld
