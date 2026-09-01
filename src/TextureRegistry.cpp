// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "TextureRegistry.hpp"
#include <stdexcept>

namespace MeshWorld {

void TextureRegistry::register_texture(TextureEntry entry) {
    entries_[entry.id] = std::move(entry);
}

const TextureEntry& TextureRegistry::get(const std::string& id) const {
    auto it = entries_.find(id);
    if (it == entries_.end())
        throw std::out_of_range("TextureRegistry: unknown id '" + id + "'");
    return it->second;
}

bool TextureRegistry::has(const std::string& id) const {
    return entries_.count(id) > 0;
}

std::vector<TextureEntry> TextureRegistry::all() const {
    std::vector<TextureEntry> result;
    result.reserve(entries_.size());
    for (const auto& [id, e] : entries_)
        result.push_back(e);
    return result;
}

TextureRegistry& TextureRegistry::instance() {
    static TextureRegistry reg;
    return reg;
}

} // namespace MeshWorld
