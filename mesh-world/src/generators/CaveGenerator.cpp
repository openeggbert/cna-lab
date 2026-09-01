// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/CaveGenerator.hpp"
#include "CaveLayout.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <algorithm>
#include <random>

namespace MeshWorld {

namespace {

// M336 (MAP21) — the elevation threshold above which BiomeClassifier::
// classify() can produce ZoneType::cave at all (Map/BiomeClassifier.cpp's
// own MOUNTAIN_ELEV_M, duplicated here as a plain float rather than
// depending on Map:: headers — matches MapContext's own decoupling
// comment in ChunkGenerator.hpp). A cave chunk sitting close to this
// boundary is on the mountain's own lower slopes, closer to the exterior
// surface; one much higher above it is deep inside a taller massif.
constexpr float kMountainElevM = 2500.0f;
// How many metres above the threshold entrance odds keep tapering off
// before settling at the floor probability.
constexpr float kEntranceTaperBandM = 1200.0f;

// M336/M337 (MAP21) — probability that this cave chunk breaches through to
// the mountain surface above, tied to how close its own elevation sits to
// the mountain/cave threshold (see kMountainElevM above). Deliberately
// never 0.0 or 1.0: caves are a genuine mix of sealed rooms and breached
// ones at every elevation band, not a hard cutoff. Requires map_context to
// be available (matches every other elevation-driven MAP17 branch in this
// file, e.g. the deep_crystal extras below) — without real elevation data
// there is nothing to tie an entrance decision to, so it defaults to 0.0.
float entrance_probability(const ChunkContext& ctx) {
    if (!ctx.map_context.available) return 0.0f;
    const float above_threshold = ctx.map_context.elevation_m - kMountainElevM;
    return std::clamp(1.0f - above_threshold / kEntranceTaperBandM, 0.1f, 0.8f);
}

} // namespace

std::string CaveGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    const float s = ctx.chunk_size_m;
    const float ceiling = 8.0f;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(3.0f, s - 3.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> sh(0.5f, 3.5f);
    std::uniform_real_distribution<float> sr(0.1f, 0.5f);
    std::uniform_real_distribution<float> mgh(0.3f, 2.0f);

    // M332 (MAP21) — which sides open into a tunnel toward another cave
    // chunk underground. Computed up front (rather than right before the
    // wall draws below) because M337's ambient-light metadata also reads
    // it — a pure static function, safe to call once and reuse.
    const CaveOpenings openings = CaveLayout::openings_for(ctx.world_seed, ctx.coord);

    // M336 (MAP21) — surface-entrance decision uses its OWN independent RNG
    // stream (distinct salt) so it never perturbs the existing stalactite/
    // stalagmite/crystal/rubble draw sequence below — same "purely
    // additive, no hidden side effect" discipline M326/M335 already used.
    std::mt19937_64 entrance_rng(ctx.seed ^ 0xE47A3000ULL);
    std::uniform_real_distribution<float> entrance_roll(0.0f, 1.0f);
    const bool has_surface_entrance = entrance_roll(entrance_rng) < entrance_probability(ctx);

    // M337 (MAP21) — ambient light/darkness data: 0.0 = pitch dark, 1.0 =
    // full outdoor daylight. Distinct from Mc3DocumentBuilder's fixed
    // directional sun + ambient fill (applied uniformly to every chunk's
    // document — nothing in the MC3Writer/Mc3SceneBuilder chain exposes a
    // per-chunk override hook yet, so this ships as DATA for a future
    // renderer to consume, not as an actual rendered light source; wiring
    // a real consumer is separate, later work). A chamber with a surface
    // entrance is lit near daylight levels; a fully sealed chamber (no
    // CaveLayout openings at all) is near-pitch-dark but never literally
    // 0.0 — bioluminescent crystals/torches always give SOME visibility in
    // practice, a deliberate game/render convention.
    const float ambient_light_level = has_surface_entrance
        ? 0.55f
        : std::clamp(0.05f + 0.05f * static_cast<float>(openings.count()), 0.05f, 0.30f);

    // Splice a "cave" object into the shared GenerationMetadata JSON rather
    // than extending that struct (used by all 20 generators) or bolting on
    // a bespoke JSON builder — to_json()'s exact trailing "\n}" is fully
    // controlled by this same codebase, so dropping the final brace and
    // appending a sibling key is a safe, deterministic suffix edit.
    std::string metadata_json =
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.cave").to_json();
    metadata_json.pop_back();
    metadata_json += ",\n  \"cave\": {"
                      "\n    \"ambientLightLevel\": " + std::to_string(ambient_light_level) +
                      ",\n    \"hasSurfaceEntrance\": " + (has_surface_entrance ? "true" : "false") +
                      "\n  }\n}";
    w.set_metadata_json(metadata_json);

    w.ground("rock_cave_floor");

    // M332 (MAP21) — a wall is drawn solid UNLESS CaveLayout says this side
    // opens into a tunnel toward that neighbor, in which case it's left
    // out entirely (an open passage the full width of that side, a
    // deliberate v1 simplification — a narrower doorway-sized gap is a
    // natural refinement, not required by "a real connected system exists
    // at all" which is this task's own actual scope). This is what turns
    // every cave chunk from an isolated sealed room (pre-M332) into a real,
    // if sparse and irregular, connected cave system across chunks — see
    // CaveLayout.hpp for the symmetric cross-chunk agreement this relies on.
    const float wt = 3.0f;
    if (!openings.north) w.box("wall_n", s*0.5f, wt*0.5f,     s, ceiling, wt,   "rock_cave_wall");
    if (!openings.south) w.box("wall_s", s*0.5f, s-wt*0.5f,   s, ceiling, wt,   "rock_cave_wall");
    if (!openings.west)  w.box("wall_w", wt*0.5f, s*0.5f,     wt, ceiling, s,   "rock_cave_wall");
    if (!openings.east)  w.box("wall_e", s-wt*0.5f, s*0.5f,   wt, ceiling, s,   "rock_cave_wall");

    // M336 (MAP21) — a solid, unbroken ceiling UNLESS this chunk rolled a
    // surface entrance, in which case it's split into 4 planes framing a
    // centered square gap (a "picture frame" decomposition: the two full-
    // width N/S strips plus the two narrower W/E strips exactly tile the
    // s×s ceiling minus the s/3×s/3 hole in the middle) — the point where
    // this cave breaches through to the mountain surface above. Distinct
    // from CaveLayout's own wall openings above, which connect to ANOTHER
    // cave chunk underground, not to the outside world (CaveLayout.hpp's
    // own doc comment explicitly deferred this exact case to M336).
    if (has_surface_entrance) {
        const float gap   = s / 3.0f;
        const float inset = (s - gap) * 0.5f;
        w.plane("ceiling_n", 0.0f,      0.0f,      s,     inset, "rock_cave_ceiling", ceiling);
        w.plane("ceiling_s", 0.0f,      s - inset, s,     inset, "rock_cave_ceiling", ceiling);
        w.plane("ceiling_w", 0.0f,      inset,     inset, gap,   "rock_cave_ceiling", ceiling);
        w.plane("ceiling_e", s - inset, inset,     inset, gap,   "rock_cave_ceiling", ceiling);
    } else {
        w.plane("ceiling", 0, 0, s, s, "rock_cave_ceiling", ceiling);
    }

    for (int i = 0; i < 14; ++i) {
        float h = sh(rng), r = sr(rng);
        w.cylinder("stalac_" + std::to_string(i),
                   pos(rng), pos(rng), r, h, "rock_stalactite", ceiling - h);
    }
    for (int i = 0; i < 10; ++i) {
        float h = mgh(rng), r = sr(rng) * 0.7f;
        w.cylinder("stalag_" + std::to_string(i),
                   pos(rng), pos(rng), r, h, "rock_stalagmite");
    }

    w.plane("pool", s*0.5f-5.0f, s*0.5f-5.0f, 10.0f, 10.0f, "water_still", -0.1f);

    for (int i = 0; i < 5; ++i)
        w.instance("crystal_" + std::to_string(i), "crystal_blue",
                   pos(rng), pos(rng), rot(rng));

    // MAP17 -- extra crystal clusters in a deep cave (below sea level --
    // deeper underground than a shallow hillside cave). Purely additive
    // after every pre-MAP17 draw above.
    if (ctx.map_context.available && ctx.map_context.elevation_m < 0.0f) {
        for (int i = 0; i < 4; ++i)
            w.instance("deep_crystal_" + std::to_string(i), "crystal_blue",
                       pos(rng), pos(rng), rot(rng));
    }

    // MAP21, M335 -- rock rubble scattered on the cave floor. Genuinely new
    // content (unlike stalactites/stalagmites/crystals above, which already
    // existed pre-M333/335), so it's added here too, not just in
    // placements() below -- same "new content goes in both channels"
    // precedent MAP20's M326 (coral/kelp) already established. Purely
    // additive after every pre-M335 draw above.
    for (int i = 0; i < 8; ++i)
        w.instance("rubble_" + std::to_string(i), "rock_rubble",
                   pos(rng), pos(rng), rot(rng));

    return w.build();
}

// MAP21, M333/M334/M335 -- ModelPlacements for stalactites/stalagmites,
// crystals, and rock rubble, mirroring ForestGenerator's M168 pattern (own
// independent RNG draw). The pool (generate()'s w.plane() call) is NOT
// converted -- a raw MC3 primitive, no ObjectDefinitionLibrary
// definition_id (same scope decision as the other MAP20 generators' own
// placements()); the elevation-conditioned deep_crystal extras are left out
// of this "first slice" the same way ForestGenerator's own river-mushroom
// extras are left out of its placements().
std::vector<ModelPlacement> CaveGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(14 + 10 + 5 + 8);

