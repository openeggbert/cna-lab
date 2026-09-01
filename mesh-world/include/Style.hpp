// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <unordered_map>

namespace MeshWorld {

struct Style {
    std::string id;
    std::string name;
    std::unordered_map<std::string, std::string> palette;

    // Return the material ID for key, or fallback if key not in palette.
    const std::string& mat(const std::string& key, const std::string& fallback) const {
        auto it = palette.find(key);
        return it != palette.end() ? it->second : fallback;
    }
};

} // namespace MeshWorld
