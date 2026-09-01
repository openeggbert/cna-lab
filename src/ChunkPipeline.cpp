// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ChunkPipeline.hpp"
#include "BuildingComposer.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "LuaSandbox.hpp"
#include "MC3Validator.hpp"
#include "Mc3MeshBuilder.hpp"
#include "RegionType.hpp"
#include "Map/MapPipeline.hpp"
#include "Map/PlanetConstants.hpp"
#include "Map/TileCoord.hpp"
#include "Map/ZoneCandidate.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace MeshWorld {

namespace {

// M157 — the only level any Lua/C++ generator ever calls setZoneCandidates()
// at (city.lua). Nothing propagates zone_candidates down the quadtree to
// finer levels (unlike elevation/biome, which every level recomputes from
// its parent) -- levels 13-18 fall through to the generic C++
// ChildGenerator, which has never heard of Map::ZoneCandidate and leaves
// MapBuilder's zone_candidates grid at its default-empty state. So the
// hand-off tile (level MAX_LEVEL) itself is the WRONG place to look for
// zone candidates; sample the level-12 ANCESTOR tile that actually covers
// this chunk instead (a separate MapPipeline::get() call, same technique
// elevation/biome use, just at a different fixed level).
constexpr int kCityZoningLevel = 12;

// M162 — the level Street/Road tile-boundary crossings are checked at.
// city.lua (12)/neighborhood.lua (15)/country.lua (5) are the only real
// sources of Street/Road features (and therefore TileEdge::crossings, via
// MapBuilder::deriveEdgeCrossings()) -- neighborhood.lua is the finest of
// the three and closest to chunk scale (~688 m vs. a 64 m chunk), so it's
// the one checked here. city.lua's (level 12) and country.lua's (level 5)
// own crossings are NOT yet translated into EdgeExits -- a real,
// documented gap, not silently assumed away.
constexpr int kStreetCrossingLevel = 15;

// True if any Road-type crossing recorded on one tile edge (world span
// [span_lo, span_hi] along that edge) falls within THIS chunk's own span
// along the same axis ([chunk_lo, chunk_hi]), and the chunk itself is
// within one chunk-width of the edge's perpendicular coordinate. Tile and
// chunk grids aren't lattice-aligned at this depth (map.md's own
// documented trade-off, §5 #12 in NEXT.md), so exact boundary alignment
// can't be assumed -- only chunks close to the tile edge are candidates.
bool chunk_has_road_crossing(const std::vector<Map::EdgeCrossing>& crossings,
                              double span_lo, double span_hi,
                              double chunk_lo, double chunk_hi,
                              double chunk_perp_center, double edge_perp_coord,
                              double chunk_size_m) {
    if (std::abs(chunk_perp_center - edge_perp_coord) > chunk_size_m) return false;
    for (const auto& c : crossings) {
        if (c.type != Map::EdgeCrossingType::Road) continue;
        const double world_pos = span_lo + static_cast<double>(c.position) * (span_hi - span_lo);
        if (world_pos >= chunk_lo && world_pos <= chunk_hi) return true;
    }
    return false;
}

// M162/R134 — a level-15 crossing is a coarse, nearby map fact (~688 m), not
// proof that a particular 64 m chunk edge is shared by the neighbouring
// chunk. Keep it as context for future map-road materialisation, but never
// turn it into local road geometry: doing so was the one-sided road-stub
// source R134 removes. WorldMap::road_connections() is the sole producer of
// physical ChunkContext exits until map roads get their own canonical local
// edge graph.
void populate_nearby_road_crossing(ChunkContext& ctx, Map::MapPipeline& map_pipeline, int chunk_size_m) {
    const double world_x0 = static_cast<double>(ctx.coord.world_x(chunk_size_m));
    const double world_z0 = static_cast<double>(ctx.coord.world_z(chunk_size_m));
    const double world_x1 = world_x0 + chunk_size_m;
    const double world_z1 = world_z0 + chunk_size_m;
    const double center_x = world_x0 + chunk_size_m * 0.5;
    const double center_z = world_z0 + chunk_size_m * 0.5;

    const Map::TileCoord      tile    = Map::TileCoord::from_world(center_x, center_z, kStreetCrossingLevel);
    const Map::MapTilePayload payload = map_pipeline.get(tile);
    const Map::WorldBounds    bounds  = tile.world_bounds();

    if (chunk_has_road_crossing(payload.edges[0].crossings, bounds.min_x, bounds.max_x,
                                world_x0, world_x1, center_z, bounds.min_z, chunk_size_m)) {
        ctx.map_context.has_road_crossing = true;
    }
    if (chunk_has_road_crossing(payload.edges[2].crossings, bounds.min_x, bounds.max_x,
                                world_x0, world_x1, center_z, bounds.max_z, chunk_size_m)) {
        ctx.map_context.has_road_crossing = true;
    }
    if (chunk_has_road_crossing(payload.edges[3].crossings, bounds.min_z, bounds.max_z,
                                world_z0, world_z1, center_x, bounds.min_x, chunk_size_m)) {
        ctx.map_context.has_road_crossing = true;
    }
    if (chunk_has_road_crossing(payload.edges[1].crossings, bounds.min_z, bounds.max_z,
                                world_z0, world_z1, center_x, bounds.max_x, chunk_size_m)) {
        ctx.map_context.has_road_crossing = true;
    }
}

// M159 — sample the hand-off tile's fields at the chunk's world-space center
// and fill ctx.map_context. Grid lookup: normalize the center into the tile's
// [0,1) extent, then scale to the field's resolution (fields may differ in
// size from each other, e.g. elevation vs biome, so each is sampled separately).
void populate_map_context(ChunkContext& ctx, Map::MapPipeline& map_pipeline, int chunk_size_m) {
    const double world_x = static_cast<double>(ctx.coord.world_x(chunk_size_m)) + chunk_size_m * 0.5;
    const double world_z = static_cast<double>(ctx.coord.world_z(chunk_size_m)) + chunk_size_m * 0.5;

    auto sample_grid_coord = [&](const Map::WorldBounds& bounds, int w, int h) {
        const double u = (world_x - bounds.min_x) / (bounds.max_x - bounds.min_x);
        const double v = (world_z - bounds.min_z) / (bounds.max_z - bounds.min_z);
        const int gx = std::clamp(static_cast<int>(u * w), 0, w - 1);
        const int gy = std::clamp(static_cast<int>(v * h), 0, h - 1);
        return std::pair{gx, gy};
    };

    const Map::TileCoord tile = Map::TileCoord::from_world(world_x, world_z, Map::MAX_LEVEL);
    const Map::MapTilePayload payload = map_pipeline.get(tile);
    const Map::WorldBounds    bounds  = tile.world_bounds();

    ctx.map_context.available = true;
    if (!payload.elevation.empty()) {
        const auto [gx, gy] = sample_grid_coord(bounds, payload.elevation.w, payload.elevation.h);
        ctx.map_context.elevation_m = payload.elevation.at(gx, gy);
    }
    if (!payload.biome.empty()) {
        const auto [gx, gy] = sample_grid_coord(bounds, payload.biome.w, payload.biome.h);
        ctx.map_context.biome_ordinal = payload.biome.at(gx, gy);
    }

    // M159 — is_city needs no new data: city.lua's markUrbanCells() already
    // writes ZoneType::city into biome wherever it runs, so this is a
    // trivial derivation from the sample just taken above, not something
    // that needs MAP9's (still-unwired) settlement algorithms.
    ctx.map_context.is_city = ctx.map_context.biome_ordinal == static_cast<std::uint8_t>(ZoneType::city);

    // M159 — nearest river: the hand-off tile itself (level MAX_LEVEL)
    // always falls through to the generic C++ ChildGenerator (no Lua
    // script exists at levels 13-18), and MAP8's Hydrology/FeatureNaming
    // are wired into PlanetGenerator/ChildGenerator generically -- so
    // `payload.features` already has real, named FeatureType::River
    // entries wherever the tile has one (confirmed empirically). V1
    // simplification: nearest DISTANCE TO ANY VERTEX along each river's
    // path, not true point-to-segment distance (a river's points are
    // already fine-grained samples, so this is a close approximation);
    // and only this tile's own features are considered -- a river just
    // across the tile boundary is missed, the same "no cross-tile lookup"
    // limitation M108's own boundary-only-2-sides note already accepts.
    if (!payload.features.empty()) {
        double      best_dist_sq = std::numeric_limits<double>::max();
        std::string best_name;
        for (const auto& f : payload.features) {
            if (f.type != Map::FeatureType::River) continue;
            for (const auto& pt : f.points) {
                const double dx = pt[0] - world_x;
                const double dz = pt[1] - world_z;
                const double d2 = dx * dx + dz * dz;
                if (d2 < best_dist_sq) {
                    best_dist_sq = d2;
                    best_name    = f.name;
                }
            }
        }
        if (!best_name.empty()) {
            ctx.map_context.nearest_river_name       = best_name;
            ctx.map_context.nearest_river_distance_m = static_cast<float>(std::sqrt(best_dist_sq));
        }
    }

    // M156/M157 — zone_candidates only ever exists at level 12 (see
    // kCityZoningLevel's own comment); look up that ancestor specifically
    // rather than the (always zone_candidates-empty) hand-off tile.
    const Map::TileCoord city_tile = Map::TileCoord::from_world(world_x, world_z, kCityZoningLevel);
    const Map::MapTilePayload city_payload = map_pipeline.get(city_tile);
    if (!city_payload.zone_candidates.empty()) {
        const Map::WorldBounds city_bounds = city_tile.world_bounds();
        const auto [gx, gy] = sample_grid_coord(city_bounds, city_payload.zone_candidates.w,
                                                city_payload.zone_candidates.h);
        ctx.map_context.zone_candidate_ordinal = city_payload.zone_candidates.at(gx, gy);
    }
    // has_road_crossing / nearest_place_name / nearest_place_kind stay at
    // their defaults: no generator anywhere populates TileEdge::crossings
    // with real per-direction data, and MAP9's Settlements::appendLabels()/
    // Countries::name() aren't wired into any real generator either (see
    // NEXT.md §2) -- nothing to sample yet.
}

// M157 — Map::ZoneCandidate -> RegionType, a direct 1:1 lookup (the two
// enums use matching spelling by design, see ZoneCandidate.hpp's own doc
// comment). `none` has no RegionType equivalent (it means "not a zoned
// block") so it maps to RegionType::open, the same generic default the
// flat WorldMap already uses for ordinary non-region-specific land.
RegionType region_from_zone_candidate(Map::ZoneCandidate c) {
    switch (c) {
        case Map::ZoneCandidate::small_house_block: return RegionType::small_house_block;
        case Map::ZoneCandidate::apartment_block:   return RegionType::apartment_block;
        case Map::ZoneCandidate::shop_street:       return RegionType::shop_street;
        case Map::ZoneCandidate::park:              return RegionType::park;
        case Map::ZoneCandidate::square:            return RegionType::square;
        case Map::ZoneCandidate::none:              break;
    }
    return RegionType::open;
}

} // namespace