    const float s = ctx.chunk_size_m;
    const float ceiling = 8.0f;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(3.0f, s - 3.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);

    const int chunk_size_i = static_cast<int>(ctx.chunk_size_m);
    const double origin_x = ctx.coord.world_x(chunk_size_i);
    const double origin_z = ctx.coord.world_z(chunk_size_i);
    const double ground_elevation_m =
        ctx.map_context.available ? static_cast<double>(ctx.map_context.elevation_m) : 0.0;

    // Ground-based placements (stalagmites/crystals/rubble): base sits at
    // ground_elevation_m and extends upward -- the same pattern every other
    // MAP20 generator's own placements() lambda already uses.
    const auto add_ground = [&](const std::string& definition_id, float local_x, float local_z, float ry) {
        const float height_m = object_height_m(definition_id);
        ModelPlacement p;
        p.definition_id = definition_id;
        p.pos_x = origin_x + local_x;
        p.pos_z = origin_z + local_z;
        p.pos_y = ground_elevation_m;
        p.y_min = ground_elevation_m;
        p.y_max = ground_elevation_m + static_cast<double>(height_m);
        p.rot_y = ry;
        p.scale = 1.0f;
        p.lod_min = lod_tier_for_height(height_m);
        result.push_back(std::move(p));
    };

