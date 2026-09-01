# MeshWorld — City Map Generation

This document describes how the world grid is assigned zone and region types, covering both manual JSON-driven configuration and procedural generation.

## Coordinate system

The world is a 2D grid of chunks. Each chunk is `chunk_size_m × chunk_size_m` meters (default: 64m).

- Chunk `(0, 0)` is the top-left (north-west) corner.
- X increases east; Y increases south.
- World position in meters: `(cx * chunk_size_m, cy * chunk_size_m)`.

## Two-level type system

Each chunk has two types:

| Type | Scope | Description |
|------|-------|-------------|
| `ZoneType` | Large biome/area | Describes the broad character of the region (city, forest, desert, …) |
| `RegionType` | Specific chunk role | Describes what this chunk IS within its zone (road, park, house block, …) |

See `include/ZoneType.hpp` and `include/RegionType.hpp` for the full enum lists.

## Manual world config (world.json)

Zones are defined as rectangular overrides. Within each zone, regions can be further overridden:

```json
{
  "version": "1.0",
  "name": "demo_world",
  "style": "central_europe_small_city",
  "seed": 42,
  "map_size_m": 1280,
  "chunk_size_m": 64,
  "grid_w": 20,
  "grid_h": 20,
  "zone_default": "empty",
  "region_default": "empty",

  "zones": [
    {
      "type": "city",
      "region_default": "small_house_block",
      "x_min": 0, "x_max": 19, "y_min": 0, "y_max": 19,
      "regions": [
        { "type": "road",      "x_min": 5, "x_max": 5, "y_min": 0, "y_max": 19 },
        { "type": "road",      "x_min": 0, "x_max": 19, "y_min": 5, "y_max": 5 },
        { "type": "park",      "x_min": 6, "x_max": 9, "y_min": 6, "y_max": 9 },
        { "type": "crossroad", "x_min": 5, "x_max": 5, "y_min": 5, "y_max": 5 }
      ]
    }
  ]
}
```

## Procedural world generation

When `"procedural": true`, `ProceduralWorldGen` uses a coarse-grid hash to assign zone types. Manual zone overrides are applied on top of the procedural base.

```json
{
  "procedural": true,
  "procedural_cell_size": 4,
  "zones": [
    {
      "type": "city",
      "region_default": "small_house_block",
      "x_min": 8, "x_max": 12, "y_min": 8, "y_max": 12
    }
  ]
}
```

### ProceduralWorldGen algorithm

```
cell_x = chunk_x / cell_size
cell_y = chunk_y / cell_size
hash   = fnv1a_64(seed ^ (cell_x * 2654435761ULL) ^ (cell_y * 40503ULL))
zone   = weighted_table[hash % total_weight]
```

Weighted zone table (example distribution):
- `meadow` (weight 4), `forest` (weight 4), `jungle` (weight 3), `desert` (weight 2)
- `mountain` (weight 2), `tundra` (weight 1), `swamp` (weight 2), `ocean` (weight 1)
- `city` (weight 1), `beach` (weight 1)

Procedural region assignment within a zone:
- For `city` zones: roads are placed on a regular grid (every N chunks), crossroads at intersections, parks and squares placed pseudo-randomly, remainder is `small_house_block`.
- For natural zones: region defaults to the zone's natural region (e.g., `forest` → `clearing` for isolated chunks, `forest` → `open` otherwise).

## Road graph and EdgeExits

After zone/region assignment, `WorldMap` computes `EdgeExits` for every chunk:

```cpp
struct EdgeExits {
    bool north_road{false}, south_road{false};
    bool east_road{false},  west_road{false};
    bool north_path{false}, south_path{false};
    bool east_path{false},  west_path{false};
};
```

A chunk has `north_road = true` if the chunk at `(x, y-1)` is a road or crossroad. This ensures road geometries align across boundaries.

`WorldMap::exits(x, y)` returns the precomputed `EdgeExits` for any chunk.

## generator selection

`get_generator(ZoneType, RegionType)` implements a 3-level fallback:

1. Exact match: `(city, park)` → `ParkGenerator`
2. Zone default: `(city, ruins)` → `SmallHouseBlockGenerator` (city's default)
3. Global fallback: `EmptyGenerator`

The factory is in `src/ChunkGenerator.cpp`. New combinations are registered there.

## Procedural city street grid (planned — M7 in NEXT_CLAUDE_PLAN)

For large procedural city zones, a dedicated `ProceduralCityGen` will:

1. Lay a regular street grid (every 5 chunks = one block width) within the city zone bounds.
2. Assign `road` or `crossroad` to grid cells automatically.
3. Fill blocks with a mix of `small_house_block`, `apartment_block`, `shop_street`, `park`, `square`.
4. Place parks every ~8 blocks at intersections.

This replaces the need to manually specify every road in `world.json` for large cities.
