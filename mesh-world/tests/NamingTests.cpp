// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M091 — Naming:: (MAP6, Lua map-generator binding). As of this task,
// Naming:: delegates to the full MAP5 pipeline (NameCulture/NameRegistry/
// NameGenerator, data/names/cultures/*.json) with the original MAP6-era
// hardcoded stub kept only as a fallback (see Naming.cpp's own doc comment).
// These tests check black-box properties of Naming::'s own API, which must
// hold regardless of which of the two is actually active.

#include <gtest/gtest.h>

#include <cctype>
#include <set>

#include "Naming.hpp"

using namespace MeshWorld;

TEST(NamingTest, CultureIsDeterministicForSameSeed) {
    EXPECT_EQ(Naming::culture(42), Naming::culture(42));
}

TEST(NamingTest, CultureReturnsOnlyKnownIds) {
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        const std::string c = Naming::culture(seed);
        EXPECT_TRUE(c == "nordic" || c == "romance" || c == "desert") << c;
    }
}

TEST(NamingTest, CultureVariesAcrossSeeds) {
    std::set<std::string> seen;
    for (std::uint64_t seed = 0; seed < 50; ++seed) seen.insert(Naming::culture(seed));
    EXPECT_GT(seen.size(), 1u);
}

TEST(NamingTest, PerFeatureNamesAreDeterministicForSameCultureAndSeed) {
    EXPECT_EQ(Naming::continent("nordic", 7), Naming::continent("nordic", 7));
    EXPECT_EQ(Naming::country("nordic", 7), Naming::country("nordic", 7));
    EXPECT_EQ(Naming::city("nordic", 7), Naming::city("nordic", 7));
    EXPECT_EQ(Naming::river("nordic", 7), Naming::river("nordic", 7));
    EXPECT_EQ(Naming::lake("nordic", 7), Naming::lake("nordic", 7));
    EXPECT_EQ(Naming::mountain("nordic", 7), Naming::mountain("nordic", 7));
    EXPECT_EQ(Naming::street("nordic", 7), Naming::street("nordic", 7));
}

TEST(NamingTest, DifferentSeedsProduceDifferentNamesWithinACulture) {
    std::set<std::string> cities;
    for (std::uint64_t seed = 0; seed < 20; ++seed) cities.insert(Naming::city("nordic", seed));
    EXPECT_GT(cities.size(), 1u);
}

TEST(NamingTest, DifferentFeatureTypesDivergeForTheSameRawSeed) {
    // Same raw seed, different domains (city/river/mountain/...) must not
    // collapse to the same base name.
    EXPECT_NE(Naming::city("nordic", 1), Naming::river("nordic", 1));
    EXPECT_NE(Naming::river("nordic", 1), Naming::mountain("nordic", 1));
    EXPECT_NE(Naming::river("nordic", 1), Naming::lake("nordic", 1));
    EXPECT_NE(Naming::lake("nordic", 1), Naming::mountain("nordic", 1));
}

TEST(NamingTest, NamesAreNonEmptyAndCapitalized) {
    for (const std::string& culture : {std::string("nordic"), std::string("romance"), std::string("desert")}) {
        const std::string continent = Naming::continent(culture, 3);
        const std::string country   = Naming::country(culture, 3);
        const std::string city      = Naming::city(culture, 3);
        const std::string river     = Naming::river(culture, 3);
        const std::string lake      = Naming::lake(culture, 3);
        const std::string mountain  = Naming::mountain(culture, 3);
        ASSERT_FALSE(continent.empty());
        ASSERT_FALSE(country.empty());
        ASSERT_FALSE(city.empty());
        ASSERT_FALSE(river.empty());
        ASSERT_FALSE(lake.empty());
        ASSERT_FALSE(mountain.empty());
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(continent[0])));
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(country[0])));
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(city[0])));
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(river[0])));
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(lake[0])));
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(mountain[0])));
    }
}

TEST(NamingTest, StreetFollowsAdjectiveNounSuffixPattern) {
    const std::string s = Naming::street("nordic", 11);
    const auto first_space = s.find(' ');
    const auto second_space = s.find(' ', first_space + 1);
    ASSERT_NE(first_space, std::string::npos);
    ASSERT_NE(second_space, std::string::npos);
    const std::string suffix = s.substr(second_space + 1);
    EXPECT_TRUE(suffix == "Street" || suffix == "Road" || suffix == "Lane") << s;
}

TEST(NamingTest, StreetVocabularyDiffersByCulture) {
    // Same seed, different culture -> different noun pool -> names should
    // diverge (not guaranteed for every seed, but true for this one).
    EXPECT_NE(Naming::street("nordic", 5), Naming::street("desert", 5));
}

TEST(NamingTest, UnknownCultureFallsBackToNordicWithoutCrashing) {
    EXPECT_EQ(Naming::city("atlantean", 9), Naming::city("nordic", 9));
}

// Regression: Naming:: now delegates to the real NameGenerator/NameRegistry
// pipeline (data/names/cultures/*.json), falling back to the original
// hardcoded stub only if that data can't be loaded. desert.json's own
// city_suffixes (abad/ir/oom) are deliberately different from the
// fallback's shared burg/ton/ham list -- if this test ever starts failing,
// Naming:: has silently fallen back to the stub instead of the real
// pipeline (e.g. data/names/cultures/ stopped being found relative to cwd).
TEST(NamingTest, RealPipelineUsesCultureSpecificSuffixesNotTheHardcodedFallback) {
    static const std::set<std::string> desert_city_suffixes{"abad", "ir", "oom"};
    bool matched = false;
    for (std::uint64_t seed = 0; seed < 30 && !matched; ++seed) {
        const std::string name = Naming::city("desert", seed);
        for (const auto& suffix : desert_city_suffixes) {
            if (name.size() > suffix.size()
                && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
                matched = true;
                break;
            }
        }
    }
    EXPECT_TRUE(matched) << "expected at least one desert city name ending in "
                            "abad/ir/oom across 30 seeds";
}

TEST(NamingTest, LakeUsesItsOwnSuffixVocabulary) {
    static const std::set<std::string> suffixes{"mere", "tarn", "pond"};
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const std::string name = Naming::lake("nordic", seed);
        bool matched = false;
        for (const auto& suffix : suffixes)
            if (name.size() > suffix.size()
                && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                matched = true;
        EXPECT_TRUE(matched) << name;
    }
}
