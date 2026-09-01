// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <memory>
#include <string>
#include "ChunkGenerator.hpp"

namespace MeshWorld {

class Mc3SceneBuilder;

// Builds a valid mc3.xml string for one chunk.
// Delegates to Mc3SceneBuilder → Mc3DocumentBuilder → MeshCraft Mc3XmlWriter.
//
// Coordinate conventions (all in meters, chunk-local):
//   plane   — x,z = min corner;  sx,sz = full width/depth
//   box     — x,z = center;      sx,sy,sz = full width/height/depth; y = base elevation
//   cylinder— x,z = center;      y = base elevation
//   sphere  — x,z = center;      y = base elevation (center at y + radius)
//   cone    — x,z = center;      y = base elevation
//   instance— x,z = center;      y = base elevation; ry = Y-rotation (degrees)
//
// All positions must stay within [0, chunk_size_m].
class MC3Writer {
public:
    explicit MC3Writer(const ChunkContext& ctx);
    ~MC3Writer();

    void ground(const std::string& material, float y = 0.0f);

    // rx/rz (G12): tilt/roll in degrees -- see Mc3SceneBuilder.hpp's own doc
    // comment for exactly what these do and don't guarantee.
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

    void sphere(const std::string& id, float x, float z,
                float radius, const std::string& material, float y = 0.0f);

    void cone(const std::string& id, float x, float z,
              float radius, float height,
              const std::string& material, float y = 0.0f);

    void instance(const std::string& id, const std::string& ref,
                  float x, float z,
                  float ry = 0.0f, float y = 0.0f, float scale = 1.0f,
                  float rx = 0.0f, float rz = 0.0f);

    // Inject structured generation metadata as <metadata format="json" type="generation">.
    // Call once before build().
    void set_metadata_json(const std::string& json);

    float size() const { return ctx_.chunk_size_m; }

    std::string build() const;

private:
    const ChunkContext&             ctx_;
    std::unique_ptr<Mc3SceneBuilder> builder_;
};

} // namespace MeshWorld