ChunkPipeline::ChunkPipeline(const WorldConfig& cfg, const WorldMap& map, ChunkCache cache,
                              Map::MapPipeline* map_pipeline)
    : cfg_(cfg), map_(map), cache_(std::move(cache)), map_pipeline_(map_pipeline) {}

ChunkContext ChunkPipeline::build_context(int x, int y) const {
    const ChunkCoord coord{x, y};
    const ChunkInfo  ci = map_.info(coord);

    ChunkContext ctx;
    ctx.coord        = coord;
    ctx.seed         = chunk_seed(cfg_.seed, coord);
    ctx.world_seed   = cfg_.seed;
    ctx.zone         = ci.zone;
    // R129 -- captured before the M157 map-layer override below can
    // replace ctx.zone; see ChunkContext::authored_zone's own doc comment.
    ctx.authored_zone = ci.zone;
    ctx.region       = ci.region;
    ctx.style        = cfg_.style;
    ctx.chunk_size_m = static_cast<float>(cfg_.chunk_size_m);
    // R134 -- a road's geometry must use only canonical, symmetric physical
    // crossings. Parcels and other non-road regions instead need the old
    // directional "which side borders a road?" frontage relation.
    ctx.exits = is_road_region(ci.region) ? map_.road_connections(coord)
                                           : map_.road_frontage(coord);

    // R128 -- resolve THIS chunk's own landmark (if WorldConfig::landmarks
    // has an entry targeting it) down to the single LandmarkInstance
    // ChunkContext carries. First match wins; a config listing more than
    // one entry for the same chunk is not validated against here (v1 is
    // deliberately simple, one landmark per chunk).
    for (const auto& lp : cfg_.landmarks) {
        if (lp.chunk_x == x && lp.chunk_y == y) {
            ctx.landmark.definition_id = lp.definition_id;
            ctx.landmark.x             = lp.x;
            ctx.landmark.z             = lp.z;
            ctx.landmark.rotation_y    = lp.rotation_y;
            break;
        }
    }

    if (map_pipeline_) {
        populate_map_context(ctx, *map_pipeline_, cfg_.chunk_size_m);

        // A configured WorldMap and a populated PersistentWorldMap are the
        // authoritative city-layout source. The planetary map may still add
        // environmental context, but it must not replace their zone/region
        // (and thereby turn a validated road into a house or water chunk).
        // The map layer owns layout only for an otherwise untouched default
        // grid, which is the original map-to-chunk hand-off use case.
        if (ctx.map_context.available) {
            const bool map_owns_layout = cfg_.zones.empty() &&
                ci.zone == cfg_.zone_default && ci.region == cfg_.region_default;
            if (map_owns_layout) {
                ctx.zone = static_cast<ZoneType>(ctx.map_context.biome_ordinal);

                // Prefer the map layer's ZoneCandidate (M156) over the flat
                // WorldMap region once one was actually assigned. `none` (the
                // default when zone_candidates is empty for this tile, or this
                // cell wasn't part of a zoned block) means "no map-derived
                // region to prefer" — same fallback discipline as zone above.
                const auto candidate =
                    static_cast<Map::ZoneCandidate>(ctx.map_context.zone_candidate_ordinal);
                if (candidate != Map::ZoneCandidate::none)
                    ctx.region = region_from_zone_candidate(candidate);
            }

            // R134 -- coarse map crossings are intentionally context only;
            // they cannot be ORed into the exact chunk-edge graph without
            // inventing one-sided road geometry.
            populate_nearby_road_crossing(ctx, *map_pipeline_, cfg_.chunk_size_m);
        }
    }

    return ctx;
}

