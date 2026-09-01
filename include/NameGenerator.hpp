// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "NameCulture.hpp"

namespace MeshWorld {

// M076 (MAP5) — assembles a name string from a NameCulture + entropy.
// Interface only (M077 implements it in src/NameGenerator.cpp), matching the
// existing MAP-series convention of separate interface/impl commits (cf.
// M074/M075's NameRegistry, M088/M089's MapBuilder).
//
// Generalizes what Naming.cpp's base_name()/pick()/domain_hash()/splitmix64()
// helpers already do informally for 3 hardcoded cultures, reading a real
// NameCulture's own syllable_templates/consonants/vowels/
// allow_repeated_seam_phoneme fields instead of a fixed C+V(+C) shape.
//
// This covers only the shared syllable-assembly primitive. The per-feature-
// type public forms (continent()/country()/city()/river()/lake()/mountain()/
// street(), each with its own suffix/morpheme handling) are M078-M081; dedupe
// within a parent scope is M082; border blending between neighboring
// cultures is M083 -- none of those land here.
class NameGenerator {
public:
    // Assembles `syllables` syllables of culture's own phoneme set, honoring
    // its syllable_templates (picked by entropy, weighted by repetition in
    // the list -- see NameCulture.hpp's own doc comment) and
    // allow_repeated_seam_phoneme: when false, guarantees the last phoneme
    // of one syllable never equals the first phoneme of the next (deterministic
    // exclusion, not a probabilistic redraw -- a blind reroll from the same
    // pool could land on the same colliding phoneme again). Pure function of
    // its inputs: the same (culture, entropy, syllables) always assembles
    // the same string, mirroring the pure/seeded convention already used by
    // Naming:: and Map::Noise's hash2i/value_noise/fbm.
    static std::string assemble_base(const NameCulture& culture, std::uint64_t entropy,
                                      int syllables);

    // Deterministically picks one entry from `pool` given `entropy`. A
    // thin, reusable wrapper around the same hash-modulo convention
    // Naming.cpp's own pick() already uses -- exposed here since
    // assemble_base() and the future per-feature-type forms (M078-M081) both
    // need it. `pool` must not be empty.
    static const std::string& pick_from(const std::vector<std::string>& pool,
                                         std::uint64_t entropy);

    // M078 -- continent()/ocean() name forms: assemble_base() +
    // continent_suffixes, mirroring Naming.cpp's own continent()
    // (base_name(...) + pick(suffixes, ...)) shape. ocean() deliberately
    // reuses continent_syllables/continent_suffixes rather than needing its
    // own NameCulture morpheme pool -- plan.md bundles the two forms into
    // one task, and no Map::FeatureType::Ocean (named feature) exists yet to
    // justify a dedicated pool. Each form derives its own entropy via a
    // domain tag (see NameGenerator.cpp's local domain_hash()) so
    // continent(culture, seed) and ocean(culture, seed) never collide on
    // the same seed, mirroring Naming.cpp's own per-form domain separation.
    static std::string continent(const NameCulture& culture, std::uint64_t entropy);
    static std::string ocean(const NameCulture& culture, std::uint64_t entropy);

    // M079 -- country()/city()/town() name forms, same assemble_base() +
    // suffix shape as M078. town() deliberately reuses
    // city_syllables/city_suffixes rather than needing its own NameCulture
    // pool -- a town is a smaller settlement using the same naming register
    // as a city, and Map::FeatureType already distinguishes City vs. Town by
    // a size_hint string (see MapBuilder::addCity()), not by name style.
    static std::string country(const NameCulture& culture, std::uint64_t entropy);
    static std::string city(const NameCulture& culture, std::uint64_t entropy);
    static std::string town(const NameCulture& culture, std::uint64_t entropy);

    // M080 -- river()/mountain()/lake() name forms, same assemble_base() +
    // suffix shape as M078/M079. Unlike ocean()/town(), each of these has its
    // own dedicated NameCulture pool (river_syllables/river_suffixes, etc.)
    // -- no reuse decision needed here.
    static std::string river(const NameCulture& culture, std::uint64_t entropy);
    static std::string mountain(const NameCulture& culture, std::uint64_t entropy);
    static std::string lake(const NameCulture& culture, std::uint64_t entropy);

    // M081 -- street() name form: "<adjective> <noun> <suffix>", e.g. "Old
    // Birch Road". Unlike every other form above, this does NOT call
    // assemble_base() -- street names are 3 independent word-pool picks
    // (street_adjectives/street_nouns/street_suffixes) joined with spaces,
    // not a phonetically-assembled base name, mirroring Naming.cpp's own
    // street() exactly. Pools are already stored pre-capitalized in the
    // culture JSON files, so no capitalize() step is needed here.
    static std::string street(const NameCulture& culture, std::uint64_t entropy);

    // M082 -- dedupe within a parent scope: unlike every form above (all pure
    // functions of their explicit inputs), "no two cities in the same
    // country" needs to know what names are already used in that scope.
    // NameGenerator itself stays stateless -- the caller owns the
    // used_in_scope set (e.g. a country's already-placed city names,
    // accumulated across calls) and passes it in here. Retries `generate`
    // with perturbed entropy (splitmix64(entropy + attempt)) until the
    // result isn't in used_in_scope, up to max_attempts; if every attempt
    // still collides, returns the last attempt anyway rather than looping
    // forever or throwing -- a small suffix/phoneme pool has a finite number
    // of truly distinct outputs, so a rare unavoidable collision is an
    // acceptable outcome, not a bug. `generate` is form-agnostic: pass any of
    // the forms above via a lambda, e.g.
    // `[&](std::uint64_t e){ return NameGenerator::city(culture, e); }`.
    static std::string dedupe(const std::function<std::string(std::uint64_t)>& generate,
                               std::uint64_t entropy,
                               const std::unordered_set<std::string>& used_in_scope,
                               int max_attempts = 8);

    // M083 -- border blending between neighboring cultures. Blends at the
    // WHOLE-NAME level, not the phoneme level: mixing consonants/vowels from
    // two different phoneme sets within one syllable risks an
    // unpronounceable result, whereas picking culture_a or culture_b as a
    // whole for a given name (weighted by blend_t) keeps every individual
    // name pronounceable while the statistical mix across many names still
    // blends smoothly as blend_t sweeps from 0 to 1 across a border region.
    // blend_t in [0, 1]: 0 always picks culture_a, 1 always picks culture_b.
    // `generate` is form-agnostic like dedupe()'s callback, e.g.
    // `[](const NameCulture& c, std::uint64_t e){ return
    // NameGenerator::city(c, e); }`. Deterministic: the same 4 inputs always
    // pick the same culture and produce the same name.
    static std::string blend(const NameCulture& culture_a, const NameCulture& culture_b,
                              double blend_t, std::uint64_t entropy,
                              const std::function<std::string(const NameCulture&, std::uint64_t)>& generate);
};

} // namespace MeshWorld
