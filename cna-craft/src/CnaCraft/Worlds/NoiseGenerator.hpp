#pragma once

#include <cstdint>

namespace CnaCraft::Worlds {

// Deterministic, dependency-free 2D Simplex noise used for terrain height.
// Swapped in (plan.md §11.1) from this project's original self-contained
// value noise to match Craft's own terrain shape more closely — Craft's
// world.c drives its heightmap with `simplex2` (see
// deps/noise/noise.c, derived from https://github.com/caseman/noise, MIT).
// Unlike Craft's own implementation (a single fixed/global permutation
// table, only reshuffled if `seed()` is explicitly called), this permutes
// the gradient table per *world* seed, so both "same seed -> same terrain"
// and "different seed -> different terrain" hold — matching this project's
// own Height(seed, x, z) API and what the smoke test relies on.
class NoiseGenerator {
public:
    // Deterministic terrain height for a world (x, z) column.
    static int Height(std::uint32_t seed, int x, int z);

    // Deterministic, fractal-summed 2D Simplex noise in roughly [0, 1].
    // `Height()` wraps this for terrain; exposed directly (plan.md §11.1
    // trees backlog item) so other generation passes can reuse the same 2D
    // noise field with their own frequency/octave parameters — e.g. tree
    // placement uses Craft's own `simplex2(x, z, 6, 0.5, 2) > 0.84` trigger
    // (`World::GenerateTrees`, verified against the real Craft checkout).
    static float Simplex2(std::uint32_t seed, float x, float y, int octaves, float persistence, float lacunarity);

    // Deterministic, fractal-summed 3D Simplex noise in roughly [0, 1].
    // Ported from the same classic Gustavson/caseman-noise algorithm as
    // Simplex2 (see Craft's deps/noise/noise.c `simplex3`/`noise3`, derived
    // from https://github.com/caseman/noise, MIT) using this project's own
    // per-seed permuted gradient table rather than Craft's fixed one. Used by
    // `World::GenerateClouds` (plan.md §11.1/§11.2).
    static float Simplex3(std::uint32_t seed, float x, float y, float z, int octaves, float persistence,
                           float lacunarity);
};

}
