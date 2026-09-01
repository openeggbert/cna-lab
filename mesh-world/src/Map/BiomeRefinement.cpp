// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/BiomeRefinement.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "ZoneType.hpp"

namespace MeshWorld::Map {

namespace {
// Local relief: max minus min elevation across a cell's own 3x3
// neighborhood (itself + up to 8 neighbors, clamped at grid edges).
// Shared by applySwampFlatnessCheck()/applyCanyonCarving()/
// applyCoastalReliefRefinement() -- all three need the same "how bumpy is
// the terrain right here" signal, just applied to different starting
// biomes/bands.
float local_relief_m(const FieldGrid& elevation, int gx, int gy, int W, int H) {
    float lo = elevation.at(gx, gy);
    float hi = lo;
    for (int dy = -1; dy <= 1; ++dy) {
        const int ny = gy + dy;
        if (ny < 0 || ny >= H) continue;
        for (int dx = -1; dx <= 1; ++dx) {
            const int nx = gx + dx;
            if (nx < 0 || nx >= W) continue;
            const float e = elevation.at(nx, ny);
            lo = std::min(lo, e);
            hi = std::max(hi, e);
        }
    }
    return hi - lo;
}

// Same as local_relief_m() above, but skips ocean-family neighbors
// entirely (checked against `biome`, not `elevation`) -- used only by
// applyCoastalReliefRefinement(). Without this, a coastal cell's "local
// relief" would always be dominated by the elevation drop into the
// adjacent seafloor (routinely tens of metres, regardless of how flat the
// LAND itself is), making tidal_flat vs. sea_cliff a proxy for "how deep
// is the water nearby" instead of the intended "how bumpy is the actual
// shoreline terrain". The cell itself is always land here (callers only
// invoke this on non-ocean-family cells), so it always contributes.
float local_land_relief_m(const FieldGrid& elevation, const BiomeGrid& biome,
                           int gx, int gy, int W, int H) {
    float lo = elevation.at(gx, gy);
    float hi = lo;
    for (int dy = -1; dy <= 1; ++dy) {
        const int ny = gy + dy;
        if (ny < 0 || ny >= H) continue;
        for (int dx = -1; dx <= 1; ++dx) {
            const int nx = gx + dx;
            if (nx < 0 || nx >= W) continue;
            const std::size_t nidx = static_cast<std::size_t>(ny) * static_cast<std::size_t>(W)
                                     + static_cast<std::size_t>(nx);
            if (MeshWorld::is_ocean_family(static_cast<MeshWorld::ZoneType>(biome.data[nidx])))
                continue;
            const float e = elevation.at(nx, ny);
            lo = std::min(lo, e);
            hi = std::max(hi, e);
        }
    }
    return hi - lo;
}
} // namespace

void BiomeRefinement::applyCoastalBeach(BiomeGrid& biome, const FieldGrid& elevation,
                                         double sea_level_m, int radius_cells,
                                         double max_beach_elevation_m) {
    if (biome.empty() || elevation.empty()) return;
    if (biome.w != elevation.w || biome.h != elevation.h) return;  // defensive; shouldn't happen
    if (radius_cells <= 0) return;

    const int W = biome.w;
    const int H = biome.h;
    const auto beach_ord = static_cast<std::uint8_t>(MeshWorld::ZoneType::beach);
    // M236-M275 (MAP16, 2026-07-10): "ocean" is now 6 distinct outcomes by
    // depth/temperature, not just ZoneType::ocean itself -- see
    // ZoneType.hpp's is_ocean_family(). A lagoon/kelp_forest/etc. cell must
    // be treated exactly like a plain ocean cell here (never itself
    // reclassified to beach, and its land neighbors still count as
    // coastal), or coastal beach detection silently breaks near any of the
    // 5 new underwater sub-types.
    const auto is_ocean_cell = [&biome](std::size_t i) {
        return MeshWorld::is_ocean_family(static_cast<MeshWorld::ZoneType>(biome.data[i]));
    };

    // Only ocean-family cells trigger a reclassification, and ocean-family
    // cells themselves are never mutated by this pass, so a single forward
    // pass is safe: a cell just turned into beach can never be mistaken for
    // ocean by a later cell's neighbor check, so there's no run-away
    // beach-spreads-into-beach cascade and no need to snapshot the grid
    // first.
    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            if (is_ocean_cell(idx)) continue;
            if (elevation.at(gx, gy) - sea_level_m > max_beach_elevation_m) continue;

            bool near_ocean = false;
            for (int dy = -radius_cells; dy <= radius_cells && !near_ocean; ++dy) {
                const int ny = gy + dy;
                if (ny < 0 || ny >= H) continue;
                for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = gx + dx;
                    if (nx < 0 || nx >= W) continue;
                    const std::size_t nidx = static_cast<std::size_t>(ny) * static_cast<std::size_t>(W)
                                             + static_cast<std::size_t>(nx);
                    if (is_ocean_cell(nidx)) {
                        near_ocean = true;
                        break;
                    }
                }
            }
            if (near_ocean) biome.data[idx] = beach_ord;
        }
    }
}

