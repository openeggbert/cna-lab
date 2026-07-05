#include "NoiseGenerator.hpp"

#include <cmath>

namespace CnaCraft::Worlds {

namespace {

float SmoothStep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

}

float NoiseGenerator::Lattice(std::uint32_t seed, int ix, int iz) {
    std::uint32_t h = seed;
    h ^= static_cast<std::uint32_t>(ix) * 0x9E3779B1u;
    h ^= static_cast<std::uint32_t>(iz) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0xCC9E2D51u;
    h ^= h >> 16;
    return static_cast<float>(h) / static_cast<float>(0xFFFFFFFFu);
}

float NoiseGenerator::Smooth(std::uint32_t seed, float x, float z) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const float tx = SmoothStep(x - static_cast<float>(x0));
    const float tz = SmoothStep(z - static_cast<float>(z0));

    const float v00 = Lattice(seed, x0, z0);
    const float v10 = Lattice(seed, x1, z0);
    const float v01 = Lattice(seed, x0, z1);
    const float v11 = Lattice(seed, x1, z1);

    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * tz;
}

float NoiseGenerator::Fractal(std::uint32_t seed, float x, float z, int octaves) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float maxAmplitude = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        const std::uint32_t octaveSeed = seed + static_cast<std::uint32_t>(i) * 101u;
        total += Smooth(octaveSeed, x * frequency, z * frequency) * amplitude;
        maxAmplitude += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return total / maxAmplitude;
}

int NoiseGenerator::Height(std::uint32_t seed, int x, int z) {
    constexpr float kScale = 0.05f;
    constexpr int kBaseHeight = 20;
    constexpr int kAmplitude = 16;
    constexpr int kMinHeight = 4;
    constexpr int kMaxHeight = 56; // leaves headroom below WORLD_SIZE_Y (64)

    const float n = Fractal(seed, static_cast<float>(x) * kScale, static_cast<float>(z) * kScale, 4);
    int height = kBaseHeight + static_cast<int>((n - 0.5f) * 2.0f * static_cast<float>(kAmplitude));

    if (height < kMinHeight) height = kMinHeight;
    if (height > kMaxHeight) height = kMaxHeight;
    return height;
}

}
