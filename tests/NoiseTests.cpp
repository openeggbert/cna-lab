// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP4 tests. M056: deterministic value/fBm/Worley noise.

#include <gtest/gtest.h>

#include <cmath>

#include "Map/Noise.hpp"

using namespace MeshWorld::Map::noise;

TEST(NoiseTest, ValueNoiseDeterministic) {
    EXPECT_EQ(value_noise(1.5, 2.5, 42), value_noise(1.5, 2.5, 42));
    EXPECT_EQ(value_noise(-3.2, 7.9, 7), value_noise(-3.2, 7.9, 7));
}

TEST(NoiseTest, ValueNoiseInUnitRange) {
    for (int i = 0; i < 1000; ++i) {
        const double x = i * 0.137;
        const double y = i * 0.0913 - 5.0;
        const float  v = value_noise(x, y, 123);
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}

// At integer coords the fade weights are zero, so the result is the corner hash.
TEST(NoiseTest, ValueNoiseEqualsCornerHashAtIntegers) {
    for (int i = -3; i <= 3; ++i) {
        const float v        = value_noise(static_cast<double>(i), 4.0, 555);
        const float expected = to_unit_float(hash2i(i, 4, 555));
        EXPECT_FLOAT_EQ(v, expected);
    }
}

TEST(NoiseTest, DifferentSeedsDiffer) {
    int diff = 0;
    for (int i = 0; i < 50; ++i)
        if (value_noise(i * 0.3, 1.1, 1) != value_noise(i * 0.3, 1.1, 2)) ++diff;
    EXPECT_GT(diff, 0);
}

// A tiny step in position produces a small change in value (smooth fade).
TEST(NoiseTest, ValueNoiseIsContinuous) {
    const float a = value_noise(2.000, 3.0, 9);
    const float b = value_noise(2.001, 3.0, 9);
    EXPECT_LT(std::fabs(a - b), 0.05f);
}

TEST(NoiseTest, FbmDeterministicAndBounded) {
    for (int i = 0; i < 500; ++i) {
        const double x = i * 0.21;
        const double y = -i * 0.07;
        const float  v = fbm(x, y, 77);
        EXPECT_EQ(v, fbm(x, y, 77));
        EXPECT_GE(v, 0.0f);
        EXPECT_LE(v, 1.0f);
    }
}

TEST(NoiseTest, WorleyNonNegativeBoundedDeterministic) {
    for (int i = 0; i < 500; ++i) {
        const double x = i * 0.19 - 3.0;
        const double y = i * 0.11;
        const float  d = worley_f1(x, y, 31);
        EXPECT_GE(d, 0.0f);
        EXPECT_LT(d, 2.0f);  // nearest feature is always within the neighbor cells
        EXPECT_EQ(d, worley_f1(x, y, 31));
    }
}

TEST(NoiseTest, Hash2iIsOrderSensitive) {
    // (x,y) and (y,x) must not collide in general.
    EXPECT_NE(hash2i(3, 7, 1), hash2i(7, 3, 1));
    EXPECT_NE(hash2i(0, 0, 1), hash2i(0, 0, 2));  // seed matters
}