namespace {

// R108 -- fills the parts of ChunkDiagnostics that come from a
// ValidationResult + the generated content itself (everything except
// `source`/`fallback_reason`, which only the caller — mid-fallback-
// decision — knows). Shared by all three ChunkPipeline::get() outcomes
// (Lua success, C++ fallback, cache hit) so they report identically-
// shaped stats.
void fill_diagnostics_from_content(ChunkDiagnostics& diag, const ChunkContext& ctx,
                                    const std::string& content,
                                    const ValidationResult& vr) {
    diag.zone                   = ctx.zone;
    diag.region                 = ctx.region;
    diag.style                  = ctx.style;
    diag.lod                    = ctx.lod;
    diag.map_context_available  = ctx.map_context.available;
    diag.generator_id           = vr.generator_id;
    diag.object_count           = vr.object_count;
    diag.material_count         = vr.material_count;
    diag.materials_used         = vr.materials_used;
    diag.light_count            = vr.light_count;
    diag.validation_errors      = vr.errors;
    diag.validation_warnings    = vr.warnings;
    diag.triangle_count         = Mc3MeshBuilder{}.build(content).total_triangles();
}

} // namespace

std::string ChunkPipeline::get(int x, int y, bool force) {
    return get(x, y, nullptr, force);
}

std::string ChunkPipeline::get(int x, int y, ChunkDiagnostics* out_diag, bool force) {
    const ChunkCoord coord{x, y};

    if (!force) {
        if (auto cached = cache_.load(coord)) {
            // R108 -- a cache hit skips generation entirely, so the fallback
            // decision that (may have) happened is lost; still surface real,
            // re-derived stats from the cached content itself rather than
            // silently leaving *out_diag empty/stale. fallback_reason stays
            // empty here on purpose -- Source::Cache IS the honest signal
            // that "whatever happened, happened on an earlier call", not a
            // claim that no fallback ever occurred.
            if (out_diag) {
                const ChunkContext ctx = build_context(x, y);
                const auto vr = MC3Validator{}.validate(*cached, ctx.chunk_size_m);
                *out_diag = ChunkDiagnostics{};
                out_diag->source = ChunkDiagnostics::Source::Cache;
                fill_diagnostics_from_content(*out_diag, ctx, *cached, vr);
            }
            if (progress_callback_) progress_callback_(coord, ChunkDiagnostics::Source::Cache);
            return *cached;
        }
    }

    const ChunkContext ctx = build_context(x, y);

    std::string content;
    MC3Validator validator;
    const float  bounds = ctx.chunk_size_m;
    ValidationResult last_vr;

    auto validate_and_store = [&](const std::string& xml, const char* source) -> bool {
        auto vr = validator.validate(xml, bounds);
        last_vr = vr;
        if (!vr.ok) {
            std::cerr << "[MeshWorld] " << source << " produced invalid MC3:\n";
            for (const auto& e : vr.errors)
                std::cerr << "  ERROR: " << e << "\n";
            return false;
        }
        for (const auto& w : vr.warnings)
            std::cerr << "[MeshWorld] " << source << " WARNING: " << w << "\n";
        cache_.store(coord, xml);
        return true;
    };

    // R113 v1 (docs/world-composer-design.md §8) -- try the C++ world
    // composer first, before Lua. Opt-in (cfg_.use_world_composer,
    // default false) and additive: compose_chunk() returning nullopt (no
    // Parcel layout for this region yet, or no matching asset) falls
    // through to the existing Lua/C++ chain completely unchanged, exactly
    // as if the composer didn't exist. Composed content that fails
    // validation also falls through, rather than ever returning invalid
    // content -- same discipline the Lua path already applies to itself
    // just below.
    if (cfg_.use_world_composer) {
        if (auto composed = BuildingComposer{}.compose_chunk(ctx)) {
            if (validate_and_store(*composed, "Composer")) {
                if (out_diag) {
                    *out_diag = ChunkDiagnostics{};
                    out_diag->source = ChunkDiagnostics::Source::Composer;
                    fill_diagnostics_from_content(*out_diag, ctx, *composed, last_vr);
                }
                if (progress_callback_) progress_callback_(coord, ChunkDiagnostics::Source::Composer);
                return *composed;
            }
        }
    }

    // T133/T134 — Try Lua generator first, validate before caching
    const std::string lua_id = "lua.zone." + to_string(ctx.region);
    std::string fallback_reason;
    auto& registry = LuaGeneratorRegistry::instance();
    if (registry.has(lua_id)) {
        std::string lua_error;
        content = LuaSandbox{}.execute(registry.get(lua_id), ctx, &lua_error);
        if (!content.empty() && validate_and_store(content, ("Lua:" + lua_id).c_str())) {
            if (out_diag) {
                *out_diag = ChunkDiagnostics{};
                out_diag->source = ChunkDiagnostics::Source::Lua;
                fill_diagnostics_from_content(*out_diag, ctx, content, last_vr);
            }
            if (progress_callback_) progress_callback_(coord, ChunkDiagnostics::Source::Lua);
            return content;
        }
        if (!lua_error.empty()) {
            std::cerr << "[MeshWorld] Lua generator '" << lua_id
                      << "' failed: " << lua_error << "\n";
            fallback_reason = "Lua generator '" + lua_id + "' failed: " + lua_error;
        } else if (content.empty()) {
            fallback_reason = "Lua generator '" + lua_id + "' returned empty content";
        } else {
            fallback_reason = "Lua generator '" + lua_id + "' produced invalid MC3 (see validation_errors)";
        }
        std::cerr << "[MeshWorld] Falling back to C++ generator for " << lua_id << "\n";
    } else {
        fallback_reason = "no Lua generator registered for '" + lua_id + "'";
    }

    // C++ fallback — also validated
    content = get_generator(ctx.zone, ctx.region)->generate(ctx);
    validate_and_store(content, "C++");
    cache_.store(coord, content);

    if (out_diag) {
        *out_diag = ChunkDiagnostics{};
        out_diag->source          = ChunkDiagnostics::Source::CppFallback;
        out_diag->fallback_reason = fallback_reason;
        fill_diagnostics_from_content(*out_diag, ctx, content, last_vr);
    }
    if (progress_callback_) progress_callback_(coord, ChunkDiagnostics::Source::CppFallback);
    return content;
}

} // namespace MeshWorld
