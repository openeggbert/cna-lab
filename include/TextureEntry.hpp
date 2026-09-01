// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "AssetLicenseInfo.hpp"
#include <string>

namespace MeshWorld {

struct TextureEntry {
    std::string      id;
    std::string      path;
    AssetLicenseInfo license;
};

} // namespace MeshWorld
