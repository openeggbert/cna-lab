// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP5 tests. M077: src/NameGenerator.cpp -- syllable assembly honoring
// templates. M078: continent()/ocean() name forms. M079: country()/city()/
// town() name forms. M080: river()/mountain()/lake() name forms. M081:
// street() name form. M082: dedupe within a parent scope. M083: border
// blending between neighboring cultures. M084: consolidated
// all-forms-stable-for-same-(culture,entropy) check (each per-form test
// above already asserts this individually; this one exists for direct
// traceability to plan.md's own M084 wording in one place).
//
// M085 ("generated names respect culture phoneme set -- no foreign
// phonemes") and M086 ("street names follow the pattern") are already
// fully covered by EveryPhonemeComesFromTheCulturesOwnPools (above) and
// StreetIsDeterministicAndFollowsAdjectiveNounSuffixShape (above)
// respectively -- audited, no new test added for either. M087 ("culture
// files load + validate; bad file rejected with clear error") is already
// fully covered by tests/NameRegistryTests.cpp's
// LoadRejectsFileMissingRequiredField/LoadRejectsEmptyRequiredPool/
// LoadRejectsMalformedJson/LoadMissingDirectoryThrows (failure path) and
// LoadsRealCultureDirectoryWithThreeKnownCultures/LoadValidFixtureSucceeds
// (success path) -- audited, no new test added.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "NameGenerator.hpp"
#include "NameRegistry.hpp"

namespace {

using MeshWorld::NameCulture;
using MeshWorld::NameGenerator;
using MeshWorld::NameRegistry;

const NameRegistry& real_registry() {
    static NameRegistry reg = [] {
        NameRegistry r;
        r.load("data/names/cultures");
        return r;
    }();
    return reg;
}

// True if every phoneme in `name` (case-insensitively) is built entirely
// from concatenations of the culture's own consonants/vowels -- i.e. no
// character sequence appears that couldn't have come from this culture's
// phoneme set. We check this the cheap way: lowercase the name and the
// whole phoneme set, then greedily strip known phonemes (longest first)
// off the front until nothing (a match) or a stuck prefix remains
// (a foreign phoneme).
bool built_entirely_from_culture_phonemes(const std::string& name, const NameCulture& culture) {
    std::string s;
    for (char c : name) s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::vector<std::string> phonemes = culture.consonants;
    phonemes.insert(phonemes.end(), culture.vowels.begin(), culture.vowels.end());
    std::sort(phonemes.begin(), phonemes.end(),
              [](const std::string& a, const std::string& b) { return a.size() > b.size(); });

    while (!s.empty()) {
        bool matched = false;
        for (const auto& p : phonemes) {
            if (s.compare(0, p.size(), p) == 0) {
                s.erase(0, p.size());
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }
    return true;
}

} // namespace

TEST(NameGeneratorTest, AssembleBaseIsDeterministicForSameInputs) {
    const NameCulture& nordic = real_registry().get("nordic");
    const std::string first = NameGenerator::assemble_base(nordic, 999, 2);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(NameGenerator::assemble_base(nordic, 999, 2), first);
    }
}

TEST(NameGeneratorTest, DifferentEntropyProducesDifferentNames) {
    const NameCulture& nordic = real_registry().get("nordic");
    std::set<std::string> names;
    for (std::uint64_t e = 0; e < 20; ++e) {
        names.insert(NameGenerator::assemble_base(nordic, e, 2));
    }
    EXPECT_GT(names.size(), 1u);
}

TEST(NameGeneratorTest, OutputIsNonEmptyAndCapitalized) {
    const NameCulture& romance = real_registry().get("romance");
    const std::string name = NameGenerator::assemble_base(romance, 42, 2);
    ASSERT_FALSE(name.empty());
    EXPECT_TRUE(std::isupper(static_cast<unsigned char>(name[0])));
}

TEST(NameGeneratorTest, EveryPhonemeComesFromTheCulturesOwnPools) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        for (std::uint64_t e = 0; e < 30; ++e) {
            const std::string name = NameGenerator::assemble_base(culture, e, 2);
            EXPECT_TRUE(built_entirely_from_culture_phonemes(name, culture))
                << "culture=" << id << " name=" << name;
        }
    }
}

