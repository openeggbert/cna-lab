# MeshWorld

**MeshWorld** is a free, open-source procedural world generator and exploration demo built on the MeshCraft ecosystem.

- Generates 3D city/world chunks as MC3 XML scenes
- 20 C++ chunk generators + 11 Lua object generators
- A planetary map subsystem (`MeshWorldPlanet`) — quadtree LOD 0-18, 52-biome
  (`ZoneType`) classification, procedural naming, real hydrology/mountain-range/
  biome-refinement terrain algorithms
- Lua scripting sandbox (sol2 + Lua 5.4) for content modding
- SQLite content packs, taxonomy registry, material registry
- MC3 validation pipeline

**1638 / 1639 tests pass.** The one remaining failure observed in the latest
full run is a documented, run-order-dependent flaky map test; see `NEXT.md`
for the current baseline.

---

## Build

**Requirements:**

- CMake 3.20+
- C++23 compiler (GCC 13+ or Clang 17+)
- `libsqlite3-dev` (system package)
- [`openeggbert/mesh-craft`](https://github.com/openeggbert/mesh-craft) cloned next to `mesh-world`

```bash
# Clone both repos side-by-side
git clone https://github.com/openeggbert/mesh-craft
git clone https://github.com/openeggbert/mesh-world
cd mesh-world

# Configure (downloads Lua 5.4.7 and sol2 3.3.0 automatically via CMake FetchContent)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel 4

# Run tests
cd build && ctest -j4 --output-on-failure
```

---

## Quick start

### Generate and export a demo world (20×20 chunks)

```bash
# Show world info and sample 4 chunks
./build/MeshWorld examples/world.json

# ASCII map of zone layout
./build/MeshWorldMap examples/world.json

# Export all 400 chunks to MC3 XML files
./build/MeshWorldExport examples/world.json output/

# List all registered materials with colors and license info
./build/MeshWorldMaterials

# Pack all generators + taxonomy + materials into a single SQLite file
./build/MeshWorldPack meshworld_content.sqlite

# Export all 400 chunks AND compile to MCB binary format
./build/MeshWorldExport --mcb examples/world.json output/chunks

# Export the curated mixed-region city showcase (49 chunks) with validation
./build/MeshWorldExport --validate --force --threads 4 \
  examples/city_showcase.json /tmp/meshworld-city-showcase
```

### Load output in MeshCraft viewer

```bash
# Open any exported chunk in the MeshCraft viewer
meshcraft output/2_9.mc3.xml    # park chunk
meshcraft output/9_9.mc3.xml    # town square chunk
```

Sample MC3 output files are in [`screenshots/`](screenshots/).

### Planetary map generation (MeshWorldPlanet)

```bash
# Create/open a planet world, descend to a tile, render a PNG preview
./build/MeshWorldPlanet /tmp/my_world --level 5 --tile 2,3 --stats --ascii --png out.png
```

See [`docs/map-generation.md`](docs/map-generation.md) for the full CLI reference
(image/GeoJSON/MBTiles export, hillshade rendering, etc.) and [`map.md`](map.md) for
the subsystem's design.

### Run the interactive viewer (MeshWorldApp)

MeshWorldApp is a standalone 3D explorer built on MeshCraft + CNA (SDL3/OpenGL).
It streams and renders chunks around the player in real time.

```bash
# Build (requires cna, sharp-runtime, easy-gl, and mesh-craft siblings)
cmake -S apps/mesh-world-app -B apps/mesh-world-app/build
cmake --build apps/mesh-world-app/build --parallel

# Run
./apps/mesh-world-app/build/MeshWorldApp

# In the menu choose "Explore City Showcase" to enter the deterministic
# mixed-region composer scene (houses, apartments, shops, square, landmark,
# trees, vehicles, and street furniture) centered around one crossroad.
# Ordinary worlds started from the app also opt into the MC3 composer; cached
# chunks from older sessions remain untouched until they are regenerated. New
# worlds start at the nearest preloaded MC3-ready city parcel when available.

# Controls
#   WASD / Q / E — move
#   Mouse         — look
#   ESC           — exit to menu
```

---

## Architecture

```
World config (world.json)
        │
        ▼
  WorldMap — assigns ZoneType + RegionType to each (x,y) chunk coord
        │
        ▼
  ChunkPipeline ─── disk cache (cache/chunks/)
        │
        ├─ 1. LuaGeneratorRegistry — try "lua.zone.<region>" first
        │         └── LuaSandbox → LuaRuntime (Lua 5.4 + sol2)
        │                   └── Mc3SceneBuilder API
        │
        └─ 2. C++ fallback: get_generator(zone, region)
                   └── 20 built-in chunk generators
                             └── MC3Writer → Mc3SceneBuilder
        │
        ▼
  MC3Validator — validates XML structure, metadata, bounds, materials
        │
        ▼
  ChunkCache — stores validated MC3 XML to disk
        │
        ▼
  MC3 XML output (mc3.xml) — load in MeshCraft viewer or MCB compiler
```

### Key classes

| Class | Description |
|---|---|
| `ChunkPipeline` | Orchestrates Lua-first → C++ fallback → validate → cache |
| `Mc3SceneBuilder` | Safe C++ API: `addBox`, `addCylinder`, `addPlane`, `addGround`, `addInstance` |
| `LuaGeneratorRegistry` | Maps generator ID → Lua source; loads from `generators/lua/` |
| `LuaSandbox` | Runs a Lua generator safely (no io, os, debug, require) |
| `TaxonomyRegistry` | Loads `data/taxonomy/taxonomy.json` — node hierarchy |
| `ContainmentRuleRegistry` | Loads `data/taxonomy/containment.json` — LOD-aware containment rules |
| `MaterialRegistry` | ~100 built-in procedural materials with RGB + roughness + license |
| `MC3Validator` | Validates MC3 XML: well-formed, metadata, bounds, material registration |
| `SqliteContentPack` | Packs generators + taxonomy + materials into a single `.sqlite` file |

### MC3 JSON schema

[`schemas/mc3.schema.json`](schemas/mc3.schema.json) is the strict Draft
2020-12 interchange contract for semantic `mc3.json` and the tracked MC3
libraries. Use a standard Draft 2020-12 validator when authoring or
exchanging JSON; MeshCraft's permissive parser and MeshWorld's semantic XML
validation remain complementary compatibility layers.

### Standalone MC3 assets

`MeshWorldPruneMc3Lib` extracts selected static definitions from an
`.mc3lib` together with their transitive instance/variant/LOD dependencies
and document-owned material/texture dependencies. Its output is an ordinary
standalone MC3 document (or MCB with `--mcb`), not another library:

```bash
./build/MeshWorldPruneMc3Lib --mcb \
  data/mc3lib/urban-street-furniture-1.0.0.mc3lib.json \
  /tmp/streetlamp.mcb streetlamp.classic_01
```

Assets with runtime MC3 scripts are rejected rather than being silently
incomplete; expand them through the separately scoped R104 path before
pruning.

### Generator ID convention

```
lua.<category>.<name>.<variant>
cpp.chunk.<region>
```

Examples: `lua.object.chair.simple`, `lua.zone.park`, `cpp.chunk.river_bank`

---

## World config format

```json
{
  "name": "demo_city",
  "seed": 42,
  "style": "central_europe_small_city",
  "grid_w": 20,
  "grid_h": 20,
  "chunk_size_m": 64,
  "zones": [
    {
      "type": "city",
      "x_min": 0, "x_max": 18, "y_min": 0, "y_max": 18,
      "region_default": "small_house_block",
      "regions": [
        { "x_min": 9, "x_max": 10, "y_min": 9, "y_max": 10, "type": "square" }
      ]
    }
  ]
}
```

---

## Lua generator API

```lua
local M = {}
M.id = "lua.zone.my_zone"

function M.generate(ctx, scene)
    -- ctx fields: variation, chunk_x, chunk_y, chunk_size_m, style, zone, region
    scene:addGround("grass_park")
    scene:addBox("bench_1", {
        position = {10, 0.22, 20},
        size     = {1.8, 0.44, 0.5},
        material = "wood_bench"
    })
    scene:addCylinder("lamp_1", {
        position = {15, 0, 18},
        radius   = 0.08,
        height   = 4.5,
        material = "metal_lamp"
    })
    scene:setMetadata({
        generator  = {id = M.id, version = "0.1.0", language = "lua"},
        generation = {variationInput = ctx.variation}
    })
end

return M
```

Place the file in `generators/lua/zone/` — it's auto-discovered at startup.

---

## Repository layout

```
mesh-world/
  data/taxonomy/        — taxonomy.json + containment.json
  docs/                 — design documents
  examples/             — world.json demo config
  generators/lua/       — Lua generators (zone/, object/, building/, room/, map/)
  include/              — C++ headers
  screenshots/          — sample MC3 output files
  src/                  — C++ sources + tools/ + materials/
  tests/                — GTest test suite (1176 tests)
  vendor/               — googletest, nlohmann/json
  CMakeLists.txt
  plan.md               — implementation task list (T001-T255 legacy chunk system;
                           M001-M354 planetary map subsystem)
```

---

## License

MIT — see [LICENSE](LICENSE).

No credits. No billing. No subscription. No marketplace. No API keys committed.

---

## Related projects

| Project | Description |
|---|---|
| [mesh-craft](https://github.com/openeggbert/mesh-craft) | MC3/MCB scene format tools and viewer |
| [sharp-runtime](https://github.com/openeggbert/sharp-runtime) | C++23 reimplementation of .NET System.* |
