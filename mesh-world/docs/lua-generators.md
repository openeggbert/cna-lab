# MeshWorld — Lua Generators

Lua is the primary scripting layer for MeshWorld generators. Lua scripts can generate objects, buildings, rooms, zones, and maps through a safe C++ API without writing raw XML.

## Design principles

- Lua calls `Mc3SceneBuilder` methods; C++ produces MC3 XML via MeshCraft's `Mc3XmlWriter`.
- Lua never concatenates raw XML strings.
- Lua runs in a sandbox (see `docs/lua-sandbox.md`).
- Every generator must call `scene:setMetadata(...)` to emit structured generation metadata.

## Generator function signature

Every Lua generator file must expose a `generate` function:

```lua
-- generators/lua/object/chair.lua
-- SPDX-License-Identifier: MIT

local M = {}

M.id       = "lua.object.chair.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local w  = ctx.parameters.width  or 0.45
    local d  = ctx.parameters.depth  or 0.45
    local h  = ctx.parameters.height or 0.9
    local lh = 0.42  -- leg height
    local lt = 0.04  -- leg thickness
    local mat = ctx.parameters.material or ctx.style.wood_material or "wood_natural"

    -- 4 legs
    for i, offset in ipairs({ {-1,-1}, {1,-1}, {-1,1}, {1,1} }) do
        scene:addCylinder("leg_" .. i, {
            position = { offset[1] * (w/2 - lt), 0, offset[2] * (d/2 - lt) },
            radius   = lt / 2,
            height   = lh,
            material = mat
        })
    end

    -- seat
    scene:addBox("seat", {
        position = { 0, lh, 0 },
        size     = { w, 0.04, d },
        material = mat
    })

    -- backrest
    if ctx.parameters.backrest ~= false then
        scene:addBox("backrest", {
            position = { 0, lh + 0.25, -(d/2 - 0.02) },
            size     = { w, 0.5, 0.04 },
            material = mat
        })
    end

    scene:setMetadata({
        generator = { id = M.id, version = M.version, category = M.category, language = "lua" },
        object    = { type = "chair", style = "simple_wooden" },
        generation = { variationInput = ctx.variation }
    })
end

return M
```

## Generator ID convention

```
lua.<category>.<name>.<variant>
```

Examples:
- `lua.object.chair.simple`
- `lua.object.tree.deciduous`
- `lua.zone.park.central_europe`
- `lua.building.house.simple`
- `lua.room.kitchen.basic`
- `lua.map.city.small_grid`

C++ generators follow:
- `cpp.chunk.park`
- `cpp.chunk.road`
- `cpp.chunk.small_house_block`

## Generator parameters via ctx

