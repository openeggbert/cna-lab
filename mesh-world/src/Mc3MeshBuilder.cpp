// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Mc3MeshBuilder.hpp"
#include "MaterialRegistry.hpp"
#include <tinyxml2.h>
#include <cmath>
#include <cstring>
#include <numbers>
#include <sstream>

namespace MeshWorld {

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

void Mc3MeshBuilder::resolve_color(RenderPrimitive& prim) {
    auto& reg = MaterialRegistry::instance();
    if (reg.has(prim.material_id)) {
        const auto& e = reg.get(prim.material_id);
        prim.r = e.r;
        prim.g = e.g;
        prim.b = e.b;
    }
}

// ---------------------------------------------------------------------------
// Box tessellation — 6 faces, each 2 triangles, unique vertices per face.
// ---------------------------------------------------------------------------

RenderPrimitive Mc3MeshBuilder::make_box(const std::string& id, const std::string& mat,
                                          float px, float py, float pz,
                                          float sx, float sy, float sz) const {
    RenderPrimitive p;
    p.object_id   = id;
    p.material_id = mat;

    const float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;

    // face[i] = {4 positions, normal}
    struct Face {
        std::array<std::array<float,3>, 4> v;
        std::array<float,3> n;
    };

    const Face faces[6] = {
        // +Y top
        { {{ {-hx, hy,-hz}, { hx, hy,-hz}, { hx, hy, hz}, {-hx, hy, hz} }}, {0, 1, 0} },
        // -Y bottom
        { {{ {-hx,-hy, hz}, { hx,-hy, hz}, { hx,-hy,-hz}, {-hx,-hy,-hz} }}, {0,-1, 0} },
        // +Z front
        { {{ {-hx,-hy, hz}, { hx,-hy, hz}, { hx, hy, hz}, {-hx, hy, hz} }}, {0, 0, 1} },
        // -Z back
        { {{ { hx,-hy,-hz}, {-hx,-hy,-hz}, {-hx, hy,-hz}, { hx, hy,-hz} }}, {0, 0,-1} },
        // +X right
        { {{ { hx,-hy, hz}, { hx,-hy,-hz}, { hx, hy,-hz}, { hx, hy, hz} }}, {1, 0, 0} },
        // -X left
        { {{ {-hx,-hy,-hz}, {-hx,-hy, hz}, {-hx, hy, hz}, {-hx, hy,-hz} }}, {-1,0, 0} },
    };

    for (const auto& face : faces) {
        uint32_t base = static_cast<uint32_t>(p.vertices.size());
        for (const auto& vp : face.v) {
            Vertex v;
            v.x  = px + vp[0]; v.y  = py + vp[1]; v.z  = pz + vp[2];
            v.nx = face.n[0];  v.ny = face.n[1];  v.nz = face.n[2];
            p.vertices.push_back(v);
        }
        // two triangles per face
        p.indices.insert(p.indices.end(), {base,base+1,base+2, base,base+2,base+3});
    }

    resolve_color(p);
    return p;
}

// ---------------------------------------------------------------------------
// Plane tessellation — horizontal quad (XZ plane), normal +Y.
// ---------------------------------------------------------------------------

RenderPrimitive Mc3MeshBuilder::make_plane(const std::string& id, const std::string& mat,
                                            float px, float py, float pz,
                                            float sx, float sz) const {
    RenderPrimitive p;
    p.object_id   = id;
    p.material_id = mat;

    const float hx = sx * 0.5f, hz = sz * 0.5f;
    const float y  = py;

    p.vertices = {
        {px - hx, y, pz - hz,  0, 1, 0},
        {px + hx, y, pz - hz,  0, 1, 0},
        {px + hx, y, pz + hz,  0, 1, 0},
        {px - hx, y, pz + hz,  0, 1, 0},
    };
    p.indices = {0, 1, 2,  0, 2, 3};

    resolve_color(p);
    return p;
}

// ---------------------------------------------------------------------------
// Cylinder tessellation — N-segment sides + top + bottom caps.
// ---------------------------------------------------------------------------

RenderPrimitive Mc3MeshBuilder::make_cylinder(const std::string& id, const std::string& mat,
                                               float px, float py, float pz,
                                               float radius, float height) const {
    RenderPrimitive p;
    p.object_id   = id;
    p.material_id = mat;

    const int N    = cylinder_segments;
    const float hy = height * 0.5f;
    const float pi = static_cast<float>(std::numbers::pi);

    // Side: two vertices per segment angle (bottom + top), quads → 2 triangles each
    for (int i = 0; i < N; ++i) {
        float a0 = 2.0f * pi * static_cast<float>(i)   / static_cast<float>(N);
        float a1 = 2.0f * pi * static_cast<float>(i+1) / static_cast<float>(N);
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);

        // Average normal for the two vertices (flat shading approximation)
        float nc = std::cos((a0 + a1) * 0.5f);
        float ns = std::sin((a0 + a1) * 0.5f);

        auto base = static_cast<uint32_t>(p.vertices.size());
        // bottom-left, bottom-right, top-right, top-left
        p.vertices.push_back({px + radius*c0, py - hy, pz + radius*s0,  c0, 0, s0});
        p.vertices.push_back({px + radius*c1, py - hy, pz + radius*s1,  c1, 0, s1});
        p.vertices.push_back({px + radius*c1, py + hy, pz + radius*s1,  c1, 0, s1});
        p.vertices.push_back({px + radius*c0, py + hy, pz + radius*s0,  c0, 0, s0});
        p.indices.insert(p.indices.end(), {base,base+1,base+2, base,base+2,base+3});
        (void)nc; (void)ns;
    }

