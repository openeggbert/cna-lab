// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace MeshWorld {

class Mc3DocumentBuilder;
struct GenerationMetadata;

// Safe C++ scene builder API.
// Wraps Mc3DocumentBuilder with camelCase names, adds material registration
// and GenerationMetadata support.  This is the API that Lua generators bind to
// via sol2 (see LuaRuntime / M5).
//
// Coordinate conventions (inherited from Mc3DocumentBuilder):
//   addPlane    — x,z = min corner;  sx,sz = full width/depth
//   addBox      — x,z = center;      sx,sy,sz = full extents; y = base elevation
//   addCylinder — x,z = center;      y = base elevation
//   addSphere   — x,z = center;      y = base elevation (center at y + radius)
//   addCone     — x,z = center;      y = base elevation
//   addIcoSphere— x,z = center;      y = base elevation (center at y + radius)
//   addInstance — x,z = center;      y = base elevation; ry = Y-rotation (degrees)
class Mc3SceneBuilder {
public:
    Mc3SceneBuilder(const std::string& model,
                    float chunk_size_m,
                    int coord_x, int coord_y);
    ~Mc3SceneBuilder();

    // --- primitives ---
    void addGround(const std::string& material, float y = 0.0f);

    // rx/rz (G12): tilt/roll in degrees. Unlike ry, rx/rz are NOT composed
    // through pushTransform()/popTransform()'s cumulative frame stack (that
    // stack only ever tracked a single ry_deg, since it was the only axis
    // ever used) -- they're applied as-is on top of whatever position/scale
    // the current frame contributes. Correct for today's actual use (a leaf
    // primitive tilted directly, e.g. a sloped roof panel or a sideways
    // wheel), not a general per-axis-independent 3D rotation composition
    // system -- a generator that both pushes a rotated frame AND sets rx/rz
    // on a primitive inside it should not assume the two compose correctly.
    void addPlane(const std::string& id, float x, float z, float sx, float sz,
                  const std::string& material, float y = 0.0f, float ry = 0.0f,
                  float rx = 0.0f, float rz = 0.0f);

    void addBox(const std::string& id, float x, float z, float sx, float sy, float sz,
                const std::string& material, float y = 0.0f, float ry = 0.0f,
                float rx = 0.0f, float rz = 0.0f);

    void addCylinder(const std::string& id, float x, float z,
                     float radius, float height,
                     const std::string& material, float y = 0.0f,
                     float ry = 0.0f, float rx = 0.0f, float rz = 0.0f);

    // addSphere — x,z = center; y = base elevation (center at y + radius)
    void addSphere(const std::string& id, float x, float z,
                   float radius, const std::string& material, float y = 0.0f);

    // addCone   — x,z = center; y = base elevation
    void addCone(const std::string& id, float x, float z,
                 float radius, float height,
                 const std::string& material, float y = 0.0f);

    // addIcoSphere — x,z = center; y = base elevation (center at y + radius).
    // M330 -- a geodesic sphere, distinct from addSphere()'s UV sphere;
    // gives Lua object generators the same canopy-shape vocabulary
    // ObjectDefinitionLibrary.cpp's C++ tree definitions already use. sx/sy/sz
    // (default 1,1,1) apply a non-uniform scale for ellipsoid deforms.
    void addIcoSphere(const std::string& id, float x, float z, float radius,
                      const std::string& material, float y = 0.0f,
                      float sx = 1.0f, float sy = 1.0f, float sz = 1.0f);

    void addInstance(const std::string& id, const std::string& definition,
                     float x, float z,
                     float ry = 0.0f, float y = 0.0f, float scale = 1.0f,
                     float rx = 0.0f, float rz = 0.0f);

    // --- materials ---
    // Register a PBR material by ID.  Referenced by name in primitives.
    void addMaterial(const std::string& id,
                     float r, float g, float b, float a = 1.0f,
                     float roughness = 0.8f, float metallic = 0.0f);

    // --- metadata ---
    void setMetadata(const GenerationMetadata& meta);
    void setMetadataJson(const std::string& json);

    // --- composition (scene:callGenerator, LuaRuntime.cpp) ---
    // Pushes a new cumulative frame onto the transform/id stack, composing
    // (x,y,z,ry_deg,scale) as a LOCAL offset relative to the CURRENT top
    // frame -- standard scenegraph modelview-stack semantics. Every add*()
    // call below transforms its own local position/rotation/size by the
    // current top frame and prefixes its id with the current cumulative
    // id_prefix before delegating to Mc3DocumentBuilder, so a sub-generator
    // invoked between push/pop lands correctly placed in the SAME document
    // with collision-free ids, with no changes needed to Mc3DocumentBuilder
    // itself. Always pop what you push (RAII is the caller's job -- see
    // LuaRuntime.cpp's callGenerator binding, which pops even on error).
    void pushTransform(float x, float y, float z, float ry_deg, float scale,
                        const std::string& id_prefix);
    void popTransform();

    // --- serialization ---
    std::string buildToString() const;
    void buildToFile(const std::filesystem::path& path) const;

private:
    std::unique_ptr<Mc3DocumentBuilder> builder_;

    struct Frame {
        float x = 0.0f, y = 0.0f, z = 0.0f; // cumulative translation
        float ry_deg = 0.0f;                // cumulative Y rotation, degrees
        float scale  = 1.0f;                // cumulative uniform scale
        std::string id_prefix;              // cumulative id prefix
    };
    // Always has >=1 entry (the identity base frame at index 0); never popped.
    std::vector<Frame> stack_{Frame{}};

    const Frame& top() const { return stack_.back(); }
    void transform_point(float x, float y, float z, float& ox, float& oy, float& oz) const;
    float transform_ry(float ry) const { return ry + top().ry_deg; }
    // G12 -- rx/rz have no cumulative frame component to compose with
    // (Frame only ever tracked ry_deg), so these are deliberately identity
    // pass-throughs; see this class's own addPlane/addBox/... doc comment
    // above for what that does and doesn't guarantee under pushTransform().
    float transform_rx(float rx) const { return rx; }
    float transform_rz(float rz) const { return rz; }
    float transform_scalar(float v) const { return v * top().scale; }
    std::string transform_id(const std::string& id) const { return top().id_prefix + id; }
};

} // namespace MeshWorld
