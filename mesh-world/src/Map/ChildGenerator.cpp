// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/ChildGenerator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>

#include "Map/BiomeClassifier.hpp"
#include "Map/BiomeRefinement.hpp"
#include "Map/Coastline.hpp"
#include "Map/Countries.hpp"
#include "Map/FeatureNaming.hpp"
#include "Map/Hydrology.hpp"
#include "Map/MountainRanges.hpp"
#include "Map/Noise.hpp"
#include "Map/RoadNetwork.hpp"
#include "Map/Settlements.hpp"
#include "Map/Volcanism.hpp"
#include "Naming.hpp"
#include "ZoneType.hpp"

namespace MeshWorld::Map {

namespace {

static constexpr double PI = 3.14159265358979323846;

// M138/M143/M144 (MAP9) -- distinct hash2i axis for naming this tile's own
// trunk roads, same "every hash2i(...) caller needs a distinct axis" rule
// FeatureNaming.cpp (601-603)/Countries.cpp (801)/Settlements.cpp (802)
// already document, picked well clear of those.
constexpr std::int64_t kRoadNameAxis = 1001;

// Countries::grow() wiring (this session's follow-up to the Settlements/
// Roads wiring above) -- the one level real multiple-capital-visibility
// country growth runs at. Chosen deliberately: map.md's own level table
// (§5.3) leaves level 4 ("1,411 km", one level coarser than country.lua's
// own level-5 "Country / large region" scale) with no generator at all --
// Lua or C++ -- so this is both semantically reasonable (a tile big enough
// to plausibly hold several nations) and, like level 6's Roads/Settlements
// wiring, guaranteed to actually run in real usage (MapPipeline's Lua-first
// lookup means a level with its own registered script, like level 5, would
// make this dead code). Not every level -- capital placement recurring at
// every nested fallback level would be geographically nonsensical (a
// "capital" inside a capital inside a capital as you zoom in).
constexpr int kCountryRegionLevel = 4;

// Bilinear interpolation of a FieldGrid at fractional position (fx, fy).
// Clamps to grid bounds so callers need not guard against ±ε overflow.
float bilinear(const FieldGrid& g, double fx, double fy) {
    fx = std::max(0.0, std::min(static_cast<double>(g.w - 1), fx));
    fy = std::max(0.0, std::min(static_cast<double>(g.h - 1), fy));
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, g.w - 1);
    const int y1 = std::min(y0 + 1, g.h - 1);
    const double tx = fx - x0, ty = fy - y0;
    const double v00 = g.at(x0, y0), v10 = g.at(x1, y0);
    const double v01 = g.at(x0, y1), v11 = g.at(x1, y1);
    return static_cast<float>(v00 + (v10 - v00) * tx + (v01 - v00) * ty
                              + (v11 - v10 - v01 + v00) * tx * ty);
}

} // namespace

ChildGenerator::ChildGenerator(PlanetParams params) : params_(std::move(params)) {}

