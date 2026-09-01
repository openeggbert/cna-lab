// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <MeshCraft/Mc3/Mc3AssetMetadata.hpp>

namespace MeshCraft::Mc3 { class Mc3Object; }

namespace MeshWorld {

// R113 (docs/world-composer-design.md §4) -- one indexed, queryable asset:
// a real Mc3Object definition plus its Mc3AssetMetadata (R111). Does NOT
// own the definition -- entries reference definitions that live in
// ObjectDefinitionLibrary (v1) or a resolved Mc3ImportResolver library map
// (v2+), exactly like those already do today.
struct AssetEntry {
    std::string id;
    std::shared_ptr<MeshCraft::Mc3::Mc3Object> def;
    MeshCraft::Mc3::Mc3AssetMetadata meta;
};

// Process-wide singleton, same "populate once at startup, read-only and
// safe to read from any thread after that" contract MaterialRegistry/
// LuaGeneratorRegistry/ObjectDefinitionLibrary already use.
class AssetRegistry {
public:
    static AssetRegistry& instance();

    void register_asset(AssetEntry entry);

    // Deliberately narrow for v1: exact category match + ALL of
    // required_style_tags must be present in the entry's own styleTags.
    // A richer scoring/ranking query (weighted tag overlap, partial
    // matches) is v2+ scope, once there's enough real content for "no
    // exact match" to be the common case rather than the only case
    // (docs/world-composer-design.md §4's own note on this).
    std::vector<const AssetEntry*> query(const std::string& category,
                                          const std::vector<std::string>& required_style_tags = {}) const;

    const AssetEntry* get(const std::string& id) const;
    bool has_category(const std::string& category) const;

    // Test-only: drops every registered entry. Mirrors
    // LuaGeneratorRegistry's own unregister-for-tests convention (tests
    // must restore/repopulate afterward if they touch the real
    // process-wide singleton).
    void clear_for_tests();

private:
    std::unordered_map<std::string, AssetEntry> by_id_;
};

} // namespace MeshWorld