Real, as of 2026-07-11 (object/chunk-mode generators, i.e. scripts bound
under `scene`, not `map` — see `LuaRuntime.cpp`'s `build_ctx_table()`):

```lua
function M.generate(ctx, scene)
    -- ctx fields:
    --   ctx.variation      int64   — variation input (not a reproducibility guarantee)
    --   ctx.zone           string  — ZoneType as string, e.g. "city"
    --   ctx.region         string  — RegionType as string, e.g. "park"
    --   ctx.style          string  — style id, e.g. "central_europe_small_city" --
    --                                NOT a resolved materials table despite many
    --                                existing generators writing
    --                                `ctx.style and ctx.style.wood_material` as if
    --                                it were one (harmless dead code today: a Lua
    --                                string has no such field, so that expression
    --                                always evaluates to nil and falls through to
    --                                the generator's own hardcoded default).
    --                                StyleRegistry (include/StyleRegistry.hpp) DOES
    --                                have real registered styles with a real
    --                                materials palette, but resolving ctx.style
    --                                through it before exposing it to Lua is a
    --                                real, still-open follow-up (see the roadmap
    --                                doc) -- new generators should use
    --                                `p.xxx_material or "sensible_default"`
    --                                instead of the ctx.style.xxx pattern for now.
    --   ctx.chunk_x        int     — chunk grid X
    --   ctx.chunk_y        int     — chunk grid Y
    --   ctx.chunk_size_m   float   — chunk size in meters (usually 64)
    --   ctx.lod            int     — LOD level (0=coarse, 4=very detailed)
    --   ctx.parameters     table   — caller-specified generator parameters (always
    --                                present, empty {} if the caller specified none;
    --                                only ever populated for real via
    --                                scene:callGenerator's sub_ctx -- a plain
    --                                top-level ChunkContext has no parameters
    --                                concept of its own, so a top-level-invoked
    --                                generator always sees an empty table here)
    --   ctx.exits          table   — {north_road, south_road, east_road, west_road,
    --                                 north_path, south_path, east_path, west_path}
    --   ctx.random()       function — float in [0,1), deterministic per (script,
    --                                  ctx.variation) pair
    --   ctx.randomInt(lo,hi) function — integer in [lo,hi] inclusive, same determinism
    --   ctx.containment.childrenOf(parent_id) function — returns an array of
    --                                {parent, child, probability, min_count,
    --                                max_count, lod_max} tables from
    --                                data/taxonomy/containment.json (see
    --                                docs/taxonomy-and-containment.md)
end
```

## Calling other generators (composition)

Real as of 2026-07-11 (`LuaRuntime.cpp`'s `register_scene_api()` +
`Mc3SceneBuilder::pushTransform()`/`popTransform()`, see
`docs/generator-composition.md` for the full mechanism). `generators/lua/
building/house/detached.lua` is the concrete worked example — a real
composed house calling 7 sub-generators.

```lua
function M.generate(ctx, scene)
    -- spawn a sub-generator, offset/rotated relative to THIS generator's
    -- own local origin (not absolute chunk coordinates -- position/rotation
    -- compose through any nested callGenerator calls too)
    local bench_ctx = {
        variation  = ctx.variation + 1,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = { material = "wood_bench" }  -- see the ctx.style note
                                                    -- above -- not ctx.style.xxx yet
    }
    scene:callGenerator("lua.object.bench.simple", bench_ctx, {
        position   = { 10, 0, 20 },
        rotation_y = 90,
        scale      = 1.0,   -- optional, default 1.0
        id         = "front_bench"  -- optional id prefix; auto-derived +
                                     -- counter-suffixed if omitted, so
                                     -- repeated calls never collide
    })
end
```

Every id the sub-generator emits gets prefixed (`"front_bench_0_seat"`, not
just `"seat"`) so multiple placements of the same generator never collide,
even with no `id` given. Unknown generator ids raise a Lua error (caught the
same way any other script error is — the whole top-level generation fails,
matching `setup_require()`'s existing blocked-call precedent). Resolution
goes through the GLOBAL `LuaGeneratorRegistry::instance()` singleton, not a
per-call local registry — anything reachable from `generators/lua/` at
startup (or packed into `meshworld_content.sqlite`) can be called.

## scene API reference

```lua
-- Primitives (positions in chunk-local meters)
scene:addPlane(id, { position, size, material, rotation_y })
scene:addBox(id, { position, size, material, rotation_y })
scene:addCylinder(id, { position, radius, height, material })
scene:addSphere(id, { position, radius, material })              -- real 2026-07-11
scene:addCone(id, { position, radius, height, material })        -- real 2026-07-11
scene:addIcoSphere(id, { position, radius, material, scale })    -- M330
scene:addInstance(id, { definition, position, rotation_y, scale })

-- Composition (real 2026-07-11)
scene:callGenerator(generator_id, sub_ctx, placement)

-- Materials
scene:addMaterial(id, { color, roughness, metallic, base_color_texture })

-- Metadata
scene:setMetadata({ generator = {...}, object = {...}, generation = {...} })
```

There is no `scene:chunkSize()`/`scene:random()`/`scene:randomInt()` (never
implemented, despite an earlier version of this doc listing them) — use
`ctx.chunk_size_m` (a field, not a function) and `ctx.random()`/
`ctx.randomInt(lo, hi)` (real, on `ctx`, not `scene` — see the ctx fields
section above) instead.

## Directory layout

```
generators/lua/
    object/         — leaf objects (chair, table, tree, bench, lamp, fridge, TV,
                       sink, wardrobe, nightstand, car, bicycle, ...)
    architecture/    — composable building PARTS (2026-07-11):
        window/     — double_pane.lua, ...
        door/       — front_panel.lua, ...
        roof/       — gable.lua, ...
        chimney/    — brick.lua, ...
        stairs/     — front_steps.lua, ...
    building/       — building-level generators; `house/detached.lua` composes
                       architecture/ + object/ generators via scene:callGenerator,
                       `simple_house.lua` (unchanged) still builds inline
    room/           — interior room generators (kitchen, living_room, bedroom,
                       bathroom — all now real; see docs/procedural-model-
                       generator-roadmap.md for the "not wired into any building
                       yet" scope note)
    zone/           — zone/area generators (park, forest, residential_block...)
    map/            — map generators (basic_city, forest_region...)
```

The registry auto-discovers generators by recursively scanning this whole
tree (`LuaGeneratorRegistry::load_from_dir()`), deriving an id from either the
script's own `M.id` (preferred) or its path relative to `generators/lua/` —
so adding a new category folder (like `architecture/` above) needs no
registry/manifest change, just the files themselves.

**Correction (2026-07-11): the "Chunk-level C++ generators remain in
`src/generators/` (and subdirs `nature/`, `furniture/`, `architecture/`,
`street/`, `electronics/`)" line this doc used to have here was never true.**
`src/generators/` is flat (only a `map/` subdirectory exists, for the
planetary map subsystem) — every chunk generator (`ParkGenerator.cpp`,
`ForestGenerator.cpp`, etc.) lives directly in `src/generators/`, one file
per chunk archetype, no category subdirectories, no `IObjectGenerator`
interface. See `docs/object-generators.md`'s own 2026-07-11 correction for
the full story on where that description came from.

## File naming

- File name: `<short_name>.lua` (e.g., `chair.lua`, `park.lua`)
- Generator ID in the file must match: `lua.<category>.<name>.<variant>`
- Version: semantic `"<major>.<minor>.<patch>"`

## Testing

Every Lua generator must have at least one test:
- `tests/LuaGeneratorTests.cpp` / `tests/ArchitectureAndRoomObjectTests.cpp` for
  standalone generators (load + shape assertions via a LOCAL
  `LuaGeneratorRegistry` + `LuaSandbox::execute(source, ctx)`).
- `tests/LuaCompositionTests.cpp` for anything using `scene:callGenerator` —
  needs the GLOBAL `LuaGeneratorRegistry::instance()` singleton populated
  (`load_from_dir("generators/lua")`) since callGenerator always resolves
  through it, never a local instance.
- Verify output contains `<mc3` and `<metadata`.
- Verify no sandbox escape (test that `io` is nil inside generator) — see
  `tests/LuaGeneratorTests.cpp`'s existing `SandboxBlocks*` tests.
