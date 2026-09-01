// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>

namespace MeshWorld {

// M170 (MAP11) / M327 (MAP20) — object height, in meters, for a
// ChunkGenerator::placements() definition_id. M327 replaced the original
// all-hardcoded lookup table with a real geometry-derived bounding box:
// when `definition_id` is registered in ObjectDefinitionLibrary, this walks
// its Mc3Object subtree (primitive shape + position.y/scale.y per node,
// see ObjectBoundingBox.cpp's object_y_extent()) and returns the actual
// max_y - min_y. A small hardcoded fallback table covers the handful of
// pre-existing definition_ids a generator's own w.instance() calls
// reference that were never actually registered under that exact name
// (e.g. ForestGenerator's "tree_pine"/"tree_beech") -- a separate,
// already-documented gap, not this function's job to close. Anything with
// neither a registered definition nor a fallback entry gets a conservative
// default.
float object_height_m(const std::string& definition_id);

// M328 (MAP20) — maps a definition's real (M327, geometry-derived) height to
// a WorldRenderer::placement_lod_visible_distance_m() LOD tier: taller
// objects stay visible from farther away (tier 0, that function's own
// max_render_distance_m, unhalved), shorter ones only need to render up
// close (that function halves the visible distance once per tier). A
// size-based heuristic, not a measured/profiled tuning (this environment has
// no GPU to profile against, see M329) — but a principled one: a real
// distant view can spot a 10 m tree across a field but not a 0.3 m flower,
// so bigger visual footprint gets a farther visible distance. Every
// ChunkGenerator::placements() override should assign
// `p.lod_min = lod_tier_for_height(object_height_m(p.definition_id))`
// instead of a fixed 0, so WorldRenderer's LOD gate actually varies
// per-object instead of always evaluating at full distance for everything.
int lod_tier_for_height(float height_m);

} // namespace MeshWorld
