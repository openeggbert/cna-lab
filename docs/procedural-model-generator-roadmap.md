# MeshWorld — Procedural Model-Generator Roadmap

Written 2026-07-11, at the start of a long-term initiative (the user's own
framing: "MeshWorld must become capable of generating many thousands of
different MC3-based 3D models ... over the coming years"). This document is
the durable reference for that vision — later sessions should read this
before adding generator infrastructure or content, to stay consistent with
the conventions established here.

## The long-term vision

MeshWorld is not just a map generator. It is an offline-first procedural
world CONSTRUCTION system:

```
map generators        decide WHAT EXISTS WHERE (continents, countries,
                       cities, zones — namespace MeshWorld::Map)
        │
        ▼
zone generators        create local environment LAYOUT within a zone
        │               (park.lua, forest.lua, residential_block generators)
        ▼
building generators    create buildings (house, apartment, shop)
        │
        ▼
room generators         create interiors (kitchen, living_room, bedroom,
        │                bathroom) — content exists now; NOT YET wired into
        │                any building (see "Known gaps" below)
        ▼
object generators       create reusable models (bench, tree, car, sofa, ...)
        │
        ▼
architecture/part       create doors, windows, roofs, chimneys, stairs —
generators               the smallest composable pieces buildings assemble
```

All of this is persisted, cached, validated, and rendered — never
regenerated live from a fixed seed (world state is non-reproducible by
design, entropy is time-based; see `NEXT.md` §1's own "Key architectural
decisions"). AI may assist in AUTHORING generator scripts offline, ahead of
time (exactly how this session's own new files were written) — that is
unrelated to, and does not conflict with, the project's separate, permanent
rule against *runtime* AI/Claude API calls (`docs/ai-generation.md`'s own
2026-07-11 clarification).

## Why reusable parts beat monolithic objects

"A house is a cube" was always the wrong shape for this system. A house
generator should produce a house BODY and call sub-generators for roofs,
doors, windows, stairs, chimneys, fences, mailboxes — each independently
authored, tested, and improved. Fixing or upgrading `architecture/window/
double_pane.lua` once (e.g. adding curtains) automatically improves every
building that composes it, instead of requiring N separate edits across N
copy-pasted monolithic house generators. This is now a REAL mechanism, not
just a principle — see "Composition is real" below.

The same logic applies one level down: a table isn't one box if a richer
shape is buildable from available primitives — legs, top, optional details,
material/size/style variation, real metadata. See `docs/object-generators.md`
for what a real object generator looks like today.

## Composition is real (as of 2026-07-11)

`scene:callGenerator(generator_id, sub_ctx, placement)` was documented for a
long time before this session (`docs/lua-generators.md`,
`docs/generator-composition.md`, `docs/taxonomy-and-containment.md`,
`docs/forest-generation.md`) but never actually implemented — a real,
significant, previously-undiagnosed infra gap, exactly the kind this
document's own directive predicted. It is now real: implemented in
`LuaRuntime.cpp` (the `callGenerator` binding) + `Mc3SceneBuilder::
pushTransform()`/`popTransform()` (a genuine transform/id-prefix stack,
composing position/rotation/scale/id-namespace across arbitrarily nested
calls). Full mechanism writeup: `docs/generator-composition.md`.
`lua.building.house.detached` (`generators/lua/building/house/detached.lua`)
is the concrete proof — composes 7 sub-generators, one of which
(`lua.object.mailbox.simple`) predates and is unrelated to this feature,
proving composition reaches the WHOLE existing generator library.

## Taxonomy

### Directory taxonomy (Lua generators)

```
generators/lua/
    object/         — freestanding, independently placeable objects
    architecture/   — composable building PARTS (window/, door/, roof/,
                       chimney/, stairs/ — new 2026-07-11)
    building/       — building-level generators (composed OR inline)
    room/           — interior room generators (content only, not yet wired)
    zone/           — zone/area generators
    map/            — map generators
```

No manifest lists these — `LuaGeneratorRegistry::load_from_dir()` scans the
whole tree recursively; a new category subdirectory needs no registry
change. This session's new `architecture/` tree is additive: nothing
existing moved, no aliasing was needed (aliasing would only matter if an
EXISTING file's canonical location changed, which none did).

### Declarative taxonomy + containment (already built, now finally reachable)

A SEPARATE, richer, data-driven taxonomy system already exists —
`TaxonomyRegistry`/`ContainmentRuleRegistry` (real C++ classes) + real data
files (`data/taxonomy/taxonomy.json` — world/region/zone/building/room/
place/path/object kind hierarchy; `data/taxonomy/containment.json` — real
rules like "zone.park can contain 8-40 object.tree instances with
probability 1.0, lod_max 2"). `ContentPackLoader::load_from_disk()` already
loads both at startup. **This was fully built but completely unreachable
from Lua** — `docs/taxonomy-and-containment.md` documented a
`ctx.containment.childrenOf(...)` Lua call for a long time with no binding
ever wired up, a second real, previously-undiagnosed infra gap found this
session. Fixed: `ctx.containment.childrenOf(parent_id)` is now real
(object/chunk-mode generators). No existing generator has been migrated to
DRIVE its own composition from these rules yet (e.g. `zone/park.lua` still
hardcodes its own bench/tree/lamp counts instead of reading
`ctx.containment.childrenOf("zone.park")`) — a real, concrete, available-
but-not-yet-adopted next step, not a promise about current behavior.

## Conventions established this session

### Parameters

Every generator reads `local p = ctx.parameters or {}` and treats every
field as optional with a sensible default — `p.width or 1.6`, never a bare
`p.width`. `ctx.parameters` is only ever populated for real via
`scene:callGenerator`'s `sub_ctx.parameters` — a plain top-level
`ChunkContext` has no parameters concept of its own (confirmed:
`build_ctx_table()` always sets `ctx.parameters` to an empty table when
building from a `ChunkContext`), so a top-level-invoked generator always
sees defaults only.

### Determinism

Every generator is a pure function of `ctx.variation`/`ctx.parameters` —
never call unseeded `math.random()` (the sandbox doesn't even expose Lua's
own `math.random` usefully for this; use `ctx.random()`/`ctx.randomInt(lo,
hi)` instead, both real for object/chunk-mode generators as of this
session, deterministic per `(script, ctx.variation)`). Non-reproducibility,
where it exists (weather, wind — see `docs/sky-weather.md`), comes from
seeding seed FROM `steady_clock` at world-start, never from a generator
itself rolling unseeded dice mid-generation. This matches the project's
permanent "entropy is time-based, not re-derived from a fixed seed" rule.

### LOD

`ctx.lod` (0=coarse silhouette .. 4=high-detail near-camera) is now a real
field on `ChunkContext` (`include/ChunkGenerator.hpp`) and populated in
`build_ctx_table()` — previously documented, never backed by any C++ field.
No caller currently sets it to anything but the default (2, "normal
gameplay object") — a real distance-based LOD picker feeding it a live
value is a separate, unbuilt follow-up. New generators should accept an
`ctx.lod`-driven detail tier where it's cheap to do (e.g. skip optional
trim/detail parts below a threshold) but should NOT block on building a
full LOD system before shipping — this session's new generators mostly
don't have meaningfully different LOD tiers yet (a single-detail-level
bench/window/door is already fairly cheap geometry).

### Style

`ctx.style` is a STRING style id (e.g. `"central_europe_small_city"`), not
a resolved materials table — despite many EXISTING generators writing
`ctx.style and ctx.style.wood_material` as if it were one. This is
harmless dead code (a Lua string has no such field, the expression always
evaluates to `nil`, generators silently fall through to their own hardcoded
default) — confirmed, not assumed, by reading `build_ctx_table()` directly.
**A real, populated `StyleRegistry` with real registered palettes already
exists** (`include/StyleRegistry.hpp`, `src/styles/BuiltinStyles.cpp` —
`central_europe_small_city`, `nordic_town`, `desert_outpost`,
`jungle_village`) and 3 real C++ chunk generators
(`ParkGenerator`/`RoadGenerator`/`SmallHouseBlockGenerator`) already try to
consume it via `StyleRegistry::instance().get(ctx.style)` — **but
`register_builtin_styles()` is never actually called anywhere in this
codebase except `apps/mesh-world-app/main.cpp`**, a THIRD real,
previously-undiagnosed gap found this session: `StyleRegistry::instance()`
is empty at runtime for every CLI tool/test binary, so even those 3 C++
generators' own style lookups always silently fall back to their own
hardcoded defaults too. **Deliberately not fixed this session** (see "Known
gaps" below — found, diagnosed, documented, left for a focused follow-up
rather than folded into an already-large session). New Lua generators
written this session use `p.xxx_material or "hardcoded_default"` directly,
NOT the `ctx.style.xxx` pattern, since the latter is currently inert.

### Metadata

Every generator calls `scene:setMetadata({...})` — see
`docs/mc3-generation-metadata.md` for the real (2026-07-11-corrected) field
names, and note that Lua generators' own nested JSON shape
(`generator={id=...}`) differs from, but is equally validated alongside,
C++'s flatter `GenerationMetadata` struct output.

## MC3Validator: real gaps found and fixed this session

Confirmed by direct testing (`tests/MC3ValidatorTests.cpp`'s own
`RealGeneratorOutputWithCorruptedPositionFailsValidation`, which would have
silently passed before this fix): `MC3Validator::validate()` used to scan
only `<mc3>`'s DIRECT children for geometry elements, but MeshCraft's real
`Mc3XmlWriter` (used by every real generator via `MC3Writer.hpp` →
`Mc3SceneBuilder` → `Mc3DocumentBuilder`) always nests actual geometry one
level deeper inside an `<objects>` wrapper — so `check_object()` was NEVER
actually invoked on any real generator's output, only on hand-written test
XML that (understandably, but incorrectly) put `<box>` directly under
`<mc3>`. Compounding this, even a reached box was checked against separate
`x`/`z` attributes that MeshCraft's writer has never emitted — real position
lives in one combined `position="x y z"` attribute, omitted entirely when
the position is exactly the origin. `MC3ValidatorTests.
ParkGeneratorPassesValidation` "passed" because there was nothing left to
check, not because the output was actually validated — a textbook case of
test fixtures accidentally matching a validator's own wrong assumptions,
both self-consistently wrong in the same way, so the mismatch with real
output went unnoticed. Both bugs fixed (a real recursive tree walk instead
of one flat loop; real `position` attribute parsing); duplicate-id
detection and malformed-numeric-attribute detection added as genuinely new
checks. See `src/MC3Validator.cpp`'s own top-of-file comment for the full
technical writeup, and `tests/MC3ValidatorTests.cpp` for the corrected
fixtures (also rewritten to use the REAL wrapped-`<objects>`,
combined-`position` XML shape instead of the old flat, unrealistic one).

## Known infra gaps

Each of these was found via direct code/data inspection, not guessed.

1. **StyleRegistry never wired at startup — FIXED 2026-07-11, same day,
   later** (user picked this as the next task when explicitly asked what
   was needed/blocking). `register_builtin_styles()` added next to
   `register_builtin_materials()` at the 2 real chunk-generating binaries
   that had neither (`src/main.cpp`, `src/tools/export_chunks.cpp`) and 2
   test files exercising the 3 style-consuming C++ generators directly
   (`tests/GeneratorTests.cpp`, `tests/DemoWorldTests.cpp`) — deliberately
   NOT added to `print_materials.cpp`/`print_map.cpp`/`planet_map.cpp`/
   `pack_content.cpp` after confirming none of them consume `StyleRegistry`
   or even invoke `ChunkPipeline` (the map-only tools drive
   `Map::MapPipeline` instead). `ctx.style` in `build_ctx_table()` now
   resolves through `StyleRegistry::instance()` into a real `{id, name,
   palette}` table when registered, falling back to the bare string
   otherwise. **Real nuance found while implementing**: this does NOT
   retroactively fix existing generators' `ctx.style.wood_material`-style
   dead code — the real `Style::palette` uses dotted namespaced keys
   ("park.lamp", "block.facade.0"), not simple names, so that exact pattern
   still returns `nil` (for a different reason now, same safe fallback
   behavior, not a regression). New generators should use
   `ctx.style.palette["some.dotted.key"] or "default"`. Also found, still
   NOT fixed (separate, smaller, genuinely out of scope): `SqliteContentPack`
   has no concept of packing `Style`/palette data at all — styles can never
   reach a packed `meshworld_content.sqlite`. See plan.md's G11 entry for
   the full detail.

Remaining gaps (not fixed this session — real next tasks), roughly
descending priority:

1. ~~**No rx/rz rotation binding**~~ **DONE (2026-07-13, G12).**
   `Mc3SceneBuilder`/`Mc3DocumentBuilder`/`MC3Writer`'s `addPlane`/`addBox`/
   `addCylinder`/`addInstance` (and their C++-facing `plane`/`box`/
   `cylinder`/`instance` counterparts) now all accept `rx`/`rz` (tilt/roll,
   degrees) alongside the existing `ry` (yaw) — a MeshWorld-side binding
   gap, not a format limitation (`Mc3Transform.rotation` was already a full
   3-component vector). Scope decision: `rx`/`rz` are leaf-level only, NOT
   composed through `pushTransform()`/`popTransform()`'s cumulative frame
   stack (that stack only ever tracked a single `ry_deg`) — correct for
   today's actual use (a leaf primitive tilted directly), not a general
   per-axis 3D rotation composition system; documented in
   `Mc3SceneBuilder.hpp`'s own doc comment. Also fixed the real,
   already-documented bug this unblocked:
   `building/simple_house.lua`'s gable roof computed a real slope `angle`
   but applied it as `ry` (yaw, spins flat) instead of `rz` (pitch, the
   ridge runs along Z) — now uses `rz`, so `roof_l`/`roof_r` are genuinely
   sloped panels, not flat rotated planks.
   `architecture/roof/gable.lua`'s own stepped-box silhouette is left as a
   deliberate style choice, not rewritten (a separate future content task).
2. **`ctx.containment` not adopted by any existing generator** — the
   binding is real (see "Taxonomy" above) but nothing drives its own
   `scene:callGenerator` calls from it yet. `zone/park.lua` is the natural
   first candidate (its own doc-comment example already shows the shape).
   Tracked as plan.md G13.
3. **No interior-room placement pipeline** — `room/*.lua` generators are
   real content, callable and tested, but no building generator places them
   inside a real footprint with real wall openings. A separate, larger,
   cross-cutting feature — now scoped (see "G10 scoping" below) but not
   implemented. Tracked as plan.md G10.
4. **`addSphere`/`addCone` newly bound, unused by any existing generator
   yet** — both existed in C++ for a while, are now reachable from Lua
   (this session), but no generator has been updated to prefer them over
   `addIcoSphere`/box-approximations where a true UV sphere or cone would
   read better.
5. **MC3Validator's `<instance definition="...">` reference is only
   structurally checked (non-empty), never resolved** — deliberate (see
   `src/MC3Validator.cpp`'s own `InstanceReferencingAnUnresolvedDefinitionIsNotAnErrorHere`
   test) since definitions are frequently injected from a separate source
   after a chunk's own XML is generated — but a HIGHER-level tool that DOES
   have access to the full merged document (chunk + injected definitions)
   could do real resolution checking; nothing like that exists yet.
6. **`SqliteContentPack` cannot pack `Style`/palette data** — found while
   fixing gap 1 above; `MeshWorldPack` packs materials/Lua-generators/
   taxonomy/containment but has no style table at all, so styles can never
   reach a packed `meshworld_content.sqlite`. Small, separate, not
   attempted.

## Content added this session (concrete inventory)

- **13 new furniture/street/vehicle objects** (T195-T212 old-backlog triage,
  done BEFORE the infra work below, at explicit user request): bed, sofa,
  bookshelf, microwave, oven, toilet, bathtub, car, bicycle, mailbox,
  fire_hydrant, sign, picnic_table. 13 new materials. 13 new tests.
- **Infra**: `scene:callGenerator` (real composition), `ctx.lod`/`ctx.exits`
  (real, previously documented-only), `ctx.random()`/`ctx.randomInt()` for
  object/chunk mode (previously map-mode only), `ctx.containment.
  childrenOf()` (real, previously documented-only), `scene:addSphere`/
  `scene:addCone` (existed in C++, now bound to Lua). MC3Validator rewrite
  (see above). 24 new infra/composition tests.
- **5 architecture-tier generators**: `window/double_pane.lua`,
  `door/front_panel.lua`, `roof/gable.lua`, `chimney/brick.lua`,
  `stairs/front_steps.lua`. 9 new tests.
- **1 composed building**: `lua.building.house.detached` — the concrete
  proof composition works end-to-end (7 sub-generator calls).
- **3 new room generators completing T216-T218**: `living_room.lua`,
  `bedroom.lua`, `bathroom.lua`, composing existing + 3 new small objects
  (`sink.lua`, `wardrobe.lua`, `nightstand.lua`). 3 composition tests + 3
  object-load tests.
- **Docs**: this file (new); `docs/lua-generators.md`,
  `docs/generator-composition.md`, `docs/object-generators.md`,
  `docs/mc3-generation-metadata.md`, `docs/taxonomy-and-containment.md`,
  `docs/ai-generation.md` all corrected to match reality (several were
  significantly fictional — `generator-composition.md`/`object-generators.md`
  described a C++ `IObjectGenerator` system that was never built at all).

## G10 scoping: an interior-room placement pipeline (design only, not implemented)

Written 2026-07-11, at explicit user request ("naplánovat rozsah" — scope
it, don't implement yet), as a follow-up to G8/T216-T218 (the 3 room
generators are real content but nothing places them inside a building).
This section is a design for a FUTURE session to implement, not a promise
about current behavior.

### The core technical blocker

MC3 has no boolean/CSG subtraction — there is no way to "cut a doorway hole"
into a solid wall box. Every wall in this codebase today is either (a) one
fully solid box (`building/simple_house.lua`, `building/house/detached.lua`
— a door/window is a separate decorative object simply OVERLAID on the
solid wall, not a real gap a camera could walk through), or (b) an
ALL-OR-NOTHING per-side omission (`CaveGenerator.cpp` + `CaveLayout` —
`if (!openings.north) w.box("wall_n", ...)`, a full floor-to-ceiling gap
the entire width of that side, appropriate for a cave tunnel but far too
wide for a house doorway). **A genuinely new technique is needed**: a
segmented wall — left pier + a lintel/header spanning above the doorway +
right pier — leaving a real, door-sized rectangular gap partway along one
wall. Nothing in this codebase does this yet; it is not a reuse of the
cave precedent, only inspired by its same "build the wall additively from
pieces, omit what shouldn't be there" philosophy instead of trying to
subtract from a solid box.

### Proposed architecture (incremental, v1 → v2 → v3)

**v1 — one walkable room, proving the mechanic end-to-end.** A new
`generators/lua/building/house/with_interior.lua` (or an opt-in
`p.interior = true` parameter on `detached.lua` — decide at implementation
time based on how much the two variants would actually share) that:
1. Builds the exterior shell like `detached.lua` already does, EXCEPT the
   front wall becomes 3 segments (left pier, lintel, right pier) leaving a
   real door-width, door-height gap instead of a solid box + an overlaid
   decorative door.
2. Composes exactly ONE room generator (e.g. `lua.room.living_room.basic`)
   filling the WHOLE interior footprint via `scene:callGenerator`, sized to
   the interior dimensions (exterior footprint minus wall thickness).
3. No multi-room subdivision, no interior partition walls, no second floor
   — deliberately the smallest slice that proves "a camera can pass through
   the front doorway gap and see real furniture inside," which is the part
   nothing in this codebase has ever done.
4. New test: assert the front wall's 3 segments leave a gap at the door's
   real world position (no geometry with the door's own x-range at the
   door's own y-range) — the direct MC3-side analog of
   `CaveGeneratorTests`' own wall-presence-matches-openings tests.

**v2 — multi-room subdivision.** Split the interior footprint into 2-4
sub-rectangles (simplest: one axis-aligned cut, e.g. a 60/40 width split —
a full BSP tree is almost certainly over-engineered for a single-story
house-sized footprint). Assign each sub-rectangle a room type via a fixed
priority list for v2 (kitchen + living_room mandatory if 2+ rooms, bedroom/
bathroom fill remaining slots) — save real `ctx.containment`-rule-driven
assignment for v3 rather than coupling two new mechanisms in one step.
Interior partition walls between rooms: for v2, leave them OPEN (no wall,
no doorway gap needed) between adjacic rooms — only the exterior needs a
real passable opening. Real interior partition walls (with their own
doorway gaps, reusing the v1 segmented-wall technique) are a v3+ refinement,
not required for "rooms exist and are individually enterable."

**v3 — containment-rule-driven room selection.** Replace v2's fixed
priority list with a real `ctx.containment.childrenOf("building.house")`-
driven selection (G13's own mechanism, by then hopefully already adopted by
at least one other generator) — how many rooms, which types, at what
probability, all read from `data/taxonomy/containment.json` instead of
hardcoded. **Correction, checked directly before publishing this doc**: real
`building.house` → `room.{kitchen,bedroom,bathroom,living_room}` entries
ALREADY EXIST in `data/taxonomy/containment.json` (with real probability/
min_count/max_count/lod_max values — e.g. kitchen 0.95 probability, exactly
1; bedroom 0.95 probability, 1-4), plus `room.kitchen`/`room.living_room` →
specific `object.*` entries (fridge/microwave/oven/sink/cabinet/table/chair/
tv). So v3 needs LESS new data-authoring work than initially assumed — the
containment rules for exactly this use case are already sitting there
unused, the same "fully built on one side, unreached from the other" shape
every other G-series infra gap this session found had. `taxonomy.json` also
already has the matching `room.*` nodes.

### Explicit non-goals (all versions)

- No multi-story / multiple floor levels.
- No windows-as-real-holes — windows stay decorative overlays exactly as
  they are today; only the front DOOR gets the segmented-wall treatment,
  since only a door needs to be walkable.
- No roof/attic interior space modeling — the roof stays exactly the
  decorative exterior shape `architecture/roof/gable.lua` already produces.
- No connection to `SmallHouseBlockGenerator.cpp` (the C++ chunk-scale
  generator) — this pipeline is scoped to the new Lua composed-house path
  (`lua.building.house.detached`) only, at least through v3.

### Why this wasn't attempted in the same session as G1-G9/G11

Genuinely separate, larger, cross-cutting scope from everything else this
session built (composition infra, architecture parts, a first composed
building, room CONTENT) — matches the same "choose a coherent subset, make
it solid" discipline this whole session followed elsewhere. The segmented-
wall technique in particular needs its own careful design/testing pass
before being built, not a rushed addition on top of an already-large diff.

## What NOT to do (carried forward from the user's own directive)

- Do not rewrite the whole project, replace MC3, introduce a different 3D
  engine, or depend on Blender.
- Do not add new runtime AI/Claude API calls — permanent project rule,
  unrelated to offline generator-script authorship (see
  `docs/ai-generation.md`'s 2026-07-11 clarification).
- Do not add huge binary assets or copyrighted model data.
- Do not create large numbers of low-quality copy-paste generators in one
  session — this session added ~24 new generator files across 2 batches
  (furniture/street/vehicle objects, then architecture+room), each
  individually tested, not a mass-produced sweep.
- Do not remove or break existing generators without a compatible
  migration — nothing existing was deleted or had its id changed this
  session; `simple_house.lua` still works exactly as before alongside the
  new composed `house/detached.lua`.
