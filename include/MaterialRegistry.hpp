// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "MaterialEntry.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace MeshWorld {

class MaterialRegistry {
public:
    void register_material(MaterialEntry entry);
    const MaterialEntry& get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<MaterialEntry> all() const;

    static MaterialRegistry& instance();

private:
    std::unordered_map<std::string, MaterialEntry> entries_;
};

} // namespace MeshWorld
