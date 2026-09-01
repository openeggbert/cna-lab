// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "NatureAssets.hpp"

#include <MeshCraft/Mc3/Mc3AssetMetadata.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include "AssetRegistry.hpp"
#include "ObjectDefinitionLibrary.hpp"
#include "Map/Noise.hpp"

using namespace MeshCraft::Mc3;

namespace MeshWorld {
namespace {

std::shared_ptr<Mc3Object> box(const std::string& id, float x, float y, float z,
                               const std::string& material, float base_y = 0.0f,
                               float cx = 0.0f, float cz = 0.0f) {
    auto object = Mc3Object::makeBox(id, {x, y, z}, material);
    object->transform.position = {cx, base_y + y * 0.5f, cz};
    return object;
}

std::shared_ptr<Mc3Object> cylinder(const std::string& id, float radius, float height,
                                    const std::string& material, float base_y = 0.0f,
                                    float cx = 0.0f, float cz = 0.0f) {
    auto object = Mc3Object::makeCylinder(id, radius, height, 12, material);
    object->transform.position = {cx, base_y + height * 0.5f, cz};
    return object;
}

std::shared_ptr<Mc3Object> sphere(const std::string& id, float radius,
                                  const std::string& material, float y,
                                  float cx = 0.0f, float cz = 0.0f,
                                  float sx = 1.0f, float sy = 1.0f, float sz = 1.0f) {
    auto object = Mc3Object::makeIcoSphere(id, radius, 2, material);
    object->transform.position = {cx, y, cz};
    object->transform.scale = {sx, sy, sz};
    return object;
}

std::shared_ptr<Mc3Object> cone(const std::string& id, float radius, float height,
                                const std::string& material, float base_y = 0.0f) {
    auto object = Mc3Object::makeCone(id, radius, height, 12, material);
    object->transform.position = {0.0f, base_y + height * 0.5f, 0.0f};
    return object;
}

std::shared_ptr<Mc3Object> low_proxy(const std::string& id, const std::string& material,
                                     float height, float radius) {
    return Mc3Object::makeGroup(id, {cylinder("trunk", radius * 0.22f, height, material),
                                     sphere("crown", radius, material, height * 0.70f,
                                            0.0f, 0.0f, 1.0f, 0.65f, 1.0f)});
}

void register_asset(const std::string& id, std::shared_ptr<Mc3Object> object,
                    const std::string& category, const std::string& biome_tag,
                    const std::array<float, 3>& size, const std::string& collision,
                    std::shared_ptr<Mc3Object> low) {
    const std::string low_id = id + ".lod_low";
    Mc3AssetMetadata meta;
    meta.category = category;
    meta.subcategory = biome_tag;
    meta.semanticTags = {"nature", biome_tag};
    meta.styleTags = {biome_tag};
    meta.regionTags = {biome_tag};
    meta.nominalSize = size;
    meta.boundsMin = {-size[0] * 0.5f, 0.0f, -size[2] * 0.5f};
    meta.boundsMax = { size[0] * 0.5f, size[1],  size[2] * 0.5f};
    meta.collisionProxy = collision;
    meta.instancingEligible = true;
    meta.shadowPolicy = "cast_receive";
    meta.selectionWeight = 1.0f;
    meta.license = "MIT";
    meta.provenance = "MeshWorld procedural R143a nature kit";
    meta.sourceGeneratorOrHash = "cpp.object.nature." + id;
    meta.semanticVersion = "0.1.0";
    meta.lods["low"] = low_id;
    object->assetMetadata = std::move(meta);

    auto& definitions = ObjectDefinitionLibrary::instance();
    definitions.register_definition(low_id, std::move(low));
    definitions.register_definition(id, object);
    AssetRegistry::instance().register_asset(AssetEntry{id, std::move(object),
        *definitions.get(id)->assetMetadata});
}

} // namespace

void register_nature_assets() {
    register_asset("nature.tree.temperate.oak_01",
        Mc3Object::makeGroup("oak", {cylinder("trunk", 0.20f, 5.0f, "wood_bark_dark"),
            sphere("crown_low", 2.4f, "foliage_oak", 4.3f, 0.0f, 0.0f, 1.2f, 0.75f, 1.2f),
            sphere("crown_high", 1.5f, "foliage_oak", 6.1f)}),
        "nature_tree", "temperate_forest", {5.8f, 7.6f, 5.8f}, "box",
        low_proxy("oak_low", "foliage_oak", 4.0f, 1.5f));
    register_asset("nature.tree.temperate.pine_01",
        Mc3Object::makeGroup("pine", {cylinder("trunk", 0.16f, 5.8f, "wood_bark_dark"),
            cone("crown_low", 2.0f, 2.4f, "foliage_pine", 3.0f),
            cone("crown_high", 1.35f, 2.3f, "foliage_pine", 4.8f)}),
        "nature_tree", "temperate_forest", {4.2f, 7.1f, 4.2f}, "box",
        low_proxy("pine_low", "foliage_pine", 4.5f, 1.2f));
    register_asset("nature.tree.jungle.canopy_01",
        Mc3Object::makeGroup("canopy", {cylinder("trunk", 0.34f, 7.0f, "wood_bark_dark"),
            box("buttress_a", 2.2f, 0.35f, 0.45f, "wood_bark_dark", 0.2f),
            box("buttress_b", 0.45f, 0.35f, 2.2f, "wood_bark_dark", 0.2f),
            sphere("crown", 3.4f, "foliage_tropical", 7.5f, 0.0f, 0.0f, 1.25f, 0.70f, 1.25f)}),
        "nature_tree", "jungle", {8.5f, 10.0f, 8.5f}, "box",
        low_proxy("canopy_low", "foliage_tropical", 5.5f, 2.0f));
    register_asset("nature.tree.jungle.bamboo_01",
        Mc3Object::makeGroup("bamboo", {cylinder("cane_a", 0.08f, 6.2f, "bamboo_cane", 0.0f, -0.35f),
            cylinder("cane_b", 0.07f, 5.3f, "bamboo_cane", 0.0f, 0.28f, 0.18f),
            cylinder("cane_c", 0.06f, 4.7f, "bamboo_cane", 0.0f, 0.12f, -0.32f),
            sphere("leaves", 1.2f, "foliage_tropical", 5.0f, 0.0f, 0.0f, 1.0f, 1.4f, 1.0f)}),
        "nature_tree", "jungle", {2.8f, 6.5f, 2.8f}, "none",
        low_proxy("bamboo_low", "foliage_tropical", 3.5f, 0.8f));
    register_asset("nature.plant.desert.barrel_01",
        Mc3Object::makeGroup("barrel", {cylinder("body", 0.52f, 1.1f, "cactus_green"),
            sphere("crown", 0.50f, "cactus_green", 1.05f, 0.0f, 0.0f, 1.0f, 0.45f, 1.0f)}),
        "nature_plant", "desert", {1.2f, 1.3f, 1.2f}, "none",
        low_proxy("barrel_low", "cactus_green", 0.7f, 0.4f));
    register_asset("nature.plant.desert.agave_01",
        Mc3Object::makeGroup("agave", {box("leaf_a", 0.15f, 0.12f, 1.6f, "plant_scrub", 0.10f),
            box("leaf_b", 1.6f, 0.12f, 0.15f, "plant_scrub", 0.10f),
            sphere("heart", 0.35f, "plant_scrub", 0.30f)}),
        "nature_plant", "desert", {2.0f, 0.7f, 2.0f}, "none",
        low_proxy("agave_low", "plant_scrub", 0.4f, 0.5f));
    register_asset("nature.rock.alpine.spire_01",
        Mc3Object::makeGroup("spire", {cone("base", 1.7f, 3.8f, "rock_cliff"),
            cone("tip", 0.85f, 2.0f, "rock_grey", 3.0f)}),
        "nature_rock", "mountain", {3.4f, 5.0f, 3.4f}, "box",
        low_proxy("spire_low", "rock_grey", 1.2f, 1.0f));
    register_asset("nature.rock.alpine.outcrop_01",
        Mc3Object::makeGroup("outcrop", {box("base", 3.4f, 1.4f, 2.2f, "rock_grey"),
            sphere("cap", 1.3f, "rock_cliff", 1.4f, 0.45f, 0.0f, 1.2f, 0.7f, 0.9f)}),
        "nature_rock", "mountain", {4.2f, 2.4f, 3.0f}, "box",
        low_proxy("outcrop_low", "rock_grey", 0.8f, 1.1f));
    register_asset("nature.tree.swamp.cypress_01",
        Mc3Object::makeGroup("cypress", {cylinder("trunk", 0.28f, 5.0f, "wood_bark_dead"),
            box("root_a", 2.2f, 0.28f, 0.35f, "wood_bark_dead", 0.10f),
            box("root_b", 0.35f, 0.28f, 2.2f, "wood_bark_dead", 0.10f),
            sphere("crown", 2.6f, "foliage_willow", 5.3f, 0.0f, 0.0f, 1.0f, 0.7f, 1.0f)}),
        "nature_tree", "swamp", {5.4f, 7.2f, 5.4f}, "box",
        low_proxy("cypress_low", "foliage_willow", 3.5f, 1.3f));
    register_asset("nature.tree.swamp.snag_01",
        Mc3Object::makeGroup("snag", {cylinder("trunk", 0.24f, 4.1f, "wood_bark_dead"),
            box("branch_w", 2.5f, 0.20f, 0.24f, "wood_bark_dead", 2.8f, -0.85f),
            box("branch_e", 1.7f, 0.16f, 0.20f, "wood_bark_dead", 3.45f, 0.58f),
            box("root", 2.1f, 0.22f, 0.32f, "wood_bark_dead", 0.08f)}),
        "nature_tree", "swamp", {3.1f, 4.3f, 2.2f}, "box",
        low_proxy("snag_low", "wood_bark_dead", 2.2f, 0.65f));
    register_asset("nature.prop.coast.driftwood_01",
        Mc3Object::makeGroup("driftwood", {box("log", 4.2f, 0.42f, 0.48f, "bark_driftwood"),
            box("branch", 1.8f, 0.18f, 0.20f, "bark_driftwood", 0.32f, 0.65f)}),
        "nature_prop", "coast", {4.4f, 0.6f, 1.5f}, "none",
        low_proxy("driftwood_low", "bark_driftwood", 0.25f, 1.2f));
    register_asset("nature.prop.coast.root_01",
        Mc3Object::makeGroup("shore_root", {box("root", 2.8f, 0.34f, 0.52f, "bark_driftwood"),
            box("prong_n", 0.26f, 0.24f, 1.9f, "bark_driftwood", 0.20f, 0.48f),
            box("prong_s", 0.22f, 0.18f, 1.4f, "bark_driftwood", 0.18f, -0.55f)}),
        "nature_prop", "coast", {3.0f, 0.55f, 2.5f}, "none",
        low_proxy("shore_root_low", "bark_driftwood", 0.22f, 0.9f));
}

const AssetEntry* pick_nature_asset(const ChunkContext& ctx, const std::string& category,
                                    const std::string& biome_tag, std::size_t ordinal) {
    const auto candidates = AssetRegistry::instance().query(category, {biome_tag});
    if (candidates.empty()) return nullptr;
    const auto hash = Map::noise::hash2i(static_cast<std::int64_t>(ordinal),
                                         4300 + static_cast<int>(category.size()), ctx.seed);
    return candidates[hash % candidates.size()];
}

const std::string& resolve_nature_asset_id(const AssetEntry& asset, const ChunkContext& ctx) {
    if (ctx.lod <= 1) {
        const auto it = asset.meta.lods.find("low");
        if (it != asset.meta.lods.end()) return it->second;
    }
    return asset.id;
}

} // namespace MeshWorld