TEST(NameGeneratorTest, PickFromIsDeterministicAndInBounds) {
    std::vector<std::string> pool{"a", "b", "c"};
    const std::string& first = NameGenerator::pick_from(pool, 12345);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(NameGenerator::pick_from(pool, 12345), first);
    }
    EXPECT_NE(std::find(pool.begin(), pool.end(), first), pool.end());
}

TEST(NameGeneratorTest, PickFromEmptyPoolThrows) {
    std::vector<std::string> empty;
    EXPECT_THROW(NameGenerator::pick_from(empty, 1), std::runtime_error);
}

// A synthetic culture with single-character phonemes and exactly one
// syllable_templates entry ("CVC") makes syllable boundaries computable by
// fixed character offset (3 chars/syllable), so the seam rule can actually
// be checked by direct string inspection -- real cultures use multi-char
// phonemes ("th"/"sk"/etc.), which would make that ambiguous.
NameCulture make_seam_test_culture(bool allow_repeat) {
    NameCulture c;
    c.id = "seamtest";
    c.consonants = {"k", "r"};
    c.vowels = {"a", "o"};
    c.syllable_templates = {"CVC"};
    c.allow_repeated_seam_phoneme = allow_repeat;
    return c;
}

TEST(NameGeneratorTest, SeamRuleForbidsRepeatWhenCultureDisallowsIt) {
    const NameCulture culture = make_seam_test_culture(/*allow_repeat=*/false);
    constexpr int kSyllables = 4;
    constexpr std::size_t kSyllableLen = 3; // "CVC" with single-char phonemes

    for (std::uint64_t e = 0; e < 500; ++e) {
        const std::string name = NameGenerator::assemble_base(culture, e, kSyllables);
        std::string lower;
        for (char ch : name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        ASSERT_EQ(lower.size(), kSyllableLen * kSyllables);

        for (int syl = 0; syl + 1 < kSyllables; ++syl) {
            const char last_of_this = lower[syl * kSyllableLen + kSyllableLen - 1];
            const char first_of_next = lower[(syl + 1) * kSyllableLen];
            EXPECT_NE(last_of_this, first_of_next)
                << "entropy=" << e << " name=" << name;
        }
    }
}

TEST(NameGeneratorTest, SeamRuleAllowsRepeatWhenCultureAllowsIt) {
    const NameCulture culture = make_seam_test_culture(/*allow_repeat=*/true);
    constexpr int kSyllables = 4;
    constexpr std::size_t kSyllableLen = 3;

    // With repeats allowed, at least one of many samples should show a
    // repeated seam phoneme -- proves the redraw logic is genuinely skipped,
    // not just coincidentally never triggered.
    bool saw_repeat = false;
    for (std::uint64_t e = 0; e < 500 && !saw_repeat; ++e) {
        const std::string name = NameGenerator::assemble_base(culture, e, kSyllables);
        std::string lower;
        for (char ch : name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        ASSERT_EQ(lower.size(), kSyllableLen * kSyllables);

        for (int syl = 0; syl + 1 < kSyllables; ++syl) {
            if (lower[syl * kSyllableLen + kSyllableLen - 1] == lower[(syl + 1) * kSyllableLen]) {
                saw_repeat = true;
                break;
            }
        }
    }
    EXPECT_TRUE(saw_repeat);
}

namespace {

bool ends_with_any(const std::string& s, const std::vector<std::string>& suffixes) {
    for (const auto& suf : suffixes) {
        if (s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(NameGeneratorTest, ContinentIsDeterministicAndEndsWithAContinentSuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::continent(culture, 7);
        EXPECT_EQ(NameGenerator::continent(culture, 7), first);
        EXPECT_TRUE(ends_with_any(first, culture.continent_suffixes)) << first;
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(first[0])));
    }
}

TEST(NameGeneratorTest, OceanIsDeterministicAndEndsWithAContinentSuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::ocean(culture, 7);
        EXPECT_EQ(NameGenerator::ocean(culture, 7), first);
        EXPECT_TRUE(ends_with_any(first, culture.continent_suffixes)) << first;
    }
}

TEST(NameGeneratorTest, ContinentAndOceanDivergeForTheSameSeed) {
    const NameCulture& nordic = real_registry().get("nordic");
    // Not a proof for every seed (domain separation is hash-based, not
    // guaranteed collision-free), but any real difference across many seeds
    // demonstrates the two forms are not just aliases of the same call.
    int differences = 0;
    for (std::uint64_t e = 0; e < 20; ++e) {
        if (NameGenerator::continent(nordic, e) != NameGenerator::ocean(nordic, e)) {
            ++differences;
        }
    }
    EXPECT_GT(differences, 0);
}

TEST(NameGeneratorTest, CountryIsDeterministicAndEndsWithACountrySuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::country(culture, 11);
        EXPECT_EQ(NameGenerator::country(culture, 11), first);
        EXPECT_TRUE(ends_with_any(first, culture.country_suffixes)) << first;
    }
}

TEST(NameGeneratorTest, CityIsDeterministicAndEndsWithACitySuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::city(culture, 13);
        EXPECT_EQ(NameGenerator::city(culture, 13), first);
        EXPECT_TRUE(ends_with_any(first, culture.city_suffixes)) << first;
    }
}

