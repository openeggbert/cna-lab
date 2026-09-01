// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "NameGenerator.hpp"

#include <cctype>
#include <stdexcept>

namespace MeshWorld {

namespace {

std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::string capitalize(std::string s) {
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

// Seeds a hash stream from (domain, seed) so different per-feature-type
// forms called with the same raw seed still diverge -- same algorithm as
// Naming.cpp's own domain_hash(), duplicated locally rather than shared
// since that copy is private to Naming.cpp's own anonymous namespace.
std::uint64_t domain_hash(const char* domain, std::uint64_t seed) {
    std::uint64_t h = seed ^ 0xcbf29ce484222325ULL;
    for (const char* p = domain; *p != '\0'; ++p) {
        h ^= static_cast<unsigned char>(*p);
        h *= 0x100000001b3ULL;
    }
    return splitmix64(h);
}

} // namespace

const std::string& NameGenerator::pick_from(const std::vector<std::string>& pool,
                                             std::uint64_t entropy) {
    if (pool.empty()) {
        throw std::runtime_error("NameGenerator::pick_from: empty pool");
    }
    return pool[splitmix64(entropy) % pool.size()];
}

std::string NameGenerator::assemble_base(const NameCulture& culture, std::uint64_t entropy,
                                          int syllables) {
    std::uint64_t h = splitmix64(entropy);
    const std::string& templ = pick_from(culture.syllable_templates, h);
    h = splitmix64(h);

    std::string out;
    std::string last_phoneme;
    bool have_last_phoneme = false;

    for (int syl = 0; syl < syllables; ++syl) {
        std::string syllable_last_phoneme;
        bool first_in_syllable = true;

        for (char shape : templ) {
            const std::vector<std::string>& pool = (shape == 'V') ? culture.vowels : culture.consonants;
            std::string phoneme = pick_from(pool, h);
            h = splitmix64(h);

            // Seam rule: the first phoneme of this syllable must not repeat
            // the last phoneme of the previous one, unless the culture
            // allows it. A blind redraw from the same pool could land on the
            // same colliding phoneme again (certain small pools even have a
            // 50% chance of it), so pick from the pool with the colliding
            // entry filtered out instead -- deterministic and guaranteed to
            // differ whenever the pool has another option.
            if (first_in_syllable && have_last_phoneme && !culture.allow_repeated_seam_phoneme &&
                phoneme == last_phoneme && pool.size() > 1) {
                std::vector<std::string> filtered;
                filtered.reserve(pool.size() - 1);
                for (const auto& candidate : pool) {
                    if (candidate != last_phoneme) filtered.push_back(candidate);
                }
                if (!filtered.empty()) {
                    phoneme = pick_from(filtered, h);
                    h = splitmix64(h);
                }
            }

            out += phoneme;
            syllable_last_phoneme = phoneme;
            first_in_syllable = false;
        }

        last_phoneme = syllable_last_phoneme;
        have_last_phoneme = true;
    }

    return capitalize(out);
}

std::string NameGenerator::continent(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("continent", entropy), culture.continent_syllables)
         + pick_from(culture.continent_suffixes, domain_hash("continent-suf", entropy));
}

std::string NameGenerator::ocean(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("ocean", entropy), culture.continent_syllables)
         + pick_from(culture.continent_suffixes, domain_hash("ocean-suf", entropy));
}

std::string NameGenerator::country(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("country", entropy), culture.country_syllables)
         + pick_from(culture.country_suffixes, domain_hash("country-suf", entropy));
}

std::string NameGenerator::city(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("city", entropy), culture.city_syllables)
         + pick_from(culture.city_suffixes, domain_hash("city-suf", entropy));
}

std::string NameGenerator::town(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("town", entropy), culture.city_syllables)
         + pick_from(culture.city_suffixes, domain_hash("town-suf", entropy));
}

std::string NameGenerator::river(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("river", entropy), culture.river_syllables)
         + pick_from(culture.river_suffixes, domain_hash("river-suf", entropy));
}

std::string NameGenerator::mountain(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("mountain", entropy), culture.mountain_syllables)
         + pick_from(culture.mountain_suffixes, domain_hash("mountain-suf", entropy));
}

std::string NameGenerator::lake(const NameCulture& culture, std::uint64_t entropy) {
    return assemble_base(culture, domain_hash("lake", entropy), culture.lake_syllables)
         + pick_from(culture.lake_suffixes, domain_hash("lake-suf", entropy));
}

std::string NameGenerator::street(const NameCulture& culture, std::uint64_t entropy) {
    return pick_from(culture.street_adjectives, domain_hash("street-adj", entropy)) + " "
         + pick_from(culture.street_nouns, domain_hash("street-noun", entropy)) + " "
         + pick_from(culture.street_suffixes, domain_hash("street-suf", entropy));
}

std::string NameGenerator::dedupe(const std::function<std::string(std::uint64_t)>& generate,
                                   std::uint64_t entropy,
                                   const std::unordered_set<std::string>& used_in_scope,
                                   int max_attempts) {
    std::string candidate;
    std::uint64_t h = entropy;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        candidate = generate(h);
        if (used_in_scope.find(candidate) == used_in_scope.end()) {
            return candidate;
        }
        h = splitmix64(h + static_cast<std::uint64_t>(attempt) + 1);
    }
    return candidate;
}

std::string NameGenerator::blend(
    const NameCulture& culture_a, const NameCulture& culture_b, double blend_t,
    std::uint64_t entropy,
    const std::function<std::string(const NameCulture&, std::uint64_t)>& generate) {
    const std::uint64_t h = domain_hash("blend-pick", entropy);
    const double roll = static_cast<double>(h) / static_cast<double>(UINT64_MAX);
    const NameCulture& chosen = (roll < blend_t) ? culture_b : culture_a;
    return generate(chosen, entropy);
}

} // namespace MeshWorld
