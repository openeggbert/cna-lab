// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP5 tests. M073: data/names/cultures/*.json — 3-6 starter cultures.
//
// No NameRegistry exists yet to load these for real (that's M074/M075) --
// this is a lightweight sanity check catching a malformed/incomplete file
// before that loader ever tries to read one, by parsing directly with
// nlohmann::json (already a project dependency, see MapPayloadCodec.cpp)
// and confirming every NameCulture field name appears with the right JSON
// type.

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// A field-type check: `is_number_integer()` (not a plain value_t compare)
// since a positive JSON integer literal ("2") parses as `number_unsigned`
// by default, distinct from `number_integer` in nlohmann::json's own
// value_t enum -- both count as "an integer" for NameCulture's purposes.
enum class FieldKind { String, Array, Bool, Integer };

bool matches(const json& value, FieldKind kind) {
    switch (kind) {
        case FieldKind::String:  return value.is_string();
        case FieldKind::Array:   return value.is_array();
        case FieldKind::Bool:    return value.is_boolean();
        case FieldKind::Integer: return value.is_number_integer();
    }
    return false;
}

// Every NameCulture (include/NameCulture.hpp) field, and the JSON kind a
// culture file's own value must have.
const std::vector<std::pair<std::string, FieldKind>>& expected_fields() {
    static const std::vector<std::pair<std::string, FieldKind>> fields{
        {"id", FieldKind::String},
        {"consonants", FieldKind::Array},
        {"vowels", FieldKind::Array},
        {"syllable_templates", FieldKind::Array},
        {"allow_repeated_seam_phoneme", FieldKind::Bool},
        {"continent_suffixes", FieldKind::Array},
        {"country_suffixes", FieldKind::Array},
        {"city_suffixes", FieldKind::Array},
        {"river_suffixes", FieldKind::Array},
        {"lake_suffixes", FieldKind::Array},
        {"mountain_suffixes", FieldKind::Array},
        {"street_adjectives", FieldKind::Array},
        {"street_nouns", FieldKind::Array},
        {"street_suffixes", FieldKind::Array},
        {"continent_syllables", FieldKind::Integer},
        {"country_syllables", FieldKind::Integer},
        {"city_syllables", FieldKind::Integer},
        {"river_syllables", FieldKind::Integer},
        {"lake_syllables", FieldKind::Integer},
        {"mountain_syllables", FieldKind::Integer},
    };
    return fields;
}

void check_culture_file(const std::string& path, const std::string& expected_id) {
    const std::string text = read_file(path);
    ASSERT_FALSE(text.empty()) << path << " not found or empty";

    json j;
    ASSERT_NO_THROW(j = json::parse(text)) << path << " is not valid JSON";

    for (const auto& [field, kind] : expected_fields()) {
        ASSERT_TRUE(j.contains(field)) << path << " missing field '" << field << "'";
        EXPECT_TRUE(matches(j.at(field), kind))
            << path << " field '" << field << "' has the wrong JSON type";
    }

    EXPECT_EQ(j.at("id").get<std::string>(), expected_id);

    // Every phoneme/morpheme pool must have at least one entry -- an empty
    // consonants/vowels list can never generate a syllable at all.
    for (const char* pool : {"consonants", "vowels", "syllable_templates"}) {
        EXPECT_GT(j.at(pool).size(), 0u) << path << " '" << pool << "' must not be empty";
    }
}

} // namespace

TEST(NameCultureCulturesJsonTest, NordicFileHasEveryNameCultureField) {
    check_culture_file("data/names/cultures/nordic.json", "nordic");
}

TEST(NameCultureCulturesJsonTest, RomanceFileHasEveryNameCultureField) {
    check_culture_file("data/names/cultures/romance.json", "romance");
}

TEST(NameCultureCulturesJsonTest, DesertFileHasEveryNameCultureField) {
    check_culture_file("data/names/cultures/desert.json", "desert");
}

// M073's own plan.md wording asks for "3-6 starter cultures" -- prove at
// least 3 real files exist and each has a distinct id (no accidental
// copy-paste leaving two files claiming the same culture).
TEST(NameCultureCulturesJsonTest, AtLeastThreeDistinctCulturesExist) {
    const std::vector<std::string> paths{
        "data/names/cultures/nordic.json",
        "data/names/cultures/romance.json",
        "data/names/cultures/desert.json",
    };
    std::vector<std::string> ids;
    for (const auto& p : paths) {
        const std::string text = read_file(p);
        ASSERT_FALSE(text.empty()) << p << " not found or empty";
        ids.push_back(json::parse(text).at("id").get<std::string>());
    }
    EXPECT_EQ(ids[0], "nordic");
    EXPECT_EQ(ids[1], "romance");
    EXPECT_EQ(ids[2], "desert");
    EXPECT_NE(ids[0], ids[1]);
    EXPECT_NE(ids[1], ids[2]);
    EXPECT_NE(ids[0], ids[2]);
}
