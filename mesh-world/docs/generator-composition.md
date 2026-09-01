# MeshWorld — Generator Composition

**Rewritten 2026-07-11.** The previous version of this document described a
`IObjectGenerator`/`ObjectContext`/`StyleRegistry`-based C++ composition
model (`include/IObjectGenerator.hpp`, `ObjectGeneratorRegistry`, category
subdirectories under `src/generators/`) that was never actually built —
confirmed by direct search: none of those files exist, `src/generators/`
has always been flat (one file per chunk archetype, no `nature/`/
`furniture/`/`architecture/`/`street/`/`electronics/` subdirectories, only
a `map/` subdirectory for the planetary map subsystem). This document now
describes the REAL composition mechanism, which is Lua-based, not C++-based.

## The real hierarchy

```
C++ ChunkGenerator (one per chunk archetype, e.g. ParkGenerator)
  │   tries a matching Lua script FIRST (LuaGeneratorRegistry → LuaSandbox),
  │   falls back to its own hardcoded C++ geometry if none exists/errors
  │   (this fallback relationship predates and is unrelated to what
  │    follows — see docs/lua-sandbox.md)
  ▼
Lua generator (M.generate(ctx, scene))
  │   composes further Lua generators via scene:callGenerator(id, sub_ctx, placement)
  ▼
Lua generator (M.generate(ctx, scene))
  │   ...composition nests arbitrarily deep...
```

`lua.building.house.detached` (`generators/lua/building/house/detached.lua`)
is the concrete worked example: its own geometry is just a floor slab + 4
walls, and it calls 7 sub-generators (front door, 2 front windows, 1 side
window, roof, chimney, front steps, mailbox) via `scene:callGenerator`,
including `lua.object.mailbox.simple` — a generator this composition
feature didn't create, proving composition reaches the WHOLE existing
generator library, not just generators purpose-built for it.

## scene:callGenerator(generator_id, sub_ctx, placement)

Implemented in `LuaRuntime.cpp` (`register_scene_api()`'s `callGenerator`
binding) + `Mc3SceneBuilder::pushTransform()`/`popTransform()`
(`Mc3SceneBuilder.hpp`/`.cpp`). Mechanism:

1. Looks up `generator_id` in the GLOBAL `LuaGeneratorRegistry::instance()`
   singleton (the same one `ChunkPipeline.cpp`/`ContentPackLoader.cpp` use
   for top-level dispatch) — NOT a registry local to the calling script.
2. Pushes a transform frame onto the SAME `Mc3SceneBuilder` the parent
   generator is already using: `placement.position` (translation),
   `placement.rotation_y` (degrees, composes additively with any already-
   pushed rotation), `placement.scale` (composes multiplicatively), and an
   id prefix (`placement.id` if given, else derived from `generator_id`
   with its `lua.<category>.` prefix stripped and dots replaced by
   underscores — e.g. `lua.object.bench.simple` → `bench_simple` — always
   suffixed with an internal auto-incrementing counter, so repeated calls
   to the same sub-generator id never collide even with no explicit
   `placement.id`).
3. Runs the sub-generator's source as a nested Lua chunk in the SAME
   `sol::state` (so it inherits the exact same sandbox restrictions
   automatically — `io`/`os`/`debug`/`package`/`require` stay blocked, no
   separate sandboxing needed), calling its `M.generate(sub_ctx, scene)`.
4. Every `scene:add*()` call the sub-generator makes — including ones made
   by a FURTHER nested `scene:callGenerator()` call — gets its local
   position/rotation/size transformed by the CURRENT CUMULATIVE frame
   (composed like a standard scenegraph modelview stack) and its id
   prefixed by the CURRENT CUMULATIVE prefix, before reaching
   `Mc3DocumentBuilder`. Neither `Mc3DocumentBuilder` nor MeshCraft's own
   `Mc3XmlWriter` know anything about composition — it's entirely a
   `Mc3SceneBuilder`-layer concern.
5. Pops the transform frame (even on error, via the binding's own
   push/…/pop structure).

```lua
-- generators/lua/building/house/detached.lua (excerpt)
scene:callGenerator("lua.architecture.door.front_panel", {
    variation  = base_variation,
    style      = ctx.style,
    lod        = ctx.lod,
    parameters = { width = 1.0, height = 2.1 }
}, {
    position   = {0, fy, d/2 + wt/2 + 0.01},
    rotation_y = 0,
    id         = "front_door"
})
```

`sub_ctx` is entirely author-controlled (same convention every `ctx` field
already has) — the one default `callGenerator` itself applies is
`parameters` (an empty table if the caller didn't set one, matching
`build_ctx_table()`'s own top-level convention).

**Errors** (unknown `generator_id`, or any error inside the sub-generator's
own `M.generate`) are raised as a C++ exception, converted by sol2 into a
Lua error the same way `setup_require()`'s blocked-require lambda already
does — a broken sub-generator call fails the WHOLE top-level generation.
There is no partial-output/graceful-degradation behavior.

## ID uniqueness

Handled entirely by the auto id-prefix + counter scheme above — a caller
does NOT need to invent unique prefixes by hand. Calling the same
sub-generator id twice with no explicit `placement.id` still produces
distinct, collision-free output (`bench_simple_0_seat`,
`bench_simple_1_seat`, ...). See
`tests/LuaCompositionTests.cpp`'s `CallGeneratorNoCollisionAcrossRepeatedCalls`.

## Variation / seed propagation

There is no automatic derivation — a calling generator must offset
`ctx.variation` itself when building `sub_ctx`, same principle the old
(fictional) doc described, just via a plain Lua table field instead of a
C++ `ObjectContext::variation` member:

```lua
local base_variation = (ctx.variation or 0) + 1
-- then base_variation, base_variation + 1, base_variation + 2, ... for
-- each subsequent sub-generator call, so no two calls reroll identically
```

## Style propagation

`sub_ctx.style` is typically just `ctx.style` passed straight through (a
style ID string — see `docs/lua-generators.md`'s own note on `ctx.style`
not yet being a resolved materials table). There is no automatic "unknown
style falls back silently" behavior to configure — a generator that doesn't
recognize a style id just uses its own hardcoded default material, the same
way it already does when `ctx.parameters` doesn't override anything.

## Real containment-driven composition (available, not yet adopted)

`ctx.containment.childrenOf(parent_id)` (real as of 2026-07-11 — see
`docs/taxonomy-and-containment.md`) returns real declarative containment
rules from `data/taxonomy/containment.json`. A generator COULD drive its own
`scene:callGenerator` calls from these rules instead of hardcoding which
sub-generators to call and how many — `zone/park.lua`'s own doc example
shows the shape. No existing generator does this yet; it's a real, available
mechanism, not a promise about specific generators' current behavior.

## What NOT to do

- Do not call `scene:addGround()` from inside a sub-generator invoked via
  `callGenerator` — it re-lays a full chunk-size ground plane at whatever
  the current transform's translated origin is, almost never what's wanted
  mid-composition. (Nothing currently prevents this at the API level — it's
  a convention, not an enforced rule.)
- Do not assume `ctx.style` is a table (see the repeated note above) in new
  generators — use `p.xxx_material or "hardcoded_default"`.
- Do not hand-roll id-collision avoidance — the auto-prefix + counter
  scheme already handles it; inventing your own `id_prefix` scheme on top
  is redundant, not wrong, just unnecessary.
