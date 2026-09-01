// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "AssetLicenseInfo.hpp"
#include <string>

namespace MeshWorld {

struct MaterialEntry {
    std::string      id;
    float            r          = 0.8f;
    float            g          = 0.8f;
    float            b          = 0.8f;
    float            roughness  = 0.8f;
    float            metallic   = 0.0f;
    std::string      texture_uri;  // path relative to working dir, e.g. assets/textures/grass.png
    AssetLicenseInfo license;
};

} // namespace MeshWorld
