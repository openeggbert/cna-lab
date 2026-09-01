// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "BuildingComposer.hpp"

#include "AssetRegistry.hpp"
#include "GenerationMetadata.hpp"
#include "MC3Writer.hpp"
#include "Map/Noise.hpp"
#include "ObjectDefinitionLibrary.hpp"
#include "Parcel.hpp"
#include "StyleProfile.hpp"

#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace MeshWorld {

namespace {
// Distinct axis constants for this composer's own asset-pick rolls, same
// "one axis constant per kind of deterministic roll" convention
// CityGenerator.cpp/DistrictGenerator.cpp already established (R105).
constexpr int kAssetPickAxis = 4001;
constexpr int kStreetFurniturePickAxis = 4002;
constexpr int kPropPickAxis = 4003;
constexpr int kVehiclePickAxis = 4004;
constexpr int kYardTreePickAxis = 4005;

// R114 (city showcase) -- a small, fixed set of real yard trees
// (ObjectDefinitionLibrary.cpp's own tree_* definitions). These carry no
// Mc3AssetMetadata (never registered into AssetRegistry, unlike every
// other category above), so they're referenced directly by id + a
// has()-check instead of AssetRegistry::query() -- the simplest way to
// give composer-driven residential blocks real tree coverage without a
// new asset-metadata/registration pass for content that already exists.
constexpr std::array<const char*, 4> kYardTreeIds = {
    "tree_oak", "tree_lime", "tree_birch", "tree_apple"};

// R113 (size-aware matching) -- how close a candidate house's own
// nominalSize[0] must be to a parcel's frontage_extent to be considered a
// real match for that parcel. house_gable_default/house.gable.modular_01
// are 10.6m against a 10.0m standard parcel (0.6m off); house.rowhouse
// .modular_01 is 15.6m against a 15.6m wide parcel (exact) -- 1.0m
// comfortably covers both without being loose enough to match the wrong
// size class. R126 -- reused as-is for apartment_block's own single size
// class (apartment.block.wide_01's 14.6m nominalSize[0] against a 14.6m
// parcel, an exact match), not renamed, since the tolerance itself isn't
// house-specific mechanism, only its doc comment's original context.
constexpr float kHouseWidthTolerance = 1.0f;

// R128 (city showcase LOD wiring) -- how far (inclusive) ctx.lod must
// drop before a composer-placed instance swaps to its own authored
// low-detail proxy. ChunkGenerator.hpp's own `ctx.lod` field (0=coarse
// .. 4=near-camera) has existed since it was documented as `ctx.lod` in
// docs/lua-generators.md, but per its own doc comment was never actually
// consulted by anything -- a real distance-based LOD *picker* that sets
// ctx.lod from live camera distance remains explicitly future work; this
// only makes the already-authored Mc3AssetMetadata.lods DATA
// (build_mc3lib_content.cpp's own lowLodId parameter) finally get
// consulted once some future caller does set a coarser ctx.lod.
constexpr int kFarLodThreshold = 1;

// Resolves the id BuildingComposer should actually instance for `asset`:
// its own authored "low" LOD proxy id (Mc3AssetMetadata.lods) once
// ctx.lod drops to/below kFarLodThreshold AND the asset actually has one
// registered, otherwise the asset's own full-detail id unchanged (the
// safe default -- every C++-authored building asset, e.g.
// house_gable_default/apartment.block.wide_01/shop.building.storefront_01,
// carries no lods entry at all and is always instanced at full detail;
// only R112 mc3lib content, e.g. streetlamp.classic_01, currently has a
// real "low" entry to resolve to).
const std::string& resolve_instance_id(const AssetEntry& asset, const ChunkContext& ctx) {
    if (ctx.lod <= kFarLodThreshold) {
        const auto it = asset.meta.lods.find("low");
        if (it != asset.meta.lods.end()) return it->second;
    }
    return asset.id;
}

// R129 (BuildingComposer v3, square) -- a hybrid composition path, not a
// full asset-driven rewrite (user-approved design): ports
// SquareGenerator.cpp's own literal fountain/plinth/water-bowl/jet/paths/
// conditional-monument geometry unchanged (a plaza centerpiece is
// one-of-a-kind civic content, same reasoning R128's landmark already
// used), but sources the 4 corner lamps from a real
// AssetRegistry::query("street_furniture", ...) pick -- the exact same
// category/mechanism compose_chunk() already uses for house/apartment/
// shop-block lamps -- so style profiles finally affect squares too.
// Benches ("bench_stone") and corner trees (kYardTreeIds) stay
// direct-by-id references, mirroring the yard-tree convention below (no
// metadata-tagged bench asset exists yet, and inventing one is out of
// scope). Deliberately does NOT call derive_parcels() -- a square is one
// whole-chunk civic composition, not a row of parcels -- and returns
// std::nullopt (never a partial/broken plaza) when no street_furniture
// asset is registered, so ChunkPipeline falls through to the untouched,
// legacy SquareGenerator.cpp exactly like every other region's own
// nullopt-means-fall-through contract.
std::optional<std::string> compose_square(const ChunkContext& ctx) {
    const StyleProfile* profile = StyleProfileRegistry::instance().pick_for(ctx.seed);
    std::vector<std::string> furniture_tags;
    if (profile && !profile->facadeFamily.empty()) furniture_tags.push_back(profile->facadeFamily);

    const auto lamp_candidates = AssetRegistry::instance().query("street_furniture", furniture_tags);
    if (lamp_candidates.empty()) return std::nullopt;

    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.composer.square").to_json()
    );
    const float s = ctx.chunk_size_m;
    const float c = s * 0.5f;

