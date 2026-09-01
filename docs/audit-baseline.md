# R107 — Audit & Baseline

Workstream **R0** from `mesh_world_revival.md` §24 ("Plan Derivation
Structure"), tracked as `plan.md` task **R107**. This document traces the
*current* (2026-07-12) runtime generation path end-to-end — which chunks/
objects go through native C++ generators vs. Lua vs. `ObjectDefinitionLibrary`
— and records a build/test baseline to compare future R10x/R11x work
against. It is the factual precursor the rest of the R-series (and
`docs/risk-register.md`'s RR1/RR3/RR4 triggers) depend on.

---

## 1. Current runtime generation path

Mesh World generates content in two independent quad-tree/grid pipelines,
both following the **same "Lua-first, native-C++-fallback" pattern**:

### 1.1 Map layer — `Map::MapPipeline` (`src/Map/MapPipeline.cpp`)

For each `TileCoord` (level 0 = planet, deeper levels = continent/country/
region/metro/city/neighborhood/...):

1. Compute a generator id: `"lua.map.planet.default"` at level 0, else
   `"lua.map.child.level<N>.default"` if a level-specific Lua generator is
   registered for level `N`, else the generic `"lua.map.child.default"`.
2. If `LuaGeneratorRegistry` has that id, run it via
   `LuaSandbox::executeMap()`.
3. Validate the resulting `MapTilePayload` (must be non-empty elevation +
   pass `MC3Validator`-style structural checks). On success, use it and mark
   `generated_from_lua = true`.
4. On any failure (missing generator, Lua error, empty elevation, failed
   validation) — log to `stderr` and **fall back** to the native C++
   generator for that tile (`PlanetGenerator`/`ChildGenerator`).

Real Lua sources today: `generators/lua/map/{planet,continent,country,
region,metro,city,neighborhood,district,level1,level2,level8,level10,
level13,level14,level16,level17,trunk_network}.lua` (17 files). Only
`planet.lua`/`continent.lua`/`country.lua`/`region.lua`/`metro.lua`/
`city.lua`/`neighborhood.lua`/`district.lua` are wired to real levels per
their own module `id`; the generic per-level files (`level1.lua` etc.) also
register but several tile levels still have **no** Lua generator and always
fall through to the generic C++ `ChildGenerator` (confirmed by the
`Level1StillUsesCppChildGeneratorWithNoLuaRegistered` test — see §3).

### 1.2 Chunk layer — `ChunkPipeline` (`src/ChunkPipeline.cpp`)

For each chunk `(x, y)`:

1. `build_context()` samples the map-layer's hand-off tile (via
   `MapPipeline::get()`) to fill `ctx.zone`/`ctx.region`/road-exit flags —
   i.e. the chunk layer already *depends on* the map layer's output.
2. Generator id: `"lua.zone." + region_name` (e.g. `lua.zone.park`,
   `lua.zone.road`). If registered, run via `LuaSandbox::execute()`.
3. Validate the resulting MC3 XML string via `MC3Validator`. On success,
   cache and return it.
4. On any failure — log to `stderr`, fall back to the native C++
   `get_generator(zone, region)->generate(ctx)` (the 19 generators under
   `src/generators/`: `ApartmentBlockGenerator`, `BeachGenerator`,
   `BridgeGenerator`, `CaveGenerator`, `CrossroadGenerator`,
   `DesertGenerator`, `EmptyGenerator`, `ForestGenerator`, `JungleGenerator`,
   `MeadowGenerator`, `MountainGenerator`, `OceanGenerator`, `ParkGenerator`,
   `RiverBankGenerator`, `RoadGenerator`, `ShopStreetGenerator`,
   `SmallHouseBlockGenerator`, `SquareGenerator`, `SwampGenerator`,
   `TundraGenerator`).

Real Lua sources today: `generators/lua/zone/{crossroad,forest,park,
road}.lua` (only 4 of the ~19 zone/region types have a Lua override; the
rest always go straight to their C++ generator).

Of the native C++ generators, only `ForestGenerator.cpp` and
`src/generators/map/PlanetGenerator.cpp` reference Lua at all (and only to
call into `LuaGeneratorRegistry`/`LuaSandbox` themselves for sub-placement,
not because they are Lua-driven).

### 1.3 Object/building/room/architecture Lua helpers

`generators/lua/{object,building,room,architecture}/*.lua` (27 object +
2 building + 4 room + 5 architecture-category files) are **not** looked up
directly by `MapPipeline`/`ChunkPipeline` by a fixed id pattern the way
map/zone generators are. They are loaded into the same
`LuaGeneratorRegistry` (registered under path- or `M.id`-derived ids like
`lua.object.chair`) and consumed via Lua `require`/helper calls *from
within* zone/map Lua scripts (e.g. `city.lua`/`road.lua` placing a `bench`
or `tree`) — i.e. they are library modules, not independently-dispatched
generators.

### 1.4 Startup wiring

`src/ContentPackLoader.cpp` calls
`LuaGeneratorRegistry::instance().load_from_dir(root / "generators" / "lua")`
once at startup, which recursively scans **every** `.lua` file under
`generators/lua/` and registers it by its module's own `id` field (falling
back to a path-derived id, e.g. `.../object/chair.lua` →
`lua.object.chair`). This is the single place all Lua generators enter the
runtime; there is no separate registration list/manifest to keep in sync.

### 1.5 Object placement — `ObjectDefinitionLibrary`

