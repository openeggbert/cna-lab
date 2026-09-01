// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MeshWorld {

struct Vertex {
    float x{0}, y{0}, z{0};
    float nx{0}, ny{0}, nz{0};
};

// Triangulated geometry for one MC3 object.
struct RenderPrimitive {
    std::string      object_id;
    std::string      material_id;
    float            r{0.8f}, g{0.8f}, b{0.8f};
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    int triangle_count() const { return static_cast<int>(indices.size()) / 3; }
};

// CPU-side tessellated geometry for a whole chunk.
struct MeshList {
    std::vector<RenderPrimitive> primitives;

    bool empty()            const { return primitives.empty(); }
    int  primitive_count()  const { return static_cast<int>(primitives.size()); }
    int  total_triangles()  const {
        int n = 0;
        for (const auto& p : primitives) n += p.triangle_count();
        return n;
    }
};

// Converts an MC3 XML string to a MeshList of tessellated geometry.
// Resolves material colours from MaterialRegistry (if registered).
// box/plane/cylinder are tessellated; <instance> objects are skipped.
class Mc3MeshBuilder {
public:
    // Cylinder tessellation segments (default: 16-sided).
    int cylinder_segments{16};

    // Build geometry from MC3 XML.  Returns empty MeshList on parse error.
    MeshList build(const std::string& xml) const;

private:
    RenderPrimitive make_box(const std::string& id, const std::string& mat,
                             float px, float py, float pz,
                             float sx, float sy, float sz) const;

    RenderPrimitive make_plane(const std::string& id, const std::string& mat,
                               float px, float py, float pz,
                               float sx, float sz) const;

    RenderPrimitive make_cylinder(const std::string& id, const std::string& mat,
                                  float px, float py, float pz,
                                  float radius, float height) const;

    static void resolve_color(RenderPrimitive& prim);
};

} // namespace MeshWorld
