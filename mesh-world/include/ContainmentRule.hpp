// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include <string>

namespace MeshWorld {

struct ContainmentRule {
    std::string parent;
    std::string child;
    float       probability  = 1.0f;
    int         min_count    = 0;
    int         max_count    = 1;
    int         lod_max      = 0;
};

} // namespace MeshWorld
