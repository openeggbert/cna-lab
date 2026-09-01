// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cmath>
#include <cstdint>

// Deterministic, seedable noise utilities for the planet generator (MAP4/M056).
// Header-only, no global state: every function is pure in (coords, seed), so the
// same inputs always yield the same value across runs and machines — a hard
// requirement for regenerate-matches-persisted determinism (M070).
namespace MeshWorld::Map::noise {

// 64-bit hash of an integer lattice cell + seed (Murmur3-style finalizer).
inline std::uint64_t hash2i(std::int64_t x, std::int64_t y, std::uint64_t seed) {
    std::uint64_t h = seed + 0x9e3779b97f4a7c15ULL;
    h ^= static_cast<std::uint64_t>(x) * 0xff51afd7ed558ccdULL;
    h  = (h ^ (h >> 29)) * 0xc4ceb9fe1a85ec53ULL;
    h ^= static_cast<std::uint64_t>(y) * 0x9e3779b97f4a7c15ULL;
    h ^= h >> 32;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 29;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 32;
    return h;
}

// Map a hash to a float in [0, 1) using its top 24 bits.
inline float to_unit_float(std::uint64_t h) {
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f);  // / 2^24
}

// Quintic smoothstep (C2-continuous): 6t^5 - 15t^4 + 10t^3.
inline double smoothstep5(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Value noise at (x, y): bilinear interpolation of lattice corner hashes with a
// quintic fade. Result in [0, 1); at integer coords it equals the corner hash.
inline float value_noise(double x, double y, std::uint64_t seed) {
    const double xf = std::floor(x);
    const double yf = std::floor(y);
    const auto   ix = static_cast<std::int64_t>(xf);
    const auto   iy = static_cast<std::int64_t>(yf);
    const double tx = smoothstep5(x - xf);
    const double ty = smoothstep5(y - yf);

    const double v00 = to_unit_float(hash2i(ix,     iy,     seed));
    const double v10 = to_unit_float(hash2i(ix + 1, iy,     seed));
    const double v01 = to_unit_float(hash2i(ix,     iy + 1, seed));
    const double v11 = to_unit_float(hash2i(ix + 1, iy + 1, seed));

    const double a = v00 + (v10 - v00) * tx;
    const double b = v01 + (v11 - v01) * tx;
    return static_cast<float>(a + (b - a) * ty);
}

// Fractal value noise (fBm): `octaves` layers, each `lacunarity`x finer and
// `gain`x weaker, normalized back to ~[0, 1). Each octave uses an independent
// seed so layers do not align.
inline float fbm(double x, double y, std::uint64_t seed,
                 int octaves = 4, double lacunarity = 2.0, double gain = 0.5) {
    double sum = 0.0, amp = 1.0, freq = 1.0, norm = 0.0;
    for (int o = 0; o < octaves; ++o) {
        sum  += amp * value_noise(x * freq, y * freq,
                                  seed + static_cast<std::uint64_t>(o) * 0x9e3779b97f4a7c15ULL);
        norm += amp;
        amp  *= gain;
        freq *= lacunarity;
    }
    return static_cast<float>(norm > 0.0 ? sum / norm : 0.0);
}

// Worley / cellular F1 noise: distance from (x, y) to the nearest feature point
// (one hash-jittered point per unit cell). Returns the Euclidean distance, which
// for unit cells lies in [0, ~1.5). Useful for cellular continent/region masks.
inline float worley_f1(double x, double y, std::uint64_t seed) {
    const auto ix = static_cast<std::int64_t>(std::floor(x));
    const auto iy = static_cast<std::int64_t>(std::floor(y));

    double best = 1.0e30;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const std::int64_t  cx = ix + dx;
            const std::int64_t  cy = iy + dy;
            const std::uint64_t h  = hash2i(cx, cy, seed);
            const double fx = static_cast<double>(cx) + to_unit_float(h);
            const double fy = static_cast<double>(cy) + to_unit_float(h ^ 0x9e3779b97f4a7c15ULL);
            const double ddx = fx - x;
            const double ddy = fy - y;
            const double d2  = ddx * ddx + ddy * ddy;
            if (d2 < best) best = d2;
        }
    }
    return static_cast<float>(std::sqrt(best));
}

} // namespace MeshWorld::Map::noise