    w.ground("cobblestone_square");

    // Central fountain / monument -- literal geometry, ported unchanged
    // from SquareGenerator.cpp.
    w.cylinder("plinth",        c, c, 3.5f, 1.5f, "stone_granite");
    w.cylinder("fountain_ring", c, c, 3.0f, 1.0f, "stone_light", 1.5f);
    w.cylinder("water_bowl",    c, c, 2.5f, 0.4f, "water",       2.5f);
    w.cylinder("jet",           c, c, 0.08f, 2.0f,"water",       2.9f);

    // 4 corner lamp posts -- now real AssetRegistry-queried instances,
    // replacing SquareGenerator.cpp's own hardcoded
    // w.cylinder(..., "metal_lamp_ornate") calls.
    const float lr = 12.0f;
    const std::array<std::pair<float, float>, 4> lamp_corners = {{
        {c + lr, c - lr}, {c - lr, c - lr}, {c + lr, c + lr}, {c - lr, c + lr}
    }};
    for (std::size_t i = 0; i < lamp_corners.size(); ++i) {
        const auto hl = Map::noise::hash2i(static_cast<std::int64_t>(i), kStreetFurniturePickAxis, ctx.seed);
        const auto* lamp = lamp_candidates[hl % lamp_candidates.size()];
        w.instance("lamp_" + std::to_string(i), resolve_instance_id(*lamp, ctx),
                   lamp_corners[i].first, lamp_corners[i].second, 0.0f);
    }

    // 8 benches around the fountain -- direct-by-id, unchanged from
    // SquareGenerator.cpp (mirrors the yard-tree direct-by-id convention
    // in the house/apartment/shop path below: no metadata-tagged bench
    // asset exists yet, and inventing one is out of scope for this task).
    const float br = 7.0f;
    const std::array<float, 8> angles = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f};
    for (std::size_t i = 0; i < angles.size(); ++i) {
        const float rad = angles[i] * 3.14159f / 180.0f;
        const float bx  = c + std::cos(rad) * br;
        const float bz  = c + std::sin(rad) * br;
        w.instance("bench_" + std::to_string(i), "bench_stone", bx, bz, angles[i]);
    }

    // Paths from edges to fountain in cardinal directions -- literal
    // geometry, ported unchanged from SquareGenerator.cpp.
    const float pw = 4.0f;
    w.plane("path_n", c - pw*0.5f, 0,   pw, c - br - 1.0f, "cobblestone_path");
    w.plane("path_s", c - pw*0.5f, c + br + 1.0f, pw, c - br - 1.0f, "cobblestone_path");
    w.plane("path_w", 0,   c - pw*0.5f, c - br - 1.0f, pw, "cobblestone_path");
    w.plane("path_e", c + br + 1.0f, c - pw*0.5f, c - br - 1.0f, pw, "cobblestone_path");

    // Decorative trees at 4 outer corners -- direct-by-id (kYardTreeIds),
    // same convention as the house/apartment/shop yard trees below.
    const std::array<std::pair<float, float>, 4> tree_corners = {{
        {c + 20.0f, c - 20.0f}, {c - 20.0f, c - 20.0f}, {c + 20.0f, c + 20.0f}, {c - 20.0f, c + 20.0f}
    }};
    for (std::size_t i = 0; i < tree_corners.size(); ++i) {
        const auto ht = Map::noise::hash2i(static_cast<std::int64_t>(i), kYardTreePickAxis, ctx.seed);
        const char* tree_id = kYardTreeIds[ht % kYardTreeIds.size()];
        if (ObjectDefinitionLibrary::instance().has(tree_id)) {
            w.instance("tree_" + std::to_string(i), tree_id,
                       tree_corners[i].first, tree_corners[i].second,
                       static_cast<float>(i) * 45.0f);
        }
    }

    // MAP17 -- a monument plinth at the square's edge when it's the named
    // center of a real settlement, ported unchanged from SquareGenerator.cpp.
    if (ctx.map_context.available && !ctx.map_context.nearest_place_name.empty())
        w.cylinder("monument", c, c - br - 3.5f, 1.2f, 3.0f, "stone_granite");

    return w.build();
}
} // namespace

