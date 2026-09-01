# MeshWorld — Final Architecture Direction (2026-06)

This document records the final architectural decisions established in June 2026.
It supersedes earlier discussion about runtime Claude API integration.

---

## Final decision: offline-first, no runtime Claude API

MeshWorld does NOT use Claude API during normal runtime.

MeshWorld is AI-**assisted** during development, not AI-**dependent** at runtime.

### Intended workflow

- Claude Code helps during development.
- Claude Code generates and improves Lua generator scripts.
- Claude Code generates and improves taxonomy/containment definitions.
- Claude Code generates and improves map/zone/region generator scripts.
- These generated Lua scripts and definitions are stored in the repository or packaged into SQLite content databases.
- End users run MeshWorld locally without Claude API, without API keys, without paid AI services, and without internet.
- MeshWorld can still be described as AI-assisted because AI helped create the generator library during development.

**Do not implement runtime Claude API integration unless explicitly requested later.**

---

## Core idea

MeshWorld is an offline procedural city/world generation and exploration project built around MeshCraft formats.

The main value is a growing library of:
- Lua generators
- C++ core generator/runtime infrastructure
- taxonomy definitions
- containment rules
- map/zone/region definitions
- material/texture registries
- MC3/MCB cache pipeline

The generated world is produced locally from bundled content.

---

## Main pipeline

```
WorldConfig / generation settings
  -> taxonomy and containment rules
  -> map/region/zone generator
  -> chunk request
  -> Lua/C++ generators
  -> internal MC3 scene/object model  (Mc3SceneBuilder)
  -> existing MeshCraft Mc3XmlWriter
  -> mc3.xml
  -> MC3 validation
  -> MCB compilation/cache
  -> MeshWorld runtime loads MCB
```

No Claude API call in this pipeline.

---

## MC3 writer reuse rule

Before creating or modifying any MC3 XML writer code, inspect the existing MeshCraft codebase.

There is already an existing MC3 XML writer in the MeshCraft project (`../mesh-craft/mc3/src/Mc3XmlWriter.cpp`).

- Do NOT create a competing MeshWorld XML writer.
- Reuse existing `Mc3XmlWriter` via `Mc3Document::saveToFile()`.
- Create only a thin adapter/wrapper (`Mc3SceneBuilder`) if needed.
- Keep MC3 XML serialization centralized in MeshCraft.

**Status: already done in M7.** `MC3Writer` now delegates to `Mc3DocumentBuilder` which uses `Mc3Document::saveToFile()`.

---

## Lua generation model

Lua scripts generate whole MC3 models, chunks, zones, interiors, forests, roads, houses, rooms, furniture, props, and map layouts.

Lua scripts must NOT manually concatenate raw XML strings.

### Wrong approach
```lua
xml = "<mc3><box ... /></mc3>"
```

### Correct approach
```lua
function generate(ctx, scene)
    scene:addMaterial("wood", { color = {0.55, 0.32, 0.12} })
    scene:addBox("table_top", {
        position = {0, 0.75, 0},
        size     = {1.2, 0.08, 0.8},
        material = "wood"
    })
    scene:addMetadata({
        generator = { id = "lua.object.table.simple", version = "0.1.0" },
        object    = { type = "table" }
    })
end
```

Lua calls a safe MeshWorld scene builder API (`Mc3SceneBuilder`).
C++ converts the scene/object model to MC3 XML using MeshCraft `Mc3XmlWriter`.

---

## Lua sandbox

Lua scripts run in a sandbox.

**Allowed:**
- MeshWorld scene builder API
- generator registry API
- safe math/random helpers
- context parameter access
- metadata creation
- calling other approved generators

**Disallowed:**
- `io`, `os`, `debug`, `package`, `dofile`, `loadfile`
- unrestricted `require`
- arbitrary filesystem access
- network access
- process execution
- writing files directly

---

## Generator library taxonomy

Generators may generate: maps, regions, zones, chunks, buildings, rooms, interiors, furniture, vegetation, props, roads, parks, playgrounds, forests, landmarks, materials/texture assignments.

### Category subdirectories

```
generators/lua/
    object/         — leaf objects (chair, table, tree, bench, lamp, fridge, TV...)
    building/       — buildings (house, apartment, shop...)
    room/           — interior rooms (kitchen, bathroom, bedroom...)
    zone/           — zone/area generators (park, forest, residential...)
    map/            — map generators (basic_city, forest_region...)

generators/cpp/     — C++ generators (legacy and high-performance)
    chunks/         — existing chunk generators
    objects/        — object generators
```

### Composition examples

