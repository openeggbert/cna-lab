// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "ContainmentRuleRegistry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace MeshWorld {

void ContainmentRuleRegistry::load(const std::filesystem::path& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open())
        throw std::runtime_error("ContainmentRuleRegistry: cannot open " + json_path.string());

    auto j = nlohmann::json::parse(f);
    for (const auto& item : j) {
        ContainmentRule rule;
        rule.parent      = item.at("parent").get<std::string>();
        rule.child       = item.at("child").get<std::string>();
        rule.probability = item.value("probability", 1.0f);
        rule.min_count   = item.value("min_count",   0);
        rule.max_count   = item.value("max_count",   1);
        rule.lod_max     = item.value("lod_max",     0);
        rules_[rule.parent].push_back(std::move(rule));
    }
}

void ContainmentRuleRegistry::load_rule(ContainmentRule rule) {
    rules_[rule.parent].push_back(std::move(rule));
}

std::vector<ContainmentRule> ContainmentRuleRegistry::children_of(const std::string& parent_id) const {
    auto it = rules_.find(parent_id);
    if (it == rules_.end())
        return {};
    return it->second;
}

bool ContainmentRuleRegistry::can_contain(const std::string& parent_id, const std::string& child_id) const {
    auto it = rules_.find(parent_id);
    if (it == rules_.end())
        return false;
    for (const auto& rule : it->second)
        if (rule.child == child_id)
            return true;
    return false;
}

std::vector<ContainmentRule> ContainmentRuleRegistry::children_at_lod(const std::string& parent_id, int lod) const {
    auto it = rules_.find(parent_id);
    if (it == rules_.end())
        return {};
    std::vector<ContainmentRule> result;
    for (const auto& rule : it->second)
        if (rule.lod_max <= lod)
            result.push_back(rule);
    return result;
}

ContainmentRuleRegistry& ContainmentRuleRegistry::instance() {
    static ContainmentRuleRegistry reg;
    return reg;
}

} // namespace MeshWorld
