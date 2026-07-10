#include "NoiseGenerator.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace CnaCraft::Worlds {

namespace {

constexpr float kF2 = 0.3660254037844386f;  // (sqrt(3) - 1) / 2
constexpr float kG2 = 0.21132486540518713f; // (3 - sqrt(3)) / 6
constexpr float kF3 = 1.0f / 3.0f;
constexpr float kG3 = 1.0f / 6.0f;

// (x, y) components of the 12 simplex gradient directions used by the
// classic 2D simplex algorithm (Gustavson/caseman-noise derivation — see
// NoiseGenerator.hpp).
constexpr float kGrad2[12][2] = {
    {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
    {1, 0}, {-1, 0}, {1, 0}, {-1, 0},
    {0, 1}, {0, -1}, {0, 1}, {0, -1},
};

// (x, y, z) components of the 12 simplex gradient directions used by the
// classic 3D simplex algorithm (same derivation as kGrad2, extended to 3D —
// matches the first 12 rows of Craft's own GRAD3 table, which also never
// indexes beyond 12 since its gradient index is taken mod 12).
constexpr float kGrad3[12][3] = {
    {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
    {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
    {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1},
};

// Seed-permuted 512-entry lookup table (256 values doubled to avoid
// wraparound checks) — same construction shape as reference simplex-noise
// implementations, but Fisher-Yates shuffled by `seed` via a small xorshift32
// PRNG instead of Craft's fixed constant table.
std::array<std::uint8_t, 512> BuildPermutation(std::uint32_t seed) {
    std::array<std::uint8_t, 256> p{};
    for (int i = 0; i < 256; ++i) p[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);

    std::uint32_t state = seed ^ 0x9E3779B9u;
    if (state == 0) state = 0x9E3779B9u;
    auto nextRandom = [&state]() -> std::uint32_t {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };
    for (int i = 255; i > 0; --i) {
        const int j = static_cast<int>(nextRandom() % static_cast<std::uint32_t>(i + 1));
        std::swap(p[static_cast<std::size_t>(i)], p[static_cast<std::size_t>(j)]);
    }

    std::array<std::uint8_t, 512> perm{};
    for (int i = 0; i < 512; ++i) perm[static_cast<std::size_t>(i)] = p[static_cast<std::size_t>(i & 255)];
    return perm;
}

// Standard 2D simplex noise (Gustavson/caseman algorithm), returning a value
// in roughly [-1, 1].
float Noise2(const std::array<std::uint8_t, 512>& perm, float x, float y) {
    const float s = (x + y) * kF2;
    const float i = std::floor(x + s);
    const float j = std::floor(y + s);
    const float t = (i + j) * kG2;

    const float x0 = x - (i - t);
    const float y0 = y - (j - t);

    const int i1 = x0 > y0 ? 1 : 0;
    const int j1 = x0 > y0 ? 0 : 1;

    const float x1 = x0 - static_cast<float>(i1) + kG2;
    const float y1 = y0 - static_cast<float>(j1) + kG2;
    const float x2 = x0 - 1.0f + 2.0f * kG2;
    const float y2 = y0 - 1.0f + 2.0f * kG2;

    const int ii = static_cast<int>(i) & 255;
    const int jj = static_cast<int>(j) & 255;

    const int g0 = perm[static_cast<std::size_t>(ii + perm[static_cast<std::size_t>(jj)])] % 12;
    const int g1 = perm[static_cast<std::size_t>(ii + i1 + perm[static_cast<std::size_t>(jj + j1)])] % 12;
    const int g2 = perm[static_cast<std::size_t>(ii + 1 + perm[static_cast<std::size_t>(jj + 1)])] % 12;

    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

    float f0 = 0.5f - x0 * x0 - y0 * y0;
    if (f0 > 0.0f) { f0 *= f0; n0 = f0 * f0 * (kGrad2[g0][0] * x0 + kGrad2[g0][1] * y0); }

    float f1 = 0.5f - x1 * x1 - y1 * y1;
    if (f1 > 0.0f) { f1 *= f1; n1 = f1 * f1 * (kGrad2[g1][0] * x1 + kGrad2[g1][1] * y1); }

    float f2 = 0.5f - x2 * x2 - y2 * y2;
    if (f2 > 0.0f) { f2 *= f2; n2 = f2 * f2 * (kGrad2[g2][0] * x2 + kGrad2[g2][1] * y2); }

    return 70.0f * (n0 + n1 + n2);
}

// Standard 3D simplex noise (Gustavson/caseman algorithm), returning a value
// in roughly [-1, 1]. Same structure as Noise2, one dimension up.
float Noise3(const std::array<std::uint8_t, 512>& perm, float x, float y, float z) {
    const float s = (x + y + z) * kF3;
    const float i = std::floor(x + s);
    const float j = std::floor(y + s);
    const float k = std::floor(z + s);
    const float t = (i + j + k) * kG3;

    const float x0 = x - (i - t);
    const float y0 = y - (j - t);
    const float z0 = z - (k - t);

    int i1, j1, k1;
    int i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    } else {
        if (y0 < z0) { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0) { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }

    const float x1 = x0 - static_cast<float>(i1) + kG3;
    const float y1 = y0 - static_cast<float>(j1) + kG3;
    const float z1 = z0 - static_cast<float>(k1) + kG3;
    const float x2 = x0 - static_cast<float>(i2) + 2.0f * kG3;
    const float y2 = y0 - static_cast<float>(j2) + 2.0f * kG3;
    const float z2 = z0 - static_cast<float>(k2) + 2.0f * kG3;
    const float x3 = x0 - 1.0f + 3.0f * kG3;
    const float y3 = y0 - 1.0f + 3.0f * kG3;
    const float z3 = z0 - 1.0f + 3.0f * kG3;

    const int ii = static_cast<int>(i) & 255;
    const int jj = static_cast<int>(j) & 255;
    const int kk = static_cast<int>(k) & 255;

    const int g0 = perm[static_cast<std::size_t>(ii + perm[static_cast<std::size_t>(jj + perm[static_cast<std::size_t>(kk)])])] % 12;
    const int g1 = perm[static_cast<std::size_t>(ii + i1 + perm[static_cast<std::size_t>(jj + j1 + perm[static_cast<std::size_t>(kk + k1)])])] % 12;
    const int g2 = perm[static_cast<std::size_t>(ii + i2 + perm[static_cast<std::size_t>(jj + j2 + perm[static_cast<std::size_t>(kk + k2)])])] % 12;
    const int g3 = perm[static_cast<std::size_t>(ii + 1 + perm[static_cast<std::size_t>(jj + 1 + perm[static_cast<std::size_t>(kk + 1)])])] % 12;

    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f, n3 = 0.0f;

    float f0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
    if (f0 > 0.0f) { f0 *= f0; n0 = f0 * f0 * (kGrad3[g0][0] * x0 + kGrad3[g0][1] * y0 + kGrad3[g0][2] * z0); }

    float f1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
    if (f1 > 0.0f) { f1 *= f1; n1 = f1 * f1 * (kGrad3[g1][0] * x1 + kGrad3[g1][1] * y1 + kGrad3[g1][2] * z1); }

    float f2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
    if (f2 > 0.0f) { f2 *= f2; n2 = f2 * f2 * (kGrad3[g2][0] * x2 + kGrad3[g2][1] * y2 + kGrad3[g2][2] * z2); }

    float f3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
    if (f3 > 0.0f) { f3 *= f3; n3 = f3 * f3 * (kGrad3[g3][0] * x3 + kGrad3[g3][1] * y3 + kGrad3[g3][2] * z3); }

    return 32.0f * (n0 + n1 + n2 + n3);
}

}

float NoiseGenerator::Simplex2(std::uint32_t seed, float x, float y, int octaves, float persistence,
                                float lacunarity) {
    const auto perm = BuildPermutation(seed);

    float freq = 1.0f;
    float amp = 1.0f;
    float maxAmp = 1.0f;
    float total = Noise2(perm, x, y);
    for (int i = 1; i < octaves; ++i) {
        freq *= lacunarity;
        amp *= persistence;
        maxAmp += amp;
        total += Noise2(perm, x * freq, y * freq) * amp;
    }
    return (1.0f + total / maxAmp) / 2.0f; // normalize to roughly [0, 1]
}

float NoiseGenerator::Simplex3(std::uint32_t seed, float x, float y, float z, int octaves, float persistence,
                                float lacunarity) {
    const auto perm = BuildPermutation(seed);

    float freq = 1.0f;
    float amp = 1.0f;
    float maxAmp = 1.0f;
    float total = Noise3(perm, x, y, z);
    for (int i = 1; i < octaves; ++i) {
        freq *= lacunarity;
        amp *= persistence;
        maxAmp += amp;
        total += Noise3(perm, x * freq, y * freq, z * freq) * amp;
    }
    return (1.0f + total / maxAmp) / 2.0f; // normalize to roughly [0, 1]
}

int NoiseGenerator::Height(std::uint32_t seed, int x, int z) {
    // Craft's real create_world formula (src/world.c), verified against the
    // checkout — re-ported exactly per user decision (2026-07-10), replacing
    // this project's earlier single-additive-simplex2 approximation. Two
    // independent simplex2 samples combined *multiplicatively*: `f` is the
    // primary heightmap noise (4 octaves/0.5 persistence/2 lacunarity, scale
    // 0.01), `g` modulates the amplitude (2 octaves/0.9 persistence/2
    // lacunarity, scale -0.01 i.e. mirrored coordinates) into `mh` (a
    // per-column max-height band), giving `h = f * mh`.
    //
    // Craft also reassigns `h = t` (t=12, a flat sea level) whenever the raw
    // h is <= t, marking that column sand instead of grass in the same step
    // (`world.c`: `if (h <= t) { h = t; w = SAND; }`). Folded in here too —
    // both the height clamp and the sand/grass split come from the same
    // source formula in Craft, and this keeps a single int-returning API
    // (World::Generate's existing `height <= kSandMaxHeight` check for
    // "is this a sand column" stays correct, since a sandy column's Height()
    // now always returns exactly kSeaLevel).
    const float f = Simplex2(seed, static_cast<float>(x) * 0.01f, static_cast<float>(z) * 0.01f, 4, 0.5f, 2.0f);
    const float g = Simplex2(seed, static_cast<float>(-x) * 0.01f, static_cast<float>(-z) * 0.01f, 2, 0.9f, 2.0f);
    const float mh = g * 32.0f + 16.0f;
    int height = static_cast<int>(f * mh);

    constexpr int kSeaLevel = 12; // Craft's own t=12 (world.c)
    if (height <= kSeaLevel) height = kSeaLevel;
    return height;
}

}
