// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "NameCulture.hpp"

namespace MeshWorld {

// M074 (MAP5) — loads NameCulture instances from data/names/cultures/*.json
// and picks one deterministically given an entropy value. Interface only
// (M075 implements load()/validation in src/NameRegistry.cpp), matching the
// existing MAP-series convention of separate interface/impl commits (cf.
// M088's MapBuilder.hpp / M089's MapBuilder.cpp).
//
// Instance-based rather than a static singleton (unlike Naming::, whose 3
// cultures are baked into source): callers construct/load their own registry
// so tests can point at a fixture directory instead of the real
// data/names/cultures/ (needed by M087's "bad file rejected with clear
// error" test), mirroring TaxonomyRegistry's load(path)-based design rather
// than MaterialRegistry's instance()-singleton one.
class NameRegistry {
public:
    // Loads every *.json file directly inside dir (non-recursive) as a
    // NameCulture, keyed by each file's own "id" field (not the filename --
    // though by convention they match, see data/names/cultures/*.json).
    // Throws std::runtime_error on a missing directory, unreadable file, or
    // a file missing a required NameCulture field (M075's validation).
    // Replaces any previously loaded cultures; safe to call more than once.
    void load(const std::filesystem::path& dir);

    // Direct lookup by id (e.g. "nordic"). Throws std::out_of_range if
    // unknown. Needed by M083's border-blending, which compares two
    // specific neighboring cultures rather than picking by entropy.
    const NameCulture& get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<NameCulture> all() const;

    // Deterministically selects one loaded culture from an entropy value --
    // same entropy always picks the same culture (mirrors the
    // pure/seeded convention already used by Naming::culture() and
    // Map::Noise's hash2i/value_noise/fbm). Throws std::runtime_error if no
    // cultures are loaded yet.
    const NameCulture& pick_culture(std::uint64_t entropy) const;

private:
    // Insertion order matters: pick_culture()'s entropy-to-index mapping
    // must be stable across runs, which an unordered_map's iteration order
    // cannot guarantee -- so cultures are held in load order here, with the
    // map below only for id-based lookup.
    std::vector<NameCulture> cultures_;
    std::unordered_map<std::string, std::size_t> index_by_id_;
};

} // namespace MeshWorld
