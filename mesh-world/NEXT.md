# NEXT.md — MeshWorld Handoff Document

_Last updated: 2026-07-25, branch `develop`. This file was
rewritten from scratch into a concise handoff format — it no longer
accumulates a full narrative history. For detailed, dated write-ups of
every past change, see `plan.md` (task-by-task log, R100+ is the active
series) and `docs/audit-baseline.md`/`docs/world-composer-design.md`
(design docs for specific subsystems). See also `CLAUDE.md` (repo root)
for a shorter, canonical handoff aimed at AI coding-agent sessions — it
was re-synced with this file on 2026-07-25 (R131–R134 completion is
reflected there too); treat this file and `plan.md` as
authoritative if the two ever drift apart again._

---

## 1. Project summary

**MeshWorld** is an offline-first, procedurally-generated 3D world explorer
written in C++23. It generates city/world content as MC3 XML scenes
(MeshCraft's scene format, from the sibling `openeggbert/mesh-craft` repo)
and renders it via MeshCraft's `SceneRenderer` (SDL3 + OpenGL ES 3) in
`apps/mesh-world-app`.

**Main goal:** a living, explorable procedural world — non-reproducible by
design (planet/world entropy is time-based, not a fixed seed), SQLite-backed
for persistence, expandable as the player moves, no internet connection or
API keys required at runtime.

**Current development phase:** the original planetary map subsystem
(M-series) and a day/night/weather system (S-series) are both complete and
closed. The active phase is the **R-series** (`plan.md`, R100+), driven by
`mesh_world_revival.md`: migrating city/world generation from a Lua-first,
C++-fallback pipeline toward a **C++ world composer** that selects and
places validated MC3 assets directly. R113 (`BuildingComposer`) is the
concrete implementation of this direction; R114 is a deterministic
vertical-slice city showcase built on top of it. R113/R126/R127/R129
cover `RegionType::small_house_block`, `RegionType::apartment_block`,
`RegionType::shop_street`, and `RegionType::square` respectively; R128
(city showcase completion -- landmark + LOD wiring) is also done. **R129
(2026-07-18) closes Stage 4 ("C++ world composer selects assets")
completely** -- all 4 common region types are now composer-covered --
and separately root-caused/fixed a real `"zone"` metadata bug (§3/§4).
**R130 (2026-07-18) then closed `MC3Validator`'s remaining
single-chunk-computable §21.2/§21.3/§21.4 checks** (texture-reference/
material-slot/LOD-mapping validity, terrain penetration, instance/
triangle/draw-call metrics) -- see §3 for the full write-up and `docs/
migration-stages.md` for the next Stage-sequencing decision. **R131
(2026-07-25) closes Stage 2** with a strict Draft 2020-12 `mc3.json`
contract at `schemas/mc3.schema.json`, exercised against every tracked
MC3 library without adding a runtime dependency. **R132 (2026-07-25)**
then closed DS7 for static assets: `MeshWorldPruneMc3Lib` emits a minimal
ordinary MC3 or MCB document from selected definitions and rejects dynamic
runtime-script assets until R104 can expand them safely.
**R133 (2026-07-25)** closes the immediate explorer collision gap by resolving
authored MC3 instance collision proxies in the core library while retaining
passable decorative props. **R134 (2026-07-25)** makes configured and
persistent city-road edges symmetric, validates legal termini during export,
and prevents coarse planet-map hints from creating one-sided local roads.
**R138 (2026-07-25)** makes that edge graph authoritative in the Lua-first
showcase path, invalidates only generated app chunk caches, and gives each
traffic head a deterministic red/amber/green visual state.
**R140 (2026-07-25)** reduces explorer work by shrinking the interactive
streaming radius, culling off-camera chunks, bounding parsed MC3 cache growth,
and coalescing placement-database queries.
**R142 (2026-07-25)** makes nature directly reachable in the app through a
curated biome showcase, routes every named biome family to a non-empty natural
fallback, and fixes the validation/material defects exposed by that route.
**R143a-c (2026-07-25)** begins the actual Stage-6 asset path: reusable tagged
forest, jungle, desert, mountain, wetland, and coast MC3 kits are selected by
the matching C++ generators, with old primitive ids only as safe fallback.
R143b makes the forest clearing a true deterministic canopy exclusion zone
for both uniform and clustered tree placement; R143c gives wetland and coast
each a second registry-selected silhouette.
**R146 (2026-07-25)** bounds the interactive app's derived chunk-XML cache at
256 entries and avoids identical rewrites, so exploration cannot grow that
cache without limit.

**R125-R128 roadmap (approved and fully closed 2026-07-18):** the
revival roadmap's next Stage-4-closing slice. **R125:** a bounded,
curated slice of `object.sqlite3` (`object.md`) is now in
`data/taxonomy/taxonomy.json`/`data/taxonomy/containment.json` (99 nodes
/ 80 rules, up from 64/56) — real window/door/roof style variants plus a
handful of street-furniture/vehicle items. **R126:** `BuildingComposer`
composes real `RegionType::apartment_block` content (new
`apartment.block.wide_01` asset, its own parcel size class). **R127:**
`BuildingComposer` composes real `RegionType::shop_street` content too
(new `shop.building.storefront_01` asset with a box-composed
shopfront/awning echoing R112's real `window.shopfront.large.urban_01`
proportions, since literally attaching that socket-based asset remains
R104-gated; cobblestone ground cross-checked against the legacy
`ShopStreetGenerator`). **R128:** one new `landmark.clocktower_01` asset
placed via a new `WorldConfig::landmarks`/`ctx.landmark` mechanism (used
by `examples/city_showcase.json`, which also now sets
`use_world_composer: true` for the first time), plus LOD wiring
(`BuildingComposer`'s `resolve_instance_id()`) so `ctx.lod` -- long
documented but never actually backed by a real field -- now resolves
composer-placed instances to their authored low-LOD proxy when set
coarse (superseding items #2/#3 in §8 below). All 4 tasks verified via
`MeshWorldExport --validate` (0 errors/0 warnings each) and the full
`MeshWorldTests` suite. See `CLAUDE.md` for the concise canonical
summary and `plan.md`'s R125-R128 entries for the full write-ups.

**Important architectural decisions:**
- **Offline-first.** No Claude/OpenAI API calls at runtime; no API keys
  required by end users.
- **Two subsystems, cleanly separated:** the planetary map layer
  (`namespace MeshWorld::Map`) sits above the legacy chunk/city layer (flat
  `namespace MeshWorld`). The map layer must never depend on chunk types.
- **`MeshWorldLib` (the core library) must never link SDL3/OpenGL/CNA** —
  rendering stays isolated in `apps/mesh-world-app`, gated behind the
  `MESH_WORLD_HAS_RENDERER` compile definition.
- **Lua is a compatibility/migration layer, not the long-term content
  source** (R100). Every chunk/map generator still tries a Lua script
  first, falling back to a C++ generator — but *new* generator content
  should not be added in Lua going forward; the C++ world composer
  (`BuildingComposer`) is the intended new path. **The G-series (adding new
  Lua generator content) is explicitly paused** — do not resume it without
  asking first.
- **Non-reproducible worlds** at the planetary/map layer: consistency on
  revisit comes from SQLite persistence, never from re-deriving a fixed
  seed. (Note: the *chunk/city demo configs*, e.g. `examples/world.json`,
  `examples/city_showcase.json`, deliberately use a fixed `seed` field for
  reproducible demo output — this is a different, intentional convention
  from the planetary map layer's own rule.)
- **Unregistered materials are a warning, not a validation error**
  (deliberate, tested `MC3Validator` behavior, reconfirmed by explicit user
  decision 2026-07-13 — do not flip this to a hard error without asking).

---

## 2. Current status

**Build status:** clean. The root project (`MeshWorldLib` + CLI tools +
tests) was rebuilt with `--parallel 4`; the standalone `MeshWorldApp` also
configured and built successfully on 2026-07-25 with `--parallel 4`
(`CCACHE_DISABLE=1` is needed in this sandbox because its shared ccache
directory is read-only). The app started under Xvfb/llvmpipe OpenGL ES 3.2
and rendered the curated scene; a normal GPU/display review is still useful
for performance and artistic quality, not for basic execution proof. A later
same-day makefile re-check attempted to reconfigure the sibling MeshCraft
dependency and could not fetch its absent NanoSVG source because this sandbox
cannot resolve `github.com`; this is an environment/dependency-cache limit,
not a source compile failure, and the previously built current binaries were
used for the validation below.

**Test status:** **1654/1654 tests pass** (manually linked against the
current objects using the existing root build artifacts, then run from the
project root on 2026-07-25, pinned to CPU 0–3). R146 adds bounded-cache
eviction coverage; R143b adds deterministic forest-canopy exclusion for all
32 primary and 10 clustered trees; R143c locks two variants for each wetland
and coast kit. R142 adds the biome configuration, non-empty family dispatch,
and multi-seed natural-bounds regressions. R140 adds three focused performance
regressions.
R138 also corrected the stale
`MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12` expectation: when its Lua
generator is intentionally absent, R105's native level-11 district fallback
is correctly `cpp.map.district`, not the older generic `child` generator.

**CLI tools/binaries available** (see `CMakeLists.txt`):
- `MeshWorld` — main interactive-ish entry point (`src/main.cpp`)
- `MeshWorldExport` — batch chunk generator (`--mcb`, `--validate`,
  `--force`, `--threads N`)
- `MeshWorldGLB` — batch `.mc3.xml` → `.glb` converter (needs
  `mc3togltf_lib`, gated at configure time)
- `MeshWorldValidate` — standalone `mc3.xml` validator
- `MeshWorldMap` — 2D biome/region map printer (`--chunk X,Y` highlight)
- `MeshWorldMaterials` — prints the material catalogue
- `MeshWorldPack` — SQLite content-pack builder
- `MeshWorldPlanet` — planetary map CLI
- `MeshWorldBuildMc3Lib` — (re)generates `data/mc3lib/*.mc3lib.json` from
  C++ source (`src/tools/build_mc3lib_content.cpp`)
- `MeshWorldTests` — the gtest binary (99 test files)

**App:** `apps/mesh-world-app` — interactive explorer with a first-person
`FPCamera` (WASD + collision), sky/weather/day-night rendering, MAP12
zoomable planetary map UI. **No overview/aerial camera mode exists.**

**Recently implemented (R-series, most recent first):**
- R146 disk-write containment — the app's generated chunk cache is a bounded
  256-entry working set, evicting only the oldest disposable XML entry before
  it grows; a byte-identical forced regeneration is not rewritten. Export and
  generic-core caches remain unlimited by default, and no existing save was
  deleted.
- R143a-c nature composition — reusable `nature.*` MC3 definitions now
  provide broad-canopy/pine, jungle canopy/bamboo, barrel-cactus/agave,
  alpine spire/outcrop, cypress/snag, and driftwood/shore-root silhouettes.
  They carry metadata, bounds, collision choice, and low-LOD proxy data and
  are selected deterministically through `AssetRegistry` by the natural
  generators. Forest primary and clustered trees now share a deterministic
  5 m clearing plus 1.5 m canopy-margin exclusion. R143 remains in progress
  for other-family density masks and visual-budget evidence.
- R142 nature showcase — the menu now offers **Explore Biome Showcase**:
  an authored forest-start route through jungle, mountain, tundra, meadow,
  desert, beach, swamp, and ocean. It deliberately uses existing natural
  generators and avoids planet-map/model-streamer mutation, so it is a real
  content tour rather than another city spawn. All MAP16 biome names now use
  the closest non-empty established family until R143 authors distinct assets.
  Export validation is 81/81 chunks, 0 errors/0 warnings.
- R114 showcase follow-up — `examples/city_showcase.json` is now a 7×7
  mixed-region, deterministic composer scene (houses/apartments/shops/
  square + landmark), and the app exposes it through an **Explore City
  Showcase** action. Crucially, the app now calls `register_composer_assets`
  at startup; without it any opted-in composer world had an empty
  `AssetRegistry` and silently fell through to legacy content.
- R114 showcase material fix — registered the `plaster_beige` facade
  material used by `apartment.block.wide_01`, so both the live renderer
  and standalone GLB export retain its intended warm facade instead of
  falling back to an unmaterialed surface.
- R114 app-runtime correction — visual testing found that
  `PersistentWorldMap` was overwriting the showcase's authored regions with
  procedural forest/ocean data, while the planet-scale `Model3DStreamer`
  rendered unrelated trees over it. The showcase now isolates both systems,
  uses its own versioned cache directory, and updates `WorldRenderer`
  independently of persistent-map presence. A Mesa/Xvfb screenshot confirms
  the actual city road/building/prop scene renders.
- R114 app adoption follow-up — ordinary interactive worlds now explicitly
  opt into the composer too. This is deliberately an app-only policy:
  `WorldConfig::use_world_composer` remains `false` by default for existing
  library callers and tools, while regions with no composer coverage retain
  their established fallback generators.
- R114 persistent-city follow-up — newly visited persistent-world cells now
  share 4×4 biome planning blocks; new city blocks contain a small street
  grid plus house/apartment/shop/square parcel types. `WorldMap::set_info()`
  propagates those late-loaded road exits to adjacent parcels without
  discarding explicit configured exits. Existing SQLite rows are read
  unchanged. Normal app entry selects the nearest preloaded composer-ready
  city parcel when available, otherwise retains the historical centre spawn.
- R130 — `MC3Validator` extended with the remaining single-chunk-
  computable §21.2/§21.3/§21.4 checks (texture-reference/material-slot/
  LOD-mapping validity, terrain penetration, instance/triangle/
  draw-call metrics added to `ValidationResult`). See §3 for the full
  write-up.
- R129 — `BuildingComposer` extended to `RegionType::square`, closing
  Stage 4 of the revival migration completely; also root-caused/fixed
  the `"zone"` metadata bug. See §3/§4 for the full write-up.
- R128 — city showcase completion (landmark asset + LOD wiring).
- R127 — `BuildingComposer` extended to `RegionType::shop_street`.
- R126 — `BuildingComposer` extended to `RegionType::apartment_block`.
- R114 v1 — deterministic city showcase (`examples/city_showcase.json`,
  a real 3×3-chunk area with one crossroad + 4 composer-driven residential
  blocks) + a real shared instance-resolution function
  (`ObjectDefinitionLibrary::resolve_instance_definitions()`) so batch
  `.glb` export actually contains real geometry for composer-placed
  content (previously it silently exported nothing for any instance ref).
- R106 — material catalogue: confirmed warning-not-error decision; fixed 4
  test files with order-dependent material-registration gaps.
- R113 — the C++ world composer (`BuildingComposer`/`Parcel`/
  `AssetRegistry`/`StyleProfile`), evolved through: parametric parcel
  layout, exits-aware cross-chunk continuity, size-aware
  parcel/house-width matching, a wide gable-roofed house variant, and a
  fix for a real parcel-overlap bug at chunk corners where two adjacent
  sides both border a road.
- R112 — first real `mc3lib` content batches (windows/doors/roofs/facade
  modules/street furniture/props/vehicles), loaded through the real
  `Mc3ImportResolver` mechanism.
- R103 — `<script>` element support (`Mc3Object::scriptId` in mesh-craft +
  a new MeshWorld-side `Mc3ScriptRunner` executing Lua at compose time).

**Known working examples:** `examples/world.json` (400-chunk demo city,
seed 42), `examples/city_showcase.json` (49-chunk mixed-region showcase,
seed 100), and `examples/biome_showcase.json` (81-chunk natural route, seed
142). The city showcase enables `use_world_composer` directly and validates
cleanly with `MeshWorldExport --validate --force --threads 4` (0 errors/0
warnings); its exported XML contains real house, apartment, shop, square,
landmark, street-furniture, vehicle, and tree instances.

**What does NOT work yet / is not implemented:**
- The C++ composer (`BuildingComposer`) now handles all 4 common region
  types: `RegionType::small_house_block`/`apartment_block`/
  `shop_street`/`square` (R113/R126/R127/R129) — Stage 4 of the revival
  migration is fully closed.
- `MC3Validator` (R130) only validates single-chunk-computable checks.
  Genuine cross-chunk spatial checks (building-road intersection,
  cross-chunk road continuity) and socket/cave-connection-alignment
  checks are NOT implemented — deferred, since they would need a real
  multi-chunk/neighbor-context interface change, and no chunk yet
  composes non-trivial-socket content (blocked on R104).
- No moving-vehicle or traffic-simulation system — all vehicles are
  static placements.
- One landmark asset/mechanism exists (R128:
  `landmark.clocktower_01`/`WorldConfig::landmarks`) but it's
  deliberately v1-simple — one config-listed instance per targeted
  chunk, not a generic "many landmarks, auto-placed by rules" system.
- No overview/aerial camera — only first-person.
- LOD tier data (`Mc3AssetMetadata.lods`) is now read at runtime for
  composer-placed chunk instances too (R128:
  `BuildingComposer::resolve_instance_id()`), not just MAP11 point-cloud
  placements — but no caller yet actually SETS `ChunkContext::lod` from
  live camera distance (it stays at its default value 2/"near"); a real
  distance-based LOD picker remains future work.
- R104 (giving mesh-craft itself real `<script>` execution, instead of
  keeping it MeshWorld-side only) is not implemented — flagged as a
  bigger, cross-repo commitment needing explicit sign-off.
- This sandboxed development environment has no discrete GPU, but the app
  can now be smoke-rendered through Xvfb/Mesa software GLES (see the
  2026-07-25 entry below). That proves execution and basic composition only;
  an interactive GPU/display review remains needed for final artistic and
  performance assessment.

---

## 3. Recent changes

**2026-07-25 — R142 closed: nature is now directly visible, not only named
in the planetary map.** The app's ordinary startup intentionally selects a
composer-ready city parcel and the city showcase is all-city, which is why a
user could play for a long time without seeing forest, jungle, desert, coast,
or mountains despite their map classifications existing. The new **Explore
Biome Showcase** starts in forest and places eight other existing natural
environments in a deterministic 3×3 tour. It is isolated from persistent map
mutation and planet placement overlays. Every MAP16 zone now dispatches to a
closest existing natural generator rather than blank `EmptyGenerator`; this
is only a temporary family fallback, not Stage-6 asset composition. The first
real whole-tour export found and fixed beach terrain penetration/out-of-bounds
driftwood, forest out-of-bounds litter, and seven missing natural material
registrations. Validation: 81/81 chunks, 0 errors/0 warnings; full suite:
1651/1651; Debug app build: `CCACHE_DISABLE=1 cmake --build
apps/mesh-world-app/cmake-build-debug --target MeshWorldApp --parallel 4`.
MeshCraft was not modified.

**2026-07-25 — R140 closed: explorer frame-time containment.** The app had
kept as many as 29 nearby chunks and submitted every object in every resident
MC3 document, including chunks behind the camera; ordinary worlds also woke
the placement SQLite streamer every frame. Interactive radius is now 2 (at
most 13 chunks), a conservative chunk bounding-circle/FOV/far-plane cull
avoids parse and draw traversal for off-camera chunks, and parsed documents
are evicted when no longer resident. Placement streaming now runs only after a
chunk or altitude-band query extent changes, while its cheap per-placement
distance check remains per-frame. CNA already targets a fixed 60 Hz timestep,
so no ad-hoc sleep was added. Tests: 1648/1648; Debug `MeshWorldApp` builds
with `--parallel 4`. No MeshCraft files were modified. A target-hardware
profile remains needed before object-level culling/batching (R141).

**2026-07-25 — R138 closed: actual showcase road adoption and traffic-light
states.** The visible defect was threefold: the app continued to read its old
showcase MC3 cache, Lua zone generators could draw full-tile fallback strips
despite R134's canonical exits, and `ChunkPipeline` let incidental map zoning
replace explicitly configured road/crossroad layout. App chunk caches now use
a versioned `r134_road_graph` subdirectory (old saves are retained, never
deleted); the next **Explore City Showcase** launch regenerates the 49 chunks.
Lua `road`/`crossroad` geometry now consists of a centre patch plus only the
declared boundary arms; configured/persisted layout wins over a map candidate.
Traffic lights have MC3 sphere lenses with one deterministic bright
red/amber/green lens per head and two dim lenses. The renderer currently does
not advance MC3 states/actions, so this is deliberately a stable snapshot,
not a claim of live signal animation. `MeshWorldExport --validate --force
--threads 4 examples/city_showcase.json` reports 0 errors/0 warnings for all
49 chunks; Debug `MeshWorldApp` builds with `--parallel 4`; focused tests and
the full 1645-test suite pass. MeshCraft was not modified.

**2026-07-25 — R134 closed: canonical city road edges.** `WorldMap` now
separates authored exits from derived frontage and builds one symmetric
physical-edge graph for roads/crossroads. `road_connections()` drives road
geometry, while `road_frontage()` drives parcel composition. Explicit
`road_termini` makes legal boundary/internal dead ends declarative and the
export validator rejects unapproved isolated or degree-one road cells. The
7×7 showcase and 20×20 demo declare their legal ends; road/crossroad output
no longer guesses missing arms. Persisted `set_info()` updates recompute rather
than accumulate exits. Coarse planet-map crossings now remain a context hint,
not fabricated exact chunk geometry; R137 tracks a proper local materialiser.
Focused tests: 40/40; both demos export-validate cleanly with four threads.
Full suite: 1642/1643, retaining only the known unrelated map Lua failure.

**2026-07-25 — R133 closed: MC3 instance collision proxies.** New
renderer-independent `Mc3Collision` resolves transformed, aliased MC3
instances through their definition metadata and exposes `none`, `box`, and
non-fatal unsupported-proxy diagnostics. `WorldRenderer` now adapts those
local boxes into chunk world space, and the app delegates capsule-vs-AABB
sliding to the core-tested `PlayerCollision` helper. Regenerated MC3 libraries
make decorative R112 assets (lamps, benches, mailboxes, windows, roofs,
facades) explicitly passable while cars/vans retain a box proxy. Eight focused
tests pass; all 49 showcase exports validate with 0 errors/0 warnings; full
regression is **1646/1647 passed**, retaining only the known flaky
`MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12`. Both app-involved source
files compile with strict flags, but a fresh Xvfb walk-through remains blocked
by the sandbox's unavailable NanoSVG fetch during CMake reconfigure. MeshCraft
was not modified.

**2026-07-25 — R132 closed: dependency-pruned standalone MC3/MCB assets.**
New `prune_mc3_dependencies()` walks selected root definitions through
instance, variant, and `assetMetadata.lods` references, copies the resulting
object graph without sharing mutable nodes, and retains only referenced
document-owned materials, bitmap/SVG textures, and scene basics. It clears
library/import identity so the result is an ordinary standalone document;
unknown definition references fail loudly while absent document materials
remain legitimate external registry references. `MeshWorldPruneMc3Lib`
loads/resolves one `.mc3lib` from its own directory, emits XML/JSON or MCB
with `--mcb`, and reports the retained counts. Runtime `scriptId` assets are
deliberately rejected: parsing or executing dynamic `def:place()` references
would cross into R104, and a silently incomplete "standalone" output would
be incorrect. New `Mc3DependencyPrunerTests` cover transitive local/aliased
variant/LOD definitions, materials, bitmap/SVG textures, deep-copy isolation,
and missing/scripted rejection. Focused tests: **2/2 passed**; real CLI
smoke: `streetlamp.classic_01` reduced from its library to 2 definitions and
saved as MCB, while a modular scripted house was correctly rejected. Full
regression: **1638/1639 passed**, with only the known unrelated map-layer
flaky failure.

**2026-07-25 — R131 closed: strict `mc3.json` interchange schema.** Added
`schemas/mc3.schema.json`, a versioned Draft 2020-12 contract covering the
semantic JSON emitted by MeshCraft and MeshWorld's tracked `.mc3lib.json`
assets: top-level scene data, imports/libraries, environment, materials,
media, definitions, recursive objects, transforms, primitive/extrusion
data, actions, and asset metadata. Fixed-shape records reject unknown
fields; open maps such as metadata, sockets, LODs, and state maps retain
their intentional dynamic keys. `MC3JsonSchemaTests` validates every tracked
library plus rejects an unknown top-level field, an invalid format, and a
malformed transform vector. The test contains only a deliberately small
evaluator for the standard keywords used by this schema, avoiding any new
MeshWorld runtime dependency; authoring tools should use a full Draft
2020-12 validator. A discovered real writer convention (`segments: 0` for
the default primitive tessellation) is explicitly accepted. This contract
does not replace MeshCraft's backwards-compatible parser or `MC3Validator`'s
semantic XML checks. Focused validation: **36/36 passed**
(`MC3JsonSchemaTests.*:MC3ValidatorTests.*`); full regression: **1636/1637
passed**, with only the documented unrelated map-layer flaky failure.

**2026-07-25 — app showcase visually exercised and corrected:** the
standalone app now builds successfully in this environment and was run in
Xvfb with Mesa OpenGL ES 3.2. The first real screenshot exposed two bugs
hidden by XML validation: `PersistentWorldMap::ensure_region()` mutated the
curated `WorldMap` into procedural forests, and `Model3DStreamer` overlaid
unrelated planet placements. Removing both from the curated path then
revealed a third coupling: `WorldRenderer::update()` was wrongly nested
under `persistent_map_`, so no chunks streamed once the mutation was fixed.
`start_city_showcase()` now gives the deterministic layout an isolated
`r114_mixed_chunks` cache, disables the two mutating/overlay systems but
retains the independent full-screen map UI, and updates the chunk renderer
whether or not a persistent map exists. The final screenshot shows the
crossroad, textured roads, city props, houses/shops, and apartment content;
the 29 chunks in its radius contain the expected composer generator IDs and
instances. This is an execution-level visual baseline, not a claim of
finished artistic/UX review.

**2026-07-25 — persistent ordinary worlds now exercise MC3 composition:**
the first post-showcase ordinary-world smoke test revealed that merely
enabling `use_world_composer` was insufficient: the legacy
`PersistentWorldMap` generated independent region cells without roads, so
the road-frontage-aware composer correctly fell back to legacy buildings.
New first-visit terrain is now coherent in 4×4 planning blocks, and each new
city block has deterministic road/crossroad cells plus varied house,
apartment, shop, and square parcels. Late population now recomputes only
additive road exits around the changed map cell, preserving explicit config
exits. The app chooses a nearby composition-ready parcel from its preloaded
radius, falling back to the centre if none exists. Xvfb smoke execution of a
fresh world produced cached `cpp.chunk.composer.apartment_block`,
`small_house_block`, and `square` documents; the screenshot confirmed the
new city scene loads. The test save was removed. Focused composer/persistence
coverage passed 18/18; the full suite remained 1634/1635 with only its known
unrelated `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12` failure.

**2026-07-24 — mixed composer showcase made app-visible:** expanded
`examples/city_showcase.json` from the original 3×3 house-oriented demo
to a deterministic 7×7/49-chunk city. The central app streaming radius
contains a house block, apartment block, two shop streets, square,
crossroad, roads, clocktower landmark, and composer props (lamps,
mailboxes, vehicles, trees). `MeshWorldApp` now registers composer assets
at startup and exposes **Explore City Showcase** in its menu. The
dedicated start path deliberately keeps the app's planetary map UI while
not passing its map candidates into the chunk streamer, so they cannot
overwrite this hand-authored showcase layout. Added a configuration test
that locks each region and its radius-3 visibility contract.

`MeshWorldExport --validate --force --threads 4 examples/city_showcase.json`
generated all 49 chunks with **0 errors and 0 warnings**. `MeshWorldGLB
--stats` then converted all 49 documents successfully (5,410 objects,
106,494 vertices, 47,596 triangles). The run exposed a real missing
`plaster_beige` registration for the composer apartment facade; it is now
registered and covered by `MaterialRegistryTests`, eliminating the
unregistered-material warnings. The remaining 49 GLB warnings are one
expected ambient-light conversion warning per chunk. At that point the
app's initial CMake dependency fetch was stalled; the preceding 2026-07-25
entry records its completed build and headless screenshot baseline. A human
artistic review remains `needs_human`.

**2026-07-18 — R130 closed: `MC3Validator` extended with the remaining
single-chunk-computable §21.2/§21.3/§21.4 checks** — the rest of R122's
scope, bounded per an explicit user Round-2 decision to what a single
chunk's own XML can answer. **R130a (§21.2):** a new
`check_materials_and_textures()` pass flags dangling
`base_color_texture`/`normal_texture`/`metallic_roughness_texture`/
`occlusion_texture`/`emissive_texture` references (must match a
declared `<texture id="...">` in the same document) and validates
`<definition><assetMetadata>` blocks (`materialSlots` non-empty/
non-duplicate; `lods` entries need a non-empty tier + definition id and
must not self-reference their own owning definition when resolvable in
the same document). **R130b (§21.3 subset + §21.4):** a
`y >= -0.05f` terrain-penetration check on `check_object()` (exempting
`material="water"`, for `RiverBankGenerator`/`BridgeGenerator`'s
intentionally recessed water planes), plus 3 new `ValidationResult`
fields — `instance_count`, `triangle_count` (reusing
`ChunkPipeline::get()`'s own `Mc3MeshBuilder` computation), and
`draw_call_estimate` (a documented placeholder, `== object_count`,
pending R120's real batching) — all now printed by `MeshWorldValidate`.
**Deliberately deferred, not silently dropped** (see `plan.md`'s
R122/R130 entries): genuine cross-chunk spatial checks (building-road
intersection, cross-chunk road continuity — needs a real
multi-chunk/neighbor-context interface change), socket/cave-
connection-alignment checks (no chunk composes non-trivial-socket
content yet, blocked on R104), and shadow-caster/LOD-reduction-ratio
metrics (need `assetMetadata` fields not computable from a single
exported chunk). 15 new tests in `tests/MC3ValidatorTests.cpp`; full
suite rebuilt clean and re-run this session: **1632/1634 pass** (the 2
failures are the same pre-existing, run-order-dependent flaky tests
documented in §2/§5). Verified via `MeshWorldExport --validate --force`
against `examples/city_showcase.json`: 0 errors/0 warnings (of 9
chunks). **Per explicit user instruction, R131 (JSON Schema) and R132
(dependency pruning) were deliberately NOT started this session** —
see §9.

**2026-07-18 — R129 closed: `BuildingComposer` v3 extended to
`RegionType::square`,** the last of the 4 common region types still on
the legacy `SquareGenerator.cpp` path. New `compose_square()` path in
`BuildingComposer.cpp`: ports the legacy generator's own literal
fountain/plinth/water-bowl/jet/paths/conditional-monument geometry
unchanged, but the 4 corner lamps are now a real
`AssetRegistry::query("street_furniture", ...)` pick (replacing the
hardcoded `"metal_lamp_ornate"` raw cylinders) — benches/corner trees
stay direct-by-id, same convention as the residential yard trees.
Deliberately does not call `derive_parcels()` (a square is one
whole-chunk composition, not a row of parcels); falls through to
`std::nullopt` when no `street_furniture` asset is registered, same
contract every other region uses. 8 new tests; verified via
`MeshWorldExport --validate --force` against a real `square` demo world:
0 errors/0 warnings, output confirmed to use the composer path with real
asset-backed lamps. **This closes Stage 4 ("C++ world composer selects
assets") completely** — see `docs/migration-stages.md`/`plan.md`'s R129
entry for the full writeup.

**2026-07-18 — `"zone"` metadata bug root-caused and fixed (R129):** the
M157 map-layer override in `ChunkPipeline::build_context()` was
unconditionally replacing `ctx.zone` (correct, flat-config-derived) with
the planet's own sampled biome ordinal. Fixed with a new
`ChunkContext::authored_zone` field (captured before the override) that
`GenerationMetadata::from_chunk_context()` and 4 Lua zone generators
(`crossroad`/`forest`/`park`/`road.lua`) now report instead. `ctx.zone`
itself is untouched — real generator routing is unaffected. 6 new
regression tests; verified against the exact repro §7 documents
(`examples/city_showcase.json` now reports `"city"` for every chunk, was
`"deep_ocean"`). See §4 for the full root-cause writeup.

**2026-07-18 — documentation consistency pass (R129 iteration, in
progress):** `docs/migration-stages.md`'s Stage 4 table was stale (still
said "not started" despite R113/R126/R127/R128 shipping) — corrected to
"done", noting R129 (in progress) closes the last remaining region type
(`square`). `plan.md`'s R114 entry got a cross-reference note (not a
history rewrite) confirming R128 closed its v2/v3 landmark/LOD gaps.
The "ambient light has no glTF equivalent" `MeshWorldGLB` warning
(former §8 item #2) is now confirmed expected/ignorable, documented with
a code comment in `src/tools/export_glb.cpp`.

**2026-07-18 — R125-R128 closed** (see `plan.md`'s own entries for full
per-task detail): `data/taxonomy/taxonomy.json`/`containment.json`
curated from `object.sqlite3` (99 nodes/80 rules, up from 64/56);
`BuildingComposer` extended to `RegionType::apartment_block` (new
`apartment.block.wide_01` asset) and `RegionType::shop_street` (new
`shop.building.storefront_01` asset); one new `landmark.clocktower_01`
asset placed via a new `WorldConfig::landmarks`/`ctx.landmark`
mechanism, used by `examples/city_showcase.json` (which now also sets
`use_world_composer: true`); `ctx.lod` now actually consulted by
`BuildingComposer::resolve_instance_id()` to swap composer-placed
instances to their authored low-LOD proxy. All verified via
`MeshWorldExport --validate` (0 errors/0 warnings) and the full
`MeshWorldTests` suite (1606/1609 pass; the 3 failures are pre-existing,
run-order-dependent flakiness unrelated to this work, documented in
`plan.md`'s R125 entry).

(See `plan.md`'s R106/R113/R114 entries and recent `git log` for full
detail. Summary of the last 5 commits on `develop`:)

- **`26d5693`** — R114 v1: `examples/city_showcase.json` added. New shared
  `ObjectDefinitionLibrary::resolve_instance_definitions()`
  (`include/ObjectDefinitionLibrary.hpp`, `src/ObjectDefinitionLibrary.cpp`),
  replacing a private duplicate in `src/WorldRenderer.cpp` and newly used
  by `src/tools/export_glb.cpp`. Fixed a real bug: the old version didn't
  recurse into a newly-injected definition's own children, and didn't
  handle qualified `"alias:id"` refs. Also fixed material-injection
  ordering in `export_glb.cpp` (was scanning for referenced materials
  *before* resolving instances). One small addition to
  `src/BuildingComposer.cpp`: one real yard tree per parcel. 5 new tests
  (`tests/ObjectDefinitionLibraryTests.cpp` ×4,
  `tests/BuildingComposerTests.cpp` ×1).
- **`4f2cb65`** — R106 closed: confirmed warning-not-error decision;
  fixed 4 test files (`tests/BuildingComposerTests.cpp`,
  `tests/ChunkPipelineDiagnosticsTests.cpp`,
  `tests/WorldRendererTests.cpp`) that only avoided spurious
  "material not registered" warnings by accidental test-run-order luck.
- **`02cc090`** — Fixed a real, 100%-reproducible parcel-overlap bug in
  `src/Parcel.cpp`: north/south and east/west parcel rows were computed
  fully independently and overlapped whenever two adjacent chunk sides
  both bordered a road. New regression test
  `ParcelTest.AdjacentActiveRowsNeverProduceOverlappingParcels`.
- **`fcf19c8`** — Added `house.gable.wide_01` (a wide house with a real
  gable roof) to close a style-coverage gap for wide-class parcels.
- **`104920c`** — Size-aware parcel/asset matching:
  `derive_parcels()`/`BuildingComposer` now match house width to parcel
  width instead of using one candidate pool per chunk.

**Tests added this session:** ~15 new tests across
`ParcelTest`/`BuildingComposerTests`/`ObjectDefinitionLibraryTests`.
**Behavior changed:** `BuildingComposer` now places a real yard tree per
parcel (previously it didn't); parcel layout no longer produces
overlapping houses at chunk corners; batch `.glb` export now contains
real geometry instead of silently dropping every instance reference.

---

## 4. Current blocker / main problem

**There is no active build-breaking blocker.** The root build is clean;
the latest full run passed 1638/1639 tests, with only the documented
run-order-dependent `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12`
failure. The expanded showcase validates with 0 errors/0 warnings, and the
standalone app now builds and renders it under Xvfb/Mesa after the three
showcase-specific runtime couplings recorded in §3 were fixed. The remaining
limitation is a normal GPU/display artistic and performance review, not a
blocked application build or basic rendering path.

**Fixed 2026-07-18 (R129):** the `"zone"` metadata bug (previously
documented here as "confirmed, not yet root-caused") is now root-caused
and fixed. **Root cause:** `ChunkPipeline::build_context()` sets
`ctx.zone = ci.zone` (the correct, flat `WorldConfig`-derived zone) but
then, when a `MapPipeline` is attached (true for every `MeshWorldExport`
run, since it always auto-creates a hand-off planet — see
`export_chunks.cpp`), unconditionally overwrites `ctx.zone` with
`static_cast<ZoneType>(ctx.map_context.biome_ordinal)` — the *planet's
own* procedurally-sampled biome at that world's coordinates, which is
wholly unrelated to the flat zone a demo world like
`examples/city_showcase.json` actually declares (that plane happens to
sample as `deep_ocean` at the default seed/coordinates used). **Fix:** a
new `ChunkContext::authored_zone` field captures `ci.zone` *before* the
map-layer override can touch it (`ctx.zone` itself is left untouched —
it still legitimately drives real generator-routing decisions);
`GenerationMetadata::from_chunk_context()` now reports
`ctx.authored_zone` instead of `ctx.zone`. The four Lua zone generators
that build their own metadata (`crossroad`/`forest`/`park`/`road.lua`)
were updated the same way, using a new `ctx.authored_zone` Lua-exposed
field. Verified via the exact repro this section used to document:
`grep '"zone"' /tmp/mw_check/*.mc3.xml` now prints `"city"` for every
chunk of `examples/city_showcase.json`, both C++- and Lua-generated.
New regression tests: `GenerationMetadataTests.*` (4 tests) and
`ChunkPipelineMapContextTest.AuthoredZone*` (2 tests).

---

## 5. Known bugs and limitations

- **Done (R130):** `MC3Validator` now also validates texture/material-
  slot/LOD-mapping consistency, checks terrain penetration, and reports
  instance/triangle/draw-call metrics via `ValidationResult` -- see §3
  for the full write-up.
- **Done (R134):** configured and persistent road continuity now has a
  canonical multi-chunk graph and `MeshWorldExport --validate` topology pass.
  **Done (R138):** the Lua-first path and a fresh app showcase cache now
  respect that exact graph; no fallback may paint an absent neighbour arm.
  **Incomplete:** planet-map crossings are only coarse context hints until
  R137 materialises them through a local symmetric graph. Existing persistent
  cities still retain their already stored region layout; R139 needs a human
  decision before any migration/regeneration changes those rows. Building-road
  and socket/cave checks remain outside `MC3Validator`.
- **Done (R129):** `BuildingComposer` now covers all 4 common region
  types -- `RegionType::small_house_block`/`apartment_block`/`shop_street`
  (R113/R126/R127, parcel-row-based) and `RegionType::square` (R129, its
  own whole-chunk civic composition path, no parcels -- see §6).
- **Incomplete:** LOD tier data (`Mc3AssetMetadata.lods`, authored by
  `build_mc3lib_content.cpp`) is now read at runtime for composer-placed
  instances too (R128), but no real distance-based LOD picker exists
  yet to actually set a coarser `ctx.lod` from live camera distance.
- **Incomplete:** no moving vehicles, no overview camera; landmark
  placement (R128) exists but stays deliberately v1-simple (see §2).
- **Done (R140):** the app no longer draws all resident chunks every frame or
  repeatedly re-queries unchanged placement bounds. **Incomplete:** a visible
  MC3 chunk still traverses all of its individual objects; profile first, then
  scope renderer-level object culling/batching as R141. Do not alter MeshCraft
  without separate approval.
- **Needs verification:** whether the "instance resolution" fix
  (`resolve_instance_definitions()`) has any performance impact on
  `apps/mesh-world-app`'s live rendering path — it's logically equivalent
  to the old code plus a fixed-point loop, but was not profiled.
- **Risky assumption:** `Parcel.cpp`'s `kParcelDepth = 8.0f` is a shared
  estimate used for *every* house width class (standard and wide), not a
  real per-asset value — documented in-code as "close enough", not
  measured against every real house's actual footprint.
- **Technical debt:** `NEXT.md` (this file) previously grew to ~4800
  lines as an ever-appended narrative log; it has been rewritten into
  this concise format. `plan.md` remains the detailed historical record
  and is not similarly trimmed.
- No `.clang-format`, linter, or CI config exists in this repository.

---

## 6. Architecture notes

**Two subsystems, cleanly separated:**
- `namespace MeshWorld::Map` (`include/Map/`, `src/Map/`) — planetary
  map: quadtree LOD 0-18, 52-biome `ZoneType` classification, hydrology,
  mountain ranges. Must never depend on chunk-layer types.
- Flat `namespace MeshWorld` — the legacy chunk/city generation layer,
  plus the newer R113 composer types. Consumes map output
  (`ChunkContext::map_context`) optionally.

**Chunk generation flow** (`ChunkPipeline::get(x, y)`):
1. `WorldMap` resolves the cell's `(ZoneType, RegionType, EdgeExits)` from
   the loaded `WorldConfig` (zones/regions defined by JSON, e.g.
   `examples/world.json`).
2. If `WorldConfig::use_world_composer` is true: `BuildingComposer::
   compose_chunk()` runs. For `small_house_block`/`apartment_block`/
   `shop_street`: `derive_parcels()` (`Parcel.cpp`) lays out parcels,
   `AssetRegistry` picks style-tag-matching + width-matching candidates,
   `MC3Writer` emits the XML. For `square` (R129): a separate
   `compose_square()` path runs instead -- no parcels at all, one
   whole-chunk civic composition (literal fountain/plinth/paths/
   monument geometry ported from `SquareGenerator.cpp`, but real
   `AssetRegistry`-queried `street_furniture` lamps). Every other
   region falls through unchanged.
3. Otherwise (or if the composer returned `std::nullopt`):
   `LuaGeneratorRegistry` tries a Lua script (`generators/lua/**`)
   first; on failure/absence, falls back to a hand-written C++
   generator (`src/generators/*.cpp`).
4. Output is real `mc3.xml` text (`Mc3SceneBuilder`/`Mc3DocumentBuilder`,
   from mesh-craft).

**Instance resolution — an important, recently-changed invariant.** Any
`<instance ref="some_id"/>` in generated XML is a *reference*, not
embedded geometry. Two consumers need to resolve these against real
definitions before the geometry is usable:
- The live renderer (`WorldRenderer::render()`/`render_placements()`).
- Batch export tools that load already-exported chunk XML
  (`MeshWorldGLB`).
Both now call the **same shared function**,
`MeshWorld::resolve_instance_definitions()`
(`include/ObjectDefinitionLibrary.hpp`). **Any new tool that loads
exported chunk XML and needs real geometry must call this too** — it is
easy to forget (this was exactly the R114 bug) since the XML "looks"
complete (validates fine, has no errors) while silently missing all
instance content.

**Definition sources:**
- `ObjectDefinitionLibrary` — the general id→`Mc3Object` lookup
  (`load_all()` registers ~250+ native definitions; R112/R113 content —
  vehicles, furniture, houses — is *also* registered here directly, via
  `register_mc3lib_batch()`/`compile_and_register_modular_building()`).
- `AssetRegistry` — an *additional* queryable index on top (category +
  style tags), used only by `BuildingComposer` to pick among candidates.
  Not a replacement for `ObjectDefinitionLibrary`.
- `MaterialRegistry`/`BuiltinMaterials.cpp` — the single material
  catalogue. Unregistered material = warning, not error (§1).

**Must remain stable / do not break:**
- `MeshWorldLib` must never gain a hard dependency on SDL3/OpenGL/CNA.
- `WorldConfig::use_world_composer` must default to `false` — existing
  worlds/tests must be unaffected unless explicitly opted in.
- The `world.local.json` override mechanism looks for that **exact
  filename** next to whatever config path is passed in — not
  `<config-name>.local.json`.
- `register_builtin_materials()`, `ObjectDefinitionLibrary::load_all()`,
  `register_composer_assets()` are all idempotent and safe to call
  multiple times/from multiple places — this is relied upon throughout
  the test suite and tools.

---

## 7. Useful commands

**Configure + build (root project):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4        # use a modest -j; this machine often runs multiple sessions
```

**Configure + build (the renderer app, separate CMake project):**
```bash
cd apps/mesh-world-app
cmake --build build -j4
```

**Run all tests:**
```bash
./build/MeshWorldTests
```

**Run a filtered subset:**
```bash
./build/MeshWorldTests --gtest_filter='ParcelTest.*:*Composer*'
```

**Check for test-order-dependent bugs (as used to find/fix R106's
material-registration gaps):**
```bash
./build/MeshWorldTests --gtest_shuffle --gtest_random_seed=<N>
```

**Generate + validate a demo world (composer enabled):**
```bash
echo '{"use_world_composer": true}' > examples/world.local.json
./build/MeshWorldExport --validate examples/city_showcase.json
```

**Convert exported chunks to inspectable `.glb`:**
```bash
./build/MeshWorldGLB --stats output/chunks output/glb
```

**Regression-check the §4 "zone" metadata fix (fixed 2026-07-18, R129):**
```bash
./build/MeshWorldExport examples/city_showcase.json /tmp/mw_check
grep '"zone"' /tmp/mw_check/*.mc3.xml   # every chunk now prints "city"
```

**No lint/format tooling exists in this repo** (no `.clang-format`, no CI
config found) — nothing to run there.

---

## 8. Next smallest tasks

Stages 2 and 4 are fully closed; R142 and R143a-c are the first visible
Stage-6 foundation but not its completion. The immediate nature roadmap is the
remaining **R143** work (other-family density/masks, variants and visual
budgets), followed by
**R144** (continuous terrain/hydrology coupling) and **R145** (target-hardware
visual review/iCity comparison once its source location is supplied). R135
(parked vehicle kit), R136 (one enterable-house vertical slice), and R137
(exact local materialisation of coarse planet-map road hints) remain
independent alternatives. R139 still requires the user's SQLite migration
decision. R141 is profile-led and needs MeshCraft approval before any
object-level renderer work. R104 remains a separate MeshCraft-repository
effort owned by a different agent; do not touch it here.

(Everything previously listed here — R129's `square` composer coverage,
R130's `MC3Validator` extensions, R114 v2/v3's landmark/LOD, the
ambient-light GLB warning, and the `"zone"` metadata bug — is done; see
§3 above for each.)

---

## 9. Do not do yet

- **No moving-vehicle/traffic-simulation system.** Explicitly scoped out
  of R114 — a much bigger, separate subsystem (pathfinding, animation)
  that needs its own design pass first.
- **No new Lua generator content.** The G-series is paused at explicit
  user instruction; it conflicts with R100's "C++ composer, not more
  Lua" direction. Do not resume without asking.
- **No R104 (mesh-craft cross-repo `<script>` engine support)** without
  explicit user sign-off — it's a bigger commitment touching a sibling
  repo (`openeggbert/mesh-craft`) that other consumers may depend on.
- **No flipping unregistered-material validation to a hard error.**
  Already decided (2026-07-13): stays a warning.
- **No full "unified, corner-aware block subdivision" rewrite** of
  `Parcel.cpp`/`BuildingComposer` (non-rectangular blocks, real corner
  lots) without a fresh scoping/design pass — what exists today (one
  independent row per bordering road side, now overlap-free) is a
  smaller, already-shipped increment, not the full vision.
  `docs/world-composer-design.md` is the precedent for how a change this
  size should be scoped before implementation.
- **No broad refactor of the legacy Lua-first/C++-fallback generator
  chain** — it is intentionally being migrated incrementally
  (region-type by region-type), not rewritten wholesale.
- **No mass edits to `plan.md`'s historical entries** — it is the
  authoritative dated record; append, don't rewrite past entries.

---

## 10. Resume prompt

```
Read NEXT.md first (this file) before doing anything else, including
§9 ("Do not do yet").

R131–R134, R138, R140, R142, R143a-c, and R146 are complete. The next nature work
is R143's remaining other-family density/variants/budgets, then R144
(terrain/hydrology), with R145 as the evidence-led visual review. R135–R137
require separate direction; R139 also requires the user's persistence/migration
choice before it starts. R141 requires profiling plus separate MeshCraft
approval.

Once a task is confirmed, inspect ONLY the files it names — do not go
exploring unrelated modules, and do not refactor anything you encounter
along the way that isn't part of the task.

Make one small, verified improvement:
- Implement the fix/feature.
- Run the exact verification command listed for that task (or the
  relevant subset of ./build/MeshWorldTests) and confirm it passes.
- Run the full ./build/MeshWorldTests suite once before considering the
  task done, to catch any regression.

Do not start a second task from the list in the same session unless the
first is fully done and verified. Do not touch anything listed in
"9. Do not do yet".

When finished, update NEXT.md: move the completed task out of §8, add a
short factual note to §3 (Recent changes), and re-check whether §4/§5
need updating (e.g. if the task fixed the "zone" bug, remove it from
both).
```
