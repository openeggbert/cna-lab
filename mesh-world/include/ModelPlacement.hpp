// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>

namespace MeshWorld {

// M167 (MAP11) — "instance of definition `tree_oak` at world position (x, y,
// z), rotation, scale, LOD" (map.md §10.1's own phrasing): the per-world,
// per-object placement record the 3D model store persists, distinct from the
// shared, never-serialized geometry definitions in ObjectDefinitionLibrary
// (definition_id is a lookup key into that library, not a copy of its data).
//
// Field shapes mirror map.md §10.1's already-decided `models/<rx>_<rz>.db`
// `placement` table schema directly (pos_x/pos_y/pos_z, y_min/y_max, rot_y,
// scale, lod_min, definition), so M169's writer can serialize this struct
// close to 1:1. `chunk_x`/`chunk_z`/`alt_band` (that schema's indexing
// columns) are deliberately NOT part of this struct -- they're derived from
// pos_x/pos_z/pos_y at write time (M169), not stored twice.
//
// Positions are double-precision, global/planet-scale world-space meters
// (mirrors Map::TileCoord's own double-precision world coordinates, needed
// at a ~22,585 km planet scale) -- NOT chunk-local or camera-relative.
// WorldRenderer's floating-origin offset (M176) converts to camera-relative
// float only at render time, not here.
struct ModelPlacement {
    // ObjectDefinitionLibrary lookup key (e.g. "tree_oak") -- the shared
    // geometry itself is never duplicated per placement.
    std::string definition_id;

    double pos_x{0.0};
    double pos_y{0.0};  // altitude
    double pos_z{0.0};

    // Altitude extent this placement's geometry occupies (M170 derives these
    // from the object's bounding box + ground elevation at pos_x/pos_z) --
    // the axis that makes proximity streaming genuinely 3D/volumetric
    // (map.md §10.1), not just a 2D heightmap.
    double y_min{0.0};
    double y_max{0.0};

    // Y-axis rotation in degrees and uniform scale -- same convention as
    // MC3Writer::instance()'s own ry/scale parameters.
    float rot_y{0.0f};
    float scale{1.0f};

    // Minimum LOD tier this placement is visible at (M177 gates detail by
    // distance against this).
    int lod_min{0};

    // M173 -- field-by-field value equality. ModelPlacement has no stable row
    // identity (the SQL schema's own `id` column, and chunk_x/chunk_z/
    // alt_band, are deliberately not part of this struct -- see above), so
    // Model3DStreamer's load/unload diff has to match placements by value.
    // A real, accepted limitation: two genuinely distinct placements that
    // happen to share every field would be indistinguishable here.
    bool operator==(const ModelPlacement& other) const {
        return definition_id == other.definition_id && pos_x == other.pos_x &&
               pos_y == other.pos_y && pos_z == other.pos_z && y_min == other.y_min &&
               y_max == other.y_max && rot_y == other.rot_y && scale == other.scale &&
               lod_min == other.lod_min;
    }
};

}  // namespace MeshWorld
