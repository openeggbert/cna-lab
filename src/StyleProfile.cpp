// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "StyleProfile.hpp"

#include <algorithm>

namespace MeshWorld {

StyleProfileRegistry& StyleProfileRegistry::instance() {
    static StyleProfileRegistry inst;
    return inst;
}

void StyleProfileRegistry::register_profile(StyleProfile p) {
    profiles_[p.id] = std::move(p);
}

const StyleProfile* StyleProfileRegistry::get(const std::string& id) const {
    auto it = profiles_.find(id);
    return it != profiles_.end() ? &it->second : nullptr;
}

bool StyleProfileRegistry::has(const std::string& id) const {
    return profiles_.count(id) > 0;
}

const StyleProfile* StyleProfileRegistry::pick_for(std::uint64_t seed) const {
    if (profiles_.empty()) return nullptr;

    // Deterministic order regardless of unordered_map bucket layout --
    // same reasoning AssetRegistry::query() sorts its own results.
    std::vector<const StyleProfile*> sorted;
    sorted.reserve(profiles_.size());
    for (const auto& [id, p] : profiles_) sorted.push_back(&p);
    std::sort(sorted.begin(), sorted.end(),
              [](const StyleProfile* a, const StyleProfile* b) { return a->id < b->id; });

    // A simple 64-bit mix (splitmix64-style) rather than `seed %
    // sorted.size()` directly -- avoids low-order-bit correlation between
    // nearby seeds (adjacent chunk seeds in this codebase are themselves
    // hash outputs, not necessarily low-bit-independent by construction).
    std::uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);

    return sorted[z % sorted.size()];
}

void StyleProfileRegistry::clear_for_tests() {
    profiles_.clear();
}

} // namespace MeshWorld
