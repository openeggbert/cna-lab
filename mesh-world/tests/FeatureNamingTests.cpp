// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP8 tests. M132: converting HydrologyNetwork/MountainRangeNetwork into
// named MapFeature entries.

#include <gtest/gtest.h>

#include "Map/FeatureNaming.hpp"

using namespace MeshWorld::Map;

namespace {

RiverSegment make_segment(int n_points, double start_x = 0.0) {
    RiverSegment seg;
    for (int i = 0; i < n_points; ++i)
        seg.points.push_back(RiverPoint{start_x + i * 10.0, 0.0, static_cast<float>(i + 1)});
    return seg;
}

// A 10x10 grid over world bounds [0,100)x[0,100) (10 m cells), uniformly
// land or ocean, for appendMountainRangeFeatures()'s land-check gate.
FieldGrid make_uniform_grid(float elevation_value) {
    FieldGrid g;
    g.w = g.h = 10;
    g.data.assign(100, elevation_value);
    return g;
}

} // namespace

TEST(FeatureNamingTest, EmptyNetworkAppendsNothing) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42);
    EXPECT_TRUE(features.empty());
}

TEST(FeatureNamingTest, ShortRiverBelowMinPointsIsSkipped) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    net.rivers.push_back(make_segment(3));  // below the default min of 5

    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42);
    EXPECT_TRUE(features.empty());
}

TEST(FeatureNamingTest, LongRiverBecomesANamedRiverFeature) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    net.rivers.push_back(make_segment(6));

    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42);

    ASSERT_EQ(features.size(), 1u);
    EXPECT_EQ(features[0].type, FeatureType::River);
    EXPECT_FALSE(features[0].name.empty());
    ASSERT_EQ(features[0].points.size(), 6u);
    EXPECT_DOUBLE_EQ(features[0].points[0][0], 0.0);
    EXPECT_DOUBLE_EQ(features[0].points[5][0], 50.0);
}

TEST(FeatureNamingTest, MinRiverPointsIsConfigurable) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    net.rivers.push_back(make_segment(3));

    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42, /*min_river_points=*/2);
    EXPECT_EQ(features.size(), 1u);
}

TEST(FeatureNamingTest, MultipleRiversGetDistinctNames) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    net.rivers.push_back(make_segment(6, 0.0));
    net.rivers.push_back(make_segment(6, 1000.0));

    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42);

    ASSERT_EQ(features.size(), 2u);
    EXPECT_NE(features[0].name, features[1].name);
}

TEST(FeatureNamingTest, LakeWithShorelineBecomesANamedLakeFeature) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    Lake                    lake;
    lake.x = 5.0;
    lake.z = 5.0;
    lake.shoreline = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};
    net.lakes.push_back(lake);

    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42);

    ASSERT_EQ(features.size(), 1u);
    EXPECT_EQ(features[0].type, FeatureType::Lake);
    EXPECT_FALSE(features[0].name.empty());
    EXPECT_EQ(features[0].points.size(), 4u);
}

TEST(FeatureNamingTest, LakeWithoutShorelineFallsBackToCentroidPoint) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    Lake                    lake;
    lake.x = 7.0;
    lake.z = 9.0;
    net.lakes.push_back(lake);  // shoreline left empty

    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42);

    ASSERT_EQ(features.size(), 1u);
    ASSERT_EQ(features[0].points.size(), 1u);
    EXPECT_DOUBLE_EQ(features[0].points[0][0], 7.0);
    EXPECT_DOUBLE_EQ(features[0].points[0][1], 9.0);
}

TEST(FeatureNamingTest, RiversAndLakesAreBothAppendedTogether) {
    std::vector<MapFeature> features;
    HydrologyNetwork        net;
    net.rivers.push_back(make_segment(6));
    net.lakes.push_back(Lake{});

    FeatureNaming::appendHydrologyFeatures(features, net, "nordic", 42);
    EXPECT_EQ(features.size(), 2u);
}

// --- MountainRangeNetwork ---

TEST(FeatureNamingTest, EmptyMountainNetworkAppendsNothing) {
    std::vector<MapFeature>    features;
    MountainRangeNetwork       net;
    const FieldGrid            land = make_uniform_grid(100.0f);
    FeatureNaming::appendMountainRangeFeatures(features, net, land, 0.0, "nordic", 42,
                                                0.0, 0.0, 100.0, 100.0);
    EXPECT_TRUE(features.empty());
}

TEST(FeatureNamingTest, InBoundsRangeOverLandBecomesANamedMountainRangeFeature) {
    std::vector<MapFeature> features;
    MountainRangeNetwork    net;
    MountainRange           range;
    range.ridge = {RidgePoint{10.0, 10.0, 3000.0}, RidgePoint{20.0, 20.0, 3200.0},
                   RidgePoint{30.0, 30.0, 3100.0}};
    net.ranges.push_back(range);
    const FieldGrid land = make_uniform_grid(100.0f);

    FeatureNaming::appendMountainRangeFeatures(features, net, land, /*sea_level_m=*/0.0,
                                                "nordic", 42, 0.0, 0.0, 100.0, 100.0);

    ASSERT_EQ(features.size(), 1u);
    EXPECT_EQ(features[0].type, FeatureType::MountainRange);
    EXPECT_FALSE(features[0].name.empty());
    EXPECT_EQ(features[0].points.size(), 3u);
}

