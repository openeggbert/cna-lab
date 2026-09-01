// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Map/MapGenerator.hpp"
#include "generators/map/PlanetGenerator.hpp"  // for PlanetParams

namespace MeshWorld::Map {

// R105 — native C++ port of generators/lua/map/city.lua (level 12, "City"
// scale). Unlike the other 16 map-level Lua scripts (see plan.md's R105
// entry / docs/audit-baseline.md for the full audit), city.lua's zoning
// (Map::ZoneCandidate block placement) and terrain-following street-grid
// generation have NO existing C++ equivalent anywhere — ChildGenerator.cpp
// covers elevation/mountains/hydrology/settlements/roads/countries for
// every level already, but never touches urban block zoning or a dense
// per-cell street grid. This class is that missing piece: same elevation/
// temperature/moisture computation ChildGenerator.cpp already uses (kept
// consistent, not reinvented), built through MapBuilder exactly the way
// LuaSandbox::executeMap() drives it for the Lua path, so this produces
// the SAME kind of payload structure city.lua does — just computed
// natively instead of interpreted.
class CityGenerator : public MapGenerator {
public:
    explicit CityGenerator(PlanetParams params);

    MapTilePayload generate(const TileCoord&      tile,
                            const MapTilePayload* parent,
                            std::uint64_t         entropy) const override;

private:
    PlanetParams params_;
};

} // namespace MeshWorld::Map