void BiomeRefinement::applySwampFlatnessCheck(BiomeGrid& biome, const FieldGrid& elevation,
                                               double max_local_relief_m) {
    if (biome.empty() || elevation.empty()) return;
    if (biome.w != elevation.w || biome.h != elevation.h) return;  // defensive; shouldn't happen

    const int W = biome.w;
    const int H = biome.h;
    const auto swamp_ord  = static_cast<std::uint8_t>(MeshWorld::ZoneType::swamp);
    const auto meadow_ord = static_cast<std::uint8_t>(MeshWorld::ZoneType::meadow);

    // Only demotes existing swamp cells and never touches anything else, so
    // (as in applyCoastalBeach()) a single forward pass is safe: a cell
    // just demoted to meadow can never be mistaken for swamp by a later
    // cell's neighbor scan.
    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            if (biome.data[idx] != swamp_ord) continue;

            if (static_cast<double>(local_relief_m(elevation, gx, gy, W, H)) > max_local_relief_m)
                biome.data[idx] = meadow_ord;
        }
    }
}

void BiomeRefinement::applyCanyonCarving(BiomeGrid& biome, const FieldGrid& elevation,
                                          double steep_relief_m) {
    if (biome.empty() || elevation.empty()) return;
    if (biome.w != elevation.w || biome.h != elevation.h) return;  // defensive; shouldn't happen

    const int W = biome.w;
    const int H = biome.h;
    const auto badlands_ord     = static_cast<std::uint8_t>(MeshWorld::ZoneType::badlands);
    const auto mesa_ord         = static_cast<std::uint8_t>(MeshWorld::ZoneType::mesa);
    const auto rocky_desert_ord = static_cast<std::uint8_t>(MeshWorld::ZoneType::rocky_desert);
    const auto canyon_ord       = static_cast<std::uint8_t>(MeshWorld::ZoneType::canyon);

    // Only reclassifies the dry, elevated rock cluster classify() already
    // produces (canyon's own enum position sits between mesa and oasis,
    // inside that same cluster); canyon is never itself one of those three
    // inputs, so a single forward pass is safe (same reasoning as
    // applySwampFlatnessCheck() above).
    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            const auto v = biome.data[idx];
            if (v != badlands_ord && v != mesa_ord && v != rocky_desert_ord) continue;

            if (static_cast<double>(local_relief_m(elevation, gx, gy, W, H)) >= steep_relief_m)
                biome.data[idx] = canyon_ord;
        }
    }
}

