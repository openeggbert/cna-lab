// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Style.hpp"

namespace MeshWorld {

class StyleRegistry {
public:
    void register_style(Style s);

    const Style& get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<Style> all() const;

    static StyleRegistry& instance();

private:
    std::unordered_map<std::string, Style> styles_;
};

} // namespace MeshWorld
