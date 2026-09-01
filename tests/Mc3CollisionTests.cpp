// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>

#include "Mc3Collision.hpp"
#include "PlayerCollision.hpp"

#include <MeshCraft/Mc3/Mc3Object.hpp>

using namespace MeshWorld;
using namespace MeshCraft::Mc3;

namespace {

std::shared_ptr<Mc3Object> make_collidable_definition(const std::string& id,
                                                       const std::string& proxy = "box") {
    auto definition = Mc3Object::makeGroup(id);
    Mc3AssetMetadata metadata;
    metadata.collisionProxy = proxy;
    metadata.boundsMin = {-1.0f, 0.0f, -2.0f};
    metadata.boundsMax = {1.0f, 1.0f, 2.0f};
    definition->assetMetadata = metadata;
    return definition;
}

} // namespace

TEST(Mc3CollisionTests, LegacyInlineCollisionBoxIsStillExtracted) {
    Mc3Document document;
    auto wall = Mc3Object::makeBox("wall", {2.0f, 3.0f, 4.0f});
    wall->collision = "box";
    wall->transform.position = {5.0f, 1.5f, -2.0f};
    document.objects.push_back(wall);

    const CollisionExtractionResult extracted = extract_mc3_collision_boxes(document);
    ASSERT_EQ(extracted.boxes.size(), 1u);
    EXPECT_FLOAT_EQ(extracted.boxes[0].min_x, 4.0f);
    EXPECT_FLOAT_EQ(extracted.boxes[0].max_x, 6.0f);
    EXPECT_FLOAT_EQ(extracted.boxes[0].min_y, 0.0f);
    EXPECT_FLOAT_EQ(extracted.boxes[0].max_y, 3.0f);
    EXPECT_FLOAT_EQ(extracted.boxes[0].min_z, -4.0f);
    EXPECT_FLOAT_EQ(extracted.boxes[0].max_z, 0.0f);
}

TEST(Mc3CollisionTests, ResolvesAliasedInstanceBoundsWithRotationAndScale) {
    Mc3Document document;
    document.defineObject("vehicle", make_collidable_definition("vehicle"));
    auto instance = Mc3Object::makeInstance("parked-car", "city:vehicle");
    instance->transform.position = {10.0f, 0.0f, 20.0f};
    instance->transform.rotation = {0.0f, 90.0f, 0.0f};
    instance->transform.scale = {2.0f, 2.0f, 2.0f};
    document.objects.push_back(instance);

    const CollisionExtractionResult extracted = extract_mc3_collision_boxes(document);
    ASSERT_EQ(extracted.boxes.size(), 1u);
    EXPECT_NEAR(extracted.boxes[0].min_x, 6.0f, 0.0001f);
    EXPECT_NEAR(extracted.boxes[0].max_x, 14.0f, 0.0001f);
    EXPECT_NEAR(extracted.boxes[0].min_y, 0.0f, 0.0001f);
    EXPECT_NEAR(extracted.boxes[0].max_y, 2.0f, 0.0001f);
    EXPECT_NEAR(extracted.boxes[0].min_z, 18.0f, 0.0001f);
    EXPECT_NEAR(extracted.boxes[0].max_z, 22.0f, 0.0001f);
    EXPECT_TRUE(extracted.diagnostics.empty());
}

TEST(Mc3CollisionTests, NoneProxyKeepsDecorativeInstancePassable) {
    Mc3Document document;
    document.defineObject("lamp", make_collidable_definition("lamp", "none"));
    document.objects.push_back(Mc3Object::makeInstance("lamp-1", "lamp"));

    const CollisionExtractionResult extracted = extract_mc3_collision_boxes(document);
    EXPECT_TRUE(extracted.boxes.empty());
    EXPECT_TRUE(extracted.diagnostics.empty());
}

TEST(Mc3CollisionTests, UnknownProxyIsDiagnosedButDoesNotBlock) {
    Mc3Document document;
    document.defineObject("sculpture", make_collidable_definition("sculpture", "convex_hull"));
    document.objects.push_back(Mc3Object::makeInstance("sculpture-1", "sculpture"));

    const CollisionExtractionResult extracted = extract_mc3_collision_boxes(document);
    EXPECT_TRUE(extracted.boxes.empty());
    ASSERT_EQ(extracted.diagnostics.size(), 1u);
    EXPECT_EQ(extracted.diagnostics[0].kind, CollisionDiagnosticKind::UnsupportedProxy);
    EXPECT_EQ(extracted.diagnostics[0].definition_id, "sculpture");
}

TEST(Mc3CollisionTests, RegeneratedLibrariesKeepLampsPassableAndVehiclesSolid) {
    const Mc3Document furniture = Mc3Document::loadFromJsonFile(
        "data/mc3lib/urban-street-furniture-1.0.0.mc3lib.json");
    const Mc3Document vehicles = Mc3Document::loadFromJsonFile(
        "data/mc3lib/urban-vehicles-1.0.0.mc3lib.json");

    ASSERT_TRUE(furniture.definitions.contains("streetlamp.classic_01"));
    ASSERT_TRUE(vehicles.definitions.contains("car.hatchback.compact_01"));
    ASSERT_TRUE(furniture.definitions.at("streetlamp.classic_01")->assetMetadata.has_value());
    ASSERT_TRUE(vehicles.definitions.at("car.hatchback.compact_01")->assetMetadata.has_value());
    EXPECT_EQ(furniture.definitions.at("streetlamp.classic_01")->assetMetadata->collisionProxy, "none");
    EXPECT_EQ(vehicles.definitions.at("car.hatchback.compact_01")->assetMetadata->collisionProxy, "box");
}

TEST(PlayerCollisionTests, VehicleBlocksForwardMovementButPassableLampDoesNot) {
    const std::vector<CollisionBox> vehicle = {{1.0f, 3.0f, -1.0f, 1.0f, 0.0f, 2.0f}};
    const PlayerMoveResult blocked = resolve_player_capsule_slide(
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.8f, 0.35f, vehicle);
    EXPECT_FLOAT_EQ(blocked.x, 0.0f);
    EXPECT_FLOAT_EQ(blocked.z, 0.0f);

    const PlayerMoveResult lamp_passable = resolve_player_capsule_slide(
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.8f, 0.35f, {});
    EXPECT_FLOAT_EQ(lamp_passable.x, 1.0f);
    EXPECT_FLOAT_EQ(lamp_passable.z, 0.0f);
}

TEST(PlayerCollisionTests, SlidesAlongBuildingInsteadOfStoppingCompletely) {
    const std::vector<CollisionBox> building = {{1.0f, 4.0f, -2.0f, 2.0f, 0.0f, 5.0f}};
    const PlayerMoveResult moved = resolve_player_capsule_slide(
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.8f, 0.35f, building);
    EXPECT_FLOAT_EQ(moved.x, 0.0f);
    EXPECT_FLOAT_EQ(moved.z, 1.0f);
}

TEST(PlayerCollisionTests, IgnoresObstacleOutsidePlayerHeight) {
    const std::vector<CollisionBox> roof = {{1.0f, 3.0f, -1.0f, 1.0f, 3.0f, 5.0f}};
    const PlayerMoveResult moved = resolve_player_capsule_slide(
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.8f, 0.35f, roof);
    EXPECT_FLOAT_EQ(moved.x, 1.0f);
}
