// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP5 tests. M072: NameCulture data model.

#include <gtest/gtest.h>

#include "NameCulture.hpp"

using namespace MeshWorld;

TEST(NameCultureTest, DefaultConstructedCultureIsEmptyWithSensibleDefaults) {
    NameCulture c;
    EXPECT_TRUE(c.id.empty());
    EXPECT_TRUE(c.consonants.empty());
    EXPECT_TRUE(c.vowels.empty());
    EXPECT_TRUE(c.syllable_templates.empty());
    EXPECT_FALSE(c.allow_repeated_seam_phoneme);

    EXPECT_TRUE(c.continent_suffixes.empty());
    EXPECT_TRUE(c.country_suffixes.empty());
    EXPECT_TRUE(c.city_suffixes.empty());
    EXPECT_TRUE(c.river_suffixes.empty());
    EXPECT_TRUE(c.lake_suffixes.empty());
    EXPECT_TRUE(c.mountain_suffixes.empty());

    EXPECT_TRUE(c.street_adjectives.empty());
    EXPECT_TRUE(c.street_nouns.empty());
    EXPECT_TRUE(c.street_suffixes.empty());

    // Shorter, punchier names for smaller features (rivers/lakes/mountains)
    // than for continents/countries/cities -- matches the existing
    // MAP6-era Naming.cpp stub's own per-type syllable counts.
    EXPECT_EQ(c.continent_syllables, 2);
    EXPECT_EQ(c.country_syllables, 2);
    EXPECT_EQ(c.city_syllables, 2);
    EXPECT_EQ(c.river_syllables, 1);
    EXPECT_EQ(c.lake_syllables, 1);
    EXPECT_EQ(c.mountain_syllables, 1);
}

// M072 — construct a real culture, populate every field, and read it back
// unchanged (a plain data struct, no derived/computed state to verify).
TEST(NameCultureTest, PopulatedCultureRoundTripsEveryField) {
    NameCulture c;
    c.id = "nordic";
    c.consonants = {"k", "r", "f", "st"};
    c.vowels = {"a", "o", "i"};
    c.syllable_templates = {"CV", "CVC", "CV"};  // "CV" listed twice: more likely
    c.allow_repeated_seam_phoneme = true;

    c.continent_suffixes = {"ia", "landia", "gard"};
    c.country_suffixes   = {"land", "mark", "shire"};
    c.city_suffixes       = {"burg", "ton", "ham"};
    c.river_suffixes      = {"water", "flow", "brook"};
    c.lake_suffixes       = {"mere", "tarn", "pond"};
    c.mountain_suffixes   = {"peak", "horn", "spire"};

    c.street_adjectives = {"Old", "Green", "High"};
    c.street_nouns       = {"Birch", "Fjord", "Pine"};
    c.street_suffixes    = {"Street", "Road", "Lane"};

    c.continent_syllables = 3;
    c.country_syllables   = 3;
    c.city_syllables       = 2;
    c.river_syllables      = 1;
    c.lake_syllables        = 1;
    c.mountain_syllables    = 2;

    EXPECT_EQ(c.id, "nordic");
    EXPECT_EQ(c.consonants.size(), 4u);
    EXPECT_EQ(c.vowels.size(), 3u);
    ASSERT_EQ(c.syllable_templates.size(), 3u);
    EXPECT_EQ(c.syllable_templates[0], "CV");
    EXPECT_EQ(c.syllable_templates[1], "CVC");
    EXPECT_TRUE(c.allow_repeated_seam_phoneme);

    EXPECT_EQ(c.continent_suffixes.size(), 3u);
    EXPECT_EQ(c.country_suffixes.size(), 3u);
    EXPECT_EQ(c.city_suffixes.size(), 3u);
    EXPECT_EQ(c.river_suffixes.size(), 3u);
    EXPECT_EQ(c.lake_suffixes.size(), 3u);
    EXPECT_EQ(c.mountain_suffixes.size(), 3u);

    EXPECT_EQ(c.street_adjectives.size(), 3u);
    EXPECT_EQ(c.street_nouns.size(), 3u);
    EXPECT_EQ(c.street_suffixes.size(), 3u);

    EXPECT_EQ(c.continent_syllables, 3);
    EXPECT_EQ(c.mountain_syllables, 2);
}

// Two independently constructed cultures must not share any hidden state
// (e.g. accidental static/shared containers) -- each instance owns its own
// vectors.
TEST(NameCultureTest, TwoCulturesAreIndependent) {
    NameCulture a;
    a.id = "nordic";
    a.consonants = {"k", "r"};

    NameCulture b;
    b.id = "desert";
    b.consonants = {"z", "q", "sh"};

    EXPECT_EQ(a.id, "nordic");
    EXPECT_EQ(b.id, "desert");
    EXPECT_EQ(a.consonants.size(), 2u);
    EXPECT_EQ(b.consonants.size(), 3u);
}
