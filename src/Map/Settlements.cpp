// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/Settlements.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_set>
#include <vector>

#include "Map/Noise.hpp"
#include "NameGenerator.hpp"
#include "Naming.hpp"

namespace MeshWorld::Map {

namespace {

// Distinct from Countries.cpp's kCountryNameAxis (801) and MountainRanges.
// cpp's open-ended 900+j ridge-turn family -- see Countries.cpp's own
// comment on why every hash2i(...) caller needs a distinct axis.
constexpr std::int64_t kSettlementNameAxis = 802;

// PlaceLabel::kind convention for a settlement: its tier, lowercased.
// Matches existing PlaceLabel test fixtures' freeform "country"/"city"/
// "river" style kind strings -- not a new convention.
const char* tier_kind(SettlementTier tier) {
    switch (tier) {
        case SettlementTier::Capital: return "capital";
        case SettlementTier::City:    return "city";
        case SettlementTier::Town:    return "town";
        case SettlementTier::Village: return "village";
    }
    return "village";
}

// Local relief in the cell's 3x3 neighborhood -- same computation as
// BiomeRefinement::applySwampFlatnessCheck() (M130), reused here rather
// than reimplemented so "flat" means the same thing in both places.
float local_relief(const FieldGrid& elevation, int gx, int gy) {
    const int W = elevation.w;
    const int H = elevation.h;
    float     lo = elevation.at(gx, gy);
    float     hi = lo;
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

// True if any cell within radius_cells (Chebyshev) is ocean -- same check
// as BiomeRefinement::applyCoastalBeach() (M128), reused here.
bool near_water(const FieldGrid& elevation, double sea_level_m, int gx, int gy,
                 int radius_cells) {
    const int W = elevation.w;
    const int H = elevation.h;
    for (int dy = -radius_cells; dy <= radius_cells; ++dy) {
        const int ny = gy + dy;
        if (ny < 0 || ny >= H) continue;
        for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
            const int nx = gx + dx;
            if (nx < 0 || nx >= W) continue;
            if (elevation.at(nx, ny) <= static_cast<float>(sea_level_m)) return true;
        }
    }
    return false;
}

// Suitability score for one cell, or a negative value if disqualified
// outright (ocean, or too high above sea level -- a hard requirement, not
// a soft bonus). Coastal and flat are independent soft bonuses on top of
// the base score every qualifying land cell gets.
double suitability(const FieldGrid& elevation, double sea_level_m, int gx, int gy,
                    double max_settlement_elevation_m, int coastal_radius_cells,
                    double max_relief_for_flat_m) {
    const float elev_above_sea = elevation.at(gx, gy) - static_cast<float>(sea_level_m);
    if (elev_above_sea <= 0.0f) return -1.0;                          // ocean
    if (elev_above_sea > static_cast<float>(max_settlement_elevation_m)) return -1.0;  // too high

    double score = 1.0;
    if (near_water(elevation, sea_level_m, gx, gy, coastal_radius_cells)) score += 1.0;
    if (local_relief(elevation, gx, gy) <= max_relief_for_flat_m) score += 1.0;
    return score;
}

} // namespace

SettlementNetwork Settlements::place(std::uint64_t entropy, const FieldGrid& elevation,
                                      double sea_level_m,
                                      double world_x0, double world_z0,
                                      double world_x1, double world_z1,
                                      int capital_count, int city_count, int town_count,
                                      double min_spacing_m,
                                      double max_settlement_elevation_m,
                                      int coastal_radius_cells,
                                      double max_relief_for_flat_m) {
    SettlementNetwork net;
    if (elevation.empty()) return net;
    if (capital_count <= 0 && city_count <= 0 && town_count <= 0) return net;

    const int    W      = elevation.w;
    const int    H      = elevation.h;
    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);
    if (cell_w <= 0.0 || cell_h <= 0.0) return net;

    struct Candidate {
        int    gx, gy;
        double score;
    };
    std::vector<Candidate> candidates;
    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const double s = suitability(elevation, sea_level_m, gx, gy,
                                          max_settlement_elevation_m, coastal_radius_cells,
                                          max_relief_for_flat_m);
            if (s > 0.0) candidates.push_back({gx, gy, s});
        }
    }
    if (candidates.empty()) return net;

    // Best score first; ties broken by a hash of (cell, entropy) rather than
    // raster scan order, so equally-suitable cells don't always resolve the
    // same predictable way regardless of world content.
    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b) {
        if (a.score != b.score) return a.score > b.score;
        return noise::hash2i(a.gx, a.gy, entropy) > noise::hash2i(b.gx, b.gy, entropy);
    });

    const auto world_x = [&](int gx) { return world_x0 + (gx + 0.5) * cell_w; };
    const auto world_z = [&](int gy) { return world_z0 + (gy + 0.5) * cell_h; };

    const auto too_close = [&](double x, double z) {
        for (const Settlement& s : net.settlements) {
            const double dx = s.x - x;
            const double dz = s.z - z;
            if (dx * dx + dz * dz < min_spacing_m * min_spacing_m) return true;
        }
        return false;
    };

    const auto place_tier = [&](int count, SettlementTier tier) {
        int placed = 0;
        for (const Candidate& c : candidates) {
            if (placed >= count) break;
            const double x = world_x(c.gx);
            const double z = world_z(c.gy);
            if (too_close(x, z)) continue;
            net.settlements.push_back(Settlement{x, z, tier, ""});
            ++placed;
        }
    };

    place_tier(capital_count, SettlementTier::Capital);
    place_tier(city_count, SettlementTier::City);
    place_tier(town_count, SettlementTier::Town);

    return net;
}

// M340 (MAP22) -- dedupe within this network: two settlements sharing an
// identical generated name (a rare but real collision -- a phoneme+suffix
// pool has a finite number of distinct outputs) previously went uncaught,
// even though MeshWorld::NameGenerator::dedupe() has existed and been unit-tested
// standalone since MAP9 (M082). used_names accumulates across the whole
// loop, so settlement i's dedupe scope includes every name already chosen
// for settlements 0..i-1 in this same network -- the network itself IS the
// natural "adjacent regions" scope this task's own audit wording describes.
// The generate lambda wraps Naming::city() unchanged (not NameGenerator::
// city() directly), so the real-registry/hardcoded-fallback duality
// Naming::city() already handles internally is preserved untouched.
// dedupe()'s first attempt always reuses the exact seed passed in, so a
// network with no actual collisions produces byte-identical names to
// before this change -- only the previously-uncaught collision case
// behaves differently now, which is this task's entire point.
void Settlements::name(SettlementNetwork& net, const std::string& culture, std::uint64_t entropy) {
    std::unordered_set<std::string> used_names;
    for (std::size_t i = 0; i < net.settlements.size(); ++i) {
        const std::uint64_t seed =
            noise::hash2i(static_cast<std::int64_t>(i), kSettlementNameAxis, entropy);
        const std::string name = MeshWorld::NameGenerator::dedupe(
            [&](std::uint64_t e) { return MeshWorld::Naming::city(culture, e); },
            seed, used_names);
        net.settlements[i].name = name;
        used_names.insert(name);
    }
}

void Settlements::appendLabels(std::vector<PlaceLabel>& labels, const SettlementNetwork& network) {
    for (const Settlement& s : network.settlements) {
        PlaceLabel label;
        label.name = s.name;
        label.pos  = {s.x, s.z};
        label.kind = tier_kind(s.tier);
        labels.push_back(std::move(label));
    }
}

} // namespace MeshWorld::Map