void BiomeRefinement::applyCoastalReliefRefinement(BiomeGrid& biome, const FieldGrid& elevation,
                                                     double sea_level_m, int radius_cells,
                                                     double max_coastal_elevation_m,
                                                     double flat_relief_m,
                                                     double steep_relief_m) {
    if (biome.empty() || elevation.empty()) return;
    if (biome.w != elevation.w || biome.h != elevation.h) return;  // defensive; shouldn't happen
    if (radius_cells <= 0) return;

    const int W = biome.w;
    const int H = biome.h;
    const auto tidal_flat_ord = static_cast<std::uint8_t>(MeshWorld::ZoneType::tidal_flat);
    const auto sea_cliff_ord  = static_cast<std::uint8_t>(MeshWorld::ZoneType::sea_cliff);
    // Same is_ocean_family() treatment applyCoastalBeach() uses above --
    // any of the 6 underwater sub-types counts as "ocean" for adjacency
    // purposes, and none of them is ever itself reclassified here.
    const auto is_ocean_cell = [&biome](std::size_t i) {
        return MeshWorld::is_ocean_family(static_cast<MeshWorld::ZoneType>(biome.data[i]));
    };

    // Reclassification here depends only on elevation/relief, never the
    // cell's own current biome value (unlike applyCoastalBeach()'s
    // ocean-adjacency check), so a single forward pass is safe regardless
    // of this pass's order relative to applyCoastalBeach() -- though the
    // natural pipeline position is right after it, so a freshly-set beach
    // cell can still be refined into tidal_flat/sea_cliff.
    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            if (is_ocean_cell(idx)) continue;

            bool near_ocean = false;
            for (int dy = -radius_cells; dy <= radius_cells && !near_ocean; ++dy) {
                const int ny = gy + dy;
                if (ny < 0 || ny >= H) continue;
                for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = gx + dx;
                    if (nx < 0 || nx >= W) continue;
                    const std::size_t nidx = static_cast<std::size_t>(ny) * static_cast<std::size_t>(W)
                                             + static_cast<std::size_t>(nx);
                    if (is_ocean_cell(nidx)) {
                        near_ocean = true;
                        break;
                    }
                }
            }
            if (!near_ocean) continue;

            const double relief =
                static_cast<double>(local_land_relief_m(elevation, biome, gx, gy, W, H));
            const double elev_above = static_cast<double>(elevation.at(gx, gy)) - sea_level_m;

            if (relief >= steep_relief_m || elev_above > max_coastal_elevation_m) {
                biome.data[idx] = sea_cliff_ord;
            } else if (relief < flat_relief_m) {
                biome.data[idx] = tidal_flat_ord;
            }
            // else: moderate relief within the coastal band -- ordinary
            // beach territory, left as whatever applyCoastalBeach() (or
            // classify()) already set.
        }
    }
}

void BiomeRefinement::applyRiparianForest(BiomeGrid& biome, const HydrologyNetwork& network,
                                           int grid_w, int grid_h,
                                           double world_x0, double world_z0,
                                           double world_x1, double world_z1,
                                           int radius_cells) {
    if (biome.empty()) return;
    if (biome.w != grid_w || biome.h != grid_h) return;  // defensive; shouldn't happen
    if (grid_w <= 0 || grid_h <= 0) return;
    if (world_x1 <= world_x0 || world_z1 <= world_z0) return;
    if (radius_cells <= 0) return;
    if (network.rivers.empty()) return;

    const int W = grid_w;
    const int H = grid_h;
    // Same cell-center convention Hydrology::trace() itself uses to place a
    // RiverPoint (world_x0 + (gx+0.5) * cell_size), so a cell's own river
    // distance is measured against the exact coordinate space the river was
    // traced in.
    const double cell_w = (world_x1 - world_x0) / W;
    const double cell_h = (world_z1 - world_z0) / H;
    const double max_dist_m  = radius_cells * 0.5 * (cell_w + cell_h);
    const double max_dist_sq = max_dist_m * max_dist_m;

    const auto savanna_ord   = static_cast<std::uint8_t>(MeshWorld::ZoneType::savanna);
    const auto steppe_ord    = static_cast<std::uint8_t>(MeshWorld::ZoneType::steppe);
    const auto prairie_ord   = static_cast<std::uint8_t>(MeshWorld::ZoneType::prairie);
    const auto chaparral_ord = static_cast<std::uint8_t>(MeshWorld::ZoneType::chaparral);
    const auto shrubland_ord = static_cast<std::uint8_t>(MeshWorld::ZoneType::shrubland);
    const auto riparian_ord  = static_cast<std::uint8_t>(MeshWorld::ZoneType::riparian_forest);
    const auto is_target = [&](std::uint8_t v) {
        return v == savanna_ord || v == steppe_ord || v == prairie_ord ||
               v == chaparral_ord || v == shrubland_ord;
    };

    // riparian_forest is never itself one of the 5 target inputs, so a
    // single forward pass is safe (same reasoning as applyCanyonCarving()
    // above) -- reclassifying one cell can never change whether a later
    // cell qualifies.
    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            if (!is_target(biome.data[idx])) continue;

            const double wx = world_x0 + (gx + 0.5) * cell_w;
            const double wz = world_z0 + (gy + 0.5) * cell_h;

            bool near_river = false;
            for (const auto& seg : network.rivers) {
                for (const auto& pt : seg.points) {
                    const double dx = pt.x - wx;
                    const double dz = pt.z - wz;
                    if (dx * dx + dz * dz <= max_dist_sq) {
                        near_river = true;
                        break;
                    }
                }
                if (near_river) break;
            }
            if (near_river) biome.data[idx] = riparian_ord;
        }
    }
}