Both the native C++ zone generators and the Lua zone/object scripts
ultimately place *instances* of reusable object shapes registered in
`ObjectDefinitionLibrary` (`src/ObjectDefinitionLibrary.cpp`,
`load_all()`) by definition id (trees, benches, etc.) rather than emitting
raw geometry inline — this is the existing, pre-R-series precedent R106
("global material catalogue") and R101/R102 ("global import/variants")
extend.

---

## 2. Summary — what R100-R106 are actually replacing

| Layer | Primary path today | Fallback | R-series successor |
|---|---|---|---|
| Map tiles | Lua (`generators/lua/map/*.lua`, 17 files, partial level coverage) | Native C++ (`PlanetGenerator`/`ChildGenerator`) | R105 (Lua→C++), R121 (migration-stage tracking) |
| Chunks/zones | Lua (`generators/lua/zone/*.lua`, only 4/~19 zones) | Native C++ (19 generators under `src/generators/`) | R100/R113 (C++ world composer + MC3 assets) |
| Objects/buildings/rooms | Lua library modules (`generators/lua/{object,building,room,architecture}/*`), invoked from zone/map scripts, not independently dispatched | — (no separate fallback; embedded in the calling script) | R101/R102/R103/R104 (`<import>`/`<script>`+ MeshCraft Lua binding) |
| Reusable shapes | `ObjectDefinitionLibrary` (C++ registry, id-based) | — | R106 (global material catalogue extends the same idea to materials) |

This confirms the revival document's framing (§1): Lua is not dead code to
delete outright — it is the **primary** path for map tiles and a sizeable
minority of zones today, with C++ generators as the safety net. R105/R113
must therefore be sequenced as *promote-and-demote*, not *delete*, matching
`docs/risk-register.md` RR4.

---

## 3. Build & test baseline (2026-07-12)

Built via the project's existing `build/` CMake directory (Ninja),
target `MeshWorldTests`:

```
$ cmake --build build --target MeshWorldTests
... (clean build, no warnings other than a pre-existing liblua54 tmpnam() linker warning)
[100%] Built target MeshWorldTests

$ ./build/MeshWorldTests --gtest_brief=1
[==========] 1442 tests from 117 test suites ran. (20544 ms total)
[  PASSED  ] 1429 tests.
[  FAILED  ] 13 tests, listed below:
```

**13 pre-existing failures, all in `MapPipelineTest`** (none introduced by
this audit — no production code was touched):

- `GetLevel0ProducesValidPayload`
- `GetLevel1ProducesValidPayload`
- `SiblingSharedEdgeOrderIndependent`
- `ConcurrentGenerationPreservesSiblingBoundaryConsistency`
- `SiblingSharedEdgeOrderIndependentDeepLevel`
- `LuaChildBoundaryIsConstrainedEvenWhenScriptIgnoresParent`
- `LevelSpecificLuaGeneratorOnlyAppliesToItsOwnLevel`
- `RealContinentLuaAppliesOnlyAtLevel3`
- `RealRegionLuaAppliesOnlyAtLevel7`
- `RealCountryLuaAppliesOnlyAtLevel5`
- `RealMetroLuaAppliesOnlyAtLevel9`
- `RealCityLuaAppliesOnlyAtLevel12`
- `Level1StillUsesCppChildGeneratorWithNoLuaRegistered`

**Correction (R124, 2026-07-12): the claim below that "all 13 fail the same
way" was wrong** — it was written from the failure summary without reading
each assertion individually. 10 of the 13 were indeed exactly this string
drift. The other 3 (`SiblingSharedEdgeOrderIndependent[DeepLevel]`,
`ConcurrentGenerationPreservesSiblingBoundaryConsistency`) were a real,
previously undiscovered production bug in
`MapBuilder::applyParentEdgeConstraints()` (`src/MapBuilder.cpp`) — a
sibling-tile boundary elevation mismatch from a swapped half-selector
variable, causing a visible terrain seam at roughly half of all tile-tile
boundaries. Both are now fixed; see `plan.md`'s R124 entry for the full
diagnosis and fix. The original (now-corrected) text follows for context:

All 13 fail the same way: the test expects a tile's `.generator` field to
read `"child"` (the old generic-C++-fallback marker) but it now reads
`"lua.map.child.level<N>.default"` — i.e. these tests were written before
the generic per-level Lua map files (`level1.lua`, `level2.lua`,
`level8.lua`, `level10.lua`, `level13.lua`, `level14.lua`, `level16.lua`,
`level17.lua`) existed and now need updating to expect the Lua id, not the
C++ fallback marker, at those levels. **This is an existing, undocumented
test/behavior drift — not something introduced by R100-R106/R123** — and is
recorded here as a concrete audit finding rather than silently fixed, per
R108's "fallback must never be silent" principle. Fixing it is out of scope
for R107 itself (R107 is audit-only); it is a good candidate follow-up
inside R121 (migration-stage tracking) or as its own small T-series
cleanup task.

No baseline screenshots were captured — the environment has no display/
GL context available for `MeshWorldApp`/`MeshCraft` interactively in this
session; §2's table plus the code-path trace above serve as the "before"
reference for R114's later showcase comparison. If GPU/screenshot capture
becomes available in a future session, a `saves`/`screenshots` before-state
should be added here.

---

## 4. How to use this document

- R114 (deterministic city showcase) should diff its own generator/asset
  selection diagnostics (R108) and visual output against §1-§3 above as its
  "before" reference.
- R105/R113/R121 should treat §2's table as the literal scope of what needs
  a C++/MC3-asset replacement before any Lua file is deleted.
- The 13-test drift in §3 should be re-checked (and ideally fixed) as part
  of whichever task next touches `MapPipelineTest`/`MapPipeline.cpp`.
