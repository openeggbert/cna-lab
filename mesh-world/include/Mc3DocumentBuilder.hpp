// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace MeshCraft::Mc3 { class Mc3Document; }

namespace MeshWorld {

// Builds a MeshCraft::Mc3::Mc3Document and serializes it via Mc3Document::saveToFile()
// (which internally uses Mc3XmlWriter).  Provides the same ergonomic API as the
// old string-building MC3Writer, but produces canonical MC3 XML output that
// MeshCraft tools can parse.
//
// Coordinate conventions (same as MC3Writer):
//   plane   — x,z = min corner;  sx,sz = full width/depth
//   box     — x,z = center;      sx,sy,sz = full extents; y = base elevation
//   cylinder— x,z = center;      y = base elevation
//   sphere  — x,z = center;      y = base elevation (center at y + radius)
//   cone    — x,z = center;      y = base elevation
//   instance— x,z = center;      y = base elevation; ry = Y-rotation in degrees
class Mc3DocumentBuilder {
public:
    Mc3DocumentBuilder(const std::string& model,
                       float chunk_size_m,
                       int coord_x, int coord_y);
    ~Mc3DocumentBuilder();

    void ground(const std::string& material, float y = 0.0f);

    // rx/rz (G12): tilt/roll in degrees, same Euler triple as MeshCraft's own
    // Mc3Transform.rotation -- previously always 0 (a MeshWorld-side binding
    // gap, not a format limitation), so e.g. a sloped roof panel could only
    // ever be built as a stepped-box approximation. No composition/ordering
    // guarantee beyond what Mc3Transform's own (rx, ry, rz) Euler triple
    // already provides.
    void plane(const std::string& id, float x, float z, float sx, float sz,
               const std::string& material, float y = 0.0f, float ry = 0.0f,
               float rx = 0.0f, float rz = 0.0f);

    void box(const std::string& id, float x, float z, float sx, float sy, float sz,
             const std::string& material, float y = 0.0f, float ry = 0.0f,
             float rx = 0.0f, float rz = 0.0f);

    void cylinder(const std::string& id, float x, float z,
                  float radius, float height,
                  const std::string& material, float y = 0.0f,
                  float ry = 0.0f, float rx = 0.0f, float rz = 0.0f);

    // sphere  — x,z = center; y = base elevation (sphere center sits radius above y).
    void sphere(const std::string& id, float x, float z,
                float radius, const std::string& material, float y = 0.0f);

    // cone    — x,z = center; y = base elevation (apex points up, like cylinder).
    void cone(const std::string& id, float x, float z,
              float radius, float height,
              const std::string& material, float y = 0.0f);

    // icoSphere — x,z = center; y = base elevation (center at y + radius).
    // M330 -- a geodesic (faceted) sphere, distinct from sphere()'s UV
    // sphere; matches ObjectDefinitionLibrary.cpp's own tree-canopy shape
    // vocabulary (Mc3Object::makeIcoSphere). sx/sy/sz (default 1,1,1) apply
    // a non-uniform scale for ellipsoid deforms (e.g. a tall narrow canopy).
    void icoSphere(const std::string& id, float x, float z, float radius,
                   const std::string& material, float y = 0.0f,
                   float sx = 1.0f, float sy = 1.0f, float sz = 1.0f);

    void instance(const std::string& id, const std::string& definition,
                  float x, float z,
                  float ry = 0.0f, float y = 0.0f, float scale = 1.0f,
                  float rx = 0.0f, float rz = 0.0f);

    // Register a PBR material in the document's materials map.
    void add_material(const std::string& id,
                      float r, float g, float b, float a = 1.0f,
                      float roughness = 0.8f, float metallic = 0.0f);

    // Set structured generation metadata injected as <metadata format="json" type="generation">.
    // Must be called before build_to_file() / build_to_string().
    void set_metadata_json(const std::string& json);

    void build_to_file(const std::filesystem::path& path) const;

    // Returns canonical MC3 XML string with optional <metadata> injected.
    std::string build_to_string() const;

private:
    std::unique_ptr<MeshCraft::Mc3::Mc3Document> doc_;
    float       chunk_size_m_;
    int         coord_x_;
    int         coord_y_;
    std::string metadata_json_; // injected as <metadata> if non-empty
};

} // namespace MeshWorld