- `HouseGenerator` → `DoorGenerator` + `WindowGenerator` + `RoofGenerator` + `KitchenGenerator`
- `KitchenGenerator` → `FridgeGenerator` + `SinkGenerator` + `TableGenerator` + `ChairGenerator`
- `ParkGenerator` → `TreeGenerator` + `FlowerGenerator` + `BenchGenerator` + `LampGenerator` + `FountainGenerator`
- `ForestGenerator` → `TreeClusterGenerator` + `BushGenerator` + `PathGenerator` + `ClearingGenerator` + `RockGenerator`

---

## Taxonomy and containment definitions

Data-driven definitions for what can contain what.

### Examples

`region.city` can contain: `zone.old_town`, `zone.residential_estate`, `zone.park`, `zone.shop_street`

`zone.residential_estate` can contain: `building.panel_house`, `place.playground`, `object.bench`, `object.tree`

`room.kitchen` can contain: `object.fridge`, `object.microwave`, `object.table`, `object.chair`

`object.fridge` can contain: `object.milk_box`, `object.water_bottle`, `object.egg_box`

### LOD limits (no infinite recursion)

- LOD0: exterior/block shape
- LOD1: facade/main visible parts
- LOD2: nearby props and outdoor details
- LOD3: interiors only when needed
- LOD4: small objects only when explicitly needed

---

## SQLite content storage

Lua scripts and definitions can be stored in SQLite content databases for release/runtime packs.

**Development mode:** Lua files and JSON definitions as normal files (Git/Claude Code friendly).

**Runtime/release mode:** packed into `meshworld_content.sqlite`.

### Key tables

```sql
CREATE TABLE lua_generator (
    id TEXT PRIMARY KEY,
    category TEXT NOT NULL,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    source_code TEXT NOT NULL,
    description TEXT,
    parameters_json TEXT,
    sha256 TEXT NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at TEXT,
    updated_at TEXT
);

CREATE TABLE taxonomy_node (
    id TEXT PRIMARY KEY,
    kind TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT
);

CREATE TABLE containment_rule (
    parent_id TEXT NOT NULL,
    child_id TEXT NOT NULL,
    min_count INTEGER,
    max_count INTEGER,
    probability REAL,
    conditions_json TEXT,
    PRIMARY KEY (parent_id, child_id)
);
```

---

## Seed / variation policy

Do NOT promise permanent generator compatibility based on seed values.

- Generated `.mc3.xml` / `.mcb` files are the source of truth once generated.
- Variation input may drive generation but is NOT a long-term compatibility contract.
- Future generator versions may produce different assets even with identical inputs.
- Store generator id, version, parameters, and variation input inside MC3 metadata.
- Use terms: "variation input", "generation settings", "generator version", "generated asset metadata".
- Do NOT use: "guaranteed reproducible", "same seed = same city forever".

---

## MC3 generation metadata

All generated MC3 files must include a `<metadata>` tag:

```xml
<metadata format="json" type="generation">
<![CDATA[
{
  "generator": {
    "id": "lua.object.chair.simple",
    "version": "0.1.0",
    "category": "furniture",
    "language": "lua"
  },
  "object": {
    "type": "chair",
    "style": "simple_wooden",
    "parameters": { "width": 0.45, "height": 0.9 }
  },
  "generation": {
    "createdBy": "MeshWorld",
    "variationInput": 12345,
    "notes": "Variation input is not a long-term compatibility guarantee."
  }
}
]]>
</metadata>
```

---

## Texture and material system

- `TextureRegistry` + `MaterialRegistry` + `AssetLicenseInfo`
- Prefer CC0/public-domain textures.
- Store license metadata for every external texture.
- Do not include textures with unclear license.
- If attribution is required, generate an attribution file.

---

## Runtime user experience

Player walks through locally generated world. No AI pause.

When player approaches missing content:
1. Check MCB cache → if found, load
2. Use bundled Lua/C++ generators → generate MC3 → validate → compile/cache to MCB → load

Missing generators: skip optional object, use fallback, log, do not crash, do not call Claude API.

Missing generators are fixed during development by future Claude Code sessions.

---

## No monetization

- No billing, credits, Stripe, subscriptions, marketplace, or virtual currency. Ever.
- No API keys committed to the repository.
- `config/` is gitignored.

---

## Development workflow with Claude Code

Example tasks for future Claude Code sessions:
- Add Lua generators for kitchen appliances
- Add Lua generators for forest vegetation
- Add taxonomy definitions for residential buildings
- Add containment rules for kitchens, bathrooms, parks
- Improve forest generator composition
- Improve city map generator
- Add SQLite packer for Lua generators
- Add validation tests for Lua generators
