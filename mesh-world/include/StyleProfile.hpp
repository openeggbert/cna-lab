// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace MeshWorld {

// R113 (docs/world-composer-design.md §5) -- mesh_world_revival.md §11's
// own JSON example, promoted to a real type. Answers "what FAMILY of
// window/door/roof is coherent together for this building" -- a
// selection concern, distinct from Style/StyleRegistry (G11), which only
// ever answers "what material for this palette key". Additive: does not
// replace Style, and materialStyleId is the bridge back to it for a
// composed building's own raw-primitive materials (foundation, ground).
struct StyleProfile {
    std::string id;               // e.g. "central_europe_default"
    std::string region;           // "central_europe"
    std::string period;           // "1890_1930"
    std::string wealth;           // "poor" | "middle" | "wealthy"
    std::string facadeFamily;     // matched against AssetEntry.meta.styleTags
    std::string windowFamily;
    std::string roofFamily;
    std::string materialStyleId;  // optional: an existing StyleRegistry Style id
};

class StyleProfileRegistry {
public:
    static StyleProfileRegistry& instance();

    void register_profile(StyleProfile p);
    const StyleProfile* get(const std::string& id) const;
    bool has(const std::string& id) const;

    // Deterministic profile pick for a given seed, among all registered
    // profiles. Returns nullptr if none are registered.
    const StyleProfile* pick_for(std::uint64_t seed) const;

    // Test-only: mirrors AssetRegistry::clear_for_tests()'s own contract.
    void clear_for_tests();

private:
    std::unordered_map<std::string, StyleProfile> profiles_;
};

} // namespace MeshWorld
