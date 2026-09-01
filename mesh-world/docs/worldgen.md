# MeshWorld — Deterministic Procedural World Generation

## Core principle

Given the same world seed and world config, the generator must always produce the same output for any given chunk. This is deterministic generation. It allows:

- the world to be regenerated from scratch on any machine
- individual chunks to be regenerated without storing every chunk permanently
- the cache (MCB files) to be treated as an optimization, not ground truth

---

## World seed

The world seed is a 64-bit integer stored in `world.json`. It drives all random decisions in the generator. Per-chunk generation uses a derived seed:

```
chunk_seed = hash(world_seed, chunk_x, chunk_y)
```

This ensures each chunk has its own reproducible randomness without cross-chunk interference.

---

## Map size and chunk grid

| Parameter | First demo | Notes |
|-----------|-----------|-------|
| World size | 1280 m × 1280 m | 1.28 km × 1.28 km |
| Chunk size | 64 m × 64 m | Fixed for v1 |
| Grid | 20 × 20 = 400 chunks | All addressable at launch |

Chunk coordinates are zero-based integers: (0,0) is the top-left corner, (19,19) is the bottom-right.

World-space to chunk-space conversion:

```
chunk_x = floor(world_x / chunk_size_m)
chunk_y = floor(world_z / chunk_size_m)
```

---

## District map

The district map assigns a `DistrictType` to every (x, y) chunk coordinate. It is computed from:

1. The world seed.
2. The list of district overrides in `world.json`.
3. Default fill rules (e.g., `empty_field` for unspecified chunks).

Overrides are applied in order from `world.json`. Later overrides take priority.

### District types

| Type | Description |
|------|-------------|
| `road` | Linear road section (1-2 lanes) |
| `crossroad` | Road intersection |
| `park` | Green space: grass, trees, benches, paths |
| `small_house_block` | Low-density residential |
| `apartment_block` | Mid-rise residential |
| `shop_street` | Retail ground floor, pedestrian street |
| `square` | Central public plaza |
| `river_bank` | Embankment, water edge |
| `bridge` | Crossing over water |
| `empty_field` | Undeveloped land |

---

## Road and path graph

The city map includes a road graph. Each chunk edge (N, S, E, W) can have:

- `road_exit` — a road continues across this edge
- `path_exit` — a footpath continues across this edge
- `none` — no connection

The road graph is computed once from the district map: road and crossroad chunks connect to neighboring road chunks on shared edges. Park and square chunks may generate path exits on edges shared with pedestrian-friendly districts.

**Connection rule:** if chunk A has a `road_exit` on its east edge, chunk B (A's eastern neighbor) must have a `road_exit` on its west edge, and both must generate geometry that connects correctly.

---

## Per-chunk generation

### Inputs

- `chunk_x`, `chunk_y` — grid coordinates
- `chunk_seed` — derived from world seed + coordinates
- `district_type` — from the district map
- `style` — city style (e.g., `central_europe_small_city`)
- `neighbor_context` — district and edge exits of N/S/E/W neighbors
- `constraints` — max objects, max triangles, allowed categories

### Generation steps

1. Lay down the ground plane (terrain, pavement, or grass).
2. Place roads/paths on the correct edges based on neighbor exits.
3. Fill the interior according to district type and style rules.
4. Apply per-style object selection (which tree models, bench styles, etc.).
5. Verify all objects are within bounds.
6. Verify object count and triangle budget.
7. Emit `mc3.xml`.

### Determinism guarantee

All random choices use the chunk seed. No `std::rand()`, no time-based seeds, no UUIDs. The same inputs always produce the same MC3 output.

---

## Style system

A style defines which assets and layouts are valid for each district type. For the first version:

**`central_europe_small_city`:**

- Brick and plaster facades for residential.
- Cobblestone squares and streets.
- Lime trees and horse chestnuts for parks and streets.
- Cast-iron lamp posts.
- Flat or shallow-pitched roofs for shops.

Styles are defined in code for v1. A data-driven style format may be introduced later.

---

## Neighboring chunk connections

Before generating chunk (x, y), the generator reads the already-committed edge exits of neighbors. If neighbors are not yet generated, their exits are derived from the road graph only (no content yet).

This means:

- Road geometry on shared edges is always derived from the road graph, not neighbor content.
- Interior chunk content (buildings, trees, etc.) does not need to match neighbors.
- Only edge exits (road/path entry/exit points) must match across borders.

---

## Generation without AI

The procedural generator produces complete, valid chunks for all district types without any AI API call. The output quality is lower than AI-assisted output but it is always available, always deterministic, and has zero cost.

Procedural output is the default and the guaranteed fallback.

---

## Scaling to larger worlds

The first demo is 20 × 20 = 400 chunks. Future targets:

| Scale | Grid | Total chunks |
|-------|------|-------------|
| Demo | 20 × 20 | 400 |
| Medium | 200 × 200 | 40 000 |
| Large | 2 000 × 2 000 | 4 000 000 |

At large scale, the chunk cache becomes essential — regenerating millions of chunks on every load is impractical even if deterministic. The MCB cache stores computed results.

Large-scale generation is not a first-version goal.