MapTilePayload ChildGenerator::generate(const TileCoord&      tile,
                                         const MapTilePayload* parent,
                                         std::uint64_t         entropy) const {
    assert(parent != nullptr && "ChildGenerator requires a parent payload");
    assert(tile.level > 0 && "ChildGenerator must not be called at level 0");
    assert(!parent->elevation.empty() && "parent elevation grid must be populated");

    const int W = parent->elevation.w;
    const int H = parent->elevation.h;

    // Which quadrant of the parent does this child occupy?
    const int cx = static_cast<int>(tile.x % 2LL);
    const int cy = static_cast<int>(tile.y % 2LL);

    // World-space extents of this child tile.
    const double child_size_m = params_.planet_size_m
                                / std::pow(2.0, static_cast<double>(tile.level));
    const double child_x0    = static_cast<double>(tile.x) * child_size_m;
    const double child_y0    = static_cast<double>(tile.y) * child_size_m;
    const double sea_m       = params_.sea_level_m;

    // fBm detail scale: ¼ of the child tile so features are visibly finer.
    const double terrain_scale = std::max(child_size_m / 4.0, 1.0);
    // Detail amplitude: ±250 m at the finest level, scaled by tile size.
    const double detail_amp    = std::min(child_size_m / 100.0, 500.0);

    FieldGrid elev; elev.w = W; elev.h = H;
    elev.data.resize(static_cast<std::size_t>(W * H));
    FieldGrid temp; temp.w = W; temp.h = H;
    temp.data.resize(static_cast<std::size_t>(W * H));
    FieldGrid moist; moist.w = W; moist.h = H;
    moist.data.resize(static_cast<std::size_t>(W * H));
    BiomeGrid bio;  bio.w  = W; bio.h  = H;
    bio.data.resize(static_cast<std::size_t>(W * H));

    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy * W + gx);

            // Map child cell → parent grid position.
            // 32/63 puts shared sibling boundaries exactly on parent columns 0, 32, 64.
            const double pgx_f = cx * 32.0 + gx * (32.0 / (W - 1));
            const double pgy_f = cy * 32.0 + gy * (32.0 / (H - 1));

            // Bilinear base from parent.
            const float base_elev = bilinear(parent->elevation, pgx_f, pgy_f);

            // sin²-fade: 0 at boundaries (gx/gy=0 or W/H-1), 1 at interior centre.
            // This makes sibling shared-boundary values identical (M112).
            const double fx    = std::sin(PI * gx / (W - 1));
            const double fy    = std::sin(PI * gy / (H - 1));
            const double fade  = fx * fx * fy * fy;

            // World coordinates for the child cell centre.
            const double wx = child_x0 + (gx + 0.5) * child_size_m / W;
            const double wy = child_y0 + (gy + 0.5) * child_size_m / H;

            // High-frequency fBm detail, faded to zero at boundaries.
            const float  detail_fbm = noise::fbm(wx / terrain_scale, wy / terrain_scale,
                                                  entropy + 2ULL);
            const double elevation  = base_elev + fade * (detail_fbm - 0.5) * detail_amp;
            elev.data[idx] = static_cast<float>(elevation);

            // Temperature: same formula as PlanetGenerator (M059).
            const double lat_factor = std::abs(wy / params_.planet_size_m - 0.5) * 2.0;
            const double base_temp  = params_.equator_temp_c
                                      + (params_.pole_temp_c - params_.equator_temp_c) * lat_factor;
            const double elev_above = std::max(elevation - sea_m, 0.0);
            const float  temperature = static_cast<float>(base_temp - 6.5 * elev_above / 1000.0);
            temp.data[idx] = temperature;

            // M060 — moisture: same broad-scale fBm treatment as PlanetGenerator,
            // recomputed from world position every level (not inherited from
            // parent, same as temperature above).
            const float moisture = std::clamp(
                noise::fbm(wx / terrain_scale, wy / terrain_scale, entropy + 3ULL), 0.0f, 1.0f);
            moist.data[idx] = moisture;

            // Biome (M061).
            const auto zone = BiomeClassifier::classify(elevation, temperature, moisture, sea_m);
            bio.data[idx]   = static_cast<std::uint8_t>(zone);
        }
    }

    // M126/M127 — seed tectonic ridge lines local to this tile and raise
    // elevation along them (interior cells only, same edge-protection rule
    // as Hydrology::carve() below). Applied before hydrology so rivers
    // trace across the now-mountainous terrain. Range count is
    // entropy-driven and small (0-2) -- most child tiles won't get a
    // range, matching real-world sparsity; decorrelated from the detail/
    // moisture entropy offsets above via a bit shift.
    const int mountain_range_count = static_cast<int>((entropy >> 8) % 3ULL);
    const MountainRangeNetwork mountain_net =
        MountainRanges::generate(entropy + 4ULL, mountain_range_count,
                                  child_x0, child_y0, child_x0 + child_size_m, child_y0 + child_size_m,
                                  /*min_peak_elevation_m=*/1500.0, /*max_peak_elevation_m=*/4000.0);
    MountainRanges::apply(elev, mountain_net, sea_m, /*falloff_width_m=*/terrain_scale,
                           child_x0, child_y0, child_x0 + child_size_m, child_y0 + child_size_m);

    // M265-268 (2026-07-11) — same "seed + uplift before hydrology" slot as
    // mountain ranges above, but far sparser: at most 1 hotspot per tile
    // (real volcanism is a much rarer, more localized phenomenon than a
    // whole mountain range), decorrelated via a fresh bit-shift axis.
    const int volcanic_hotspot_count = (static_cast<int>((entropy >> 24) % 5ULL) == 0) ? 1 : 0;
    const VolcanicField volcanic_field =
        Volcanism::generate(entropy + 8ULL, volcanic_hotspot_count,
                             child_x0, child_y0, child_x0 + child_size_m, child_y0 + child_size_m,
                             /*min_peak_elevation_m=*/1500.0, /*max_peak_elevation_m=*/4000.0);
    Volcanism::apply(elev, volcanic_field, sea_m,
                      child_x0, child_y0, child_x0 + child_size_m, child_y0 + child_size_m);

    // M125 — trace rivers on this tile's own elevation field and carve
    // valleys into it (interior cells only; never touches the edge rows/
    // columns the loop below copies into TileEdge, so this can't break
    // parent/child boundary matching, M108/M112/M117). Biome/temperature
    // above were already classified from the pre-carve, pre-uplift
    // elevation -- reclassifying near carved valleys or raised peaks
    // beyond M128's coastal pass below is MAP8's remaining biome-
    // refinement tasks (M129-M131), not this one or M126/M127.
    const HydrologyNetwork hydro_net = Hydrology::trace(
        elev, sea_m, child_x0, child_y0, child_x0 + child_size_m, child_y0 + child_size_m);
    Hydrology::carve(elev, hydro_net, sea_m,
                      child_x0, child_y0, child_x0 + child_size_m, child_y0 + child_size_m);

    // M128 — reclassify land within the coastal band to beach (see
    // PlanetGenerator.cpp's identical call for why this runs last/here).
    BiomeRefinement::applyCoastalBeach(bio, elev, sea_m);

    // M130 — demote any swamp cell on locally steep terrain to meadow.
    BiomeRefinement::applySwampFlatnessCheck(bio, elev);

    // M259/M274/M275 (2026-07-11) — canyon carving (dry, elevated rock cut
    // by local relief) + coastal relief refinement (tidal_flat/sea_cliff),
    // same "runs after the earlier passes so it can refine their output"
    // placement as applyCoastalBeach/applySwampFlatnessCheck above.
    BiomeRefinement::applyCanyonCarving(bio, elev);
    BiomeRefinement::applyCoastalReliefRefinement(bio, elev, sea_m);

    // M247 (2026-07-11) — riparian_forest: grassland/dry-climate cells near
    // the rivers just traced above become a forest corridor. Uses hydro_net
    // (already computed above) and the same world bounds Hydrology::trace()
    // itself was given.
    BiomeRefinement::applyRiparianForest(bio, hydro_net, W, H, child_x0, child_y0,
                                          child_x0 + child_size_m, child_y0 + child_size_m);

    // M265-268 (2026-07-11) — volcanic/geothermal/ash_plain/volcanic_island.
    // Runs LAST among the refinement passes (see BiomeRefinement::
    // applyVolcanicBiomes()'s own doc comment for why): a volcanic hotspot
    // is the most dramatic natural override in this whole group.
    BiomeRefinement::applyVolcanicBiomes(bio, volcanic_field, W, H, child_x0, child_y0,
                                          child_x0 + child_size_m, child_y0 + child_size_m);

    // M354 (MAP24, found 2026-07-10) — propagate an inherited "city" zone
    // from the parent's own already-classified biome grid. BiomeClassifier
    // has no "city" output (only city.lua's markUrbanCells(), M153, ever
    // writes it, as a raster override on top of classify()'s result), so
    // without this, any tile that falls through to this generic C++ path
    // one or more levels below city.lua's own (12) would silently lose that
    // override and re-derive a purely natural biome instead — a "City" zone
    // "evaporating" one level down. Nearest-neighbor, not bilinear: biome is
    // categorical, not a continuous field. Runs last, after both refinement
    // passes above, so an inherited city cell always wins — same precedence
    // city.lua's own markUrbanCells() (its final call) already has.
    if (!parent->biome.empty()) {
        const auto city_ord = static_cast<std::uint8_t>(ZoneType::city);
        for (int gy = 0; gy < H; ++gy) {
            for (int gx = 0; gx < W; ++gx) {
                const double pgx_f = cx * 32.0 + gx * (32.0 / (W - 1));
                const double pgy_f = cy * 32.0 + gy * (32.0 / (H - 1));
                const int pgx = std::clamp(static_cast<int>(std::lround(pgx_f)), 0, parent->biome.w - 1);
                const int pgy = std::clamp(static_cast<int>(std::lround(pgy_f)), 0, parent->biome.h - 1);
                if (parent->biome.at(pgx, pgy) == city_ord) {
                    bio.data[static_cast<std::size_t>(gy * W + gx)] = city_ord;
                }
            }
        }
    }

    MapTilePayload p;
    p.tile       = tile;
    p.entropy    = entropy;
    p.generator  = "child";
    // M132 — inherit the parent's culture so naming stays consistent within
    // one continent/region rather than re-rolling per tile; PlanetGenerator
    // is the only place a culture actually gets chosen (M132's addition
    // there). Falls back to a fresh derivation if the parent somehow has
    // none (shouldn't happen once level 0 always sets it, but a level-0
    // payload predating this change could still be loaded from a save).
    p.culture    = parent->culture.empty() ? MeshWorld::Naming::culture(entropy) : parent->culture;
    p.elevation  = std::move(elev);
    p.temperature = std::move(temp);
    p.moisture   = std::move(moist);
    p.biome      = std::move(bio);

    // M132 — name the traced rivers/lakes/ranges and add them as
    // MapFeature entries (see PlanetGenerator.cpp's identical call).
    FeatureNaming::appendHydrologyFeatures(p.features, hydro_net, p.culture, entropy);
    FeatureNaming::appendMountainRangeFeatures(p.features, mountain_net, p.elevation, sea_m,
                                                p.culture, entropy, child_x0, child_y0,
                                                child_x0 + child_size_m, child_y0 + child_size_m);

    // M342 (MAP22) — trace the ocean/land boundary and append it as real
    // FeatureType::Coastline entries. This enum value has existed (with its
    // own feature_rgb_color() entry in PlanetMapLogic.cpp) since MAP8/MAP9,
    // but nothing ever constructed one — the same "exists in name only" gap
    // ZoneType::cave had before MAP21. Empty on most tiles (uniformly ocean
    // or uniformly land, Coastline::trace()'s own cheap early-out), so this
    // is not meaningful bloat on the common case. Purely additive.
    for (auto& points : Coastline::trace(p.elevation, sea_m, child_x0, child_y0,
                                          child_x0 + child_size_m, child_y0 + child_size_m)) {
        MapFeature f;
        f.type   = FeatureType::Coastline;
        f.points = std::move(points);
        p.features.push_back(std::move(f));
    }

    // §5 candidate (MAP9 wiring) — place a sparse, tile-local set of
    // cities/towns and connect them with a real Kruskal-MST/routed road
    // network (Settlements::place()/Roads::build(), tested standalone since
    // MAP9 but never called from a real generator until now). This only
    // ever runs where the C++ fallback actually generates real output, i.e.
    // a level with no registered Lua script (MapPipeline's Lua-first lookup
    // means this is a no-op in practice at every level a Lua script already
    // covers -- see NEXT.md). Counts are small and entropy-driven, same
    // "most tiles get none" sparsity MountainRanges' range_count above
    // already established; distinct bit shifts keep this decorrelated from
    // that count.
    //
    // Capitals are the one exception: 0 everywhere except
    // kCountryRegionLevel (see that constant's own doc comment for why),
    // where 2-4 capitals are requested so Countries::grow() below actually
    // has multiple capitals to grow contested borders between -- the real
    // gap that made Countries::grow()/name() untestable-in-practice despite
    // being fully implemented and unit-tested since MAP9.
    const int settlement_capital_count =
        (tile.level == kCountryRegionLevel) ? static_cast<int>(2 + (entropy >> 12) % 3ULL) : 0;
    const int settlement_city_count = static_cast<int>((entropy >> 16) % 2ULL);
    const int settlement_town_count = static_cast<int>((entropy >> 20) % 3ULL);
    SettlementNetwork settlement_net = Settlements::place(
        entropy + 5ULL, p.elevation, sea_m, child_x0, child_y0,
        child_x0 + child_size_m, child_y0 + child_size_m,
        settlement_capital_count, settlement_city_count, settlement_town_count,
        /*min_spacing_m=*/child_size_m / 8.0);
    Settlements::name(settlement_net, p.culture, entropy + 6ULL);
    Settlements::appendLabels(p.labels, settlement_net);

    // Countries::grow()/name() — real multi-capital contested-border growth,
    // wired in at kCountryRegionLevel only (see that constant's own doc
    // comment). Safe to call at every level: with 0 requested capitals
    // elsewhere, settlement_net has none, and grow()'s own guard clause
    // ("no capitals -> empty network") makes this a no-op without needing
    // an extra level check here too. hydro_net/mountain_net (already traced
    // above) bias borders toward natural features, same M140 behavior
    // Countries::grow()'s own standalone tests already exercise.
    const CountryNetwork country_net = Countries::grow(
        settlement_net, p.elevation, sea_m, child_x0, child_y0,
        child_x0 + child_size_m, child_y0 + child_size_m, &hydro_net, &mountain_net);
    if (!country_net.empty()) {
        CountryNetwork named_countries = country_net;
        Countries::name(named_countries, entropy + 7ULL);
        for (auto& country : named_countries.countries) {
            if (country.border.empty()) continue;
            MapFeature f;
            f.type   = FeatureType::Border;
            f.name   = country.name;
            f.points = std::move(country.border);
            p.features.push_back(std::move(f));
        }
    }

    // M143/M144 — connect this tile's own settlements (Town tier or above,
    // i.e. every settlement just placed) with a real routed road network:
    // slope-aware, river-bridging (M144), not the naive straight-line
    // hub-and-spoke country.lua's V1 uses for its own, differently-scoped
    // (cross-tile) capital+towns case. Safe on <2 settlements: Roads::build()
    // returns an empty edge list (its own documented guard clause).
    const RoadNetwork road_net = Roads::build(
        settlement_net, /*max_tier=*/SettlementTier::Town, /*extra_redundant_links=*/1,
        &p.elevation, child_x0, child_y0, child_x0 + child_size_m, child_y0 + child_size_m,
        &hydro_net);
    for (std::size_t i = 0; i < road_net.edges.size(); ++i) {
        const std::uint64_t seed =
            noise::hash2i(static_cast<std::int64_t>(i), kRoadNameAxis, entropy);
        MapFeature f;
        f.type   = FeatureType::Road;
        f.name   = MeshWorld::Naming::street(p.culture, seed);
        f.points = road_net.edges[i].path;
        p.features.push_back(std::move(f));
    }

    // Edge descriptors: copy boundary rows/cols of the elevation grid (same as M066).
    auto& en = p.edges[0]; en.elevation.resize(static_cast<std::size_t>(W));
    auto& ee = p.edges[1]; ee.elevation.resize(static_cast<std::size_t>(H));
    auto& es = p.edges[2]; es.elevation.resize(static_cast<std::size_t>(W));
    auto& ew = p.edges[3]; ew.elevation.resize(static_cast<std::size_t>(H));
    for (int i = 0; i < W; ++i) {
        en.elevation[static_cast<std::size_t>(i)] = p.elevation.at(i, 0);
        es.elevation[static_cast<std::size_t>(i)] = p.elevation.at(i, H - 1);
    }
    for (int i = 0; i < H; ++i) {
        ee.elevation[static_cast<std::size_t>(i)] = p.elevation.at(W - 1, i);
        ew.elevation[static_cast<std::size_t>(i)] = p.elevation.at(0,     i);
    }

    // M345 (MAP22) -- exports River/Road EdgeCrossings onto this tile's own
    // edges. Both wire up a function that existed, unit-tested standalone,
    // but was never called from a real generator: Roads::exportCrossings()
    // since M145 (its own doc comment says so explicitly), and
    // Hydrology::exportCrossings() (new this same task, alongside the fix
    // to Hydrology::trace() that stops a river reaching this tile's own
    // edge from being wrongly dammed into a spurious lake -- see
    // RiverSegment::exits_tile). Purely additive/order-independent (both
    // only ever append to edges[i].crossings, never clear it first).
    Hydrology::exportCrossings(hydro_net, W, H, child_x0, child_y0,
                                child_x0 + child_size_m, child_y0 + child_size_m, p.edges);
    Roads::exportCrossings(road_net, child_x0, child_y0,
                            child_x0 + child_size_m, child_y0 + child_size_m, p.edges);

    return p;
}

} // namespace MeshWorld::Map
