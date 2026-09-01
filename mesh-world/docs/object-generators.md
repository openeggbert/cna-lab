# MeshWorld — Object Generators

**Rewritten 2026-07-11.** The previous version of this document described a
C++ `IObjectGenerator`/`ObjectContext`/`ObjectGeneratorRegistry` interface
and a `src/generators/{nature,furniture,electronics,architecture,street}/`
subdirectory layout. None of it was ever built: confirmed by direct search,
`include/IObjectGenerator.hpp` and `include/ObjectGeneratorRegistry.hpp`
don't exist, and `src/generators/` has always been flat (only a `map/`
subdirectory, for the planetary map subsystem — every chunk generator like
`ParkGenerator.cpp` lives directly in `src/generators/`, one file per chunk
archetype). This document now describes the REAL object-generator system,
which is Lua-based.

## What a real object generator looks like

```lua
-- generators/lua/object/bench.lua
local M = {}
M.id       = "lua.object.bench.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p   = ctx.parameters or {}
    local w   = p.width or 1.6
    local mat = p.material or "wood_bench"
    scene:addBox("seat", { position = {0, 0.46, 0.08}, size = {w, 0.04, 0.35}, material = mat })
    -- ... legs, backrest ...
    scene:setMetadata({
        generator  = {id = M.id, version = M.version, category = M.category, language = "lua"},
        object     = {type = "bench", style = "park_bench"},
        generation = {variationInput = ctx.variation or 0}
    })
end
return M
```

See `docs/lua-generators.md` for the full `M.generate(ctx, scene)` contract
and `ctx` field reference. Object generators write into whatever
`Mc3SceneBuilder` they're handed — the SAME builder a parent chunk generator
or a parent Lua generator (via `scene:callGenerator`, see
`docs/generator-composition.md`) is already using. They never create their
own `<mc3>` root and never call `scene:addGround()`.

## Registry

Real, and much simpler than the fictional `ObjectGeneratorRegistry` the old
doc described — no explicit per-generator registration call anywhere.
`LuaGeneratorRegistry` (`include/LuaGeneratorRegistry.hpp`) recursively
scans `generators/lua/` at startup (`ContentPackLoader::load_from_disk()`)
and derives each generator's id from its own `M.id` field (falling back to
a path-derived id if `M.id` is absent). Look up a generator by id with
`LuaGeneratorRegistry::instance().get(id)` (throws if unknown) or
`.has(id)` (bool check) — the same registry `scene:callGenerator` resolves
against.

## Real object generators (as of 2026-07-11)

`generators/lua/object/`: `bed`, `sofa`, `bookshelf`, `microwave`, `oven`,
`toilet`, `bathtub`, `car`, `bicycle`, `mailbox`, `fire_hydrant`, `sign`,
`picnic_table`, `sink`, `wardrobe`, `nightstand` (all added 2026-07-11,
T195-T218 old-backlog triage), plus the pre-existing `bench`, `chair`,
`table`, `door`, `window`, `fountain`, `fridge`, `lamp`, `trash_can`,
`tree`, `tv`.

`generators/lua/architecture/` (new 2026-07-11 — composable building PARTS,
distinct from whole freestanding objects): `window/double_pane.lua`,
`door/front_panel.lua`, `roof/gable.lua`, `chimney/brick.lua`,
`stairs/front_steps.lua`. `lua.building.house.detached`
(`generators/lua/building/house/detached.lua`) composes all 5 of these
plus `lua.object.mailbox.simple` via `scene:callGenerator` — see
`docs/generator-composition.md`.

## Directory layout (real)

```
generators/lua/
    object/         — leaf objects, freestanding and independently placeable
    architecture/   — composable building PARTS: window/, door/, roof/, chimney/, stairs/
    building/       — building-level generators (composed OR inline, both real)
    room/           — interior room generators (content only -- see the scope
                       note below; not yet wired into any building generator)
    zone/           — zone/area generators
    map/            — map generators
```

No manifest/registry file lists these — any `.lua` file anywhere under
`generators/lua/` is auto-discovered by `LuaGeneratorRegistry::
load_from_dir()`'s recursive scan. Adding a new category subdirectory (like
`architecture/` this session) needs no code change.

## Known limitation: no rx/rz rotation

`Mc3SceneBuilder::addBox()`/`addCylinder()`/etc. only accept a `ry`
(Y-axis / yaw) rotation — there is no binding to tilt a shape around X or Z.
Two real generators work around this deliberately, both documented in their
own file:

- `car.lua`/`bicycle.lua`: "cylinder wheels" are built as flattened
  `addIcoSphere` ellipsoids (thin along the axle axis, full radius on the
  other two) instead — a sphere scaled non-uniformly needs no rotation to
  read as a wheel disc.
- `architecture/roof/gable.lua`: a genuinely sloped single-plane roof face
  isn't buildable at all without rx; it approximates the gable silhouette
  with stepped, progressively narrower boxes instead (a common low-poly
  technique). `building/simple_house.lua`'s own OLDER gable roof code
  (predating this finding) computes a real slope angle but then applies it
  as `ry` — which only spins the roof panel flat in the horizontal plane,
  not into an actual slope. Left as-is (behavior-preserving, not a
  regression) with an explanatory comment added in place; a real fix needs
  the same missing rx/rz primitive.

Adding rx/rz rotation to `Mc3SceneBuilder`/`Mc3DocumentBuilder` (and,
underneath, MeshCraft's own `Mc3Document`/`Mc3XmlWriter`, which DOES support
a full 3-axis `rotation="rx ry rz"` transform attribute already — this is a
binding gap, not a format limitation) is a real, concrete, scoped follow-up
task — see `docs/procedural-model-generator-roadmap.md`.

## Rooms are content, not yet a placement pipeline

`generators/lua/room/{kitchen,living_room,bedroom,bathroom}.lua` are all
real, independently-testable Lua generators. None of them are called by any
building generator — there is still no interior-room PLACEMENT pipeline
anywhere in this codebase (deciding where inside a building's footprint a
room goes, at what floor, with real wall openings connecting to the
exterior shell). `SmallHouseBlockGenerator.cpp` and
`lua.building.house.detached` both currently build exterior shells only.
Building that pipeline is a separate, larger, cross-cutting feature — see
the roadmap doc's backlog.

## Rules

- Object/architecture generators must not call `scene:addGround()`.
- Object/architecture generators should keep their own geometry within a
  reasonable bounding box around their own local origin — a parent
  generator's `scene:callGenerator` placement is what positions them in the
  world, not anything the object generator itself decides.
- ID uniqueness across composed calls is the composition mechanism's job
  (auto id-prefix + counter, see `docs/generator-composition.md`), not the
  object generator's own responsibility.
- Variation input: derive `sub_ctx.variation` by offsetting the caller's own
  `ctx.variation` — never reuse the exact same value for multiple sibling
  calls (they'd all make identical random choices).
