// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <vector>

namespace MeshWorld {

// M072 (MAP5) — the data model one naming "culture" is built from: phoneme
// sets, syllable templates, join rules, and per-feature-type morpheme
// pools (continent/country/city/river/lake/mountain/street). Loaded from
// data/names/cultures/*.json by NameRegistry (M073-M075); consumed by
// NameGenerator (M076-M083) to assemble actual names.
//
// Supersedes the MAP6-era `Naming.cpp` stub's hardcoded 3-culture
// `Phonemes` struct (nordic/romance/desert baked directly into source) —
// see `Naming.hpp`'s own doc comment. `Naming::` stays in place, calling
// into the new system once it exists, rather than every Lua/MAP10 call
// site switching at once.
//
// A syllable template is a string over {C, V} (e.g. "CV", "CVC", "VC"):
// C consumes one entry from `consonants`, V one entry from `vowels`.
// Listing a shape more than once makes it proportionally more likely —
// the same "weight by repetition, not a separate weight field" convention
// this codebase already uses for hash-modulo `pick()` (Naming.cpp) rather
// than introducing a new weighted-choice mechanism.
struct NameCulture {
    std::string id;  // e.g. "nordic" -- referenced by NameRegistry/Naming callers

    std::vector<std::string> consonants;
    std::vector<std::string> vowels;
    std::vector<std::string> syllable_templates;

    // --- join rules ---
    // Whether two adjacent syllables may repeat the same consonant at the
    // seam (e.g. "...k" + "k..." -> "...kk..."). Most cultures want this
    // false for readability; a culture with a sparse consonant set may
    // need it true to stay generatable at all. Kept as a single rule for
    // now (a V1 data model, per this codebase's own established
    // "simplest first, extend when a real culture needs more" pattern) —
    // add more fields here if a starter culture (M073) genuinely needs one.
    bool allow_repeated_seam_phoneme{false};

    // --- per-feature-type morpheme pools ---
    // Suffix appended after the assembled base name for each feature type.
    // Empty means "no suffix, use the base name as-is" — a culture is not
    // required to have a distinct convention for every feature type.
    std::vector<std::string> continent_suffixes;
    std::vector<std::string> country_suffixes;
    std::vector<std::string> city_suffixes;
    std::vector<std::string> river_suffixes;
    std::vector<std::string> lake_suffixes;
    std::vector<std::string> mountain_suffixes;

    // Street names assemble differently ("<adjective> <noun> <suffix>",
    // e.g. "Old Birch Road") — 3 independent pools, not one suffix list.
    std::vector<std::string> street_adjectives;
    std::vector<std::string> street_nouns;
    std::vector<std::string> street_suffixes;

    // How many syllables the base name has for each feature type, before
    // that type's suffix is appended — mirrors the existing stub's own
    // per-type syllable counts (2 for continent/country/city, 1 for
    // river/lake/mountain: shorter, punchier names for smaller features).
    int continent_syllables{2};
    int country_syllables{2};
    int city_syllables{2};
    int river_syllables{1};
    int lake_syllables{1};
    int mountain_syllables{1};
};

}  // namespace MeshWorld
