// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/Volcanism.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "Map/Noise.hpp"

namespace MeshWorld::Map {

namespace {
// A hotspot's radius is a fraction of the tile's own smaller dimension,
// same "scale with tile size" reasoning MountainRanges::generate()'s own
// step_m uses -- a real volcano is a much smaller, more localized feature
// than a mountain range, hence the narrower fraction (1/16 to 1/6 vs.
// ranges spanning up to 1/20 of a tile PER STEP over 12 steps).
constexpr double kMinRadiusFrac = 1.0 / 16.0;
constexpr double kMaxRadiusFrac = 1.0 / 6.0;
// Real volcanic terrain is mostly dormant/extinct at any given snapshot;
// only a minority is presently active.
constexpr double kActiveProbability = 0.3;
} // namespace

VolcanicField Volcanism::generate(std::uint64_t entropy, int count,
                                   double world_x0, double world_z0,
                                   double world_x1, double world_z1,
                                   double min_peak_elevation_m,
                                   double max_peak_elevation_m) {
    VolcanicField field;
    if (count <= 0) return field;

    const double width  = world_x1 - world_x0;
    const double height = world_z1 - world_z0;
    if (width <= 0.0 || height <= 0.0) return field;

    const double min_dim    = std::min(width, height);
    const double min_radius = min_dim * kMinRadiusFrac;
    const double max_radius = min_dim * kMaxRadiusFrac;

    field.hotspots.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const std::uint64_t seed_h = noise::hash2i(i, 601, entropy);
        VolcanicHotspot h;
        h.x = world_x0 + static_cast<double>(noise::to_unit_float(seed_h)) * width;
        h.z = world_z0
            + static_cast<double>(noise::to_unit_float(seed_h ^ 0x9e3779b97f4a7c15ULL)) * height;
        h.peak_elevation_m = min_peak_elevation_m
            + static_cast<double>(noise::to_unit_float(seed_h ^ 0xabcdef1234567890ULL))
                  * (max_peak_elevation_m - min_peak_elevation_m);
        h.radius_m = min_radius
            + static_cast<double>(noise::to_unit_float(seed_h ^ 0x1234567890abcdefULL))
                  * (max_radius - min_radius);
        h.active = static_cast<double>(noise::to_unit_float(seed_h ^ 0x0fedcba987654321ULL))
                   < kActiveProbability;
        field.hotspots.push_back(h);
    }

    return field;
}

double Volcanism::sampleElevation(const VolcanicField& field, double x, double z) {
    double best = 0.0;
    for (const VolcanicHotspot& h : field.hotspots) {
        if (h.radius_m <= 0.0) continue;
        const double dx   = x - h.x;
        const double dz   = z - h.z;
        const double dist = std::sqrt(dx * dx + dz * dz);
        if (dist >= h.radius_m) continue;

        const double falloff = 1.0 - dist / h.radius_m;
        const double contrib = h.peak_elevation_m * falloff;
        best = std::max(best, contrib);
    }
    return best;
}

void Volcanism::apply(FieldGrid& elevation, const VolcanicField& field, double sea_level_m,
                       double world_x0, double world_z0, double world_x1, double world_z1) {
    if (elevation.empty() || field.empty()) return;

    const int W = elevation.w;
    const int H = elevation.h;
    if (W < 3 || H < 3) return;  // no interior cells to raise without touching an edge

    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);
    if (cell_w <= 0.0 || cell_h <= 0.0) return;

    for (int gy = 1; gy < H - 1; ++gy) {
        for (int gx = 1; gx < W - 1; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            // Only uplift existing land -- never turn an ocean cell into
            // land (mirrors MountainRanges::apply()'s identical rule).
            if (elevation.data[idx] <= static_cast<float>(sea_level_m)) continue;

            const double wx  = world_x0 + (gx + 0.5) * cell_w;
            const double wz  = world_z0 + (gy + 0.5) * cell_h;
            const double add = sampleElevation(field, wx, wz);
            if (add <= 0.0) continue;

            elevation.data[idx] += static_cast<float>(add);
        }
    }
}

} // namespace MeshWorld::Map
