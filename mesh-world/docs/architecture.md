# MeshWorld — Architecture

## Overview

MeshWorld is an offline procedural world generation and exploration system built around MeshCraft formats.

**Core principle: offline-first.** No Claude API at runtime. No API keys required by end users. Claude Code is used during development to create and improve Lua generator scripts and taxonomy definitions. End users receive a complete offline package.

---

## Main pipeline

```
WorldConfig / generation settings
  -> TaxonomyRegistry + ContainmentRuleRegistry (data-driven containment)
  -> WorldMap (zone/region assignment, road graph)
  -> chunk request
  -> GeneratorRegistry (lookup: Lua generator or C++ generator)
  -> LuaSandbox::execute(script, ctx) or CppGenerator::generate(ctx)
  -> Mc3SceneBuilder API calls
  -> Mc3Document (MeshCraft internal document model)
  -> Mc3Document::saveToFile() / Mc3XmlWriter (MeshCraft canonical writer)
  -> mc3.xml
  -> MC3Validator
  -> MCB compilation/cache
  -> MeshWorld runtime loads MCB
```

No Claude API call in this pipeline.

---

## Subsystems

### WorldConfig

Loaded from `world.json`. Contains world seed, map size, chunk size, grid dimensions, style name, zone/region overrides. See `WorldConfig.hpp`.

### WorldMap

Generated from WorldConfig. Assigns `ZoneType` (large area: city, forest, desert...) and `RegionType` (specific chunk role: road, park, small_house_block...) to every (x, y) chunk coordinate. Also computes road graph and `EdgeExits` for each chunk. See `WorldMap.hpp`.

### GeneratorRegistry

Maps generator IDs to implementations:
- C++ generators: `"cpp.chunk.park"` → `ParkGenerator`
- Lua generators: `"lua.zone.park.central_europe"` → loaded via `LuaGeneratorRegistry`

Lookup order: check Lua generator first, fall back to C++ generator, fall back to `EmptyGenerator`.

### LuaRuntime + LuaSandbox

Lua 5.4 execution environment with sol2 C++ binding layer.

- Blocked: `io`, `os`, `debug`, `package`, `dofile`, `loadfile`
- Available: `Mc3SceneBuilder` API, `ctx` table, safe math/random helpers, approved sub-generators
- Errors caught and logged; C++ fallback always available

See `docs/lua-sandbox.md`.

### Mc3SceneBuilder

The Lua-callable C++ API that builds an `Mc3Document` without writing raw XML. Provides `addBox`, `addPlane`, `addCylinder`, `addInstance`, `addMaterial`, `setMetadata`, `callGenerator`.

Lua scripts call this API; C++ builds the document; `Mc3Document::saveToFile()` serializes via MeshCraft's `Mc3XmlWriter`.

**Do not create competing XML writers.** All MC3 serialization goes through MeshCraft.

### TaxonomyRegistry + ContainmentRuleRegistry

Data-driven definitions loaded from `data/taxonomy/taxonomy.json` and `data/taxonomy/containment.json`. Define what regions/zones/buildings/rooms can contain what, with probability, count, and LOD limits.

See `docs/taxonomy-and-containment.md`.

### MaterialRegistry + TextureRegistry

Central registries for all material IDs and texture URIs, with `AssetLicenseInfo` per entry. Generators must register materials before using them. See `docs/textures-and-licenses.md`.

### ChunkCache + ChunkPipeline

- `ChunkCache`: file-based at `cache/chunks/<x>_<y>.mc3.xml` + `<x>_<y>.mcb`
- `ChunkPipeline::get(x, y)`: check cache → generate → validate → store → return

### GenerationMetadata

Every generated MC3 file includes a structured `<metadata format="json" type="generation">` tag. Contains: generator ID, version, variation input, zone, region, style, meshworld version, timestamp. See `docs/mc3-generation-metadata.md`.

### ContentPackLoader

Loads Lua generator source code and taxonomy definitions from either:
- Dev mode: plain `.lua` files in `generators/lua/` + JSON in `data/taxonomy/`
- Release mode: `meshworld_content.sqlite` content pack

See `docs/sqlite-content-packs.md`.

---

## Directory layout

```
mesh-world/
  include/                  — C++ headers
  src/                      — C++ implementation
    generators/             — C++ chunk generators (existing)
      nature/               — object generators: trees, shrubs, rocks
      furniture/            — chairs, tables, shelves
      electronics/          — TV, radio, laptop
      architecture/         — doors, windows, roofs, fences
      street/               — lamp posts, fountains, signs
    tools/                  — CLI tool binaries
  generators/lua/           — Lua generator scripts (dev mode)
    object/                 — leaf objects
    building/               — building generators
    room/                   — interior rooms
    zone/                   — zone generators
    map/                    — map generators
  data/taxonomy/            — taxonomy + containment JSON definitions
  tests/                    — GoogleTest tests
  docs/                     — documentation
  examples/                 — world.json, world_procedural.json
  vendor/                   — vendored libraries (googletest, nlohmann/json)
  config/                   — local user config (gitignored, no keys committed)
  build/                    — CMake build output (gitignored)
  cache/                    — generator scratch (gitignored)
```

---

## Technology stack

| Layer | Technology |
|-------|-----------|
| Language | C++23 |
| Build | CMake 3.20+ |
| Scripting | Lua 5.4 + sol2 (C++ binding) |
| XML serialization | MeshCraft Mc3XmlWriter (canonical, no competing writer) |
| XML parsing | tinyxml2 (vendored by MeshCraft) |
| Content packs | SQLite 3 (optional) |
| Scene formats | MC3 / MCB (MeshCraft) |
| Testing | GoogleTest |
| AI at runtime | **None** (AI used during development only, via Claude Code) |

---

## AI and development workflow

Claude Code is used during development:
- Generate and improve Lua generator scripts
- Generate taxonomy/containment definitions
- Review and improve generator composition
- Add new zone/building/object types

End users receive the committed output of these development sessions. They do not need an API key, internet access, or any AI service to run MeshWorld.

See `docs/new-direction-offline-first.md` for the full architectural direction document.

---

## What is explicitly excluded

- Runtime Claude API calls
- Monetization, billing, credits, subscriptions
- Characters, NPCs, dialogue
- Multiplayer or server-authoritative gameplay
- API keys committed to the repository