    // Top cap (fan)
    {
        auto centre_idx = static_cast<uint32_t>(p.vertices.size());
        p.vertices.push_back({px, py + hy, pz,  0, 1, 0});
        auto ring_start = static_cast<uint32_t>(p.vertices.size());
        for (int i = 0; i < N; ++i) {
            float a = 2.0f * pi * static_cast<float>(i) / static_cast<float>(N);
            p.vertices.push_back({px + radius*std::cos(a), py + hy, pz + radius*std::sin(a),  0, 1, 0});
        }
        for (int i = 0; i < N; ++i)
            p.indices.insert(p.indices.end(), {centre_idx, ring_start + static_cast<uint32_t>(i),
                                                ring_start + static_cast<uint32_t>((i+1) % N)});
    }

    // Bottom cap (fan, reversed winding)
    {
        auto centre_idx = static_cast<uint32_t>(p.vertices.size());
        p.vertices.push_back({px, py - hy, pz,  0,-1, 0});
        auto ring_start = static_cast<uint32_t>(p.vertices.size());
        for (int i = 0; i < N; ++i) {
            float a = 2.0f * pi * static_cast<float>(i) / static_cast<float>(N);
            p.vertices.push_back({px + radius*std::cos(a), py - hy, pz + radius*std::sin(a),  0,-1, 0});
        }
        for (int i = 0; i < N; ++i)
            p.indices.insert(p.indices.end(), {centre_idx, ring_start + static_cast<uint32_t>((i+1) % N),
                                                ring_start + static_cast<uint32_t>(i)});
    }

    resolve_color(p);
    return p;
}

// ---------------------------------------------------------------------------
// Parse MC3 XML and build MeshList
// ---------------------------------------------------------------------------

static bool parse_float3(const char* str, float& a, float& b, float& c) {
    if (!str) return false;
    return std::sscanf(str, "%f %f %f", &a, &b, &c) == 3;
}

static bool parse_float2(const char* str, float& a, float& b) {
    if (!str) return false;
    return std::sscanf(str, "%f %f", &a, &b) == 2;
}

static float attr_f(const tinyxml2::XMLElement* el, const char* name, float def = 0.0f) {
    const char* v = el->Attribute(name);
    if (!v) return def;
    return std::stof(v);
}

static const char* attr_s(const tinyxml2::XMLElement* el, const char* name) {
    const char* v = el->Attribute(name);
    return v ? v : "";
}

MeshList Mc3MeshBuilder::build(const std::string& xml) const {
    MeshList list;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return list;

    const auto* mc3 = doc.FirstChildElement("mc3");
    if (!mc3) return list;
    const auto* objects = mc3->FirstChildElement("objects");
    if (!objects) return list;

    for (const auto* el = objects->FirstChildElement(); el; el = el->NextSiblingElement()) {
        const char* tag = el->Name();
        const char* id  = attr_s(el, "id");
        const char* mat = attr_s(el, "material");
        std::string sid(id);
        std::string smat(mat);

        if (std::strcmp(tag, "box") == 0) {
            float px{}, py{}, pz{};
            parse_float3(el->Attribute("position"), px, py, pz);
            float sx{1}, sy{1}, sz{1};
            const char* sz_str = el->Attribute("size");
            if (sz_str) {
                // size can be "sx sy sz"
                if (std::sscanf(sz_str, "%f %f %f", &sx, &sy, &sz) < 3) {
                    // two-value fallback
                    std::sscanf(sz_str, "%f %f", &sx, &sz);
                    sy = 1.0f;
                }
            }
            list.primitives.push_back(make_box(sid, smat, px, py, pz, sx, sy, sz));

        } else if (std::strcmp(tag, "plane") == 0) {
            float px{}, py{}, pz{};
            parse_float3(el->Attribute("position"), px, py, pz);
            float sx{1}, sz_val{1};
            parse_float2(el->Attribute("size"), sx, sz_val);
            list.primitives.push_back(make_plane(sid, smat, px, py, pz, sx, sz_val));

        } else if (std::strcmp(tag, "cylinder") == 0) {
            float px{}, py{}, pz{};
            parse_float3(el->Attribute("position"), px, py, pz);
            float radius = attr_f(el, "radius", 0.5f);
            float height = attr_f(el, "height", 1.0f);
            // Cylinders in MC3 have their base at py; centre is at py + height/2
            list.primitives.push_back(
                make_cylinder(sid, smat, px, py + height * 0.5f, pz, radius, height));

        }
        // <instance> and <metadata> are skipped — no geometry to tessellate
    }

    return list;
}

} // namespace MeshWorld
