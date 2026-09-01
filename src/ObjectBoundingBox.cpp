// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ObjectBoundingBox.hpp"

#include <algorithm>

#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include "ObjectDefinitionLibrary.hpp"

namespace MeshWorld {

namespace {

using MeshCraft::Mc3::Mc3Object;
using MeshCraft::Mc3::Mc3Primitive;
using MeshCraft::Mc3::PrimitiveType;

struct YExtent {
    float min_y{0.0f};
    float max_y{0.0f};
};

// M327 -- local (pre-transform) Y extent of one primitive, centered on its
// own object-local origin, mirroring exactly how ObjectDefinitionLibrary.cpp's
// own shape helpers (cyl/box/icosphere/cone) position each primitive: a
// cylinder/cone/box is created centered at its local origin and then shifted
// by transform.position.y (e.g. cyl()'s "base_y + h/2"), so the primitive's
// own half-extent is independent of that shift.
YExtent primitive_y_extent(const Mc3Primitive& p) {
    switch (p.primitiveType) {
        case PrimitiveType::Box:
        case PrimitiveType::Cube:
            return {-p.size[1] / 2.0f, p.size[1] / 2.0f};
        case PrimitiveType::Sphere:
        case PrimitiveType::IcoSphere:
            return {-p.radius, p.radius};
        case PrimitiveType::Cylinder:
        case PrimitiveType::Cone:
            return {-p.height / 2.0f, p.height / 2.0f};
        case PrimitiveType::Capsule:
            return {-(p.height / 2.0f + p.radius), p.height / 2.0f + p.radius};
        case PrimitiveType::Torus:
            return {-p.minorRadius, p.minorRadius};
        case PrimitiveType::Plane:
        case PrimitiveType::Disk:
        case PrimitiveType::Grid:
        default:
            // Flat / no meaningful vertical extent of their own.
            return {0.0f, 0.0f};
    }
}

// M327 -- recursively computes an Mc3Object subtree's own Y extent (world
// space RELATIVE TO the object passed in, i.e. as if it were freshly
// instantiated with no further parent transform above it -- exactly the
// frame every ObjectDefinitionLibrary definition's root object is authored
// in). Rotation is deliberately ignored: every shape helper in
// ObjectDefinitionLibrary.cpp only ever sets yaw (Y-axis) rotation via
// box()'s ry_deg parameter, which cannot change a Y extent -- there is no
// pitch/roll anywhere in this codebase's object library to account for.
// position.y and scale.y (used by e.g. birch_tree()'s deformed canopy
// icosphere) both DO matter and are applied per child.
YExtent object_y_extent(const Mc3Object& obj) {
    bool  any   = false;
    float min_y = 0.0f;
    float max_y = 0.0f;

    if (obj.primitive) {
        const YExtent e = primitive_y_extent(*obj.primitive);
        min_y = e.min_y;
        max_y = e.max_y;
        any   = true;
    }

    for (const auto& child : obj.children) {
        if (!child) continue;
        const YExtent  c  = object_y_extent(*child);
        const float    cy = child->transform.position[1];
        const float    sy = child->transform.scale[1];
        const float    lo = cy + c.min_y * sy;
        const float    hi = cy + c.max_y * sy;
        if (!any) {
            min_y = lo;
            max_y = hi;
            any   = true;
        } else {
            min_y = std::min(min_y, lo);
            max_y = std::max(max_y, hi);
        }
    }

    return any ? YExtent{min_y, max_y} : YExtent{0.0f, 0.0f};
}

// Conservative fallback for a definition_id with no known height at all.
constexpr float kDefaultHeightM = 2.0f;

} // namespace

// M327 -- a real geometry-derived bounding-box height wherever the
// definition actually exists in ObjectDefinitionLibrary. Lazily loads the
// library exactly once so this function is self-sufficient regardless of
// whether a caller (a real app's main(), a test) already populated it --
// ObjectDefinitionLibrary::load_all() is idempotent, so this is safe even
// if it runs again elsewhere.
//
// Used to also fall back to a small hardcoded height table for a handful of
// definition_ids that ChunkGenerator::placements() referenced but
// ObjectDefinitionLibrary didn't register under the exact same name
// (ForestGenerator's "tree_pine"/"tree_beech", JungleGenerator's
// "tree_banyan"/"tree_tropical_fern"/"tree_bamboo", MeadowGenerator's
// "flower_poppy"/"flower_bluebell"/"flower_buttercup") -- those were real,
// currently-invisible-in-game bugs (found via a T-series backlog triage,
// 2026-07-11: a new AllGeneratorsTest.InstanceDefinitionsResolveFromObjectDefinitionLibrary
// test proved every one of these 8 IDs was actually unresolvable), not name
// mismatches to paper over here. Fixed at the source instead
// (ObjectDefinitionLibrary::load_all() now registers all 8) -- the fallback
// table is removed entirely rather than left as unreachable dead code now
// that nothing needs it.
float object_height_m(const std::string& definition_id) {
    static const bool loaded = (ObjectDefinitionLibrary::instance().load_all(), true);
    (void)loaded;

    if (const auto def = ObjectDefinitionLibrary::instance().get(definition_id)) {
        const YExtent e = object_y_extent(*def);
        const float   h = e.max_y - e.min_y;
        if (h > 0.0f) return h;
    }

    return kDefaultHeightM;
}

int lod_tier_for_height(float height_m) {
    if (height_m >= 3.0f) return 0;
    if (height_m >= 1.0f) return 1;
    return 2;
}

} // namespace MeshWorld
