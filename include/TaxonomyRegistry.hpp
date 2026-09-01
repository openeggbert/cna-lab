// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "TaxonomyNode.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

namespace MeshWorld {

class TaxonomyRegistry {
public:
    void load(const std::filesystem::path& json_path);
    void load_node(TaxonomyNode node);

    const TaxonomyNode& get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<TaxonomyNode> all() const;
    std::vector<TaxonomyNode> by_kind(const std::string& kind) const;

    static TaxonomyRegistry& instance();

private:
    std::unordered_map<std::string, TaxonomyNode> nodes_;
};

} // namespace MeshWorld
