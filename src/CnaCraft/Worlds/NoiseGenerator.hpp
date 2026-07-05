#pragma once

#include <cstdint>

namespace CnaCraft::Worlds {

// Deterministic, dependency-free 2D fractal value noise used for terrain
// height. Inspired by (but not copied from) Craft's use of position-seeded
// noise for the same purpose — see THIRD_PARTY_NOTICES.md. No external noise
// library or std::rand() involved: the same seed always reproduces the same
// world, which the smoke test in tests/worlds_smoke_test.cpp relies on.
class NoiseGenerator {
public:
    // Deterministic terrain height for a world (x, z) column.
    static int Height(std::uint32_t seed, int x, int z);

private:
    static float Lattice(std::uint32_t seed, int ix, int iz);
    static float Smooth(std::uint32_t seed, float x, float z);
    static float Fractal(std::uint32_t seed, float x, float z, int octaves);
};

}
