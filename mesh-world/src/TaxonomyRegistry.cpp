// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "TaxonomyRegistry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace MeshWorld {

void TaxonomyRegistry::load(const std::filesystem::path& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open())
        throw std::runtime_error("TaxonomyRegistry: cannot open " + json_path.string());

    auto j = nlohmann::json::parse(f);
    for (const auto& item : j) {
        TaxonomyNode node;
        node.id   = item.at("id").get<std::string>();
        node.kind = item.at("kind").get<std::string>();
        node.name = item.at("name").get<std::string>();
        nodes_[node.id] = std::move(node);
    }
}

void TaxonomyRegistry::load_node(TaxonomyNode node) {
    nodes_[node.id] = std::move(node);
}

const TaxonomyNode& TaxonomyRegistry::get(const std::string& id) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end())
        throw std::out_of_range("TaxonomyRegistry: unknown id '" + id + "'");
    return it->second;
}

bool TaxonomyRegistry::has(const std::string& id) const {
    return nodes_.count(id) > 0;
}

std::vector<TaxonomyNode> TaxonomyRegistry::all() const {
    std::vector<TaxonomyNode> result;
    result.reserve(nodes_.size());
    for (const auto& [id, node] : nodes_)
        result.push_back(node);
    return result;
}

std::vector<TaxonomyNode> TaxonomyRegistry::by_kind(const std::string& kind) const {
    std::vector<TaxonomyNode> result;
    for (const auto& [id, node] : nodes_)
        if (node.kind == kind)
            result.push_back(node);
    return result;
}

TaxonomyRegistry& TaxonomyRegistry::instance() {
    static TaxonomyRegistry reg;
    return reg;
}

} // namespace MeshWorld
