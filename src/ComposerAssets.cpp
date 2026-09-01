// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ComposerAssets.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3ImportResolver.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <filesystem>
#include <iostream>
#include <map>

#include "AssetRegistry.hpp"
#include "Mc3ScriptRunner.hpp"
#include "NatureAssets.hpp"
#include "ObjectDefinitionLibrary.hpp"
#include "StyleProfile.hpp"

using namespace MeshCraft::Mc3;

namespace MeshWorld {

namespace {

// R112 -- imports one real .mc3lib library (built by
// src/tools/build_mc3lib_content.cpp, checked into data/mc3lib/) through
// the actual R101/R102 Mc3ImportResolver mechanism, registering every
// resolved definition into ObjectDefinitionLibrary (so WorldRenderer's
// existing inject_definitions() render-time fallback resolves it exactly
// like any other id, zero renderer-side changes needed) and, for
// definitions that carry real Mc3AssetMetadata, into AssetRegistry too
// (queryable by category/style tags). LOD proxy definitions built by the
// tool deliberately carry NO assetMetadata of their own -- they still
// land in ObjectDefinitionLibrary (instantiable by id, e.g. via a future
// LOD-tier consumer) but are never independently queryable top-level
// assets, only reachable via their parent's assetMetadata.lods map.
//
// Never a hard error (mirrors BuildingComposer's own "no match -> fall
// through" fallback discipline): if the library file is missing/
// unparsable, this batch is skipped with a stderr warning rather than
// crashing every tool/test that calls register_composer_assets() at
// startup.
void register_mc3lib_batch(const std::string& alias, const std::string& libraryUri) {
    Mc3Document importer;
    importer.model = "composer_asset_importer";
    importer.imports.push_back(Mc3Import{alias, libraryUri, ""});

    const Mc3ImportResolver resolver({std::filesystem::path{"data/mc3lib"}});
    std::map<std::string, std::shared_ptr<Mc3Object>> resolved;
    try {
        resolved = resolver.resolve(importer);
    } catch (const std::exception& e) {
        std::cerr << "[MeshWorld] R112 WARNING: failed to resolve " << libraryUri
                  << ": " << e.what() << "\n";
        return;
    }

    auto& obj_lib = ObjectDefinitionLibrary::instance();
    auto& assets  = AssetRegistry::instance();
    for (const auto& [qualified_id, def] : resolved) {
        if (!def) continue;
        // resolve() keys everything "<alias>:<definitionId>" (see
        // Mc3ImportResolver's own doc comment) -- strip the alias back
        // off before registering. Definitions are authored with globally
        // -unqualified ids (same convention house_gable_default already
        // uses), and Mc3AssetMetadata::lods values reference sibling
        // definitions WITHIN the same library file by that same bare id
        // (build_mc3lib_content.cpp has no way to know what alias a
        // future importer will choose) -- registering under the
        // qualified id here would silently break every lods lookup.
        const auto sep = qualified_id.find(':');
        const std::string id =
            (sep == std::string::npos) ? qualified_id : qualified_id.substr(sep + 1);

        obj_lib.register_definition(id, def);
        if (def->assetMetadata.has_value()) {
            assets.register_asset(AssetEntry{id, def, *def->assetMetadata});
        }
    }
}

// R103/R104 v1 -- compiles one definition already present in `doc`
// (which must already have had its own imports merged into
// doc.definitions -- the script's own def:place()/place_at() calls need
// those imported definitions in the SAME document its children get
// appended to, not a throwaway one) by running its attached script once
// (a one-time "compile" step, not per-instantiation), then registers the
// resulting fully-expanded definition into ObjectDefinitionLibrary and,
// if `register_in_asset_registry`, into AssetRegistry too. Never a hard
// error: a missing definition or script failure is a stderr warning and
// a skipped registration, not a process crash.
void compile_and_register_modular_building(Mc3Document& doc, const std::string& definitionId,
                                            bool register_in_asset_registry) {
    const auto it = doc.definitions.find(definitionId);
    if (it == doc.definitions.end() || !it->second) {
        std::cerr << "[MeshWorld] R103 WARNING: " << definitionId
                  << " missing from urban-buildings\n";
        return;
    }

    const auto script_error = Mc3ScriptRunner{}.run(*it->second, doc);
    if (!script_error.empty()) {
        std::cerr << "[MeshWorld] R103 WARNING: " << definitionId
                  << " script failed: " << script_error << "\n";
        return;
    }

    ObjectDefinitionLibrary::instance().register_definition(definitionId, it->second);
    if (register_in_asset_registry && it->second->assetMetadata.has_value()) {
        AssetRegistry::instance().register_asset(
            AssetEntry{definitionId, it->second, *it->second->assetMetadata});
    }
}

void register_modular_buildings() {
    const std::filesystem::path path = "data/mc3lib/urban-buildings-1.0.0.mc3lib.json";
    Mc3Document doc;
    try {
        doc = Mc3Document::loadFromLibraryJsonFile(path);
        Mc3ImportResolver({std::filesystem::path{"data/mc3lib"}}).resolveAndMergeInto(doc);
    } catch (const std::exception& e) {
        std::cerr << "[MeshWorld] R103 WARNING: failed to load/resolve " << path.string()
                  << ": " << e.what() << "\n";
        return;
    }

    // house.gable.modular_01: a second, additive house candidate
    // alongside house_gable_default (same footprint, so it's a safe
    // AssetRegistry "house" entry).
    compile_and_register_modular_building(doc, "house.gable.modular_01",
                                           /*register_in_asset_registry=*/true);

    // house.rowhouse.modular_01 -- R112 facade_module consumption demo
    // (tiles 6 real facade bay modules along its own front wall via
    // Mc3ScriptRunner::place_at()). Now safely registered into
    // AssetRegistry's "house" category too: BuildingComposer's
    // size-aware matching (its own kHouseWidthTolerance filter against
    // each parcel's real frontage_extent) means this wider (15.6m)
    // candidate only ever gets picked for a wide-class parcel/row
    // (derive_parcels()'s own per-row width-class roll), never placed
    // where it would overlap a standard-width neighbor.
    compile_and_register_modular_building(doc, "house.rowhouse.modular_01",
                                           /*register_in_asset_registry=*/true);

    // house.gable.wide_01 -- R113 size-aware matching's own follow-up: a
    // second wide (15.6m) house, this one with a REAL gable roof (unlike
    // the rowhouse's genuine flat roof), so it carries the "gable_roof"
    // style tag the shipped central_europe_default profile requires --
    // wide-class parcels now have a style-compatible candidate under that
    // profile, not just the flat-roofed rowhouse.
    compile_and_register_modular_building(doc, "house.gable.wide_01",
                                           /*register_in_asset_registry=*/true);
}

} // namespace

void register_composer_assets() {
    auto& obj_lib = ObjectDefinitionLibrary::instance();
    auto& assets  = AssetRegistry::instance();

    // v1's only house asset -- see ObjectDefinitionLibrary.cpp's own
    // make_house_gable() for the geometry and metadata. Each registered
    // id here must already exist in ObjectDefinitionLibrary (that's
    // where the real Mc3Object geometry lives, resolved the same way
    // every other w.instance(...) definition already is) AND must carry
    // real assetMetadata (checked, not assumed -- a missing metadata
    // would silently register an entry with an empty category, which
    // would never match any query()).
    if (auto def = obj_lib.get("house_gable_default"); def && def->assetMetadata.has_value()) {
        assets.register_asset(AssetEntry{"house_gable_default", def, *def->assetMetadata});
    }

    // R126 (BuildingComposer v2, apartment_block) -- v1's only "apartment"
    // category asset. See ObjectDefinitionLibrary.cpp's own
    // make_apartment_block() for the geometry/metadata; registered here
    // the same hand-registered way house_gable_default is above (not
    // through Mc3ImportResolver like the R112 mc3lib batches below), since
    // it's a single, directly-authored C++ definition, not a resolved
    // library import.
    if (auto def = obj_lib.get("apartment.block.wide_01"); def && def->assetMetadata.has_value()) {
        assets.register_asset(AssetEntry{"apartment.block.wide_01", def, *def->assetMetadata});
    }

    // R127 (BuildingComposer v2, shop_street) -- v1's only "shop" category
    // asset. See ObjectDefinitionLibrary.cpp's own make_shop_building()
    // for the geometry/metadata; hand-registered the same way as
    // house_gable_default/apartment.block.wide_01 above.
    if (auto def = obj_lib.get("shop.building.storefront_01"); def && def->assetMetadata.has_value()) {
        assets.register_asset(AssetEntry{"shop.building.storefront_01", def, *def->assetMetadata});
    }

    // R128 (city showcase completion) -- registered for discoverability/
    // consistency with every other C++-authored building-like asset, even
    // though BuildingComposer places it directly by id from ctx.landmark
    // (WorldConfig::landmarks), never through AssetRegistry::query().
    if (auto def = obj_lib.get("landmark.clocktower_01"); def && def->assetMetadata.has_value()) {
        assets.register_asset(AssetEntry{"landmark.clocktower_01", def, *def->assetMetadata});
    }

    // R112 -- the first real mc3lib content batch, genuinely resolved
    // through Mc3ImportResolver (not hand-registered C++ like
    // house_gable_default above) -- see src/tools/build_mc3lib_content.cpp
    // for how data/mc3lib/*.mc3lib.json was built. Roofs/facades/vehicles
    // are a later batch (plan.md's own R112 entry documents the deferred
    // scope); windows/doors are registered and queryable but not yet
    // PLACED anywhere (socket-aware facade placement is R103's job) --
    // street_furniture/prop are placed directly by BuildingComposer since
    // parcel-level placement needs no socket.
    register_mc3lib_batch("windows",          "mc3lib://urban-windows@1.0.0");
    register_mc3lib_batch("doors",            "mc3lib://urban-doors@1.0.0");
    register_mc3lib_batch("street_furniture", "mc3lib://urban-street-furniture@1.0.0");
    register_mc3lib_batch("props",            "mc3lib://urban-props@1.0.0");
    register_mc3lib_batch("roofs",            "mc3lib://urban-roofs@1.0.0");
    register_mc3lib_batch("facades",          "mc3lib://urban-facades@1.0.0");
    register_mc3lib_batch("vehicles",         "mc3lib://urban-vehicles@1.0.0");

    // R103/R104 v1 -- house.gable.modular_01: imports windows/doors/roofs
    // and places them at its own sockets via an attached Lua script
    // (Mc3ScriptRunner), proving R101/R102's import mechanism actually
    // gets consumed dynamically, not just via a single hand-authored
    // fixed transform. See register_modular_buildings()'s own doc
    // comment for the full mechanism.
    register_modular_buildings();

    // R143a -- nature assets are authored in C++ MC3 definitions like the
    // original house assets, then indexed through the same AssetRegistry.
    // They are intentionally separate from the urban mc3lib batches because
    // their first placement consumers are the legacy natural generators.
    register_nature_assets();

    // A single default style profile matching house_gable_default's own
    // styleTags, so v1 actually exercises real style-tag filtering (not
    // just "any house") -- co-located here for v1 simplicity since the
    // asset and its compatible profile are being authored together; a
    // real multi-profile v2 would likely split style-profile
    // registration into its own function/data file.
    StyleProfileRegistry::instance().register_profile(StyleProfile{
        "central_europe_default",
        "central_europe",
        "1890_1930",
        "middle",
        "central_europe", // facadeFamily
        "central_europe", // windowFamily
        "gable_roof",     // roofFamily
        "central_europe_small_city", // materialStyleId (existing StyleRegistry Style, G11)
    });
}

} // namespace MeshWorld
