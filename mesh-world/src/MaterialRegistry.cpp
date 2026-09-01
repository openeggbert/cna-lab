// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "MaterialRegistry.hpp"
#include <stdexcept>

namespace MeshWorld {

void MaterialRegistry::register_material(MaterialEntry entry) {
    entries_[entry.id] = std::move(entry);
}

const MaterialEntry& MaterialRegistry::get(const std::string& id) const {
    auto it = entries_.find(id);
    if (it == entries_.end())
        throw std::out_of_range("MaterialRegistry: unknown id '" + id + "'");
    return it->second;
}

bool MaterialRegistry::has(const std::string& id) const {
    return entries_.count(id) > 0;
}

std::vector<MaterialEntry> MaterialRegistry::all() const {
    std::vector<MaterialEntry> result;
    result.reserve(entries_.size());
    for (const auto& [id, e] : entries_)
        result.push_back(e);
    return result;
}

MaterialRegistry& MaterialRegistry::instance() {
    static MaterialRegistry reg;
    return reg;
}

} // namespace MeshWorld
