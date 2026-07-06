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

private:
    static float Simplex2(std::uint32_t seed, float x, float y, int octaves, float persistence, float lacunarity);
};

}
