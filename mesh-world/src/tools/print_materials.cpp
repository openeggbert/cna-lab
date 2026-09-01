// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldMaterials — prints all registered materials with license info.

#include "BuiltinMaterials.hpp"
#include "MaterialRegistry.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>

int main() {
    MeshWorld::register_builtin_materials();

    auto materials = MeshWorld::MaterialRegistry::instance().all();
    std::sort(materials.begin(), materials.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });

    std::printf("%-35s  %-8s %-8s %-8s  %-6s %-6s  %-12s  %s\n",
                "ID", "R", "G", "B", "Rough", "Metal", "License", "Author");
    std::printf("%s\n", std::string(110, '-').c_str());

    for (const auto& m : materials) {
        std::printf("%-35s  %-8.3f %-8.3f %-8.3f  %-6.2f %-6.2f  %-12s  %s\n",
                    m.id.c_str(),
                    m.r, m.g, m.b,
                    m.roughness, m.metallic,
                    m.license.spdx_license.empty() ? "-" : m.license.spdx_license.c_str(),
                    m.license.author.empty()        ? "-" : m.license.author.c_str());
    }

    std::printf("\nTotal: %zu materials\n", materials.size());
    return 0;
}
