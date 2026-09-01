// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Naming.hpp"

#include <cctype>
#include <vector>

#include "NameCulture.hpp"
#include "NameGenerator.hpp"
#include "NameRegistry.hpp"

namespace MeshWorld {

namespace {

std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// Seed a hash stream from (domain, seed) so different Naming functions
// called with the same raw seed still diverge.
std::uint64_t domain_hash(const char* domain, std::uint64_t seed) {
    std::uint64_t h = seed ^ 0xcbf29ce484222325ULL;
    for (const char* p = domain; *p != '\0'; ++p) {
        h ^= static_cast<unsigned char>(*p);
        h *= 0x100000001b3ULL;
    }
    return splitmix64(h);
}

std::string pick(const std::vector<std::string>& options, std::uint64_t h) {
    return options[h % options.size()];
}

// --- Lazy NameRegistry load (real pipeline) -------------------------------
//
// Loaded exactly once (C++11 "magic statics" guarantee thread-safe init) on
// first Naming:: call in the process. Returns nullptr if data/names/cultures
// couldn't be loaded relative to the process's cwd (e.g. a binary run from
// somewhere other than the repo root) -- every function below falls back to
// the original hardcoded stub in that case rather than throwing.
const NameRegistry* registry() {
    static const NameRegistry* const reg = [] () -> const NameRegistry* {
        static NameRegistry r;
        try {
            r.load("data/names/cultures");
        } catch (const std::exception&) {
            return nullptr;
        }
        return &r;
    }();
    return reg;
}

// Resolves `culture` through the loaded registry: the exact culture if
// known, else the registry's own "nordic" entry (matching this class's
// documented "unrecognized culture ids fall back to nordic" contract with a
// *real* nordic definition, not a separately-hand-maintained clone of it).
// nullptr only when the registry itself failed to load.
const NameCulture* resolved_culture(const std::string& culture) {
    const NameRegistry* reg = registry();
    if (!reg) return nullptr;
    if (reg->has(culture)) return &reg->get(culture);
    return reg->has("nordic") ? &reg->get("nordic") : nullptr;
}

// --- MAP6-era fallback stub (used only if the registry can't load) -------

struct Phonemes {
    std::vector<std::string> consonants;
    std::vector<std::string> vowels;
};

const Phonemes& fallback_phonemes_for(const std::string& culture) {
    static const Phonemes nordic{
        {"k", "r", "f", "st", "v", "h", "l", "b", "g", "sk", "n", "th"},
        {"a", "o", "i", "e", "u", "y"}};
    static const Phonemes romance{
        {"m", "r", "l", "v", "d", "t", "c", "p", "n", "s"},
        {"a", "e", "i", "o", "u"}};
    static const Phonemes desert{
        {"z", "q", "sh", "r", "m", "s", "t", "n", "dh", "kh"},
        {"a", "i", "u"}};

    if (culture == "romance") return romance;
    if (culture == "desert") return desert;
    return nordic; // default / unrecognized culture id
}

std::string capitalize(std::string s) {
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

// Assembles `syllables` syllables of C+V(+C) shape, capitalized.
std::string fallback_base_name(const std::string& culture, std::uint64_t seed, int syllables) {
    const Phonemes& ph = fallback_phonemes_for(culture);
    std::string out;
    std::uint64_t h = seed;
    for (int i = 0; i < syllables; ++i) {
        h = splitmix64(h + static_cast<std::uint64_t>(i) * 0x9E3779B1ULL);
        out += pick(ph.consonants, h);
        h = splitmix64(h);
        out += pick(ph.vowels, h);
        h = splitmix64(h);
        if ((h & 1u) != 0 && i + 1 < syllables) {
            out += pick(ph.consonants, h); // occasional closing consonant, mid-name only
        }
    }
    return capitalize(out);
}

} // namespace

const NameCulture* Naming::try_resolve_culture(const std::string& culture) {
    return resolved_culture(culture);
}

std::string Naming::culture(std::uint64_t seed) {
    if (const NameRegistry* reg = registry()) return reg->pick_culture(seed).id;

    static const std::vector<std::string> cultures{"nordic", "romance", "desert"};
    return pick(cultures, domain_hash("culture", seed));
}

std::string Naming::continent(const std::string& culture, std::uint64_t seed) {
    if (const NameCulture* c = resolved_culture(culture)) return NameGenerator::continent(*c, seed);

    static const std::vector<std::string> suffixes{"ia", "landia", "gard"};
    return fallback_base_name(culture, domain_hash("continent", seed), 2)
         + pick(suffixes, domain_hash("continent-suf", seed));
}

std::string Naming::country(const std::string& culture, std::uint64_t seed) {
    if (const NameCulture* c = resolved_culture(culture)) return NameGenerator::country(*c, seed);

    static const std::vector<std::string> suffixes{"land", "mark", "shire"};
    return fallback_base_name(culture, domain_hash("country", seed), 2)
         + pick(suffixes, domain_hash("country-suf", seed));
}

std::string Naming::city(const std::string& culture, std::uint64_t seed) {
    if (const NameCulture* c = resolved_culture(culture)) return NameGenerator::city(*c, seed);

    // map.md §9: cities get "-burg/-ton" style morphemes.
    static const std::vector<std::string> suffixes{"burg", "ton", "ham"};
    return fallback_base_name(culture, domain_hash("city", seed), 2)
         + pick(suffixes, domain_hash("city-suf", seed));
}

std::string Naming::river(const std::string& culture, std::uint64_t seed) {
    if (const NameCulture* c = resolved_culture(culture)) return NameGenerator::river(*c, seed);

    // map.md §9: rivers get "-water/-flow" style morphemes.
    static const std::vector<std::string> suffixes{"water", "flow", "brook"};
    return fallback_base_name(culture, domain_hash("river", seed), 1)
         + pick(suffixes, domain_hash("river-suf", seed));
}

std::string Naming::lake(const std::string& culture, std::uint64_t seed) {
    if (const NameCulture* c = resolved_culture(culture)) return NameGenerator::lake(*c, seed);

    // M132: lakes get "-mere/-tarn/-pond" style morphemes -- distinct from
    // river's "-water/-flow/-brook" so the two read as different feature
    // types even before the map.md §9 sketch (which predates hydrology
    // generation, M121+) ever considered lakes.
    static const std::vector<std::string> suffixes{"mere", "tarn", "pond"};
    return fallback_base_name(culture, domain_hash("lake", seed), 1)
         + pick(suffixes, domain_hash("lake-suf", seed));
}

std::string Naming::mountain(const std::string& culture, std::uint64_t seed) {
    if (const NameCulture* c = resolved_culture(culture)) return NameGenerator::mountain(*c, seed);

    // map.md §9: mountains get "-peak/-horn" style morphemes.
    static const std::vector<std::string> suffixes{"peak", "horn", "spire"};
    return fallback_base_name(culture, domain_hash("mountain", seed), 1)
         + pick(suffixes, domain_hash("mountain-suf", seed));
}

std::string Naming::street(const std::string& culture, std::uint64_t seed) {
    if (const NameCulture* c = resolved_culture(culture)) return NameGenerator::street(*c, seed);

    static const std::vector<std::string> adjectives{"Old", "Green", "High", "Silver", "New", "Quiet"};
    static const std::vector<std::string> nouns_nordic{"Birch", "Fjord", "Pine", "Elk", "Frost"};
    static const std::vector<std::string> nouns_romance{"Vine", "Piazza", "Olive", "Fountain", "Laurel"};
    static const std::vector<std::string> nouns_desert{"Palm", "Oasis", "Dune", "Cedar", "Well"};
    static const std::vector<std::string> suffixes{"Street", "Road", "Lane"};

    const std::vector<std::string>* nouns = &nouns_nordic;
    if (culture == "romance") nouns = &nouns_romance;
    else if (culture == "desert") nouns = &nouns_desert;

    const std::string adjective = pick(adjectives, domain_hash("street-adj", seed));
    const std::string noun      = pick(*nouns, domain_hash("street-noun", seed));
    const std::string suffix    = pick(suffixes, domain_hash("street-suf", seed));
    return adjective + " " + noun + " " + suffix;
}

} // namespace MeshWorld