    // Ceiling-based placements (stalactites only): every object definition
    // in this codebase has its own local origin at its BOTTOM (see
    // ObjectDefinitionLibrary.cpp's own doc comment on make_stalactite()),
    // so a stalactite's base must sit at (ceiling - height) for its TOP to
    // land exactly at this cave's own ceiling height -- the opposite
    // direction every ground-based object positions itself, hence its own
    // lambda rather than reusing add_ground.
    const auto add_hanging = [&](const std::string& definition_id, float local_x, float local_z, float ry) {
        const float  height_m = object_height_m(definition_id);
        const double base_y   = ground_elevation_m + static_cast<double>(ceiling) - static_cast<double>(height_m);
        ModelPlacement p;
        p.definition_id = definition_id;
        p.pos_x = origin_x + local_x;
        p.pos_z = origin_z + local_z;
        p.pos_y = base_y;
        p.y_min = base_y;
        p.y_max = base_y + static_cast<double>(height_m);
        p.rot_y = ry;
        p.scale = 1.0f;
        p.lod_min = lod_tier_for_height(height_m);
        result.push_back(std::move(p));
    };

    for (int i = 0; i < 14; ++i)
        add_hanging("stalactite_hanging", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 10; ++i)
        add_ground("stalagmite_rising", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 5; ++i)
        add_ground("crystal_blue", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 8; ++i)
        add_ground("rock_rubble", pos(rng), pos(rng), rot(rng));

    return result;
}

} // namespace MeshWorld
