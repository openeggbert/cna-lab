// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP5 tests. M075: src/NameRegistry.cpp -- implementation + validation of
// culture files.

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "NameRegistry.hpp"

namespace {

using MeshWorld::NameRegistry;

std::filesystem::path real_cultures_dir() {
    return "data/names/cultures";
}

// A scratch directory this test suite fully owns -- created fresh, removed
// at the end. Used for the malformed-file cases, which must not touch the
// real data/names/cultures/ directory.
class TempCultureDir {
public:
    TempCultureDir() {
        dir_ = std::filesystem::temp_directory_path() /
               "meshworld_name_registry_test";
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }
    ~TempCultureDir() { std::filesystem::remove_all(dir_); }

    void write(const std::string& filename, const std::string& contents) const {
        std::ofstream f(dir_ / filename);
        f << contents;
    }

    const std::filesystem::path& path() const { return dir_; }

private:
    std::filesystem::path dir_;
};

const char* kValidCulture = R"({
  "id": "testculture",
  "consonants": ["k", "r"],
  "vowels": ["a", "o"],
  "syllable_templates": ["CV"],
  "allow_repeated_seam_phoneme": false,
  "continent_suffixes": ["ia"],
  "country_suffixes": ["land"],
  "city_suffixes": ["burg"],
  "river_suffixes": ["water"],
  "lake_suffixes": ["mere"],
  "mountain_suffixes": ["peak"],
  "street_adjectives": ["Old"],
  "street_nouns": ["Birch"],
  "street_suffixes": ["Street"],
  "continent_syllables": 2,
  "country_syllables": 2,
  "city_syllables": 2,
  "river_syllables": 1,
  "lake_syllables": 1,
  "mountain_syllables": 1
})";

} // namespace

TEST(NameRegistryTest, LoadsRealCultureDirectoryWithThreeKnownCultures) {
    NameRegistry reg;
    reg.load(real_cultures_dir());

    EXPECT_TRUE(reg.has("nordic"));
    EXPECT_TRUE(reg.has("romance"));
    EXPECT_TRUE(reg.has("desert"));
    EXPECT_FALSE(reg.has("no-such-culture"));

    EXPECT_EQ(reg.all().size(), 3u);
    EXPECT_EQ(reg.get("nordic").id, "nordic");
    EXPECT_TRUE(reg.get("desert").allow_repeated_seam_phoneme);
}

TEST(NameRegistryTest, GetUnknownIdThrows) {
    NameRegistry reg;
    reg.load(real_cultures_dir());
    EXPECT_THROW(reg.get("atlantean"), std::out_of_range);
}

TEST(NameRegistryTest, PickCultureIsDeterministicForSameEntropy) {
    NameRegistry reg;
    reg.load(real_cultures_dir());

    const std::uint64_t entropy = 123456789ULL;
    const std::string first = reg.pick_culture(entropy).id;
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(reg.pick_culture(entropy).id, first);
    }
}

TEST(NameRegistryTest, PickCultureVariesAcrossDifferentEntropy) {
    NameRegistry reg;
    reg.load(real_cultures_dir());

    std::set<std::string> picked;
    for (std::uint64_t e = 0; e < 20; ++e) {
        picked.insert(reg.pick_culture(e).id);
    }
    EXPECT_GT(picked.size(), 1u);
}

TEST(NameRegistryTest, PickCultureWithNoCulturesLoadedThrows) {
    NameRegistry reg;
    EXPECT_THROW(reg.pick_culture(42), std::runtime_error);
}

TEST(NameRegistryTest, LoadMissingDirectoryThrows) {
    NameRegistry reg;
    EXPECT_THROW(reg.load("data/names/cultures/does-not-exist"), std::runtime_error);
}

TEST(NameRegistryTest, LoadValidFixtureSucceeds) {
    TempCultureDir tmp;
    tmp.write("test.json", kValidCulture);

    NameRegistry reg;
    reg.load(tmp.path());

    ASSERT_TRUE(reg.has("testculture"));
    EXPECT_EQ(reg.get("testculture").consonants.size(), 2u);
}

TEST(NameRegistryTest, LoadRejectsFileMissingRequiredField) {
    TempCultureDir tmp;
    // Missing "vowels" entirely.
    tmp.write("bad.json", R"({
      "id": "broken",
      "consonants": ["k"],
      "syllable_templates": ["CV"],
      "allow_repeated_seam_phoneme": false,
      "continent_suffixes": [], "country_suffixes": [], "city_suffixes": [],
      "river_suffixes": [], "lake_suffixes": [], "mountain_suffixes": [],
      "street_adjectives": [], "street_nouns": [], "street_suffixes": [],
      "continent_syllables": 2, "country_syllables": 2, "city_syllables": 2,
      "river_syllables": 1, "lake_syllables": 1, "mountain_syllables": 1
    })");

    NameRegistry reg;
    try {
        reg.load(tmp.path());
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("vowels"), std::string::npos);
    }
}

TEST(NameRegistryTest, LoadRejectsEmptyRequiredPool) {
    TempCultureDir tmp;
    // "vowels" present but empty -- can never produce a syllable.
    tmp.write("bad.json", R"({
      "id": "broken",
      "consonants": ["k"],
      "vowels": [],
      "syllable_templates": ["CV"],
      "allow_repeated_seam_phoneme": false,
      "continent_suffixes": [], "country_suffixes": [], "city_suffixes": [],
      "river_suffixes": [], "lake_suffixes": [], "mountain_suffixes": [],
      "street_adjectives": [], "street_nouns": [], "street_suffixes": [],
      "continent_syllables": 2, "country_syllables": 2, "city_syllables": 2,
      "river_syllables": 1, "lake_syllables": 1, "mountain_syllables": 1
    })");

    NameRegistry reg;
    EXPECT_THROW(reg.load(tmp.path()), std::runtime_error);
}

TEST(NameRegistryTest, LoadRejectsMalformedJson) {
    TempCultureDir tmp;
    tmp.write("bad.json", "{ this is not valid json");

    NameRegistry reg;
    EXPECT_THROW(reg.load(tmp.path()), std::runtime_error);
}

TEST(NameRegistryTest, LoadIsIdempotentAndReplacesPreviousState) {
    TempCultureDir tmp;
    tmp.write("test.json", kValidCulture);

    NameRegistry reg;
    reg.load(real_cultures_dir());
    ASSERT_EQ(reg.all().size(), 3u);

    reg.load(tmp.path());
    EXPECT_EQ(reg.all().size(), 1u);
    EXPECT_FALSE(reg.has("nordic"));
    EXPECT_TRUE(reg.has("testculture"));
}