TEST(NameGeneratorTest, TownIsDeterministicAndEndsWithACitySuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::town(culture, 17);
        EXPECT_EQ(NameGenerator::town(culture, 17), first);
        // town() deliberately reuses city_suffixes (see NameGenerator.hpp).
        EXPECT_TRUE(ends_with_any(first, culture.city_suffixes)) << first;
    }
}

TEST(NameGeneratorTest, RiverIsDeterministicAndEndsWithARiverSuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::river(culture, 19);
        EXPECT_EQ(NameGenerator::river(culture, 19), first);
        EXPECT_TRUE(ends_with_any(first, culture.river_suffixes)) << first;
    }
}

TEST(NameGeneratorTest, MountainIsDeterministicAndEndsWithAMountainSuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::mountain(culture, 23);
        EXPECT_EQ(NameGenerator::mountain(culture, 23), first);
        EXPECT_TRUE(ends_with_any(first, culture.mountain_suffixes)) << first;
    }
}

TEST(NameGeneratorTest, LakeIsDeterministicAndEndsWithALakeSuffix) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::lake(culture, 29);
        EXPECT_EQ(NameGenerator::lake(culture, 29), first);
        EXPECT_TRUE(ends_with_any(first, culture.lake_suffixes)) << first;
    }
}

TEST(NameGeneratorTest, StreetIsDeterministicAndFollowsAdjectiveNounSuffixShape) {
    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        const std::string first = NameGenerator::street(culture, 31);
        EXPECT_EQ(NameGenerator::street(culture, 31), first);

        std::vector<std::string> words;
        std::string word;
        for (char c : first) {
            if (c == ' ') {
                words.push_back(word);
                word.clear();
            } else {
                word += c;
            }
        }
        words.push_back(word);

        ASSERT_EQ(words.size(), 3u) << first;
        EXPECT_NE(std::find(culture.street_adjectives.begin(), culture.street_adjectives.end(),
                             words[0]),
                  culture.street_adjectives.end())
            << first;
        EXPECT_NE(std::find(culture.street_nouns.begin(), culture.street_nouns.end(), words[1]),
                  culture.street_nouns.end())
            << first;
        EXPECT_NE(std::find(culture.street_suffixes.begin(), culture.street_suffixes.end(),
                             words[2]),
                  culture.street_suffixes.end())
            << first;
    }
}

