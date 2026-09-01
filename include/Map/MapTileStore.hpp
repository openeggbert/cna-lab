// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Map/MapTilePayload.hpp"
#include "Map/TileCoord.hpp"

struct sqlite3;

namespace MeshWorld::Map {

// Per-level SQLite store of MapTilePayloads. One DB file per quadtree level:
//   <world_dir>/map_level{level}.db
// Mirrors PersistentWorldMap's open-or-create + WAL approach. M018 establishes
// the handle lifecycle (open/create on construction, close on destruction),
// M019 the `tile (x, y) PRIMARY KEY + payload` schema, and M020/M021 the
// store/load/has CRUD (payload encoded via MapPayloadCodec).
//
// The tile's level is implied by the DB file, so the (x, y) key omits it; a
// store/load/has whose TileCoord.level != level() is a programming error and
// throws std::invalid_argument.
class MapTileStore {
public:
    MapTileStore(const std::string& world_dir, int level);
    ~MapTileStore();

    MapTileStore(const MapTileStore&)            = delete;
    MapTileStore& operator=(const MapTileStore&) = delete;

    int                level() const { return level_; }
    const std::string& path()  const { return path_; }
    bool               is_open() const { return db_ != nullptr; }

    // Upsert: insert the tile or overwrite an existing one with the same (x, y).
    void store(const TileCoord& tile, const MapTilePayload& payload);

    // Load the payload for a tile, or std::nullopt if it has never been stored.
    std::optional<MapTilePayload> load(const TileCoord& tile) const;

    // True if a payload exists for the tile, without decoding it.
    bool has(const TileCoord& tile) const;

    // Every tile coordinate stored at this level (this store's level()),
    // undefined order. Used by MAP15 M231 (MBTiles-style export) and MAP13
    // M210 (MapValidator) to enumerate what's already generated without
    // decoding each payload.
    std::vector<TileCoord> list_tiles() const;

private:
    int         level_{0};
    std::string path_;
    sqlite3*    db_{nullptr};
};

} // namespace MeshWorld::Map
