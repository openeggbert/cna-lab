// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Mc3SceneBuilder.hpp"
#include "Mc3DocumentBuilder.hpp"
#include "GenerationMetadata.hpp"
#include "MaterialRegistry.hpp"
#include <cmath>
#include <iostream>

namespace MeshWorld {

Mc3SceneBuilder::Mc3SceneBuilder(const std::string& model,
                                 float chunk_size_m,
                                 int coord_x, int coord_y)
    : builder_(std::make_unique<Mc3DocumentBuilder>(model, chunk_size_m, coord_x, coord_y))
{}

Mc3SceneBuilder::~Mc3SceneBuilder() = default;

void Mc3SceneBuilder::transform_point(float x, float y, float z,
                                       float& ox, float& oy, float& oz) const {
    const Frame& f = top();
    const float rad = f.ry_deg * (3.14159265358979323846f / 180.0f);
    const float c = std::cos(rad), s = std::sin(rad);
    const float rx = c * x + s * z;
    const float rz = -s * x + c * z;
    ox = f.x + f.scale * rx;
    oy = f.y + f.scale * y;
    oz = f.z + f.scale * rz;
}

void Mc3SceneBuilder::pushTransform(float x, float y, float z, float ry_deg, float scale,
                                     const std::string& id_prefix) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    Frame f;
    f.x = wx; f.y = wy; f.z = wz;
    f.ry_deg    = top().ry_deg + ry_deg;
    f.scale     = top().scale * scale;
    f.id_prefix = top().id_prefix + id_prefix;
    stack_.push_back(f);
}

void Mc3SceneBuilder::popTransform() {
    // Never pop the identity base frame -- an unbalanced pop (e.g. after a
    // caller bug) degrades to a no-op instead of corrupting subsequent
    // top-level geometry.
    if (stack_.size() > 1) stack_.pop_back();
}

void Mc3SceneBuilder::addGround(const std::string& material, float y) {
    builder_->ground(material, transform_scalar(y) + top().y);
}

void Mc3SceneBuilder::addPlane(const std::string& id, float x, float z,
                                float sx, float sz,
                                const std::string& material, float y, float ry,
                                float rx, float rz) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    builder_->plane(transform_id(id), wx, wz, transform_scalar(sx), transform_scalar(sz),
                     material, wy, transform_ry(ry), transform_rx(rx), transform_rz(rz));
}

void Mc3SceneBuilder::addBox(const std::string& id, float x, float z,
                              float sx, float sy, float sz,
                              const std::string& material, float y, float ry,
                              float rx, float rz) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    builder_->box(transform_id(id), wx, wz,
                  transform_scalar(sx), transform_scalar(sy), transform_scalar(sz),
                  material, wy, transform_ry(ry), transform_rx(rx), transform_rz(rz));
}

void Mc3SceneBuilder::addCylinder(const std::string& id, float x, float z,
                                   float radius, float height,
                                   const std::string& material, float y,
                                   float ry, float rx, float rz) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    builder_->cylinder(transform_id(id), wx, wz,
                        transform_scalar(radius), transform_scalar(height), material, wy,
                        transform_ry(ry), transform_rx(rx), transform_rz(rz));
}

void Mc3SceneBuilder::addSphere(const std::string& id, float x, float z,
                                 float radius, const std::string& material, float y) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    builder_->sphere(transform_id(id), wx, wz, transform_scalar(radius), material, wy);
}

void Mc3SceneBuilder::addCone(const std::string& id, float x, float z,
                               float radius, float height,
                               const std::string& material, float y) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    builder_->cone(transform_id(id), wx, wz,
                   transform_scalar(radius), transform_scalar(height), material, wy);
}

void Mc3SceneBuilder::addIcoSphere(const std::string& id, float x, float z, float radius,
                                    const std::string& material, float y,
                                    float sx, float sy, float sz) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    builder_->icoSphere(transform_id(id), wx, wz, transform_scalar(radius), material, wy,
                         sx * top().scale, sy * top().scale, sz * top().scale);
}

void Mc3SceneBuilder::addInstance(const std::string& id, const std::string& definition,
                                   float x, float z, float ry, float y, float scale,
                                   float rx, float rz) {
    float wx, wy, wz;
    transform_point(x, y, z, wx, wy, wz);
    builder_->instance(transform_id(id), definition, wx, wz,
                        transform_ry(ry), wy, scale * top().scale,
                        transform_rx(rx), transform_rz(rz));
}

void Mc3SceneBuilder::addMaterial(const std::string& id,
                                   float r, float g, float b, float a,
                                   float roughness, float metallic) {
    if (!MaterialRegistry::instance().has(id))
        std::cerr << "[MeshWorld] Warning: material '" << id
                  << "' is not registered in MaterialRegistry\n";
    builder_->add_material(id, r, g, b, a, roughness, metallic);
}

void Mc3SceneBuilder::setMetadata(const GenerationMetadata& meta) {
    builder_->set_metadata_json(meta.to_json());
}

void Mc3SceneBuilder::setMetadataJson(const std::string& json) {
    builder_->set_metadata_json(json);
}

std::string Mc3SceneBuilder::buildToString() const {
    return builder_->build_to_string();
}

void Mc3SceneBuilder::buildToFile(const std::filesystem::path& path) const {
    builder_->build_to_file(path);
}

} // namespace MeshWorld