TEST(NameGeneratorTest, AllPerFeatureTypeFormsDivergeForTheSameSeed) {
    const NameCulture& nordic = real_registry().get("nordic");
    using Form = std::string (*)(const NameCulture&, std::uint64_t);
    const std::vector<std::pair<std::string, Form>> forms{
        {"continent", &NameGenerator::continent}, {"ocean", &NameGenerator::ocean},
        {"country", &NameGenerator::country},     {"city", &NameGenerator::city},
        {"town", &NameGenerator::town},           {"river", &NameGenerator::river},
        {"mountain", &NameGenerator::mountain},   {"lake", &NameGenerator::lake},
        {"street", &NameGenerator::street},
    };

    // At some sampled seed, every pair of forms should produce a different
    // name at least once -- proves each form has its own domain tag, not
    // that two forms happen to share one.
    std::set<std::pair<int, int>> ever_differed;
    for (std::uint64_t e = 0; e < 20; ++e) {
        std::vector<std::string> names;
        for (const auto& [name, fn] : forms) names.push_back(fn(nordic, e));
        for (std::size_t i = 0; i < names.size(); ++i) {
            for (std::size_t j = i + 1; j < names.size(); ++j) {
                if (names[i] != names[j]) {
                    ever_differed.insert({static_cast<int>(i), static_cast<int>(j)});
                }
            }
        }
    }
    const std::size_t expected_pairs = forms.size() * (forms.size() - 1) / 2;
    EXPECT_EQ(ever_differed.size(), expected_pairs);
}

TEST(NameGeneratorTest, DedupeReturnsFirstCandidateWhenNothingCollides) {
    const NameCulture& nordic = real_registry().get("nordic");
    const std::unordered_set<std::string> used_in_scope; // empty -- nothing collides
    const std::string expected = NameGenerator::city(nordic, 41);
    const std::string result = NameGenerator::dedupe(
        [&](std::uint64_t e) { return NameGenerator::city(nordic, e); }, 41, used_in_scope);
    EXPECT_EQ(result, expected);
}

TEST(NameGeneratorTest, DedupeFindsADifferentNameWhenFirstCandidateCollides) {
    const NameCulture& nordic = real_registry().get("nordic");
    const std::string first_candidate = NameGenerator::city(nordic, 41);
    const std::unordered_set<std::string> used_in_scope{first_candidate};

    const std::string result = NameGenerator::dedupe(
        [&](std::uint64_t e) { return NameGenerator::city(nordic, e); }, 41, used_in_scope);
    EXPECT_NE(result, first_candidate);
}

TEST(NameGeneratorTest, DedupeIsDeterministic) {
    const NameCulture& nordic = real_registry().get("nordic");
    const std::unordered_set<std::string> used_in_scope{NameGenerator::city(nordic, 41)};
    auto generate = [&](std::uint64_t e) { return NameGenerator::city(nordic, e); };

    const std::string first = NameGenerator::dedupe(generate, 41, used_in_scope);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(NameGenerator::dedupe(generate, 41, used_in_scope), first);
    }
}

TEST(NameGeneratorTest, DedupeGivesUpGracefullyWhenEveryAttemptCollides) {
    // A culture whose phoneme/suffix/template pools each have exactly one
    // entry can only ever produce one possible name -- every dedupe attempt
    // collides. dedupe() must still return promptly (not hang) and return
    // that one name rather than throwing.
    NameCulture single_output;
    single_output.id = "single";
    single_output.consonants = {"k"};
    single_output.vowels = {"a"};
    single_output.syllable_templates = {"CV"};
    single_output.allow_repeated_seam_phoneme = true;
    single_output.continent_suffixes = {"x"};
    single_output.continent_syllables = 1;

    const std::string only_possible_name = NameGenerator::continent(single_output, 1);
    const std::unordered_set<std::string> used_in_scope{only_possible_name};

    std::string result;
    EXPECT_NO_THROW(result = NameGenerator::dedupe(
        [&](std::uint64_t e) { return NameGenerator::continent(single_output, e); }, 1,
        used_in_scope, /*max_attempts=*/8));
    EXPECT_EQ(result, only_possible_name);
}

