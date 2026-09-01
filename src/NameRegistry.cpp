// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "NameRegistry.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
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

template <typename T>
T require(const nlohmann::json& j, const char* field, const std::string& path) {
    if (!j.contains(field)) {
        throw std::runtime_error("NameRegistry: " + path + " missing field '" + field + "'");
    }
    return j.at(field).get<T>();
}

std::vector<std::string> require_pool(const nlohmann::json& j, const char* field,
                                       const std::string& path) {
    auto pool = require<std::vector<std::string>>(j, field, path);
    if (pool.empty()) {
        throw std::runtime_error("NameRegistry: " + path + " field '" + field + "' must not be empty");
    }
    return pool;
}

NameCulture parse_culture(const nlohmann::json& j, const std::string& path) {
    NameCulture c;
    c.id = require<std::string>(j, "id", path);

    c.consonants         = require_pool(j, "consonants", path);
    c.vowels              = require_pool(j, "vowels", path);
    c.syllable_templates  = require_pool(j, "syllable_templates", path);

    c.allow_repeated_seam_phoneme = require<bool>(j, "allow_repeated_seam_phoneme", path);

    c.continent_suffixes = require<std::vector<std::string>>(j, "continent_suffixes", path);
    c.country_suffixes   = require<std::vector<std::string>>(j, "country_suffixes", path);
    c.city_suffixes      = require<std::vector<std::string>>(j, "city_suffixes", path);
    c.river_suffixes     = require<std::vector<std::string>>(j, "river_suffixes", path);
    c.lake_suffixes      = require<std::vector<std::string>>(j, "lake_suffixes", path);
    c.mountain_suffixes  = require<std::vector<std::string>>(j, "mountain_suffixes", path);

    c.street_adjectives = require<std::vector<std::string>>(j, "street_adjectives", path);
    c.street_nouns      = require<std::vector<std::string>>(j, "street_nouns", path);
    c.street_suffixes   = require<std::vector<std::string>>(j, "street_suffixes", path);

    c.continent_syllables = require<int>(j, "continent_syllables", path);
    c.country_syllables   = require<int>(j, "country_syllables", path);
    c.city_syllables       = require<int>(j, "city_syllables", path);
    c.river_syllables      = require<int>(j, "river_syllables", path);
    c.lake_syllables       = require<int>(j, "lake_syllables", path);
    c.mountain_syllables   = require<int>(j, "mountain_syllables", path);

    return c;
}

} // namespace

void NameRegistry::load(const std::filesystem::path& dir) {
    if (!std::filesystem::is_directory(dir)) {
        throw std::runtime_error("NameRegistry: not a directory: " + dir.string());
    }

    std::vector<NameCulture> loaded;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;

        const std::string path = entry.path().string();
        std::ifstream f(entry.path());
        if (!f.is_open()) {
            throw std::runtime_error("NameRegistry: cannot open " + path);
        }

        std::ostringstream ss;
        ss << f.rdbuf();

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(ss.str());
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error("NameRegistry: " + path + " is not valid JSON: " + e.what());
        }

        loaded.push_back(parse_culture(j, path));
    }

    cultures_.clear();
    index_by_id_.clear();
    for (auto& c : loaded) {
        index_by_id_[c.id] = cultures_.size();
        cultures_.push_back(std::move(c));
    }
}

const NameCulture& NameRegistry::get(const std::string& id) const {
    auto it = index_by_id_.find(id);
    if (it == index_by_id_.end()) {
        throw std::out_of_range("NameRegistry: unknown culture id '" + id + "'");
    }
    return cultures_[it->second];
}

bool NameRegistry::has(const std::string& id) const {
    return index_by_id_.count(id) > 0;
}

std::vector<NameCulture> NameRegistry::all() const {
    return cultures_;
}

const NameCulture& NameRegistry::pick_culture(std::uint64_t entropy) const {
    if (cultures_.empty()) {
        throw std::runtime_error("NameRegistry: pick_culture() called with no cultures loaded");
    }
    const std::uint64_t h = splitmix64(entropy);
    return cultures_[h % cultures_.size()];
}

} // namespace MeshWorld
