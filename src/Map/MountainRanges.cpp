// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/MountainRanges.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "Map/Noise.hpp"

namespace MeshWorld::Map {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Ridge shape tuning: a fixed-length walk with a per-step heading drift cap
// (radians) so ridges meander instead of running dead straight, and a
// profile wavelength (in ridge-point steps) controlling how quickly the
// peak/pass elevation profile oscillates along the ridge.
constexpr int    kRidgeSegments      = 12;
constexpr double kMaxTurnRad         = 0.6;
constexpr double kProfileWavelength  = 4.0;
} // namespace

MountainRangeNetwork MountainRanges::generate(std::uint64_t entropy, int count,
                                               double world_x0, double world_z0,
                                               double world_x1, double world_z1,
                                               double min_peak_elevation_m,
                                               double max_peak_elevation_m) {
    MountainRangeNetwork net;
    if (count <= 0) return net;

    const double width  = world_x1 - world_x0;
    const double height = world_z1 - world_z0;
    if (width <= 0.0 || height <= 0.0) return net;

    // Step length scales with the area so a ridge spans a sensible fraction
    // of it regardless of tile/planet scale (mirrors PlanetGenerator's
    // terrain_scale/coast_scale being derived from planet_size_m).
    const double step_m = std::max(std::min(width, height) / 20.0, 1.0);

    net.ranges.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const std::uint64_t seed_h = noise::hash2i(i, 401, entropy);
        double x       = world_x0 + static_cast<double>(noise::to_unit_float(seed_h)) * width;
        double z       = world_z0
                        + static_cast<double>(noise::to_unit_float(seed_h ^ 0x9e3779b97f4a7c15ULL)) * height;
        double heading = static_cast<double>(noise::to_unit_float(seed_h ^ 0xabcdef1234567890ULL))
                        * 2.0 * kPi;

        MountainRange range;
        range.ridge.reserve(static_cast<std::size_t>(kRidgeSegments) + 1);
        for (int j = 0; j <= kRidgeSegments; ++j) {
            // M127 — elevation profile: fBm along the ridge's own step index
            // (plus a per-range offset so ranges don't share a profile),
            // scaled into [min_peak, max_peak]. Peaks and passes are
            // wherever this profile is locally high/low, not separately
            // tracked.
            const float  profile_fbm  = noise::fbm(static_cast<double>(j) / kProfileWavelength,
                                                    static_cast<double>(i) * 17.0, entropy + 701ULL);
            const double elevation_m  = min_peak_elevation_m
                                       + static_cast<double>(profile_fbm)
                                             * (max_peak_elevation_m - min_peak_elevation_m);
            range.ridge.push_back(RidgePoint{x, z, elevation_m});

            if (j == kRidgeSegments) break;

            const std::uint64_t turn_h = noise::hash2i(i, 900 + j, entropy);
            const double        turn   = (static_cast<double>(noise::to_unit_float(turn_h)) - 0.5)
                                       * 2.0 * kMaxTurnRad;
            heading += turn;
            x += std::cos(heading) * step_m;
            z += std::sin(heading) * step_m;
        }
        net.ranges.push_back(std::move(range));
    }

    return net;
}

double MountainRanges::sampleElevation(const MountainRangeNetwork& network, double x, double z,
                                        double falloff_width_m) {
    if (network.empty() || falloff_width_m <= 0.0) return 0.0;

    double best_dist = std::numeric_limits<double>::max();
    double best_elev = 0.0;
    for (const MountainRange& range : network.ranges) {
        for (const RidgePoint& p : range.ridge) {
            const double dx   = x - p.x;
            const double dz   = z - p.z;
            const double dist = std::sqrt(dx * dx + dz * dz);
            if (dist < best_dist) {
                best_dist = dist;
                best_elev = p.elevation_m;
            }
        }
    }
    if (best_dist >= falloff_width_m) return 0.0;

    const double falloff = 1.0 - best_dist / falloff_width_m;
    return best_elev * falloff;
}

void MountainRanges::apply(FieldGrid& elevation, const MountainRangeNetwork& network,
                            double sea_level_m, double falloff_width_m,
                            double world_x0, double world_z0, double world_x1, double world_z1) {
    if (elevation.empty() || network.empty()) return;

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
            // land (mirrors Hydrology::carve()'s symmetric rule against
            // turning land into ocean).
            if (elevation.data[idx] <= static_cast<float>(sea_level_m)) continue;

            const double wx  = world_x0 + (gx + 0.5) * cell_w;
            const double wz  = world_z0 + (gy + 0.5) * cell_h;
            const double add = sampleElevation(network, wx, wz, falloff_width_m);
            if (add <= 0.0) continue;

            elevation.data[idx] += static_cast<float>(add);
        }
    }
}

} // namespace MeshWorld::Map
