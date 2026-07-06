#include "NoiseGenerator.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace CnaCraft::Worlds {

namespace {

constexpr float kF2 = 0.3660254037844386f;  // (sqrt(3) - 1) / 2
constexpr float kG2 = 0.21132486540518713f; // (3 - sqrt(3)) / 6

// (x, y) components of the 12 simplex gradient directions used by the
// classic 2D simplex algorithm (Gustavson/caseman-noise derivation — see
// NoiseGenerator.hpp).
constexpr float kGrad2[12][2] = {
    {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
    {1, 0}, {-1, 0}, {1, 0}, {-1, 0},
    {0, 1}, {0, -1}, {0, 1}, {0, -1},
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

int NoiseGenerator::Height(std::uint32_t seed, int x, int z) {
    constexpr float kScale = 0.05f;
    constexpr int kBaseHeight = 20;
    constexpr int kAmplitude = 16;
    constexpr int kMinHeight = 4;
    constexpr int kMaxHeight = 56; // leaves headroom below WORLD_SIZE_Y (64)

    const float n = Simplex2(seed, static_cast<float>(x) * kScale, static_cast<float>(z) * kScale, 4, 0.5f, 2.0f);
    int height = kBaseHeight + static_cast<int>((n - 0.5f) * 2.0f * static_cast<float>(kAmplitude));

    if (height < kMinHeight) height = kMinHeight;
    if (height > kMaxHeight) height = kMaxHeight;
    return height;
}

}
