// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Map/MapGenerator.hpp"
#include "generators/map/PlanetGenerator.hpp"  // for PlanetParams

namespace MeshWorld::Map {

// R105 — native C++ port of generators/lua/map/district.lua (level 11,
// "district layout"). Splits the tile into 4 axis-aligned quadrants
// (NW/NE/SW/SE); each quadrant with a suitable (non-ocean, non-mountain)
// site becomes its own named district (a Border polygon covering exactly
// that quadrant) plus one town, mirroring country.lua's own "border ==
// this tile's/quadrant's own rectangle, not a traced political boundary"
// honesty (see district.lua's own header for why no real Voronoi/growth
// partition is used). The secondary half of R105's audited gap — see
// CityGenerator.hpp for the primary (city.lua) half.
class DistrictGenerator : public MapGenerator {
public:
    explicit DistrictGenerator(PlanetParams params);

    MapTilePayload generate(const TileCoord&      tile,
                            const MapTilePayload* parent,
                            std::uint64_t         entropy) const override;

private:
    PlanetParams params_;
};

} // namespace MeshWorld::Map
