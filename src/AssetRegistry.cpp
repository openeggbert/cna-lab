// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "AssetRegistry.hpp"

#include <algorithm>

namespace MeshWorld {

AssetRegistry& AssetRegistry::instance() {
    static AssetRegistry inst;
    return inst;
}

void AssetRegistry::register_asset(AssetEntry entry) {
    by_id_[entry.id] = std::move(entry);
}

std::vector<const AssetEntry*> AssetRegistry::query(
    const std::string& category,
    const std::vector<std::string>& required_style_tags) const {
    std::vector<const AssetEntry*> out;
    for (const auto& [id, entry] : by_id_) {
        if (entry.meta.category != category) continue;
        bool all_tags_present = true;
        for (const auto& tag : required_style_tags) {
            if (std::find(entry.meta.styleTags.begin(), entry.meta.styleTags.end(), tag)
                == entry.meta.styleTags.end()) {
                all_tags_present = false;
                break;
            }
        }
        if (!all_tags_present) continue;
        out.push_back(&entry);
    }
    // Deterministic order regardless of unordered_map bucket layout --
    // callers (BuildingComposer) pick among results via a seeded index,
    // which must be stable across runs/processes.
    std::sort(out.begin(), out.end(),
              [](const AssetEntry* a, const AssetEntry* b) { return a->id < b->id; });
    return out;
}

const AssetEntry* AssetRegistry::get(const std::string& id) const {
    auto it = by_id_.find(id);
    return it != by_id_.end() ? &it->second : nullptr;
}

bool AssetRegistry::has_category(const std::string& category) const {
    for (const auto& [id, entry] : by_id_)
        if (entry.meta.category == category) return true;
    return false;
}

void AssetRegistry::clear_for_tests() {
    by_id_.clear();
}

} // namespace MeshWorld