TEST(NameGeneratorTest, BlendZeroAlwaysPicksCultureA) {
    const NameCulture& nordic = real_registry().get("nordic");
    const NameCulture& romance = real_registry().get("romance");
    auto generate = [](const NameCulture& c, std::uint64_t e) { return NameGenerator::city(c, e); };

    for (std::uint64_t e = 0; e < 20; ++e) {
        EXPECT_EQ(NameGenerator::blend(nordic, romance, 0.0, e, generate),
                  NameGenerator::city(nordic, e));
    }
}

TEST(NameGeneratorTest, BlendOneAlwaysPicksCultureB) {
    const NameCulture& nordic = real_registry().get("nordic");
    const NameCulture& romance = real_registry().get("romance");
    auto generate = [](const NameCulture& c, std::uint64_t e) { return NameGenerator::city(c, e); };

    for (std::uint64_t e = 0; e < 20; ++e) {
        EXPECT_EQ(NameGenerator::blend(nordic, romance, 1.0, e, generate),
                  NameGenerator::city(romance, e));
    }
}

TEST(NameGeneratorTest, BlendIsDeterministic) {
    const NameCulture& nordic = real_registry().get("nordic");
    const NameCulture& romance = real_registry().get("romance");
    auto generate = [](const NameCulture& c, std::uint64_t e) { return NameGenerator::city(c, e); };

    const std::string first = NameGenerator::blend(nordic, romance, 0.5, 77, generate);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(NameGenerator::blend(nordic, romance, 0.5, 77, generate), first);
    }
}

TEST(NameGeneratorTest, BlendHalfProducesARoughlyEvenMixAcrossManySamples) {
    const NameCulture& nordic = real_registry().get("nordic");
    const NameCulture& romance = real_registry().get("romance");
    auto generate = [](const NameCulture& c, std::uint64_t e) { return NameGenerator::city(c, e); };

    int from_a = 0, from_b = 0;
    constexpr std::uint64_t kSamples = 200;
    for (std::uint64_t e = 0; e < kSamples; ++e) {
        const std::string result = NameGenerator::blend(nordic, romance, 0.5, e, generate);
        if (result == NameGenerator::city(nordic, e)) {
            ++from_a;
        } else {
            ASSERT_EQ(result, NameGenerator::city(romance, e));
            ++from_b;
        }
    }
    // Statistical, not exact -- allow a wide tolerance band around the 50/50 split.
    EXPECT_GT(from_a, static_cast<int>(kSamples) / 4);
    EXPECT_GT(from_b, static_cast<int>(kSamples) / 4);
}

// M084: names are stable for the same (culture, entropy, feature-type) --
// consolidates what every per-form test above already asserts individually
// into one place, directly traceable to plan.md's own M084 wording.
TEST(NameGeneratorTest, AllFormsAreStableForTheSameCultureEntropyAndFeatureType) {
    using Form = std::string (*)(const NameCulture&, std::uint64_t);
    const std::vector<Form> forms{
        &NameGenerator::continent, &NameGenerator::ocean,    &NameGenerator::country,
        &NameGenerator::city,      &NameGenerator::town,     &NameGenerator::river,
        &NameGenerator::mountain,  &NameGenerator::lake,     &NameGenerator::street,
    };

    for (const char* id : {"nordic", "romance", "desert"}) {
        const NameCulture& culture = real_registry().get(id);
        for (auto form : forms) {
            for (std::uint64_t e = 0; e < 5; ++e) {
                const std::string first = form(culture, e);
                for (int repeat = 0; repeat < 3; ++repeat) {
                    EXPECT_EQ(form(culture, e), first) << "culture=" << id << " entropy=" << e;
                }
            }
        }
    }
}