void BiomeRefinement::applyVolcanicBiomes(BiomeGrid& biome, const VolcanicField& field,
                                           int grid_w, int grid_h,
                                           double world_x0, double world_z0,
                                           double world_x1, double world_z1,
                                           int coastal_radius_cells, double inner_fraction) {
    if (biome.empty()) return;
    if (biome.w != grid_w || biome.h != grid_h) return;  // defensive; shouldn't happen
    if (grid_w <= 0 || grid_h <= 0) return;
    if (world_x1 <= world_x0 || world_z1 <= world_z0) return;
    if (field.empty()) return;

    const int W = grid_w;
    const int H = grid_h;
    const double cell_w = (world_x1 - world_x0) / W;
    const double cell_h = (world_z1 - world_z0) / H;

    const auto volcanic_ord        = static_cast<std::uint8_t>(MeshWorld::ZoneType::volcanic);
    const auto geothermal_ord      = static_cast<std::uint8_t>(MeshWorld::ZoneType::geothermal);
    const auto ash_plain_ord       = static_cast<std::uint8_t>(MeshWorld::ZoneType::ash_plain);
    const auto volcanic_island_ord = static_cast<std::uint8_t>(MeshWorld::ZoneType::volcanic_island);
    const auto is_ocean_cell = [&biome](std::size_t i) {
        return MeshWorld::is_ocean_family(static_cast<MeshWorld::ZoneType>(biome.data[i]));
    };

    // Reclassification here depends only on distance-to-hotspot and the
    // cell's own coastal-adjacency, never the cell's own current biome, so
    // a single forward pass is safe (same reasoning applyCoastalReliefRefinement()
    // uses).
    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            if (is_ocean_cell(idx)) continue;

            const double wx = world_x0 + (gx + 0.5) * cell_w;
            const double wz = world_z0 + (gy + 0.5) * cell_h;

            // Nearest qualifying hotspot wins (same convention
            // Volcanism::sampleElevation() uses for elevation).
            const VolcanicHotspot* nearest = nullptr;
            double nearest_dist = std::numeric_limits<double>::max();
            for (const VolcanicHotspot& h : field.hotspots) {
                if (h.radius_m <= 0.0) continue;
                const double dx   = wx - h.x;
                const double dz   = wz - h.z;
                const double dist = std::sqrt(dx * dx + dz * dz);
                if (dist < h.radius_m && dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest      = &h;
                }
            }
            if (nearest == nullptr) continue;

            if (!nearest->active) {
                biome.data[idx] = ash_plain_ord;
                continue;
            }

            bool near_ocean = false;
            for (int dy = -coastal_radius_cells; dy <= coastal_radius_cells && !near_ocean; ++dy) {
                const int ny = gy + dy;
                if (ny < 0 || ny >= H) continue;
                for (int dx = -coastal_radius_cells; dx <= coastal_radius_cells; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = gx + dx;
                    if (nx < 0 || nx >= W) continue;
                    const std::size_t nidx = static_cast<std::size_t>(ny) * static_cast<std::size_t>(W)
                                             + static_cast<std::size_t>(nx);
                    if (is_ocean_cell(nidx)) {
                        near_ocean = true;
                        break;
                    }
                }
            }

            if (near_ocean) {
                biome.data[idx] = volcanic_island_ord;
            } else if (nearest_dist < nearest->radius_m * inner_fraction) {
                biome.data[idx] = volcanic_ord;
            } else {
                biome.data[idx] = geothermal_ord;
            }
        }
    }
}

} // namespace MeshWorld::Map
