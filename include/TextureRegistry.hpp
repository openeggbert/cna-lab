// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "TextureEntry.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace MeshWorld {

class TextureRegistry {
public:
    void register_texture(TextureEntry entry);
    const TextureEntry& get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<TextureEntry> all() const;

    static TextureRegistry& instance();

private:
    std::unordered_map<std::string, TextureEntry> entries_;
};

} // namespace MeshWorld
