// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include <filesystem>

namespace MeshWorld {

// ContentPackLoader auto-detects the content source:
//  • dev mode  — loads from disk (generators/lua/, data/taxonomy/)
//  • pack mode — loads from a meshworld_content.sqlite file
//
// Call load() once at startup; it populates LuaGeneratorRegistry,
// TaxonomyRegistry, and ContainmentRuleRegistry.
class ContentPackLoader {
public:
    // Load from the given root directory (dev mode).
    // Looks for generators/lua/ and data/taxonomy/ under root.
    void load_from_disk(const std::filesystem::path& root);

    // Load from a packed SQLite content pack.
    void load_from_pack(const std::filesystem::path& pack_path);

    // Auto-detect: if pack_path exists, use pack; otherwise use disk root.
    void load_auto(const std::filesystem::path& root,
                   const std::filesystem::path& pack_path);
};

} // namespace MeshWorld
