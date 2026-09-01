// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "MC3Writer.hpp"
#include "Mc3SceneBuilder.hpp"

namespace MeshWorld {

MC3Writer::MC3Writer(const ChunkContext& ctx)
    : ctx_(ctx)
    , builder_(std::make_unique<Mc3SceneBuilder>(
          "chunk_" + ctx.coord.to_string(),
          ctx.chunk_size_m,
          ctx.coord.x, ctx.coord.y))
{}

MC3Writer::~MC3Writer() = default;

void MC3Writer::ground(const std::string& material, float y) {
    builder_->addGround(material, y);
}

void MC3Writer::plane(const std::string& id,
                      float x, float z, float sx, float sz,
                      const std::string& material, float y, float ry,
                      float rx, float rz) {
    builder_->addPlane(id, x, z, sx, sz, material, y, ry, rx, rz);
}

void MC3Writer::box(const std::string& id,
                    float x, float z, float sx, float sy, float sz,
                    const std::string& material, float y, float ry,
                    float rx, float rz) {
    builder_->addBox(id, x, z, sx, sy, sz, material, y, ry, rx, rz);
}

void MC3Writer::cylinder(const std::string& id,
                         float x, float z, float radius, float height,
                         const std::string& material, float y,
                         float ry, float rx, float rz) {
    builder_->addCylinder(id, x, z, radius, height, material, y, ry, rx, rz);
}

void MC3Writer::sphere(const std::string& id,
                       float x, float z, float radius,
                       const std::string& material, float y) {
    builder_->addSphere(id, x, z, radius, material, y);
}

void MC3Writer::cone(const std::string& id,
                     float x, float z, float radius, float height,
                     const std::string& material, float y) {
    builder_->addCone(id, x, z, radius, height, material, y);
}

void MC3Writer::instance(const std::string& id, const std::string& ref,
                         float x, float z, float ry, float y, float scale,
                         float rx, float rz) {
    builder_->addInstance(id, ref, x, z, ry, y, scale, rx, rz);
}

void MC3Writer::set_metadata_json(const std::string& json) {
    builder_->setMetadataJson(json);
}

std::string MC3Writer::build() const {
    return builder_->buildToString();
}

} // namespace MeshWorld
