// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Mc3Collision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace MeshWorld {
namespace {

using MeshCraft::Mc3::Mc3Object;
using MeshCraft::Mc3::Mc3Transform;
using MeshCraft::Mc3::ObjectType;

bool is_valid_bounds(const std::array<float, 3>& min, const std::array<float, 3>& max) {
    return min[0] <= max[0] && min[1] <= max[1] && min[2] <= max[2];
}

std::array<float, 3> transform_point(const Mc3Transform& transform,
                                     std::array<float, 3> point) {
    // MC3 documents rotations as extrinsic XYZ Euler angles.  Apply the
    // local X/Y/Z rotations in that documented order after scale/pivot.
    for (std::size_t axis = 0; axis < point.size(); ++axis)
        point[axis] = (point[axis] - transform.pivot[axis]) * transform.scale[axis];

    constexpr float radians_per_degree = std::numbers::pi_v<float> / 180.0f;
    const auto rotate_x = [&](float degrees) {
        const float c = std::cos(degrees * radians_per_degree);
        const float s = std::sin(degrees * radians_per_degree);
        const float y = point[1] * c - point[2] * s;
        point[2] = point[1] * s + point[2] * c;
        point[1] = y;
    };
    const auto rotate_y = [&](float degrees) {
        const float c = std::cos(degrees * radians_per_degree);
        const float s = std::sin(degrees * radians_per_degree);
        const float x = point[0] * c + point[2] * s;
        point[2] = -point[0] * s + point[2] * c;
        point[0] = x;
    };
    const auto rotate_z = [&](float degrees) {
        const float c = std::cos(degrees * radians_per_degree);
        const float s = std::sin(degrees * radians_per_degree);
        const float x = point[0] * c - point[1] * s;
        point[1] = point[0] * s + point[1] * c;
        point[0] = x;
    };
    rotate_x(transform.rotation[0]);
    rotate_y(transform.rotation[1]);
    rotate_z(transform.rotation[2]);
    for (std::size_t axis = 0; axis < point.size(); ++axis)
        point[axis] += transform.position[axis];
    return point;
}

CollisionBox transform_bounds(const std::array<float, 3>& min, const std::array<float, 3>& max,
                              const Mc3Transform& definition_transform,
                              const Mc3Transform& instance_transform) {
    CollisionBox out;
    out.min_x = out.min_y = out.min_z = std::numeric_limits<float>::infinity();
    out.max_x = out.max_y = out.max_z = -std::numeric_limits<float>::infinity();
    for (int bits = 0; bits < 8; ++bits) {
        std::array<float, 3> point = {
            (bits & 1) ? max[0] : min[0],
            (bits & 2) ? max[1] : min[1],
            (bits & 4) ? max[2] : min[2],
        };
        point = transform_point(instance_transform, transform_point(definition_transform, point));
        out.min_x = std::min(out.min_x, point[0]);
        out.max_x = std::max(out.max_x, point[0]);
        out.min_y = std::min(out.min_y, point[1]);
        out.max_y = std::max(out.max_y, point[1]);
        out.min_z = std::min(out.min_z, point[2]);
        out.max_z = std::max(out.max_z, point[2]);
    }
    return out;
}

const Mc3Object* find_definition(const MeshCraft::Mc3::Mc3Document& document,
                                 const std::string& key) {
    if (const auto it = document.definitions.find(key);
        it != document.definitions.end() && it->second) return it->second.get();
    const std::size_t alias = key.find(':');
    if (alias == std::string::npos) return nullptr;
    if (const auto it = document.definitions.find(key.substr(alias + 1));
        it != document.definitions.end() && it->second) return it->second.get();
    return nullptr;
}

void append_inline_box(const Mc3Object& object, float min_height_m,
                       CollisionExtractionResult& result) {
    if (object.type != ObjectType::Box || object.collision != "box" || !object.primitive) return;
    const auto& size = object.primitive->size;
    if (size[1] < min_height_m) return;
    const std::array<float, 3> min = {-size[0] * 0.5f, -size[1] * 0.5f, -size[2] * 0.5f};
    const std::array<float, 3> max = { size[0] * 0.5f,  size[1] * 0.5f,  size[2] * 0.5f};
    result.boxes.push_back(transform_bounds(min, max, Mc3Transform{}, object.transform));
}

void append_instance_box(const MeshCraft::Mc3::Mc3Document& document, const Mc3Object& instance,
                         float min_height_m, CollisionExtractionResult& result) {
    if (instance.type != ObjectType::Instance) return;
    const std::string& definition_key = instance.resolvedInstanceDefinitionKey();
    const Mc3Object* definition = find_definition(document, definition_key);
    if (!definition) {
        result.diagnostics.push_back({CollisionDiagnosticKind::UnresolvedDefinition, definition_key,
                                      "instance definition was not available in the document"});
        return;
    }
    if (!definition->assetMetadata) return;
    const auto& metadata = *definition->assetMetadata;
    if (metadata.collisionProxy.empty() || metadata.collisionProxy == "none") return;
    if (metadata.collisionProxy != "box") {
        result.diagnostics.push_back({CollisionDiagnosticKind::UnsupportedProxy, definition_key,
                                      "collisionProxy='" + metadata.collisionProxy + "' is not implemented"});
        return;
    }
    if (!is_valid_bounds(metadata.boundsMin, metadata.boundsMax)) {
        result.diagnostics.push_back({CollisionDiagnosticKind::InvalidBounds, definition_key,
                                      "assetMetadata bounds min exceeds max"});
        return;
    }
    CollisionBox box = transform_bounds(metadata.boundsMin, metadata.boundsMax,
                                        definition->transform, instance.transform);
    if (box.max_y - box.min_y < min_height_m) return;
    result.boxes.push_back(box);
}

} // namespace

CollisionExtractionResult extract_mc3_collision_boxes(
    const MeshCraft::Mc3::Mc3Document& document, float min_height_m) {
    CollisionExtractionResult result;
    for (const auto& object : document.objects) {
        if (!object) continue;
        append_inline_box(*object, min_height_m, result);
        append_instance_box(document, *object, min_height_m, result);
    }
    return result;
}

} // namespace MeshWorld