TEST(FeatureNamingTest, RangeEntirelyOverOceanIsSkipped) {
    // Ridge geometry exists and is fully in-bounds, but every underlying
    // cell is ocean -- MountainRanges::apply() would have raised nothing
    // here, so this must not be named as a mountain range either.
    std::vector<MapFeature> features;
    MountainRangeNetwork    net;
    MountainRange           range;
    range.ridge = {RidgePoint{10.0, 10.0, 3000.0}, RidgePoint{20.0, 20.0, 3200.0},
                   RidgePoint{30.0, 30.0, 3100.0}};
    net.ranges.push_back(range);
    const FieldGrid ocean = make_uniform_grid(-100.0f);

    FeatureNaming::appendMountainRangeFeatures(features, net, ocean, /*sea_level_m=*/0.0,
                                                "nordic", 42, 0.0, 0.0, 100.0, 100.0);
    EXPECT_TRUE(features.empty());
}

TEST(FeatureNamingTest, RangeTouchingLandAtOnlyOnePointIsStillNamed) {
    // Most of the range is over ocean, but one in-bounds point is over
    // land -- that's enough to count as a real, visible range here.
    std::vector<MapFeature> features;
    MountainRangeNetwork    net;
    MountainRange           range;
    range.ridge = {RidgePoint{10.0, 10.0, 3000.0}, RidgePoint{20.0, 20.0, 3200.0}};
    net.ranges.push_back(range);

    FieldGrid mixed = make_uniform_grid(-100.0f);
    // World bounds [0,100)x[0,100), 10x10 grid -> 10 m cells. (20,20) falls
    // in cell (2,2); raise just that cell above sea level.
    mixed.data[static_cast<std::size_t>(2 * mixed.w + 2)] = 100.0f;

    FeatureNaming::appendMountainRangeFeatures(features, net, mixed, /*sea_level_m=*/0.0,
                                                "nordic", 42, 0.0, 0.0, 100.0, 100.0);
    EXPECT_EQ(features.size(), 1u);
}

TEST(FeatureNamingTest, RangeEntirelyOutsideBoundsIsSkipped) {
    std::vector<MapFeature> features;
    MountainRangeNetwork    net;
    MountainRange           range;
    range.ridge = {RidgePoint{-50.0, -50.0, 3000.0}, RidgePoint{-40.0, -40.0, 3000.0}};
    net.ranges.push_back(range);
    const FieldGrid land = make_uniform_grid(100.0f);

    FeatureNaming::appendMountainRangeFeatures(features, net, land, 0.0, "nordic", 42,
                                                0.0, 0.0, 100.0, 100.0);
    EXPECT_TRUE(features.empty());
}

TEST(FeatureNamingTest, RangeCrossingTheBoundaryKeepsOnlyInBoundsPoints) {
    std::vector<MapFeature> features;
    MountainRangeNetwork    net;
    MountainRange           range;
    range.ridge = {RidgePoint{-10.0, 10.0, 3000.0},  // outside (x < 0)
                   RidgePoint{10.0, 10.0, 3000.0},   // inside
                   RidgePoint{20.0, 20.0, 3000.0},   // inside
                   RidgePoint{150.0, 20.0, 3000.0}}; // outside (x >= 100)
    net.ranges.push_back(range);
    const FieldGrid land = make_uniform_grid(100.0f);

    FeatureNaming::appendMountainRangeFeatures(features, net, land, 0.0, "nordic", 42,
                                                0.0, 0.0, 100.0, 100.0);

    ASSERT_EQ(features.size(), 1u);
    ASSERT_EQ(features[0].points.size(), 2u);
    EXPECT_DOUBLE_EQ(features[0].points[0][0], 10.0);
    EXPECT_DOUBLE_EQ(features[0].points[1][0], 20.0);
}

TEST(FeatureNamingTest, RangeWithOnlyOneInBoundsPointIsSkipped) {
    std::vector<MapFeature> features;
    MountainRangeNetwork    net;
    MountainRange           range;
    range.ridge = {RidgePoint{10.0, 10.0, 3000.0},   // inside -- only one
                   RidgePoint{-10.0, 10.0, 3000.0}};  // outside
    net.ranges.push_back(range);
    const FieldGrid land = make_uniform_grid(100.0f);

    FeatureNaming::appendMountainRangeFeatures(features, net, land, 0.0, "nordic", 42,
                                                0.0, 0.0, 100.0, 100.0);
    EXPECT_TRUE(features.empty()) << "one point isn't a meaningful polyline";
}