std::optional<std::string> BuildingComposer::compose_chunk(const ChunkContext& ctx) const {
    // R129 -- square is one whole-chunk civic composition, not a row of
    // parcels; handled by its own path entirely, before derive_parcels()
    // (which returns {} for square anyway -- Parcel.cpp is intentionally
    // unchanged by this task).
    if (ctx.region == RegionType::square) return compose_square(ctx);

    const auto parcels = derive_parcels(ctx);
    if (parcels.empty()) return std::nullopt;

    const bool is_apartment = (ctx.region == RegionType::apartment_block);
    const bool is_shop      = (ctx.region == RegionType::shop_street);

    const StyleProfile* profile = StyleProfileRegistry::instance().pick_for(ctx.seed);

    // R126/R127 -- apartment_block/shop_street query their own category
    // and deliberately omit StyleProfile::roofFamily: v1's only apartment
    // asset (apartment.block.wide_01) and only shop asset
    // (shop.building.storefront_01) are both real flat-roofed buildings,
    // so requiring the shipped central_europe_default profile's
    // "gable_roof" tag would make either NEVER match (there is no
    // gable-roofed apartment/shop asset, nor should there be) -- same
    // asymmetric-tags precedent the street_furniture/prop queries below
    // already use (facadeFamily only, no roofFamily, since neither has a
    // roof either).
    std::vector<std::string> required_tags;
    if (profile) {
        if (!profile->facadeFamily.empty()) required_tags.push_back(profile->facadeFamily);
        if (!is_apartment && !is_shop && !profile->roofFamily.empty())
            required_tags.push_back(profile->roofFamily);
    }

    const std::string building_category = is_apartment ? "apartment" : is_shop ? "shop" : "house";
    auto candidates = AssetRegistry::instance().query(building_category, required_tags);
    if (candidates.empty()) {
        // v1 has no partial-match fallback (docs/world-composer-design.md
        // §9's own fallback spec): a style-tag mismatch against an
        // otherwise-available "house"/"apartment"/"shop" category is
        // treated the same as no matching assets existing at all -- fall
        // through to the existing chain rather than composing with a
        // style-incoherent asset.
        return std::nullopt;
    }

    // R112 -- street_furniture/prop are placed directly at parcel level
    // (no socket needed, unlike window/door which need R103's facade-
    // socket placement): one streetlamp near the parcel's street-facing
    // corner and one mailbox near the other, mirroring real curbside
    // placement. Missing categories are NOT a fallback trigger for the
    // whole chunk (houses remain the primary content) -- each is simply
    // skipped if AssetRegistry has no match, same per-item-optional
    // discipline as ContainmentRuleRegistry's own probability-gated
    // children.
    std::vector<std::string> furniture_tags;
    if (profile && !profile->facadeFamily.empty()) furniture_tags.push_back(profile->facadeFamily);
    const auto lamp_candidates    = AssetRegistry::instance().query("street_furniture", furniture_tags);
    const auto mailbox_candidates = AssetRegistry::instance().query("prop", furniture_tags);

    // Vehicles aren't regionally style-tagged (a parked car's model choice
    // isn't a "central_europe vs modern" facade decision) -- query with no
    // required tags so both variants are real candidates, unlike the
    // single-candidate lamp/mailbox selection above.
    const auto vehicle_candidates = AssetRegistry::instance().query("vehicle");

    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.composer." + to_string(ctx.region)).to_json()
    );
    // R127 -- shop_street uses a paved ground surface (cross-checked
    // against ShopStreetGenerator.cpp's own w.ground("cobblestone")
    // call), not the residential grass_garden house/apartment blocks use.
    w.ground(is_shop ? "cobblestone" : "grass_garden");

    for (std::size_t i = 0; i < parcels.size(); ++i) {
        const auto& parcel = parcels[i];

        // R113 (size-aware matching) -- filter to candidates whose real
        // nominalSize[0] roughly matches THIS parcel's own frontage_extent
        // (not parcel.width/depth, which SWAP by row orientation --
        // frontage_extent stays the real building width regardless), so a
        // wide row (house.rowhouse.modular_01) and a standard row
        // (house_gable_default/house.gable.modular_01) each get a
        // correctly-sized house instead of one uniform pool for the whole
        // chunk. No match for THIS parcel's size is a per-item skip (like
        // lamp/mailbox/vehicle below), not a whole-chunk fallback --
        // candidates.empty() above already covers "no houses exist at
        // all".
        std::vector<const AssetEntry*> size_matching;
        for (const auto* c : candidates) {
            if (std::abs(c->meta.nominalSize[0] - parcel.frontage_extent) < kHouseWidthTolerance)
                size_matching.push_back(c);
        }

        if (!size_matching.empty()) {
            const auto h = Map::noise::hash2i(static_cast<std::int64_t>(i), kAssetPickAxis, ctx.seed);
            const auto* asset = size_matching[h % size_matching.size()];
            const char* prefix = is_apartment ? "apartment_" : is_shop ? "shop_" : "house_";
            w.instance(prefix + std::to_string(i), resolve_instance_id(*asset, ctx),
                       parcel.center_x, parcel.center_z, parcel.rotation_y);
        }

        // R113 v3 -- generic for any of the 4 row orientations: "right"
        // is perpendicular to the parcel's own facing direction
        // (normal_x, normal_z), used to offset street furniture to
        // either side of the frontage; "forward a bit further" (beyond
        // street_x/street_z, further from the house) parks a vehicle
        // past the curb line. Replaces v1/v2's own X-only/Z-only offset
        // math, which only worked for north/south rows.
        const float right_x = -parcel.normal_z;
        const float right_z =  parcel.normal_x;
        const float half_frontage_plus_yard = parcel.frontage_extent / 2.0f + 1.0f;

        if (!lamp_candidates.empty()) {
            const auto hl = Map::noise::hash2i(static_cast<std::int64_t>(i), kStreetFurniturePickAxis, ctx.seed);
            const auto* lamp = lamp_candidates[hl % lamp_candidates.size()];
            w.instance("streetlamp_" + std::to_string(i), resolve_instance_id(*lamp, ctx),
                       parcel.street_x + right_x * half_frontage_plus_yard,
                       parcel.street_z + right_z * half_frontage_plus_yard,
                       parcel.rotation_y);
        }

        if (!mailbox_candidates.empty()) {
            const auto hm = Map::noise::hash2i(static_cast<std::int64_t>(i), kPropPickAxis, ctx.seed);
            const auto* mailbox = mailbox_candidates[hm % mailbox_candidates.size()];
            w.instance("mailbox_" + std::to_string(i), resolve_instance_id(*mailbox, ctx),
                       parcel.street_x - right_x * half_frontage_plus_yard,
                       parcel.street_z - right_z * half_frontage_plus_yard,
                       parcel.rotation_y);
        }

        if (!vehicle_candidates.empty()) {
            // Parked on the street itself, a bit further out (along the
            // parcel's own facing direction) than the street reference
            // line.
            constexpr float kVehicleStreetOffset = 1.5f;
            const auto hv = Map::noise::hash2i(static_cast<std::int64_t>(i), kVehiclePickAxis, ctx.seed);
            const auto* vehicle = vehicle_candidates[hv % vehicle_candidates.size()];
            w.instance("vehicle_" + std::to_string(i), resolve_instance_id(*vehicle, ctx),
                       parcel.street_x + parcel.normal_x * kVehicleStreetOffset,
                       parcel.street_z + parcel.normal_z * kVehicleStreetOffset,
                       parcel.rotation_y);
        }

        // R114 (city showcase) -- one real yard tree per parcel, in the
        // back yard (opposite the street, along -normal) so it never
        // conflicts with the house's own front-facing facade/sockets or
        // the street-side lamp/mailbox/vehicle placements above. A fixed
        // real offset (roughly half a standard parcel's own depth, see
        // Parcel.cpp's kParcelDepth) rather than a derived one -- Parcel
        // doesn't expose its own along-normal depth directly, and every
        // parcel's real depth is close enough to this estimate regardless
        // of width class (same "estimate, not exact" precedent
        // Parcel.cpp's own kParcelDepth already established).
        constexpr float kYardTreeBackOffset = 4.0f;
        const auto ht = Map::noise::hash2i(static_cast<std::int64_t>(i), kYardTreePickAxis, ctx.seed);
        const char* tree_id = kYardTreeIds[ht % kYardTreeIds.size()];
        if (ObjectDefinitionLibrary::instance().has(tree_id)) {
            w.instance("tree_" + std::to_string(i), tree_id,
                       parcel.center_x - parcel.normal_x * kYardTreeBackOffset,
                       parcel.center_z - parcel.normal_z * kYardTreeBackOffset,
                       0.0f);
        }
    }

    // R128 (city showcase completion) -- a single, fixed-position landmark
    // for THIS chunk, if WorldConfig::landmarks configured one (see
    // ChunkPipeline::build_context()). Deliberately NOT derived through
    // derive_parcels()/AssetRegistry's category+width-matching query above
    // -- it's placed once, unconditionally, at its own configured local
    // (x, z), independent of the parcel loop entirely.
    if (!ctx.landmark.definition_id.empty()) {
        w.instance("landmark", ctx.landmark.definition_id,
                   ctx.landmark.x, ctx.landmark.z, ctx.landmark.rotation_y);
    }

    return w.build();
}

} // namespace MeshWorld
