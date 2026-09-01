// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/MapPipeline.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "LuaGeneratorRegistry.hpp"
#include "LuaSandbox.hpp"
#include "Map/ChildGenerator.hpp"
#include "Map/TileEntropy.hpp"
#include "MapValidator.hpp"
#include "PlanetWorld.hpp"
#include "generators/map/CityGenerator.hpp"
#include "generators/map/DistrictGenerator.hpp"
#include "generators/map/PlanetGenerator.hpp"

namespace {
// R105 -- the two map levels with a dedicated native C++ generator beyond
// the generic ChildGenerator fallback (see plan.md's R105 entry / docs/
// audit-baseline.md for the full audit of why every OTHER map-level Lua
// script is redundant with ChildGenerator, but city.lua's block zoning +
// street-grid generation and district.lua's per-quadrant borders are not).
// Match city.lua's/district.lua's own "lua.map.child.level{12,11}.default"
// level numbers exactly, so the C++ fallback for these levels is at least
// as rich as every other level's fallback, not just the generic
// terrain+settlements ChildGenerator already provides everywhere.
constexpr int kCityLevel     = 12;
constexpr int kDistrictLevel = 11;
} // namespace

namespace MeshWorld::Map {

MapPipeline::MapPipeline(MeshWorld::PlanetWorld& world, const PlanetParams& params,
                          std::optional<std::set<int>> named_levels, std::size_t cache_capacity)
    : world_(world), params_(params), named_levels_(std::move(named_levels)),
      cache_capacity_(cache_capacity) {}

bool MapPipeline::is_named_level(int level) const {
    return !named_levels_.has_value() || named_levels_->count(level) > 0;
}

// M105/M106 — recursive descent: ensure parent exists, then generate child.
MapTilePayload MapPipeline::get(const TileCoord& tile) {
    // M113/M218 — check in-memory cache first; a hit refreshes recency.
    {
        auto it = cache_.find(tile);
        if (it != cache_.end()) {
            cache_touch(tile);
            return it->second;
        }
    }

    // M114 — only named levels are persisted; others always regenerate below
    // (deterministically, from the same world entropy + parent chain) rather
    // than reading/writing world_.tile_store() for themselves at all.
    const bool persist_this_level = is_named_level(tile.level);

    if (persist_this_level) {
        auto& store  = world_.tile_store(tile.level);
        auto  loaded = store.load(tile);
        if (loaded) {
            cache_put(tile, *loaded);
            return *loaded;
        }
    }

    const std::uint64_t entropy = tile_entropy(world_.world_entropy(), tile);

    // Ensure the parent exists first (M105: recursive descent). Needed by
    // both the Lua and C++ generation paths below for level > 0.
    std::optional<MapTilePayload> parent_storage;
    const MapTilePayload*         parent_ptr = nullptr;
    if (tile.level > 0) {
        parent_storage = get(tile.parent());
        parent_ptr     = &*parent_storage;
    }

    // M097 — try a registered Lua generator first (additive; mirrors
    // ChunkPipeline's "lua.zone." + region pattern for the map layer).
    // "lua.map.planet.default" is planet.lua (M095). For level > 0, M115
    // makes the lookup level-aware: a level-specific id (e.g.
    // "lua.map.child.level3.default" for continent.lua) is tried first, so a
    // generator can target one specific level (mountain ranges only make
    // sense at continent scale, not at every level). If no level-specific
    // generator is registered, "lua.map.child.default" is the generic
    // fallback id (still unregistered in production as of this task — every
    // level still falls through to ChildGenerator below, unchanged for now).
    const std::string lua_id = [&] {
        if (tile.level == 0) return std::string("lua.map.planet.default");
        const std::string level_id =
            "lua.map.child.level" + std::to_string(tile.level) + ".default";
        return MeshWorld::LuaGeneratorRegistry::instance().has(level_id)
                   ? level_id
                   : std::string("lua.map.child.default");
    }();
    auto&                    lua_registry = MeshWorld::LuaGeneratorRegistry::instance();
    const MeshWorld::MapValidator validator;

    MapTilePayload payload;
    bool           generated_from_lua = false;
    if (lua_registry.has(lua_id)) {
        MeshWorld::MapGenContext mapctx;
        mapctx.tile        = tile;
        mapctx.entropy     = entropy;
        mapctx.sea_level_m = params_.sea_level_m;
        mapctx.parent      = parent_ptr;

        std::string    lua_error;
        MapTilePayload lua_payload =
            MeshWorld::LuaSandbox{}.executeMap(lua_registry.get(lua_id), mapctx, &lua_error);

        // M100 — the Lua payload must also pass structural validation to be
        // accepted; an invalid payload is treated the same as an error.
        if (lua_error.empty() && !lua_payload.elevation.empty()) {
            const MeshWorld::ValidationResult vr = validator.validate(lua_payload);
            if (vr.ok) {
                payload            = std::move(lua_payload);
                generated_from_lua = true;
            } else {
                std::cerr << "[MeshWorld] Lua map generator '" << lua_id
                          << "' produced an invalid payload:\n";
                for (const auto& e : vr.errors)
                    std::cerr << "  ERROR: " << e << "\n";
            }
        }

        if (!generated_from_lua) {
            if (!lua_error.empty())
                std::cerr << "[MeshWorld] Lua map generator '" << lua_id
                          << "' failed: " << lua_error << "\n";
            std::cerr << "[MeshWorld] Falling back to C++ generator for " << lua_id << "\n";
        }
    }

    // C++ fallback — level 0 uses PlanetGenerator; level kCityLevel/
    // kDistrictLevel use their own dedicated generators (R105 — the two
    // levels whose Lua scripts have content with no generic-fallback
    // equivalent: block zoning + street grids, and per-quadrant borders,
    // respectively); every other deeper level uses the generic
    // ChildGenerator with the parent payload (M106 — pass real parent).
    if (!generated_from_lua) {
        if (tile.level == 0) {
            PlanetGenerator gen(params_);
            payload = gen.generate(tile, nullptr, entropy);
        } else if (tile.level == kCityLevel) {
            CityGenerator gen(params_);
            payload = gen.generate(tile, parent_ptr, entropy);
        } else if (tile.level == kDistrictLevel) {
            DistrictGenerator gen(params_);
            payload = gen.generate(tile, parent_ptr, entropy);
        } else {
            ChildGenerator child_gen(params_);
            payload = child_gen.generate(tile, parent_ptr, entropy);
        }

        // M100 — validate before persisting. Logged only, not blocking: the
        // C++ generator is the last resort, there's nowhere further to fall
        // back to (mirrors ChunkPipeline's C++-fallback path, which also
        // validates and logs but always stores).
        const MeshWorld::ValidationResult vr = validator.validate(payload);
        if (!vr.ok) {
            std::cerr << "[MeshWorld] C++ map generator produced an invalid payload:\n";
            for (const auto& e : vr.errors)
                std::cerr << "  ERROR: " << e << "\n";
        }
        for (const auto& w : vr.warnings)
            std::cerr << "[MeshWorld] WARNING: " << w << "\n";
    }

    if (persist_this_level) world_.tile_store(tile.level).store(tile, payload);
    cache_put(tile, payload);
    return payload;
}

void MapPipeline::cache_put(const TileCoord& tile, const MapTilePayload& payload) {
    if (cache_.count(tile)) return;  // already cached
    // Evict the least-recently-used entry when at capacity (cache_order_'s
    // front is always the LRU end -- cache_touch() keeps it that way).
    if (cache_.size() >= cache_capacity_ && !cache_order_.empty()) {
        cache_.erase(cache_order_.front());
        cache_order_.pop_front();
    }
    cache_.emplace(tile, payload);
    cache_order_.push_back(tile);
}

void MapPipeline::cache_touch(const TileCoord& tile) {
    const auto it = std::find(cache_order_.begin(), cache_order_.end(), tile);
    if (it != cache_order_.end()) cache_order_.erase(it);
    cache_order_.push_back(tile);
}

} // namespace MeshWorld::Map
