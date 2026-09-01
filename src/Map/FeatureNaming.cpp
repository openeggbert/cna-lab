// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/FeatureNaming.hpp"

#include <algorithm>
#include <cstddef>

#include "Map/Noise.hpp"
#include "Naming.hpp"

namespace MeshWorld::Map {

namespace {
// Arbitrary, distinct hash2i axis numbers so river/lake/mountain-range name
// seeds never collide with each other or with unrelated hash2i(...) callers
// (PlanetGenerator's continent seeding uses axes 0/2, MountainRanges uses
// 401/900+j) even when given the same feature index and entropy.
constexpr std::int64_t kRiverNameAxis = 601;
constexpr std::int64_t kLakeNameAxis  = 602;
constexpr std::int64_t kRangeNameAxis = 603;
} // namespace

void FeatureNaming::appendHydrologyFeatures(std::vector<MapFeature>& features,
                                             const HydrologyNetwork& network,
                                             const std::string& culture,
                                             std::uint64_t entropy,
                                             int min_river_points) {
    for (std::size_t i = 0; i < network.rivers.size(); ++i) {
        const RiverSegment& seg = network.rivers[i];
        if (static_cast<int>(seg.points.size()) < min_river_points) continue;

        const std::uint64_t seed =
            noise::hash2i(static_cast<std::int64_t>(i), kRiverNameAxis, entropy);

        MapFeature f;
        f.type = FeatureType::River;
        f.name = MeshWorld::Naming::river(culture, seed);
        f.points.reserve(seg.points.size());
        for (const RiverPoint& p : seg.points) f.points.push_back({p.x, p.z});
        features.push_back(std::move(f));
    }

    for (std::size_t i = 0; i < network.lakes.size(); ++i) {
        const Lake& lake = network.lakes[i];
        const std::uint64_t seed =
            noise::hash2i(static_cast<std::int64_t>(i), kLakeNameAxis, entropy);

        MapFeature f;
        f.type   = FeatureType::Lake;
        f.name   = MeshWorld::Naming::lake(culture, seed);
        f.points = lake.shoreline.empty()
                       ? std::vector<std::array<double, 2>>{{lake.x, lake.z}}
                       : lake.shoreline;
        features.push_back(std::move(f));
    }
}

void FeatureNaming::appendMountainRangeFeatures(std::vector<MapFeature>& features,
                                                 const MountainRangeNetwork& network,
                                                 const FieldGrid& elevation, double sea_level_m,
                                                 const std::string& culture,
                                                 std::uint64_t entropy,
                                                 double world_x0, double world_z0,
                                                 double world_x1, double world_z1) {
    if (elevation.empty()) return;

    const int    W      = elevation.w;
    const int    H      = elevation.h;
    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);
    if (cell_w <= 0.0 || cell_h <= 0.0) return;

    for (std::size_t i = 0; i < network.ranges.size(); ++i) {
        const MountainRange& range = network.ranges[i];

        std::vector<std::array<double, 2>> in_bounds;
        in_bounds.reserve(range.ridge.size());
        bool touches_land = false;
        for (const RidgePoint& p : range.ridge) {
            if (p.x < world_x0 || p.x >= world_x1 || p.z < world_z0 || p.z >= world_z1) continue;
            in_bounds.push_back({p.x, p.z});

            const int gx = std::clamp(static_cast<int>((p.x - world_x0) / cell_w), 0, W - 1);
            const int gy = std::clamp(static_cast<int>((p.z - world_z0) / cell_h), 0, H - 1);
            if (elevation.at(gx, gy) > static_cast<float>(sea_level_m)) touches_land = true;
        }
        if (in_bounds.size() < 2) continue;  // doesn't meaningfully cross this tile
        // Ridge geometry is independent of elevation (see MountainRanges::
        // generate()'s own doc comment); apply() would have skipped every
        // cell of a range that never touches land, so naming it here would
        // label a "mountain range" with no actual uplifted terrain.
        if (!touches_land) continue;

        const std::uint64_t seed =
            noise::hash2i(static_cast<std::int64_t>(i), kRangeNameAxis, entropy);

        MapFeature f;
        f.type   = FeatureType::MountainRange;
        f.name   = MeshWorld::Naming::mountain(culture, seed);
        f.points = std::move(in_bounds);
        features.push_back(std::move(f));
    }
}

} // namespace MeshWorld::Map
