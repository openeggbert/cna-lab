// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <vector>

#include "ChunkGenerator.hpp"
#include "RegionType.hpp"

namespace MeshWorld {

// R113 (docs/world-composer-design.md §4) -- a placement region for one
// building. Deliberately reuses RegionType (not Map::ZoneCandidate) for
// `kind`, keeping this root-level type decoupled from Map:: headers, the
// same convention ChunkContext::MapContext already established -- and
// RegionType already shares ZoneCandidate's exact naming for the
// zone-block kinds (small_house_block/apartment_block/shop_street/park/
// square) by design (see Map/ZoneCandidate.hpp's own comment).
struct Parcel {
    // World-AXIS footprint extents (X and Z respectively) -- used for
    // straightforward axis-aligned bounds checking regardless of
    // orientation. A 90-degree yaw (east/west rows) swaps which of the
    // building's own local dimensions maps to world X vs Z, so `width`/
    // `depth` are swapped for those rows relative to a north/south one;
    // `frontage_extent` below is what stays orientation-INDEPENDENT.
    float center_x{0.0f}, center_z{0.0f};   // chunk-local, meters
    float width{0.0f}, depth{0.0f};         // world X/Z footprint, meters
    float rotation_y{0.0f};                 // degrees, orients the building's front

    // R113 v3 -- a real reference point on the street-facing edge
    // (street_x, street_z), replacing v2's own street_edge_z-only field
    // (a bare Z value, implicitly assuming every row's street ran along
    // Z -- true for north/south rows, meaningless for east/west ones).
    // (normal_x, normal_z) is the unit vector from (center_x, center_z)
    // toward (street_x, street_z) -- i.e. which way this parcel's own
    // building faces. BuildingComposer derives a perpendicular "right"
    // vector from it (right = (-normal_z, normal_x)) to place street
    // furniture on either side of the frontage, generically for any of
    // the 4 orientations rather than v1/v2's own X-only/Z-only offsets.
    float street_x{0.0f}, street_z{0.0f};
    float normal_x{0.0f}, normal_z{0.0f};

    // The building's own frontage width (ALONG the row, i.e. along
    // whichever axis parcels in this row are spaced on) -- unlike
    // width/depth above, this does NOT swap with orientation, since it's
    // always the same real dimension (the spacing pitch every row uses).
    // BuildingComposer uses this (not width/depth) to offset street
    // furniture a consistent real distance from the building regardless
    // of which side the row faces.
    float frontage_extent{0.0f};

    RegionType kind{RegionType::empty};
};

// R113 v3 -- a real, exits-aware street-first parcel algorithm: emits one
// row of parcels for EACH side of the chunk that a real road actually
// borders (per ctx.exits, WorldMap.cpp -- R113 v3 also fixed exits to be
// computed for every region type, not just road/crossroad chunks
// themselves), rather than v1/v2's own "always assume north+south are
// streets" convention, which drew fictional streets even where nothing
// real bordered the chunk and had no way to face a real road on the
// east/west side. Returns EMPTY if no side has a real adjacent road (the
// caller, BuildingComposer, must treat empty as "nothing to compose
// here, fall through to the existing generator chain" -- not an error;
// this is the intended, honest behavior for a landlocked block with no
// street reference, not a bug).
//
// A full road-network-aware block subdivision (blocks bounded on all
// sides by real streets, non-rectangular blocks, mesh_world_revival.md
// §11's full pipeline) remains later scope -- this still places one
// independent row per bordering side, not a single unified block layout.
// Produces parcels for RegionType::small_house_block, (R126)
// RegionType::apartment_block, and (R127) RegionType::shop_street, each
// with its own size class; returns empty for every other region.
//
// R113 (corner-aware layout) -- each row's own usable placement range IS
// now clipped so it stops short of an ACTIVE PERPENDICULAR row's own real
// footprint (plus a small gap) near a shared corner, instead of always
// spanning the full chunk minus a fixed side margin -- eliminating a
// real, 100%-reproducible overlap (verified across 200 seeds per
// combination) that existed whenever two ADJACENT sides (e.g. north+east)
// both bordered a road. Opposite pairs (north+south, east+west) never
// overlapped and are unaffected. A row can legitimately come out with
// ZERO parcels now (not clamped to a minimum of 1) if the reservation
// leaves no usable room at all -- an honest "no room here" for a small
// chunk with several active adjacent sides, not a bug.
//
// R113 (size-aware matching) -- each row independently, deterministically
// rolls a WIDTH CLASS (standard ~10m vs wide ~15.6m, matching
// house.rowhouse.modular_01's real footprint) before its column count is
// computed, so BuildingComposer can filter house candidates per-parcel by
// Parcel::frontage_extent instead of using one uniform pool for the whole
// chunk. A row that rolls wide and has no real style/size-compatible wide
// house registered (e.g. under the shipped central_europe_default profile,
// which requires a "gable_roof" style tag the flat-roofed rowhouse doesn't
// carry) still produces its Parcel -- BuildingComposer skips only the
// house instance for that one parcel (street furniture/vehicle still
// place normally), not the whole chunk.
std::vector<Parcel> derive_parcels(const ChunkContext& ctx);

} // namespace MeshWorld
