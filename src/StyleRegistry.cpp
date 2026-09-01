// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "StyleRegistry.hpp"
#include <stdexcept>
#include <vector>

namespace MeshWorld {

StyleRegistry& StyleRegistry::instance() {
    static StyleRegistry inst;
    return inst;
}

void StyleRegistry::register_style(Style s) {
    styles_[s.id] = std::move(s);
}

const Style& StyleRegistry::get(const std::string& id) const {
    auto it = styles_.find(id);
    if (it == styles_.end())
        throw std::runtime_error("StyleRegistry: unknown style '" + id + "'");
    return it->second;
}

bool StyleRegistry::has(const std::string& id) const {
    return styles_.count(id) > 0;
}

std::vector<Style> StyleRegistry::all() const {
    std::vector<Style> result;
    result.reserve(styles_.size());
    for (const auto& [_, s] : styles_)
        result.push_back(s);
    return result;
}

} // namespace MeshWorld
