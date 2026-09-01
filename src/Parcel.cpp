// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Parcel.hpp"

#include <algorithm>
#include <cmath>

#include "Map/Noise.hpp"

namespace MeshWorld {

namespace {

// R113 v2/v3 -- distinct axis constant for this file's own deterministic
// per-row column-count roll, same "one axis constant per kind of
// deterministic roll" convention BuildingComposer.cpp/CityGenerator.cpp/
// DistrictGenerator.cpp already established (R105). The 4 directions
// each get their own first hash argument (0/1/2/3), same north=0/south=1
// values v2 already used, so an existing north+south chunk's column
// counts are completely unaffected by v3's new east/west support.
constexpr int kParcelColumnRollAxis = 4101;

// R113 (size-aware matching) -- a second, independent axis constant for
// each row's own width-CLASS roll (standard vs wide), kept separate from
// kParcelColumnRollAxis so picking a width class doesn't perturb the
// already-shipped column-count roll for existing (standard-only) chunks.
constexpr int kRowWidthClassAxis = 4102;

// Real footprints of the houses actually registered today: standard
// (house_gable_default / house.gable.modular_01, both 10.6m wide) and
// wide (house.rowhouse.modular_01, 15.6m -- see build_mc3lib_content.cpp
// for both). kParcelDepth is shared by both width classes: both houses'
// own real depth (8.6m, w/wt included) is close enough to this same
// already-shipped 8.0m estimate that a separate wide-depth constant
// isn't warranted (matches this file's own pre-existing "estimate, not
// exact" precedent for depth). A genuinely more precise, registry-driven
// footprint query (rather than these 2 hand-picked constants) is later
// scope, once more than 2 real sizes exist.
constexpr float kStandardParcelWidth = 10.0f;
constexpr float kWideParcelWidth     = 15.6f;
constexpr float kParcelDepth         = 8.0f;

// R126 (BuildingComposer v2, apartment_block) -- a single, wider/deeper
// size class matching ObjectDefinitionLibrary.cpp's own
// make_apartment_block() (nominalSize[0]=14.6m, footprint depth 10.6m
// plus a real yard allowance) -- unlike the house's standard/wide roll,
// apartment rows don't vary width class (v1 has only one apartment
// asset), so this is used directly, not through width_for()'s roll.
constexpr float kApartmentParcelWidth = 14.6f;
constexpr float kApartmentParcelDepth = 12.0f;

// R127 (BuildingComposer v2, shop_street) -- a single, NARROW size class
// matching ObjectDefinitionLibrary.cpp's own make_shop_building()
// (nominalSize[0]=8.6m); depth close to its real 9.6m footprint, same
// "close estimate, not exact" convention kParcelDepth's own doc comment
// already established. v1 has only one shop asset, so no width-class
// roll, same precedent as the apartment's single size class above.
constexpr float kShopParcelWidth = 8.6f;
constexpr float kShopParcelDepth = 9.0f;

// Layout constants. kSideMargin only bounds the MAXIMUM column count the
// chunk's real width can support (the row is then centered, which in
// practice leaves more clearance than this on every side, not a hard
// placement constraint itself); kParcelGap is the yard between adjacent
// houses along the street -- both are real object-scale meters, so they
// do NOT scale with chunk_size_m (a house needs the same real yard
// regardless of chunk size).
constexpr float kSideMargin = 1.0f;
constexpr float kParcelGap  = 1.0f;

// Row/street-edge position FRACTIONS of chunk_size_m (not absolute meter
// offsets -- v1's own literal z values only worked for exactly a 64m
// chunk, see R113 v2's own history). North/south are exactly v1's own
// values / 64, so a real 64m chunk reproduces v1/v2's identical layout.
// East/west (R113 v3, new) reuse the SAME fractions applied to the OTHER
// axis: west mirrors north's "close to the zero edge" structure, east
// mirrors south's "close to the s edge" structure -- a consistent,
// symmetric choice, since no prior east/west convention existed to match.
constexpr float kNorthStreetFraction = 2.0f  / 64.0f;
constexpr float kNorthRowFraction    = 13.0f / 64.0f;
constexpr float kSouthStreetFraction = 62.0f / 64.0f;
constexpr float kSouthRowFraction    = 51.0f / 64.0f;
constexpr float kWestStreetFraction  = kNorthStreetFraction;
constexpr float kWestRowFraction     = kNorthRowFraction;
constexpr float kEastStreetFraction  = kSouthStreetFraction;
constexpr float kEastRowFraction     = kSouthRowFraction;

// Rotation (degrees) for each orientation, derived so a definition whose
// own front (Mc3AssetMetadata.facing) is authored as "+Z" faces AWAY from
// (center_x, center_z) and TOWARD (street_x, street_z): normal =
// (-sin(rotation), -cos(rotation)) in (x, z). North/south reuse v1/v2's
// own already-shipped 0/180 (never independently visually verified in
// this sandboxed, no-GPU environment -- a known, pre-existing limitation,
// not new); east/west (270/90) are chosen by the SAME self-consistent
// derivation, equally unverified visually.
constexpr float kNorthRotationDeg = 0.0f;
constexpr float kSouthRotationDeg = 180.0f;
constexpr float kEastRotationDeg  = 270.0f;
constexpr float kWestRotationDeg  = 90.0f;
constexpr float kPi = 3.14159265358979323846f;

// Given a row's own real USABLE placement range (already clipped for any
// active perpendicular corner reservation -- see derive_parcels()'s own
// R113 corner-aware layout comment) and a candidate parcel width, the
// maximum number of parcels of that width that fit along the row. Zero
// (not clamped to a minimum of 1) is a real, honest answer when the
// reservation leaves no usable room at all -- a small chunk with every
// side bordering a road can legitimately produce a row with no parcels
// rather than one that wrongly overlaps a perpendicular neighbor.
int max_columns_for(float usable_width, float parcel_width) {
    if (usable_width <= 0.0f) return 0;
    const float column_pitch = parcel_width + kParcelGap;
    int columns = static_cast<int>(std::floor((usable_width + kParcelGap) / column_pitch));
    return std::max(columns, 0);
}

// Places `count` parcels of `parcel_width` evenly along one street-facing
// row, centered within [usable_start, usable_end] (already clipped for any
// active perpendicular corner reservation, NOT always the full chunk
// span). `along_x` selects whether the row runs along X (north/south rows
// -- parcels spaced along X, facing +/-Z) or along Z (east/west rows --
// parcels spaced along Z, facing +/-X). `row_pos`/`street_pos` are both
// expressed along the row's OWN perpendicular axis (Z for a north/south
// row, X for an east/west row). A 90-degree yaw swaps which world axis
// carries the building's own local width vs depth, so an east/west row's
// Parcel::width/depth (world X/Z footprint extents) are swapped relative
// to a north/south row's, even though the row's own along-axis spacing
// pitch (parcel_width + gap) stays the same either way -- a building's
// ORIGINAL width becomes its world Z-extent post-rotation, which is
// exactly the dimension that matters for spacing parcels along Z.
// R126 -- `parcel_depth`/`kind` are now parameters (were the hardcoded
// kParcelDepth/RegionType::small_house_block) so this same row-placement
// algorithm serves apartment_block rows too, with its own depth/kind,
// without duplicating the function. Every existing small_house_block call
// site passes kParcelDepth/RegionType::small_house_block explicitly, so
// house-row output is bit-for-bit unchanged.
void place_row(int count, float parcel_width, float usable_start, float usable_end,
               float row_pos, float street_pos, bool along_x, float rotation_deg,
               float parcel_depth, RegionType kind,
               std::vector<Parcel>& out) {
    if (count <= 0) return;

    const float column_pitch = parcel_width + kParcelGap;
    const float total_span   = static_cast<float>(count) * parcel_width +
                                static_cast<float>(count - 1) * kParcelGap;
    const float usable_width  = usable_end - usable_start;
    const float start         = usable_start + (usable_width - total_span) / 2.0f + parcel_width / 2.0f;

    const float rotation_rad = rotation_deg * kPi / 180.0f;
    const float normal_x     = -std::sin(rotation_rad);
    const float normal_z     = -std::cos(rotation_rad);

    for (int i = 0; i < count; ++i) {
        const float along = start + static_cast<float>(i) * column_pitch;
        Parcel p;
        p.rotation_y      = rotation_deg;
        p.normal_x        = normal_x;
        p.normal_z        = normal_z;
        p.frontage_extent = parcel_width;
        p.kind            = kind;
        if (along_x) {
            p.center_x = along;
            p.center_z = row_pos;
            p.width    = parcel_width;
            p.depth    = parcel_depth;
            p.street_x = along;
            p.street_z = street_pos;
        } else {
            p.center_x = row_pos;
            p.center_z = along;
            p.width    = parcel_depth;
            p.depth    = parcel_width;
            p.street_x = street_pos;
            p.street_z = along;
        }
        out.push_back(p);
    }
}

} // namespace

std::vector<Parcel> derive_parcels(const ChunkContext& ctx) {
    // R126/R127 -- apartment_block and shop_street reuse the exact same
    // street-first row algorithm as small_house_block (corner-aware
    // clipping, per-side exits.*_road gating, deterministic column-count
    // variety), just with their own single size class
    // (kApartmentParcelWidth/Depth, kShopParcelWidth/Depth) instead of the
    // house's standard/wide roll -- see width_for()/row_depth below.
    // Every other region still returns empty, unchanged.
    if (ctx.region != RegionType::small_house_block &&
        ctx.region != RegionType::apartment_block &&
        ctx.region != RegionType::shop_street) {
        return {};
    }

    const bool  is_apartment = (ctx.region == RegionType::apartment_block);
    const bool  is_shop      = (ctx.region == RegionType::shop_street);
    const float row_depth    = is_apartment ? kApartmentParcelDepth
                              : is_shop      ? kShopParcelDepth
                                             : kParcelDepth;
    const float s = ctx.chunk_size_m;

    // R113 (size-aware matching) -- each row independently, deterministically
    // picks between the standard (10m) and wide (15.6m, matching
    // house.rowhouse.modular_01's real footprint) parcel width, same
    // "seeds drive variety" rule as the column-count roll below. A row's
    // own width class is decided BEFORE its column count, since the
    // maximum column count itself depends on which width is being
    // packed. Roughly 1 in 4 rows come out wide.
    // R126/R127 -- apartment_block/shop_street rows always use their own
    // single width (v1 has only one apartment asset and one shop asset,
    // so there's no size class to roll for either).
    auto width_for = [&](std::int64_t axis) {
        if (is_apartment) return kApartmentParcelWidth;
        if (is_shop)      return kShopParcelWidth;
        const auto roll = Map::noise::hash2i(axis, kRowWidthClassAxis, ctx.seed) % 4;
        return (roll == 0) ? kWideParcelWidth : kStandardParcelWidth;
    };

    // Deterministic per-chunk variety (R100's own "seeds drive variety,
    // never a compatibility guarantee" rule): each row independently may
    // have one fewer column than the space allows, so not every
    // small_house_block chunk looks identical. Zero (not clamped to a
    // minimum of 1) is a legitimate outcome now that `usable_width` can
    // itself be a real corner-constrained range, not just the full chunk.
    auto columns_for = [&](std::int64_t axis, float parcel_width, float usable_width) {
        const int max_columns = max_columns_for(usable_width, parcel_width);
        if (max_columns <= 0) return 0;
        const auto roll = Map::noise::hash2i(axis, kParcelColumnRollAxis, ctx.seed) % 2;
        return std::max(max_columns - static_cast<int>(roll), 0);
    };

    // Each row's own perpendicular-axis position (Z for north/south, X
    // for east/west) -- computed unconditionally since the corner
    // reservation below needs a side's position even when placing the
    // OTHER, perpendicular row (e.g. the north row's own usable range
    // needs to know where an active east row sits, not just whether the
    // north row itself is active).
    const float north_row_pos = s * kNorthRowFraction;
    const float south_row_pos = s * kSouthRowFraction;
    const float east_row_pos  = s * kEastRowFraction;
    const float west_row_pos  = s * kWestRowFraction;

    // R113 (corner-aware layout) -- north/south rows (spaced along X) and
    // east/west rows (spaced along Z) used to always span the full chunk
    // width minus a fixed side margin, computed completely independently
    // of one another. When two ADJACENT sides (e.g. north+east) both
    // border a real road, that independence is wrong: verified by direct
    // computation that every adjacent-side combination overlaps near the
    // shared corner for 100% of 200 sampled seeds (opposite pairs,
    // north+south or east+west, never overlap -- they sit on opposite
    // ends of the chunk). Each row's own usable placement range is now
    // clipped to stop short of an ACTIVE perpendicular row's own real
    // footprint (row_depth) plus a small gap, instead of always
    // reaching the fixed side margin -- eliminating the overlap by
    // construction rather than detecting/fixing it after the fact. A
    // side with no active perpendicular neighbor keeps exactly its old
    // [kSideMargin, s - kSideMargin] range (bit-for-bit unchanged
    // behavior for every single-direction or opposite-pair chunk, which
    // is every chunk this project's own `examples/world.json` currently
    // produces except the 4 corner cells where two adjacent ring roads
    // meet). R126 -- `row_depth` replaces the hardcoded kParcelDepth so
    // apartment_block's own, deeper footprint clips corners correctly too;
    // small_house_block passes kParcelDepth via row_depth unchanged.
    const float ns_start = ctx.exits.west_road
        ? (west_row_pos + row_depth / 2.0f + kParcelGap) : kSideMargin;
    const float ns_end = ctx.exits.east_road
        ? (east_row_pos - row_depth / 2.0f - kParcelGap) : (s - kSideMargin);
    const float ew_start = ctx.exits.north_road
        ? (north_row_pos + row_depth / 2.0f + kParcelGap) : kSideMargin;
    const float ew_end = ctx.exits.south_road
        ? (south_row_pos - row_depth / 2.0f - kParcelGap) : (s - kSideMargin);

    // R113 v3 -- one row per side a real road ACTUALLY borders (per
    // ctx.exits, computed by WorldMap.cpp for every region type, not
    // just road/crossroad chunks themselves -- see that file's own R113
    // v3 comment). No sides bordering a road means no parcels at all:
    // an honest "nothing to compose here" (BuildingComposer falls
    // through to the existing generator chain), not a fabricated street.
    std::vector<Parcel> parcels;
    if (ctx.exits.north_road) {
        const float w = width_for(0);
        place_row(columns_for(0, w, ns_end - ns_start), w, ns_start, ns_end,
                  north_row_pos, s * kNorthStreetFraction,
                  /*along_x=*/true, kNorthRotationDeg, row_depth, ctx.region, parcels);
    }
    if (ctx.exits.south_road) {
        const float w = width_for(1);
        place_row(columns_for(1, w, ns_end - ns_start), w, ns_start, ns_end,
                  south_row_pos, s * kSouthStreetFraction,
                  /*along_x=*/true, kSouthRotationDeg, row_depth, ctx.region, parcels);
    }
    if (ctx.exits.east_road) {
        const float w = width_for(2);
        place_row(columns_for(2, w, ew_end - ew_start), w, ew_start, ew_end,
                  east_row_pos, s * kEastStreetFraction,
                  /*along_x=*/false, kEastRotationDeg, row_depth, ctx.region, parcels);
    }
    if (ctx.exits.west_road) {
        const float w = width_for(3);
        place_row(columns_for(3, w, ew_end - ew_start), w, ew_start, ew_end,
                  west_row_pos, s * kWestStreetFraction,
                  /*along_x=*/false, kWestRotationDeg, row_depth, ctx.region, parcels);
    }

    return parcels;
}

} // namespace MeshWorld
