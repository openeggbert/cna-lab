// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "ContainmentRule.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

namespace MeshWorld {

class ContainmentRuleRegistry {
public:
    void load(const std::filesystem::path& json_path);
    void load_rule(ContainmentRule rule);

    std::vector<ContainmentRule> children_of(const std::string& parent_id) const;
    bool can_contain(const std::string& parent_id, const std::string& child_id) const;
    std::vector<ContainmentRule> children_at_lod(const std::string& parent_id, int lod) const;

    static ContainmentRuleRegistry& instance();

private:
    std::unordered_map<std::string, std::vector<ContainmentRule>> rules_;
};

} // namespace MeshWorld
