// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>

namespace MeshWorld {

struct NameCulture;

// Procedural place-name generator (MAP6, M091 — originally a stub for the
// Lua `names.*` binding). Phonotactic: assembles syllables from a phoneme
// set per "culture" and appends a feature-type morpheme, per map.md §9's
// naming sketch.
//
// Every function is a pure function of its inputs (culture id + seed) —
// same inputs always produce the same name, matching map.md's "deterministic
// given the tile entropy" requirement. Unlike the Lua sketch in map.md §9
// (which shows e.g. `names.city(culture)` with no seed), each function here
// also takes an explicit `seed` so that repeated calls for the same culture
// (many cities in one continent, say) produce distinct names — callers vary
// `seed` per call (e.g. a feature index). This mirrors the existing
// pure/seeded convention used by Map::Noise's hash2i/value_noise/fbm,
// avoiding a stateful RNG object threaded through the Lua binding.
//
// As of this task, every function here delegates to the full MAP5 pipeline
// (NameCulture/NameRegistry/NameGenerator, data/names/cultures/*.json) —
// this class's own external API (culture id + seed in, name out) never
// changed, exactly as NameCulture.hpp's own doc comment anticipated: "Naming::
// stays in place, calling into the new system once it exists, rather than
// every Lua/MAP10 call site switching at once." If data/names/cultures/
// can't be loaded relative to the process's cwd (a binary run from
// somewhere other than the repo root), every function falls back to the
// original MAP6-era hardcoded 3-culture stub instead of throwing — this
// class has never thrown and shouldn't start now.
class Naming {
public:
    // Deterministically derive a culture id ("nordic"/"romance"/"desert")
    // from a seed. Unrecognized culture ids passed to the functions below
    // fall back to "nordic".
    static std::string culture(std::uint64_t seed);

    static std::string continent(const std::string& culture, std::uint64_t seed);
    static std::string country(const std::string& culture, std::uint64_t seed);
    static std::string city(const std::string& culture, std::uint64_t seed);
    static std::string river(const std::string& culture, std::uint64_t seed);
    static std::string lake(const std::string& culture, std::uint64_t seed);
    static std::string mountain(const std::string& culture, std::uint64_t seed);
    // "<adjective> <culture-flavored noun> <Road|Street|Lane>", e.g. "Old Birch Road".
    static std::string street(const std::string& culture, std::uint64_t seed);

    // M341 (MAP22) -- exposes NameCulture object resolution (same lookup
    // culture()/continent()/country()/etc. already use internally) for
    // callers that need real NameGenerator::blend()/dedupe()-style access
    // beyond the simple (culture id, seed) forms above -- e.g. Countries::
    // name()'s border-blending. nullptr only when the registry itself
    // couldn't load (matches every other function's own registry-
    // unavailable fallback point) -- never for an unrecognized culture id
    // (falls back to "nordic", same as every other form here).
    static const NameCulture* try_resolve_culture(const std::string& culture);
};

} // namespace MeshWorld
