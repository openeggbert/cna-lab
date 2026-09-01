# MeshWorld — Master Task List

Single source of truth. Tasks are ordered by dependency. Mark `[x]` when done.
Architecture rules: `docs/new-direction-offline-first.md`. Full docs: `docs/`.

---

## Architecture rules (never break)

- Offline-first. No Claude API at runtime. No API keys required by end users.
- Lua generators call `Mc3SceneBuilder` — never write raw XML.
- All MC3 serialization via `Mc3Document::saveToFile()`. No competing XML builder.
- Every generated file includes `<metadata format="json" type="generation">`.
- Seeds drive variety but are NOT compatibility guarantees.
- No API keys committed. `config/` gitignored. No billing/credits/marketplace.
- C++23, CMake 3.20+, MIT license, `-Wall -Wextra -Werror`.
- SPDX header on every new file: `// SPDX-License-Identifier: MIT`.

---

## Current pipeline

```
MC3Writer → Mc3SceneBuilder → Mc3DocumentBuilder → Mc3XmlWriter (MeshCraft)
```

**State:** 31 tests pass, zero warnings. Binaries: `MeshWorld`, `MeshWorldExport`, `MeshWorldMap`.

---

## Archived: Completed / M5-M20 / Map mechanism (M-series) / Sky-weather (S-series)

**2026-07-11**: the "## Completed" section, M5-M15, M16-M20, the full
planetary map subsystem (M-series, M001-M354), and the entire Sky/day-night/
weather backlog (S-series, S101-S1103) were moved to
`archive/plan20260711.md` (user request — this file had grown to ~3200
lines). All of it is effectively closed; a short list of the individually
still-open sub-items (T194, T306/T307, M009/M012/M022/M025/M033/M064/M065,
M131, M178/M180, M329/M352/M353, S203/S303) is carried forward at the top of
the archive file itself, with a one-line note on each. `NEXT.md` remains the
authoritative source for current overall project status; this file (from
here down) now holds only the actively-worked-on backlog (the old T-series
object/building/tooling backlog below) and the current G-series.

---

## Backlog — More Lua object generators

**Triaged 2026-07-11 (user-requested audit of the whole old T-series backlog,
dedicated research agent, real evidence per task, not assumed).** None of these
31 exact files exist on disk. Some are genuinely still open; others are
functionally covered by later work under a different mechanism (built inline
into a specific generator instead of a standalone reusable Lua object, or
superseded by the later C++ biome-generator expansion) — each marked
accordingly, not blindly ticked.

- [x] T195 generators/lua/object/bed.lua — box frame + mattress + pillow boxes
      (done 2026-07-11 — frame, headboard, mattress, 2 pillows; `fabric_bed`/
      `fabric_pillow` materials)
- [x] T196 generators/lua/object/sofa.lua — box seat + box back + 2 box arms
      (done 2026-07-11 — `fabric_sofa` material)
- [x] T197 generators/lua/object/bookshelf.lua — box frame + box shelves
      (done 2026-07-11 — back/side panels + N evenly-spaced shelves,
      `shelf_count` parameter, default 4)
- [x] T198 generators/lua/object/microwave.lua — box body + small box handle
      (done 2026-07-11 — `appliance_white` body, `plastic_black` handle)
- [x] T199 generators/lua/object/oven.lua — box body + box door
      (done 2026-07-11 — `generators/lua/room/kitchen.lua` still has no oven
      wired in, but the standalone reusable object now exists)
- [x] T200 generators/lua/object/sink.lua — box basin + cylinder tap
      (built inline in `generators/lua/room/kitchen.lua` — `sink_basin` box +
      `tap` cylinder, matches spec, just not extracted as a standalone reusable
      object)
- [x] T201 generators/lua/object/toilet.lua — box base + box tank
      (done 2026-07-11 — `ceramic_white` material)
- [x] T202 generators/lua/object/bathtub.lua — box body
      (done 2026-07-11 — outer body + recessed basin inset, `ceramic_white`)
- [x] T203 generators/lua/object/car.lua — box body + box roof + 4 cylinder wheels (parked)
      (done 2026-07-11 — "cylinder" wheels implemented as flattened
      `addIcoSphere` ellipsoids instead: the scene API's `addCylinder` is
      always vertical-axis, with no rx/rz rotation binding to lay one on its
      side — documented in the generator's own header comment. 4 paint
      colors picked by variation, `tire_rubber` wheels)
- [x] T204 generators/lua/object/bicycle.lua — 2 cylinder wheels + box frame
      (done 2026-07-11 — same flattened-icosphere wheel technique as T203;
      `ApartmentBlockGenerator.cpp`'s `bikerack` boxes are unrelated parking
      infrastructure, not a bicycle model, so this doesn't overlap it)
- [x] T205 generators/lua/object/mailbox.lua — box body + cylinder post
      (done 2026-07-11 — `mailbox_body` + `wood_natural` post)
- [x] T206 generators/lua/object/fire_hydrant.lua — cylinder body + box cap
      (done 2026-07-11 — plus 2 small side nozzle stubs, `hydrant_red`)
- [x] T207 generators/lua/object/traffic_light.lua — cylinder pole + box head + 3 box lights
      (built inline in `generators/lua/zone/crossroad.lua` — "traffic lights at
      all 4 corners", matches spec exactly, not extracted standalone)
- [x] T208 generators/lua/object/sign.lua — box panel + cylinder post
      (done 2026-07-11 — `sign_panel` + `metal_dark` post; `ShopStreetGenerator.cpp`'s
      awnings are still unrelated shop-specific decoration, not generic signage)
- [x] T209 generators/lua/object/fence.lua — repeating box posts + box rails
      (built inline in `src/generators/SmallHouseBlockGenerator.cpp` — "garden
      fence along street side", `wood_fence` material)
- [x] T210 generators/lua/object/street_light_modern.lua — cylinder pole + box arm + box head
      (superseded — `generators/lua/object/lamp.lua` already covers pole+head+
      base with ornate/simple variants)
- [x] T211 generators/lua/object/park_bench_modern.lua — metal + wood material variant
      (superseded — `generators/lua/object/bench.lua` already covers legs+seat+
      backrest with a wood+metal material variant; `bench_park`/`bench_stone`/
      `bench_street` also exist in `ObjectDefinitionLibrary`)
- [x] T212 generators/lua/object/picnic_table.lua — table + 2 attached benches
      (done 2026-07-11 — tabletop + 4 trestle legs + 2 attached bench planks,
      each with its own 2 support legs; `wood_natural`)

---

## Backlog — More Lua building / room / zone generators

- [x] T213 generators/lua/building/apartment.lua — multi-floor box + windows + door
      (superseded — `src/generators/ApartmentBlockGenerator.cpp`, C++,
      chunk-scale block with courtyard/facades/canopy/lamps/bike racks —
      different scale (whole block) but same concept)
- [x] T214 generators/lua/building/shop.lua — ground floor box + signage + door + windows
      (superseded — `src/generators/ShopStreetGenerator.cpp`, C++, zone-scale
      shopfronts+awnings+cobblestone)
- [ ] T215 generators/lua/building/warehouse.lua — large box + sliding door
      (genuinely open — no warehouse generator anywhere, C++ or Lua)
- [x] T216 generators/lua/room/living_room.lua — sofa + coffee table + tv + bookshelves
      (done 2026-07-11 — composes `lua.object.{sofa,table,tv.flatscreen,
      bookshelf}.simple` via `scene:callGenerator`, real as of this session's
      own infra work. Content only — still no interior-room PLACEMENT
      pipeline wiring this into any building generator, unchanged finding
      from the earlier triage, see G-series in the map-mechanism section
      below and `docs/procedural-model-generator-roadmap.md`)
- [x] T217 generators/lua/room/bedroom.lua — bed + wardrobe + nightstand
      (done 2026-07-11 — composes `lua.object.bed.simple` (T195, done same
      session) + 2 new objects, `wardrobe.lua`/`nightstand.lua`)
- [x] T218 generators/lua/room/bathroom.lua — toilet + sink + bathtub
      (done 2026-07-11 — composes `lua.object.{toilet,bathtub}.simple`
      (T201/T202, done same session) + a new standalone `sink.lua`, closer
      to T200's own original literal spec than the pre-existing inline
      `room/kitchen.lua` sink ever was)
- [x] T219 generators/lua/zone/desert.lua — sand ground + cactus + rocks
      (superseded — `src/generators/DesertGenerator.cpp`, `ZoneType::desert`,
      C++ MAP16-24 biome expansion)
- [x] T220 generators/lua/zone/jungle.lua — dense tree layer + undergrowth
      (superseded — `src/generators/JungleGenerator.cpp`, `ZoneType::jungle`)
- [x] T221 generators/lua/zone/beach.lua — sand + water plane + beach chairs
      (superseded — `src/generators/BeachGenerator.cpp`, `ZoneType::beach`)
- [x] T222 generators/lua/zone/square.lua — paved ground + fountain + benches
      (superseded — `src/generators/SquareGenerator.cpp`, matches spec exactly)
- [x] T223 generators/lua/zone/shop_street.lua — shop facades + sidewalk + lamps
      (superseded — `src/generators/ShopStreetGenerator.cpp`)
- [x] T224 generators/lua/map/basic_city.lua — city block layout using containment rules
      (superseded — `generators/lua/map/city.lua` (M153/MAP10) plus the far
      larger C++ Map pipeline (RoadNetwork, Settlements, ZoneType zoning))
- [x] T225 generators/lua/map/desert_outpost.lua — small settlement in desert zone
      (superseded — `src/styles/DesertOutpost.cpp`, a C++ "Style" palette
      re-skinning existing block/road/park generators into a desert
      settlement, achieving the goal via the Style/Settlements mechanism)

---

## Backlog — C++ chunk generator improvements

**Triaged 2026-07-11 (real evidence per task, dedicated research agent).**

- [x] T226 ForestGenerator: TreeClusterGenerator sub-generator (dense tree groups)
      (2026-07-12 — implemented as a focused free function,
      `add_tree_clusters()`, in `ForestGenerator.cpp`'s own anonymous
      namespace rather than a full separate class-with-header: 2 cluster
      centers, 5 trees tightly jittered around each, purely additive after
      the existing 32-tree uniform scatter. T226-229 batched together as
      4 such functions — see their own entries below for the same
      "function, not a class" scoping note, not repeated per-task.)
- [x] T227 ForestGenerator: BushGenerator (small cylinder clusters)
      (2026-07-12 — `add_bushes()`: 5 bushes, each 2 overlapping cylinders
      using the existing `shrub_foliage` material — no new material
      needed.)
- [x] T228 ForestGenerator: ClearingGenerator (open area within forest)
      (2026-07-12 — `add_clearing()`: a visible ground-break (a
      `grass_courtyard`-material patch replacing the usual forest floor in
      one area). V1 simplification, documented in place: does not
      retroactively exclude the existing uniform tree scatter from that
      area — reworking that loop to respect an exclusion zone is separate,
      larger scope than this task's own ask.)
- [x] T229 ForestGenerator: PathGenerator (winding dirt path through forest)
      (2026-07-12 — `add_forest_path()`: a short random walk of connected
      `path_gravel` plane segments crossing the chunk, same cheap
      per-segment-plane technique `city.lua`'s own street-grid bending
      uses, simplified to one path. 6 new tests across all 4 sub-
      generators (`GeneratorTests.cpp`); fixed one now-stale hardcoded
      `<instance>` count in `ModelPlacementTests.cpp` (38→48, the 10 new
      tree-cluster instances). 1488/1488 tests pass;
      `apps/mesh-world-app` verified cleanly.)
- [x] T230 CrossroadGenerator: auto-detect 3-way vs 4-way from EdgeExits, adjust geometry
      (2026-07-12 — decomposed the old 2 full-tile-spanning "+"-shaped
      planes into a center junction square + 4 independent half-length
      arms (N/S/E/W), each drawn only if `ctx.exits` has that direction's
      road; ambiguous (0-1 real exits) falls back to drawing all 4 arms,
      matching the generator's own prior unconditional behavior. Corners/
      sidewalks/lamps deliberately left unconditional (still make sense
      visually even with one arm missing) to keep this bounded. 3 new
      tests: 4-way draws all arms, a west-missing T-junction omits only
      `road_w`, ambiguous exits fall back to all 4.)
- [ ] T231 ParkGenerator: integrate Lua bench + lamp + tree via LuaGeneratorRegistry
      (genuinely open — `src/generators/ParkGenerator.cpp` resolves
      `bench_park`/`lamp_post_ornate`/`tree_lime` via the hardcoded C++
      `ObjectDefinitionLibrary`, never calls `LuaGeneratorRegistry`;
      `generators/lua/object/{bench,lamp,tree}.lua` exist and have their own
      tests but are orphaned — unused by any C++ generator.
      **Investigated 2026-07-12, deliberately deferred, not attempted**:
      this needs real new plumbing, not a quick wiring fix — a Lua object
      script's own `LuaSandbox::execute()` output is a full mc3.xml STRING
      (whole `<mc3>...<objects>...</objects></mc3>` document with ground/
      metadata), not a reusable `Mc3Object` subtree
      `ObjectDefinitionLibrary::register_definition()` expects, so making
      this real requires parsing that XML back into an `Mc3Document`
      (`Mc3Document::loadFromString`) and re-wrapping its objects into a
      group, PLUS deciding a real id-mapping convention (existing
      hardcoded ids like `"bench_park"` don't match the Lua registry's own
      `"lua.object.bench.simple"`-style naming at all). A genuinely new,
      moderately-sized feature comparable in scope to R101/R110, not a
      quick fix — left for a dedicated future session rather than rushed.
      Confirms/extends `docs/migration-stages.md` §2.2's own finding that
      these 27 Lua object files are currently unreachable at runtime.)
- [x] T232 SmallHouseBlockGenerator: vary house sizes from variation seed
      (`src/generators/SmallHouseBlockGenerator.cpp:28-64` varies
      wall height/width/depth/shape/material per house from
      `std::mt19937_64 rng(ctx.seed)`)
- [x] T233 ApartmentBlockGenerator: vary floor count from variation seed
      (2026-07-12 — replaced the continuous height jitter with a real
      discrete `floor_count` in [5,7] × a fixed 3.2 m storey height,
      landing in the same 16-22.4 m range the old continuous jitter used
      so every other placement/dimension reads the same. 1 new test
      proving the height is always an exact floor-height multiple across
      many seeds, not an arbitrary continuous value.)
- [x] T234 RiverBankGenerator: add water plane + reeds + stones
      (water plane pre-existing; reeds + scattered stones added 2026-07-12
      — `plant_marsh_grass` instances just inside the water plane's own
      edge nearest the embankment, and a random mix of the 3 existing
      `rock_grey_small`/`rock_mossy`/`rock_pile_small` definitions
      scattered on the grass side, distinct from the embankment's own
      dressed `stone_embank` wall material. 1 new test.)
- [x] T235 BridgeGenerator: add railing boxes along edges
      (`src/generators/BridgeGenerator.cpp:33-34,40-41` already draws
      `rail_n/rail_s`/`rail_w/rail_e` boxes along both deck edges)

---

## Backlog — Tooling and CLI

**Triaged 2026-07-11 (real evidence per task).**

- [x] T236 MeshWorldExport --threads N flag (parallel chunk generation)
      (2026-07-13 — implemented carefully given a real concurrency risk:
      `Map::MapPipeline` mutates an in-memory cache with no locking, and
      the underlying `Map::MapTileStore` is SQLite-backed, so sharing one
      `ChunkPipeline`/`MapPipeline`/`PlanetWorld` across threads would be a
      data race. Investigated first rather than assuming: found
      `WorldStreamer` (`src/WorldStreamer.cpp`) already solves exactly this
      by giving each of its own worker threads a private
      `ChunkCache`/`PlanetWorld`/`MapPipeline`/`ChunkPipeline`, relying on
      `MapTileStore`'s WAL mode + busy-timeout(5s) + a construction mutex
      (`src/Map/MapTileStore.cpp`) to make several independent connections
      targeting the same `map_level{z}.db` file safe — an already-proven
      pattern, not a novel one. `export_chunks.cpp` now mirrors it: a
      dynamic work-stealing queue (an atomic cursor over the flattened
      grid, since chunk cost varies a lot by zone) hands coords to N
      worker threads, each owning its own full pipeline stack; a
      print_mutex/zone_mutex guard the shared progress line and
      zone-distribution map. `MaterialRegistry`/`LuaGeneratorRegistry`/
      `StyleRegistry` stay safe because they're fully populated once,
      single-threaded, before any worker starts (documented contract,
      `LuaGeneratorRegistry`'s own header already says so); `WorldMap`
      builds its whole grid once in its constructor and is read-only
      after that; `LuaSandbox::execute()` builds a fresh `LuaRuntime` per
      call, no shared interpreter state.

      **Verification found and ruled out a false alarm, then found and
      fixed a real (unrelated) bug**: an early `--threads 8` vs.
      sequential content diff showed ALL 400 files differing — turned out
      to be a pre-existing, harmless non-determinism in Lua-generated
      `<metadata>` JSON *key order* (not values), reproducible even
      between two purely single-threaded runs; confirmed by normalizing
      key order (Python, `sort_keys=True`) before diffing, which then
      showed 0 real mismatches. Separately, an intermittent
      material-registration warning (`plant_sea_weed_float`, `--validate`)
      looked at first like a genuine race — but reproduced identically
      with `--threads 1` and even with a single-threaded, non-`--threads`
      run, and turned out to depend only on which random planet
      `PlanetWorld::create_new()` happens to generate for a fresh
      `output_dir` (its entropy is deliberately non-reproducible,
      `PlanetWorld.hpp`'s own doc comment) placing an ocean biome at a
      grid-edge chunk — confirmed by re-running `--force` repeatedly
      against the SAME already-created world directory (stable, same
      result every time). The material gap itself was real, though
      (`OceanGenerator.cpp`'s "weed_0".."weed_4" planes unconditionally
      reference it) — same class as R106/T238's earlier finds; fixed by
      registering `plant_sea_weed_float` in `BuiltinMaterials.cpp`.
      **Rigorously re-verified determinism** after that fix: generated a
      world once with 1 thread, then regenerated the SAME persisted world
      with 1/2/4/8/16/32 threads and diffed every one of 400 files
      (key-order-normalized) against the 1-thread baseline — 0 mismatches
      at every thread count.

      2 new automated tests (`tests/ChunkPipelineConcurrencyTests.cpp`,
      not just the ad-hoc manual verification above) exercise the same
      multi-instance-pipeline pattern directly and stayed green across 5
      repeated runs. 1497/1497 tests pass (1495+2); `apps/mesh-world-app`
      verified building cleanly.

      **New finding, deliberately NOT fixed here (out of scope for a CLI
      flag)**: the Lua-generated `<metadata>` JSON's key order is not
      stable across separate process runs even without any threading —
      likely stems from an unordered/hash-based container somewhere in
      the Lua-side metadata-table-to-JSON path. Harmless (values are
      correct, only key order varies) but worth a real look eventually;
      flagged in NEXT.md for a future task, not filed as an R/T number
      yet.)
- [x] T237 MeshWorldExport --force flag (ignore cache, regenerate all)
      (2026-07-12 -- added `bool force = false` to both
      `ChunkPipeline::get()` overloads (default preserves every existing
      call site); `force=true` skips the cache-read, generates for real,
      then `cache_.store()` overwrites the old entry same as any other
      call -- so a forced run also repairs a stale/corrupt cache file for
      later plain `get()`s, not just for itself. Wired `--force` through
      `src/tools/export_chunks.cpp`. New test
      `ChunkPipelineDiagnosticsFixture.ForceRegeneratesEvenWhenCache
      HasStaleContent` seeds a bogus cache entry, confirms force ignores
      it and a later plain get() reads back the fresh (not stale)
      content. Also verified end-to-end: generated the demo world,
      hand-tampered a chunk file, confirmed a plain rerun leaves the
      tampering and `--force` overwrites it with real content. 1489/1489
      tests pass; `apps/mesh-world-app` builds cleanly.)
- [x] T238 MeshWorldExport --validate flag (run MC3Validator, print errors)
      (2026-07-12 — wired via `ChunkPipeline::get(x, y, ChunkDiagnostics*)`
      (R108's own diagnostics overload, already runs `MC3Validator`
      internally) rather than re-invoking `MC3Validator` separately;
      prints every chunk's errors/warnings with its generator id, plus a
      final summary line and a non-zero exit code if any chunk had a real
      error. **Immediately found 2 real, live, previously-invisible
      material gaps on the first real run** (`examples/world.json`, 400
      chunks): `"sidewalk"` (`road.lua`'s/`crossroad.lua`'s own
      `scene:addGround("sidewalk")`) and `"awning_stripe"`
      (`ShopStreetGenerator.cpp`'s own awning planes) — both registered
      now, same class of gap R106 found for `flowers_*`. Re-ran after
      each fix: 144→32→0 chunks with warnings across the full demo world.
      Verified via a real smoke-test run (not just a unit test — this is
      a CLI tool's `main()`, no existing precedent for unit-testing this
      binary directly). 1488/1488 tests pass (unchanged — this task
      didn't touch any tested library code, only `export_chunks.cpp` +
      2 new material registrations); `apps/mesh-world-app` verified
      cleanly.)
- [x] T239 MeshWorldExport --mcb flag (compile mc3.xml → mcb after generation)
      (fully implemented, `src/tools/export_chunks.cpp:49,131-156`, compiles
      via `MeshCraft::Mcb::saveToFile`)
- [x] T240 MeshWorldValidate binary (standalone validator, reads mc3.xml, prints result)
      (2026-07-12 — new `src/tools/validate_mc3.cpp` + `MeshWorldValidate`
      CMake target, independent of `ChunkPipeline`/`WorldConfig` — reads
      one or more mc3.xml file paths from argv, runs the existing
      `MC3Validator` directly, prints OK/WARNING/ERROR per file plus a
      summary line, exits non-zero on any error or unreadable file.
      `--chunk-size N` (default 64, `WorldConfig::chunk_size_m`'s own
      default) sets the bounds-check range; `--chunk-size 0` skips bounds
      checking, for files whose real chunk size is unknown. Calls
      `register_builtin_materials()` at startup — first smoke-test run
      without it flooded every file with false "material not registered"
      warnings, since a standalone binary starts with an empty
      `MaterialRegistry`; same class of gotcha `export_chunks.cpp`
      already handles for the same reason. Verified with a real
      hand-tampered chunk file (bounds violation → real ERROR output,
      correct exit code) and a missing-file case. 1489/1489 tests pass
      (unchanged — no unit-test precedent for validating a CLI tool's
      `main()` directly, same as T237/T238/T239); `apps/mesh-world-app`
      verified building cleanly.)
- [x] T241 MeshWorldGLB binary (convert mc3.xml → glb via MeshCraft tooling)
      (2026-07-13 — investigated `mesh-craft/mc3togltf` first (per this
      task's own long-standing note to confirm real capability before
      assuming scope): a mature, complete, heavily-tested single-file
      mc3.xml→gltf/glb CLI tool + reusable `mc3togltf_lib` (`GltfExporter`
      class), already builds via Manifold (CSG) + tinygltf + tinyobjloader.
      New `src/tools/export_glb.cpp` + `MeshWorldGLB` CMake target links
      `mc3togltf_lib` directly (matching mc3/mcb's own existing
      `add_subdirectory` pattern, guarded by `MESH_WORLD_HAS_GLTF` the same
      way `--mcb` is guarded by `MESH_WORLD_HAS_MCB`) rather than shelling
      out per file — batch-converts every `*.mc3.xml` in a directory to
      `.glb`, mirroring `--mcb`'s own batch-loop precedent but as its own
      binary since this dependency chain is heavier than Mcb's. First real
      conversion run found a genuine gap: MeshWorld generators reference
      materials by bare id, resolved only via the runtime-only
      `MaterialRegistry` — never serialized into exported chunk XML at all
      — so `mc3togltf` (a generic, MeshWorld-agnostic tool) reported every
      single material as "unknown" and wrote 0 real glTF materials despite
      14k+ objects processed. Fixed by walking each loaded `Mc3Document`
      (`doc.objects`/`doc.definitions`, recursively through `children`) to
      collect every actually-referenced material id, then injecting a real
      `Mc3Material` (base color + roughness + metallic, sourced from
      MeshWorld's own `MaterialRegistry`, no texture — a chunk file lives
      in a different directory than `assets/textures/`, so a texture URI
      wouldn't resolve there either, same class of bug already fixed once
      this session for the live renderer) for exactly those ids before
      export. **First attempt injected the WHOLE ~165-entry catalogue into
      every file unconditionally and was rejected** on measurement: it
      roughly doubled per-file `.glb` size (20KB→49KB on a real demo
      chunk) for materials that specific chunk never even used — switched
      to the referenced-only collection above, which came back to ~19KB
      (even smaller than the pre-injection baseline) with 0 "unknown
      material" warnings. Verified three ways: the tool's own `--stats`
      output (400/400 written, 0 errors, ~7 materials/file, no unknown-
      material warnings — only the expected/inherent "ambient light has no
      glTF equivalent" glTF-format limitation remains); a real GLB magic-
      byte/JSON-chunk check; and an independent Python parse of 3 sample
      files confirming valid glTF 2.0 structure with populated
      materials/meshes/nodes arrays. 1497/1497 tests pass (unchanged — no
      unit-test precedent for this binary, same as the other new CLI
      tools); `apps/mesh-world-app` verified building cleanly (unaffected,
      separate CMake project). Note: this is the one Tooling/CLI task that
      pulls a real new dependency chain (Manifold/tinygltf/tinyobjloader)
      into mesh-world's own root build for the first time — a stale,
      already-populated `build/_deps/manifold-*` checkout from an
      unrelated, never-committed earlier experiment (found via `git log`
      showing zero history of CMakeLists.txt ever referencing "manifold")
      meant the first configure only took ~20s, not a fresh network
      clone.)
- [x] T242 world.local.json override support (merge into world.json at load time)
      (2026-07-12 — `WorldConfig::load_from_file()` now looks for
      `world.local.json` alongside the given path (same directory) and,
      if present, merges it over the base JSON via RFC 7396 JSON Merge
      Patch (`nlohmann::json::merge_patch`) before parsing fields —
      nested objects merge key-by-key, arrays/scalars in the override
      replace the base wholesale (a "zones" override replaces the whole
      array, doesn't append). Absent is the common case, not an error;
      applies uniformly everywhere `WorldConfig::load_from_file()` is
      called (`MeshWorldExport`, `MeshWorld`, `MeshWorldMap`, tests, the
      app), no per-tool wiring needed. Deliberate choice: a malformed
      override file fails the whole load (same as a malformed base file)
      rather than silently ignoring it — the caller asked for the
      override to apply. `*.local.*` was already gitignored (`.gitignore`
      line 27), no gitignore change needed. 4 new tests (temp-directory
      fixtures, never touching the repo's own `examples/` dir, to avoid
      any risk of a stray `examples/world.local.json` silently affecting
      every other test that loads `examples/world.json`): override wins,
      no-override behaves like a plain load, malformed override fails the
      load, array replacement (not append) semantics. Verified end-to-end
      too: real `MeshWorldExport` run against a scratch dir with a
      `world.local.json` overriding `seed`, confirmed the override seed
      appeared in the tool's own printed config. 1493/1493 tests pass
      (1489+4); `apps/mesh-world-app` verified building cleanly.)
- [x] T243 Progress callback in ChunkPipeline (for CLI progress bar)
      (2026-07-12 — new `ChunkPipeline::ProgressCallback` /
      `set_progress_callback()`, invoked once per `get()` call (cache hit
      or real generation) at all 3 return points with the coord and
      `ChunkDiagnostics::Source`. Deliberately reports only the per-call
      event, not "N / total" — different callers have different notions
      of "total" (`export_chunks.cpp`'s fixed grid vs. `WorldStreamer`'s
      on-demand per-connection requests vs. a future GUI), so
      `ChunkPipeline` stays agnostic and the caller derives whatever
      progress metric it needs. Not thread-safe on its own — documented
      in the header that a future multi-threaded caller (T236) must
      synchronize inside the callback body itself. Wired into
      `export_chunks.cpp`: the previous manual `done % 50 == 0` counter
      inline in the loop body is now inside the callback instead —
      verified the printed progress line is byte-identical on a real run
      against `examples/world.json`. 2 new tests: fires once per `get()`
      with the right coord/source (Lua then Cache on a repeat call for
      the same coord), and no-callback-set is safe (default, unchanged
      behavior). 1495/1495 tests pass (1493+2); `apps/mesh-world-app`
      verified building cleanly.)
- [x] T244 MeshWorldMap --region flag (show RegionType instead of ZoneType)
      (`src/tools/print_map.cpp:10,134-135`, `-r`/`--region` switches to
      `region_char()`)
- [x] T245 MeshWorldMap --chunk X,Y flag (highlight a single chunk)
      (2026-07-12 — `src/tools/print_map.cpp`: `--chunk X,Y` wraps that
      cell in `[brackets]` in the ASCII grid (legacy flat-`WorldMap` mode
      only, `-r`/`--region` combines fine) and prints a
      `Chunk (x,y): zone=... region=... exits=[...]` line below the map —
      a small value-add beyond the literal "highlight" ask, since the map
      alone doesn't show `EdgeExits`, and this is a debug/inspection tool
      where that's directly useful. Malformed `X,Y` (e.g. non-numeric)
      exits 1 with a clear error; an out-of-range coordinate prints a
      warning and renders the map with nothing highlighted rather than
      crashing; `--chunk` combined with `--planet` prints a warning and is
      ignored (the planet-mode branch renders a different map entirely,
      via `PlanetMapLogic`, not `WorldMap`). Verified via direct CLI runs
      against `examples/world.json`: default, `--chunk 3,2`,
      `-r --chunk 0,0`, an out-of-range coord, and a malformed arg.
      1489/1489 tests pass (unchanged — no unit-test precedent for this
      binary, same as T237/T238/T240); `apps/mesh-world-app` verified
      building cleanly.)

---

## Backlog — Optional AI generation (BYOK, disabled by default)

**Triaged 2026-07-11.** None of T246-T255 exist at all — zero files/symbols
for `PromptBuilder`/`AIProvider`/`AnthropicAdapter`/`OpenAIAdapter`/
`ai_requested`, no `config/` directory. Only trace: `.gitignore:29-30`
reserves `ai-keys.*`/`ai-config.*` patterns (different names than this
section's `config/ai.local.json`), unused by any code. Since none of this is
implemented, there is currently no actual conflict with the project's
offline-first rule to resolve — that question only becomes real if/when this
section is picked up. Whether to build this at all (BYOK-optional AI
generation vs. staying strictly offline-first) is a scope decision for the
user, not something to self-select.

- [ ] T246 include/PromptBuilder.hpp — builds generation prompt from ChunkContext (genuinely open)
- [ ] T247 src/PromptBuilder.cpp — implementation (genuinely open)
- [ ] T248 include/AIProvider.hpp — abstract interface: generate(prompt) → string (genuinely open)
- [ ] T249 src/AnthropicAdapter.cpp — calls Claude API (reads config/ai.local.json, gitignored) (genuinely open)
- [ ] T250 src/OpenAIAdapter.cpp — calls OpenAI API (reads config/ai.local.json) (genuinely open)
- [ ] T251 AI retry logic + procedural fallback (3 retries, then C++ generator) (genuinely open)
- [ ] T252 MC3Validator integration: AI output validated before use (genuinely open)
- [ ] T253 config/ai.local.json schema doc (file itself gitignored, never committed) (genuinely open)
- [ ] T254 ChunkContext: ai_requested flag — only then call AIProvider (genuinely open)
- [ ] T255 Test: AI disabled by default (no config) → C++ generator runs, no crash (genuinely open)

---

## Procedural model-generator ecosystem (G-series)

**PAUSED 2026-07-13, at explicit user instruction.** The user pointed out
a real conflict: this series adds MORE Lua generator content (G12/G13/G14),
which works against the already-agreed `mesh_world_revival.md`/R100
direction (Lua demoted to migration/compatibility layer, a C++ world
composer becomes the primary runtime path — see R100/R113 above).
Everything already built (G1-G14's two subsets) stays — nothing is being
removed or reverted, it remains real, tested, useful compatibility-layer
content per R100's own wording. Autonomous work is redirected to R100/R113
(`docs/world-composer-design.md`) until the user says otherwise; do not
resume self-selecting new G-series subsets without asking first.

Started 2026-07-11, at explicit user direction: a long-term (the user's own
framing: "over the coming years") initiative to evolve MeshWorld from simple
primitive chunk generation into a composable procedural model-generation
ecosystem — full writeup in `docs/procedural-model-generator-roadmap.md`
(read that first; this section is the task-tracking counterpart).

Before any implementation, the user was asked a compact set of clarifying
questions (session focus, C++ API changes allowed, generator philosophy,
taxonomy migration approach, first content-batch priority, visual style,
determinism, LOD) — all answered with the recommended option in each case.
Work then proceeded autonomously per standing instruction, without pausing
for further check-ins except where a decision was already deferred here.

**G1 — Infra: `scene:callGenerator` composition.** [x] Done 2026-07-11.
Implemented `scene:callGenerator(generator_id, sub_ctx, placement)` — long
documented (`docs/lua-generators.md`/`docs/generator-composition.md`/
`docs/taxonomy-and-containment.md`/`docs/forest-generation.md`) but never
actually built. `Mc3SceneBuilder::pushTransform()`/`popTransform()` (a real
transform/id-prefix stack) + `LuaRuntime.cpp`'s `callGenerator` binding
(resolves via the global `LuaGeneratorRegistry::instance()` singleton, runs
the sub-generator as a nested chunk in the SAME sandboxed `sol::state`).
Auto id-prefix + counter guarantees no collision even with no explicit
`placement.id`. 11 new tests (`tests/LuaCompositionTests.cpp`).

**G2 — Infra: `ctx.lod`/`ctx.exits`/`ctx.random`/`ctx.randomInt` for
object-mode.** [x] Done 2026-07-11. All 4 were documented
(`docs/lua-generators.md`) but never backed for object/chunk-mode Lua
generators (`ctx.lod`/`ctx.exits` had no C++ field/copy at all;
`ctx.random`/`ctx.randomInt` existed only for map-mode). `ChunkContext::lod`
is a new field (`include/ChunkGenerator.hpp`, default 2). Tests included in
G1's count above.

**G3 — Infra: `ctx.containment.childrenOf()`.** [x] Done 2026-07-11. A real,
already-populated `ContainmentRuleRegistry` + real
`data/taxonomy/containment.json` data existed for a while
(`ContentPackLoader::load_from_disk()` already loaded both at startup) but
had ZERO Lua binding despite being documented
(`docs/taxonomy-and-containment.md`) — a real, previously-undiagnosed gap.
Now real (object/chunk-mode only). No existing generator has been migrated
to DRIVE its own composition from it yet (see G9 below). 2 new tests.

**G4 — Infra: `scene:addSphere`/`scene:addCone` Lua bindings.** [x] Done
2026-07-11. Both existed in `Mc3SceneBuilder`/`Mc3DocumentBuilder` C++ for a
while, simply never bound to Lua. 1 new test.

**G5 — MC3Validator: real structural validation.** [x] Done 2026-07-11. Two
real, significant, previously-undiagnosed bugs found and fixed: (1) the
validator only scanned `<mc3>`'s DIRECT children for geometry, but every
real generator's output nests geometry inside an `<objects>` wrapper
(MeshCraft's own `Mc3XmlWriter`) — so `check_object()` was NEVER actually
invoked on real output, only on unrealistic hand-written test XML that
(coincidentally) matched the validator's own wrong assumption; (2) even a
reached box was checked against separate `x`/`z` attributes that don't
exist in real output — real position is one combined `position="x y z"`
attribute, omitted entirely at the origin. `MC3ValidatorTests.
ParkGeneratorPassesValidation` "passed" only because there was nothing left
to check. Both fixed (real recursive tree walk; real position parsing);
duplicate-id detection and malformed-numeric detection added as genuinely
new checks, scoped correctly to not false-positive across independent
`<definitions>` templates. 8 new/rewritten tests
(`tests/MC3ValidatorTests.cpp`), including one that corrupts REAL
`ParkGenerator` output to prove the fix reaches inside `<objects>`.

**G6 — 5 architecture-tier generators.** [x] Done 2026-07-11.
`generators/lua/architecture/{window/double_pane,door/front_panel,
roof/gable,chimney/brick,stairs/front_steps}.lua` — composable building
PARTS, distinct from freestanding objects, central-European-small-town
style. `roof/gable.lua` documents a real infra limitation it works around
(no rx/rz rotation binding exists — see G-backlog below) with a stepped-box
approximation instead of a literally sloped roof face. 9 new tests.

**G7 — Composed house, proving composition end-to-end.** [x] Done
2026-07-11. `lua.building.house.detached`
(`generators/lua/building/house/detached.lua`) — floor + 4 walls of its own,
then 7 `scene:callGenerator` calls (door, 2 front windows, 1 rotated side
window, roof, chimney, front steps, and `lua.object.mailbox.simple` — a
PRE-EXISTING generator this feature didn't create, proving composition
reaches the whole library). `simple_house.lua` (pre-existing, inline,
unrelated id) is untouched and still works standalone. 1 test covering all
7 sub-calls landing correctly.

**G8 (= T216-T218) — Interior room generators.** [x] Done 2026-07-11. See
T216-T218 above (Backlog — More Lua building/room/zone generators) for the
per-task detail; 3 new small objects (`sink.lua`/`wardrobe.lua`/
`nightstand.lua`) plus 3 composed room generators. Explicitly content-only —
no interior-room PLACEMENT pipeline exists to wire these into a real
building yet (G10 below).

**G9 — Docs corrected to match reality.** [x] Done 2026-07-11. New
`docs/procedural-model-generator-roadmap.md` (the long-term vision + a full
inventory of this session's work + the "known infra gaps" list G11-G14
below are drawn from). `docs/lua-generators.md`/`docs/generator-composition.md`/
`docs/object-generators.md`/`docs/mc3-generation-metadata.md`/
`docs/taxonomy-and-containment.md`/`docs/ai-generation.md` all corrected —
`generator-composition.md` and `object-generators.md` in particular had
described a C++ `IObjectGenerator`/`ObjectContext`/`ObjectGeneratorRegistry`
system that was NEVER built at all (confirmed by direct search).

**G10 — Interior-room placement pipeline.** `[ ]` Scoped 2026-07-11 (user
explicitly picked "scope it now" when asked what was needed/blocking), NOT
implemented — full design in `docs/procedural-model-generator-roadmap.md`'s
own "G10 scoping" section. Summary: MC3 has no CSG, so a real walkable
doorway needs a genuinely new segmented-wall technique (left pier + lintel
+ right pier leaving a real gap — distinct from `CaveGenerator`'s existing
all-or-nothing per-side wall omission, which is too coarse for a house
door). Proposed incremental path: **v1** one single room filling a whole
house interior + a real front-door gap (proves the walkable-doorway
mechanic in isolation); **v2** simple 2-4-room subdivision via one
axis-aligned split, fixed room-type priority, no interior partition walls
yet; **v3** real `ctx.containment`-rule-driven room type/count selection —
`data/taxonomy/containment.json` already has real `building.house` →
`room.*` → `object.*` rules sitting unused for exactly this, found while
scoping (same "built on one side, unreached from the other" shape as
G1/G3/G11's own findings). Next session can implement v1 directly from this
scoping without further design work.

**G11 — Wire `StyleRegistry` at startup + resolve `ctx.style` for Lua.**
`[x]` Done 2026-07-11 (user picked this as the next task when asked what was
needed/blocking). `register_builtin_styles()` added next to
`register_builtin_materials()` at the 2 real chunk-generating CLI binaries
that had NEITHER (`src/main.cpp`, `src/tools/export_chunks.cpp` — both use
`ChunkPipeline` for real generation) and the 2 test files that directly
exercise the 3 style-consuming C++ generators
(`tests/GeneratorTests.cpp`'s two fixtures, `tests/DemoWorldTests.cpp`'s two
`ChunkPipeline`-driving tests) — deliberately NOT added to
`print_materials.cpp`/`print_map.cpp`/`planet_map.cpp`/`pack_content.cpp`
after confirming none of them actually consume `StyleRegistry` or invoke
chunk generation (`print_map`/`planet_map` only drive `Map::MapPipeline`,
never `ChunkPipeline`) — would have been dead code. `ctx.style` in
`build_ctx_table()` now resolves through `StyleRegistry::instance()` into a
real `{id, name, palette}` Lua table when the style id is registered,
falling back to the bare string when it isn't (unchanged pre-fix behavior
for that case). **Real nuance found while implementing, not assumed**: this
does NOT retroactively make existing generators' `ctx.style.wood_material`-
style dead code start working — the REAL `Style::palette`
(`src/styles/*.cpp`) uses dotted namespaced keys ("park.lamp",
"block.facade.0"), not simple names like "wood_material", so that specific
pattern still returns `nil` (now because no such literal key exists, rather
than `ctx.style` not being a table at all) — same safe fallback-to-hardcoded-
default behavior as before, not a regression. New generators should use
`ctx.style.palette["some.dotted.key"] or "default"`. Also found (separate,
smaller, genuinely out of scope for this task): `SqliteContentPack` has no
concept of packing `Style`/palette data at all — `MeshWorldPack` packs
materials/Lua-generators/taxonomy/containment but never styles, so styles
can never make it into a packed `meshworld_content.sqlite` — noted, not
fixed. 2 new tests (proving both the resolved-table and unresolved-string
paths). Full suite verified (1442/1442, same 1 pre-existing flaky test),
`apps/mesh-world-app` verified building.

**G12 — Add rx/rz rotation to `Mc3SceneBuilder`/`Mc3DocumentBuilder`.**
`[x]` Done 2026-07-13. MeshCraft's own `Mc3Transform.rotation` was already
a full 3-component vector — this was purely a MeshWorld-side binding gap.
Added `rx`/`rz` (tilt/roll, degrees) alongside the existing `ry` to
`Mc3SceneBuilder`/`Mc3DocumentBuilder`/`MC3Writer`'s `addPlane`/`addBox`/
`addCylinder`/`addInstance` (and C++-facing `plane`/`box`/`cylinder`/
`instance`) — `addCylinder` previously had no rotation parameter at all,
now has all 3. Bound through to Lua as `rx`/`rz` table fields in
`LuaRuntime.cpp`'s `scene:addBox`/`addPlane`/`addCylinder`/`addInstance`.
**Scope decision**: `rx`/`rz` are leaf-level only, NOT composed through
`pushTransform()`/`popTransform()`'s cumulative frame stack (`Frame` only
ever tracked a single `ry_deg`, since ry was the only axis ever used) —
`transform_rx()`/`transform_rz()` are deliberate identity pass-throughs,
documented in `Mc3SceneBuilder.hpp`. Correct for today's actual use (a
leaf primitive tilted directly — a roof panel, a wheel), not a general
per-axis 3D rotation composition system; a generator that both pushes a
rotated frame AND sets rx/rz on a primitive inside it should not assume
the two compose correctly.

**Also fixed the real, already-documented bug this unblocked**:
`building/simple_house.lua`'s gable-roof code computed a real slope
`angle` (and the correct hypotenuse `slope_w`) but applied the angle as
`ry` (yaw), which only spins a box flat in the horizontal plane — so
`roof_l`/`roof_r` rendered as flat horizontal planks rotated in-plane,
not sloped roof faces. Now uses `rz` (the ridge runs along Z, so pitching
the panel from flat to sloped is a rotation around Z). Comment updated
in-place with the fix; `architecture/roof/gable.lua`'s own stepped-box
silhouette is left as a deliberate style choice (its own comment updated
to say so, not rewritten — a separate future content task, not attempted
here since G12's actual ask is the binding, not a content redesign).

6 new tests (`tests/RotationBindingTests.cpp`): `addBox`/`addPlane`/
`addCylinder`/`addInstance` each accept and correctly emit `rx`/`ry`/`rz`
in the generated `<box rotation="...">`-style XML attribute; a box with
no rotation at all still omits the attribute entirely (Mc3XmlWriter's own
`nonzero3()` gate, not a regression for the common case); and — running
the REAL `lua.building.simple_house.standard` generator through
`LuaGeneratorRegistry`+`LuaSandbox` exactly as production does, not just
an isolated inline generator — `roof_l`/`roof_r` now carry a real,
non-zero `rz` and `ry == 0` (proving the bug fix landed on the actual
file), with `roof_l`'s slope the exact negation of `roof_r`'s (a
symmetric ridge). 1503/1503 tests pass (1497+6); real full-demo-world
`--validate` export still 0 errors/0 warnings across all 400 chunks;
`apps/mesh-world-app` verified building cleanly.

**G13 — Migrate at least one existing generator to consume
`ctx.containment.childrenOf()`.** `[x]` Done 2026-07-13. `zone/park.lua`
migrated — real containment data for `zone.park` (5 rules: tree/bench/
lamp_post/flower_bed/fountain, each with its own probability/count-range/
lod_max) now genuinely drives how many of each get placed each generation,
not the old hardcoded exactly-16-trees/4-benches/6-lamps/4-flower-beds/
1-fountain layout. **Found while implementing (checked, not assumed)**:
taxonomy child ids ("object.tree") don't map 1:1 to real
`LuaGeneratorRegistry` ids ("lua.object.tree.deciduous") — no generic
resolver exists, so tree/bench/lamp_post/fountain now delegate via
`scene:callGenerator()` to their real registered generators (a quality
upgrade too, e.g. `lua.object.tree.deciduous`'s real icosphere canopy vs.
this file's old flattened-box approximation), while `object.flower_bed`
has NO matching Lua generator anywhere in `generators/lua/object/` at all
— kept as this file's own inline geometry, still driven by the same
containment rule's count/probability roll, not silently left unmigrated.
Position/orientation algorithms (quadrant tree scatter, a bench ring
around the plaza, lamps along the path cross, flower beds at plaza
corners) are simple count-parameterized generalizations of the old fixed
layout, not a from-scratch design pass, and not visually verified (no GPU
here, same limitation as every other Lua content change this session).

**Found and fixed a real cross-fixture gap this surfaced**: `scene:
callGenerator()` always resolves sub-generators against the GLOBAL
`LuaGeneratorRegistry::instance()` singleton (an already-documented
constraint in `LuaRuntime.cpp`'s own doc comment) — 3 existing tests
(`tests/LuaGeneratorTests.cpp`'s `LuaGenFixture`,
`tests/LuaGeneratorRegistryTests.cpp`'s `ParkGeneratorProducesExpected
Geometry`) used their own LOCAL, non-singleton registries and never
populated `ContainmentRuleRegistry` either, so park.lua silently produced
almost nothing (just ground/plaza/paths) under those fixtures. Fixed by
adding the same global-singleton-population pattern
`LuaCompositionFixture` already established. Also updated one
now-legitimately-stale assertion (`ParkGeneratorProducesExpectedGeometry`
required "fountain" unconditionally present — now genuinely conditional,
probability=0.5/min_count=0 — loosened to "lamp or fountain", matching
`LuaGenFixture.ParkContainsBenchTreeLamp`'s own already-correct pattern).

3 new tests (`tests/LuaCompositionTests.cpp`) prove the migration is real,
not vacuous: tree count actually varies across 40 seeds within the
containment rule's [8,40] range (not the old fixed 16); the fountain is
statistically confirmed present in some seeds and absent in others across
60 trials (not the old always-exactly-one); the flower-bed inline
fallback still fires. 1506/1506 tests pass (1503+3, after fixing the 3
tests above). A real full-demo-world `--validate` export stayed 0
errors/0 warnings across all 400 chunks; `apps/mesh-world-app` verified
building cleanly. One transient, already-diagnosed test-execution-order
flake (`CtxRandomIsDeterministicForSameSeed`, unrelated to this change —
see project-direction.md memory) appeared once in a full-suite run and
did not reproduce across 2 immediate re-runs.

**G14 — Broader taxonomy migration / more architecture-tier generators.**
`[~]` Started 2026-07-13 — open-ended by design (this is explicitly the
"years, thousands of generators" part of the vision, not a single bounded
task), so intentionally NOT closing this checkbox after one coherent
subset; picking up again in a later session is expected, not a gap.

**This session's subset: `architecture/fence/wood_picket.lua` +
`architecture/gate/simple.lua`** — a coherent, visually-matched pair (the
gate reuses the fence's own post/picket proportions and material
defaults). Fence: 2 end posts + top/bottom rails + N evenly-spaced
pickets, count derived from `length`/`picket_width`/`picket_gap`
parameters (not hardcoded) — verified to actually produce >1 picket at
the default 2m length, not a vacuous single-iteration loop. Gate: 2
posts + a closed panel (frame + 4 pickets) + hinges + a latch, sized for
a walkway opening. Both follow the exact same conventions every other
architecture-tier file here already established (M.id/version/category,
`ctx.parameters` overrides, local-origin coordinate convention for
`scene:callGenerator()` placement, `scene:setMetadata()`). Deliberately
scoped down: neither renders an "open" swung-out gate state or a
pointed-top picket silhouette -- both would need a per-picket/per-panel
rx/rz rotation composed through a nested transform, which G12's own rx/rz
addition does NOT guarantee correctly yet (leaf-level only, not composed
through `pushTransform`/`popTransform` -- see G12's own entry); documented
in both files rather than silently attempted and left subtly wrong.

**Real production reachability note, same as `building/simple_house.lua`/
`building/house/detached.lua` before them**: neither file is wired into
any real chunk-dispatch path yet (no `zone/small_house_block.lua` exists
at all -- `RegionType::small_house_block` chunks are handled entirely by
the C++ `SmallHouseBlockGenerator`). Whether/how to wire Lua-authored
architecture-tier content into real production dispatch (replacing or
complementing existing working C++ generators) is a separate, bigger
architectural question deliberately NOT decided here as a side effect of
adding 2 small files -- these exist as tested, reusable composition
building blocks for future work, matching the established precedent.

5 new tests (`tests/ArchitectureAndRoomObjectTests.cpp`, same
`ArchRoomFixture` pattern as every other architecture-tier generator):
structural presence checks for both files, plus a real proof the picket
count loop actually iterates more than once at default parameters (not
just "doesn't crash"). 1509/1509 tests pass (1506 after G13 + 3 here); a
real full-demo-world `--validate` export stayed 0 errors/0 warnings
across all 400 chunks (these 2 files aren't yet reachable from that path,
so this only proves nothing else broke); `apps/mesh-world-app` verified
building cleanly.

**Second subset, same day: `building/garage.lua` (`lua.building.garage.
detached`).** A detached single-car garage — floor slab + 3 solid walls +
a front wall built as two pillars + a lintel around a large roller-door
opening (no CSG wall-cutting in MC3, same pillar/lintel technique
`simple_house.lua`'s own door/window cut-outs already use, generalized to
a much wider opening) + a flat roof. The roller door itself is inline
geometry (a flat panel + horizontal groove-line accents — no matching
generator exists for it), but the optional side pedestrian door
(`parameters.side_door`, default true) is genuinely COMPOSED via
`scene:callGenerator("lua.architecture.door.front_panel", ...)` rather
than duplicating door geometry a third time — following
`building/house/detached.lua`'s own established composition pattern
exactly (same `base_variation`/`style`/`lod` sub-context shape, same
`rotation_y=-90` side-wall placement convention its own side window
uses). 2 new tests (`tests/LuaCompositionTests.cpp`, since this generator
uses `scene:callGenerator` and therefore needs the GLOBAL registry
singleton fixture, not `ArchitectureAndRoomObjectTests.cpp`'s local one):
shell + inline roller door + composed side door all present; `side_door=
false` genuinely omits the composed sub-generator call entirely (not
just hides it — proves the flag gates the `callGenerator` call itself).
Same production-reachability caveat as every other `building/*.lua` file
this session (`simple_house.lua`, `house/detached.lua`): not wired into
any real chunk-dispatch path yet. 1511/1511 tests pass (1509+2); a real
full-demo-world `--validate` export stayed 0 errors/0 warnings;
`apps/mesh-world-app` verified building cleanly.

**`object.sqlite3` (2026-07-11/12, gitignored, NOT part of the tracked
G-series backlog)** — a large-scale STAGING/brainstorm taxonomy database, see
`object.md` for the full design analysis. Not a replacement for
`data/taxonomy/{taxonomy,containment}.json` (the real, curated, committed
source of truth) — a much bigger, disposable upstream working set meant to
be curated down before anything gets promoted into those files. Four waves
(all Workflow-driven, Sonnet 5, medium-effort generation / high-effort
curation / low-effort mechanical writes): Wave 1 (87 domains, common
everyday objects) + Wave 2 (54 domains, deeper/additional — civic/commercial
buildings, trade-specific tools, deeper food/clothing/hobby chains) + Wave 3
(52 domains, home-entertainment/hobby rooms, niche civic/retail interiors,
more outdoor place types, deeper containment for 5 existing parents, new
equipment "concept groups") + Wave 4 (56 domains, transportation
infrastructure, industrial/factory equipment, agriculture equipment,
medical/retail depth, sports/recreation depth, architecture element variety,
office spaces), Wave 2-4 explicitly excluding any living creature/animal/
human-figure objects, matching this project's standing "no characters/NPCs"
rule. After Wave 4, a user-requested quality audit found and fixed 56 junk
self-referencing `cancontain` rows (deleted directly) and 1962 orphaned
`object`-kind rows (28% of all object nodes, zero incoming edge — root
cause: curation correctly dropping synthetic `*_group` scaffolding parents
as low-quality, which silently orphans their children). A 5th repair
`Workflow` reattached 1958/1962 of them to one of 187 real existing
container parents via a semantic matching pass (only 4 genuinely
unmatched, left unattached rather than forced). **Combined final state:
7108 total object rows / 7337 cancontain rows**, zero orphan references,
zero duplicate ids/edges, zero self-loops, only 4 unattached object-kind
rows remaining (down from 1962) — a 111x increase in `object`-kind nodes
over the 64-node starting seed. See `object.md` §7/§7b/§7c/§7d/§7e for the
full writeup including a real, recurring infra hiccup in Waves 1-2 (a
transient safety-classifier false-positive on ordinary SQLite INSERT
calls, resolved each time by resuming the same `Workflow` run — completed
agents replay from cache, only the blocked ones re-run; Waves 3-4 and the
repair pass all hit zero classifier errors). Session paused here at the
user's explicit request after the repair pass.

---

## Revival architecture tasks (R-series)

Added 2026-07-12 from `mesh_world_revival.md` (new strategic design document,
user-added). These are new, not-yet-started architecture tasks that
implement the revival document's direction: MC3 assets/libraries/imports
replace ad-hoc Lua-generated geometry as the primary runtime content path.
See `mesh_world_revival.md` §6-8 ("Definitions, Instances, Variants" /
"Global Libraries and Reuse" / "Library Imports and Dependency Resolution" /
"Proposed File Types") for the full design this section implements. This is
a large, multi-session initiative — expect it to interleave with the
existing G-series rather than fully block it.

- [ ] R100 3D models are no longer generated via Lua generators — generate
      `mc3.xml` (and eventually `mc3.json`) directly instead. Existing Lua
      object/building/room/zone generators become migration tools /
      compatibility layer, not the primary path (per Executive Summary,
      §1: "Lua should not be deleted immediately... The long-term runtime
      path should favor a C++ world composer selecting and placing
      validated MC3 assets and modules"). **2026-07-13 — user flagged a
      real conflict**: this session's own G-series work (G12/G13/G14) was
      adding MORE Lua generator content, directly against this already-
      agreed direction. G-series paused at the user's explicit
      instruction; autonomous work redirected to this R100/R113 direction
      instead. See R113's own entry below and `docs/world-composer-
      design.md` for the design produced in response. **2026-07-13 —
      R113 v1 implemented** (real C++ `BuildingComposer` now produces
      `mc3.xml` directly for `small_house_block`, opt-in via
      `WorldConfig::use_world_composer`): the first concrete step of
      R100's own direction, not just a design doc. G-series remains
      paused; next steps are R113 v2/v3 or other R-series work, not new
      Lua content.
- [x] R101 New `<import>` element for `mc3.xml` (MeshCraft `Mc3Document`/
      `Mc3XmlWriter`/`Mc3XmlReader`) so a scene/model can pull in globally
      available 3D models and their variants from a shared library, per
      `mesh_world_revival.md` §7 ("Library Imports and Dependency
      Resolution"): `<imports><import namespace="..." source="mc3lib://..."
      hash="sha256:..."/></imports>`, then reference instances as
      `namespace:definition.id`. Needs namespace/version/hash resolution,
      cycle detection, and missing-dependency errors.
      **Implemented 2026-07-12 in `mesh-craft/mc3`**, same "one Mc3Document
      AST, two surfaces" pattern R109/R110/R111 all use: new `Mc3Import`
      struct (`importNamespace`/`source`/`hash`) + `std::vector<Mc3Import>
      Mc3Document::imports`, round-tripped through both XML (a new
      `<imports><import .../></imports>` block) and JSON (`"imports":
      [...]`). New `Mc3ImportResolver` (`Mc3ImportResolver.hpp/.cpp`) —
      resolution scheme is a user-decided (not guessed), deliberately
      simple v1: a `mc3lib://<name>@<version>` source resolves to the
      first of `<searchDir>/<name>-<version>.mc3lib.xml` or `.mc3lib.json`
      found across a caller-supplied search-directory list (no package
      registry/index yet — revisit once R112's real content batches need
      one). `resolve(doc)` returns definitions from `doc`'s own DIRECT
      imports only, keyed `"<importNamespace>:<definitionId>"`; content
      hash (if the import declares one) is checked against the resolved
      library's own `computeLibraryContentHash()` (R110), mismatch
      throws. **Scoping decision**: if a resolved library itself has
      further imports, those are resolved recursively for cycle-detection
      and missing-dependency validation ONLY — their definitions are
      NOT merged into the top-level caller's map (a library's own
      internal `"<alias>:<id>"` references are that library's own
      concern when it is itself loaded/compiled, not flattened upward);
      standalone-compile dependency pruning remains R110's own explicitly
      deferred follow-up, not this class's job. New
      `mc3/test/import_resolver_test.cpp` (ctest `mc3_import_resolver`,
      writes real `.mc3lib.xml` fixture files to a temp dir): basic
      resolution, missing-dependency error, content-hash match/mismatch,
      a 3-level nested-import chain proving grandchild definitions stay
      unmerged, a missing NESTED dependency still being caught, and a
      genuine A-imports-B-imports-A cycle being detected with a message
      naming the cyclic chain. **All 18 `mesh-craft/mc3` ctest targets
      pass** (standalone build, 2 more than R111's 16 — this task's own
      `mc3_import_resolver` plus R111's `mc3_asset_metadata`, both landed
      since the last count). Mesh-world's own 1468/1468 tests pass with
      the updated `mc3/` linked in; `apps/mesh-world-app` build verified
      cleanly too. **Not yet wired into any MeshWorld-side consumption**
      (WorldRenderer/ObjectDefinitionLibrary resolving an actual
      `namespace:id` instance reference at runtime) — that is separate,
      later scope (R102/R112 territory), consistent with R101's own
      literal text covering only the import/resolution mechanism itself.
- [x] R102 Split composite objects into independent, separately-imported
      parts instead of embedding everything in one definition — e.g. a
      house 3D model does not contain its own door geometry; the door is a
      separate global definition (with its own variants) that the house
      imports and places via R101/R103.
      **Mechanism implemented 2026-07-12** (the STATIC-placement half via
      R101; the full R103 dynamic `<script>`-driven placement remains its
      own separate, not-yet-started task — this closes what's achievable
      without it, matching R101's own "mechanism, not full content batch"
      scoping precedent). New `Mc3ImportResolver::resolveAndMergeInto(doc)`
      (`mesh-craft/mc3`): calls `resolve(doc)` and inserts every result
      directly into `doc.definitions[...]` under its own
      `"<importNamespace>:<definitionId>"` key — makes an imported part
      resolvable through the exact SAME `doc.definitions[...]` lookup
      every existing instance consumer (`SceneRenderer`, `mc3togltf`, CSG
      evaluation — see `Mc3Object::resolvedInstanceDefinitionKey()`'s own
      doc comment) already uses, with zero consumer-side changes needed.
      New test in `mc3/test/import_resolver_test.cpp` proves the full
      worked example this task's own description names: a `house`
      document imports a `door-parts` library, places a real `<instance
      definition="door_lib:door.simple">` child object, and — after
      `resolveAndMergeInto()` — that instance's own
      `resolvedInstanceDefinitionKey()` resolves to a real entry in
      `doc.definitions`. All 18 `mesh-craft/mc3` ctest targets pass;
      mesh-world's own 1468/1468 tests pass with the updated `mc3/` linked
      in; `apps/mesh-world-app` build verified cleanly too. **Not done**:
      an actual first real content library (that's R112's own scope, a
      genuine urban asset batch) and variant-aware placement rules beyond
      a single fixed instance (that needs R103's `<script>` element for
      anything beyond a hand-authored fixed transform).
- [x] R103 New `<script>` element inside `mc3.xml` 3D models/definitions:
      embedded Lua code that runs at build/compose time and places imported
      definitions (R101/R102) inside the model at the correct sockets/
      anchors, following containment/placement rules (relates to existing
      `ctx.containment.childrenOf()` work, G3/G13, and to §6's
      sockets/clearance metadata).
      **2026-07-13 — v1 implemented** (user request: "udělej r103").
      **mesh-craft/mc3** (commit `f392d41`, pushed): new
      `Mc3Object::scriptId` string field, referencing an entry in the
      owning document's own `scripts` map (`Mc3Script`, already existed
      as inert data since before this session — round-tripped/editable
      but never executed by anything). Round-trips through XML (a new
      `script="..."` attribute, same pattern as the existing `layer`
      attribute) and JSON (`"script"` key); empty by default so every
      existing definition is unaffected. 2 new round-trip tests. All 18
      `mesh-craft/mc3` ctest targets pass.

      **MeshWorld** gets the actual execution engine and a real worked
      example — see R104's own entry below for why the engine lives in
      MeshWorld, not MeshCraft, for v1 (an explicit, honest scope
      reduction from R104's own literal wording). New
      `data/mc3lib/urban-buildings-1.0.0.mc3lib.json`: a MODULAR house
      (`house.gable.modular_01`, `src/tools/build_mc3lib_content.cpp`'s
      new `build_buildings_library()`) with the exact same wall/floor/
      gable dimensions as `ObjectDefinitionLibrary.cpp`'s own
      `make_house_gable()`, but with windows/door/roof NOT baked in --
      instead: real `assetMetadata.sockets` (`window_front_l/r`,
      `door_front`, `roof_mount`, at the same positions the inline
      elements used to occupy), a real `doc.imports` declaration for
      `urban-windows`/`urban-doors`/`urban-roofs` (R112), and an attached
      script (`scriptId`) whose Lua source calls `def:place(childId,
      definitionRef, socketName)` 4 times to place 2 windows + 1 door +
      1 roof, all sourced from R112's own real content. This is R102's
      own "split composite objects" vision made real for the first
      time — a house that imports its parts instead of baking them in —
      and the first genuine consumer of R112's window/door/roof
      categories (previously registered-but-unplaced).

      New `tests/Mc3ScriptRunnerTests.cpp` (11 tests: success/placement,
      multiple placements, unknown script id, empty script id/source as
      legitimate no-ops, unknown socket, unknown/unresolved
      definitionRef, missing assetMetadata, Lua syntax error, `has_socket`,
      sandbox blocks `io`/`os`) plus 2 `BuildingComposerTests.cpp`
      integration tests (the modular house registers as a second house
      candidate; its def really carries the placed window/door/roof
      instances as children, not just bare walls). Two PRE-EXISTING tests
      needed fixing (not new breakage, a direct consequence of adding a
      real second house candidate): `QueryFindsHouseByCategoryAndStyleTags`
      and `ComposesRealInstancesForSmallHouseBlock` both hardcoded
      "exactly 1/one specific house" assumptions — fixed the same way the
      R112 deeper-coverage pass fixed its own analogous assumptions
      (membership check instead of a pinned single id/count). **1558/1558
      tests pass** (1545 + 13 new). `apps/mesh-world-app` rebuilt clean.
      End-to-end verified via `MeshWorldExport --validate`: 0 errors/0
      warnings across 400 chunks; both house variants appear in real
      generated output (114 `house.gable.modular_01`, 111
      `house_gable_default` — genuine mixed selection, not one always
      winning).
- [ ] R104 MeshCraft engine support for `<script>`: the Lua code inside
      `<script>` must be able to manipulate the `Mc3Document`/scene being
      built (add/position/parent instances, resolve imports) — requires a
      dedicated Lua binding in MeshCraft (mirrors MeshWorld's own
      `Mc3SceneBuilder` Lua bindings, but lives in MeshCraft itself so any
      `mc3.xml` consumer gets the same scripting capability).
      **2026-07-13 — deliberately NOT done as literally written; a
      MeshWorld-side v1 built instead, honestly scoped down.** This
      task's own text asks for the Lua execution engine to live IN
      MeshCraft — a shared cross-project library also consumed by CNA/
      NOXNA/mc3togltf, which currently has ZERO Lua/sol2 dependency
      anywhere in its build (checked, not assumed: grepped `CMakeLists
      .txt` for `lua`/`sol2` before deciding). Adding one there would be
      a materially bigger, harder-to-reverse commitment (a new
      third-party dependency on a library other projects build against)
      than R103's actual immediate goal needed. Judgment call, made
      without asking first (unlike MAP19/G-series's own "ask before a
      genuine architecture fork" precedent) since the lower-risk path
      achieves the same user-visible result with a clean, reversible
      fallback if the full cross-repo version is wanted later: new
      `include/Mc3ScriptRunner.hpp`/`src/Mc3ScriptRunner.cpp`
      (MeshWorld), a small dedicated sol2 sandbox (same discipline as
      `LuaRuntime.cpp`: only base/math/string/table Lua libs, `io`/`os`/
      `debug`/`package`/`require` removed) exposing one minimal binding
      as the global `def`: `def:place(childId, definitionRef,
      socketName)` (validates the socket exists in the target's own
      `assetMetadata.sockets` and the definitionRef already exists in
      the caller-supplied, already-import-resolved `Mc3Document` --
      throws a catchable error otherwise, never silently no-ops) and
      `def:has_socket(socketName)`. `ComposerAssets.cpp`'s new
      `register_modular_buildings()` is the real caller: loads
      `urban-buildings-1.0.0.mc3lib.json` directly (not via a throwaway
      importer document like `register_mc3lib_batch()` -- the script's
      own `def:place()` calls need the imported definitions in THIS
      SAME document), merges its own imports via
      `Mc3ImportResolver::resolveAndMergeInto()`, runs the script once at
      startup (a one-time "compile" step), then registers the resulting
      expanded definition. **What's genuinely NOT done**: any other
      `mc3.xml` consumer (MeshCraftApplication, mc3togltf, a future CNA
      integration) gets NO scripting capability from this -- R104's own
      "any consumer" goal remains fully open. Revisit if/when a second
      real consumer of this mechanism exists outside MeshWorld.

      **2026-07-13 (same day) — facade_module consumption added**
      (user: "pokracuj" — continue; self-selected as the highest-value
      bounded next step: two mechanisms already built this session,
      R112's facade bay modules and R103's `Mc3ScriptRunner`, but nothing
      had ever connected them). `place()`'s own fixed-named-socket API
      can't express "tile N modules along a wall whose length varies" --
      N and every position are a function of the wall's own span, not a
      hand-authored constant list. Added `Mc3ScriptRunner`'s second
      binding, `def:place_at(childId, definitionRef, x, y, z)`: same
      validation as `place()` (definitionRef must already be resolved
      into `doc.definitions`), but at RAW coordinates instead of a named
      socket lookup -- and, deliberately, does NOT require
      `target.assetMetadata` to exist at all (unlike `place()`), since a
      script computing its own positions has no need for pre-authored
      sockets. Refactored both to share one internal `place_instance()`
      helper.

      New `house.rowhouse.modular_01` (`build_mc3lib_content.cpp`, same
      `urban-buildings-1.0.0.mc3lib.json` library as the gable house): a
      wider (15m) terraced house whose ENTIRE front wall is tiled from
      6 real `urban-facades` bay modules (2.5m each, 15m ÷ 2.5m = 6
      exactly, no remainder handling needed for this first pass) via a
      Lua `for` loop calling `def:place_at()` -- computing each bay's
      `x` position from `bay_w`/`count`/loop index entirely in the
      script, not from any fixed socket list. Bay 0 is a door
      (`facade.residential_bay_door_01`), the rest alternate the two
      window bay variants. Side/back walls and a flat roof cap are still
      built directly (no roof module at this width exists to import;
      keeps this pass scoped to facade tiling specifically). **First
      real consumer of R112's `facade_module` category** (previously
      registered-but-unplaced, same as window/door/roof were before
      R103 v1).

      **Deliberately registered into `ObjectDefinitionLibrary` only, NOT
      `AssetRegistry`'s `"house"` category** (`ComposerAssets.cpp`'s
      `register_modular_buildings()`, refactored into a shared
      `compile_and_register_modular_building(doc, id,
      register_in_asset_registry)` helper): this building's real 15m
      width does not match `BuildingComposer`'s fixed 10m parcel grid
      (`Parcel.cpp`'s own `kParcelWidth`) -- if picked by the existing
      `query("house", ...)`, it would visually overlap a neighboring
      parcel's building. A real size-aware parcel/asset match (already
      flagged as later scope in `Parcel.cpp`'s own comments) is a
      prerequisite for actually placing a wider building; until then this
      stays a real, fully resolvable, fully tested definition with no
      automatic placement consumer -- an honest, explicit scope limit,
      not silently deferred.

      6 new tests in `tests/Mc3ScriptRunnerTests.cpp` (`place_at` basic
      placement, working with no `assetMetadata` at all, a real tiling
      loop computing 4 positions matching hand-worked-out math exactly,
      unknown-definitionRef error) plus 2 in
      `tests/BuildingComposerTests.cpp` (the rowhouse resolves into
      `ObjectDefinitionLibrary` but is absent from every `"house"`
      query result; its 6 children are the exact expected tiled
      definitions at the exact expected `x` positions, door first, then
      alternating windows, all at the same `z`/`y`). **1577/1577 tests
      pass** (1571 + 6 net new). `apps/mesh-world-app` rebuilt clean.
      End-to-end verified via `MeshWorldExport --validate`: 0 errors/0
      warnings across all 400 chunks; confirmed the rowhouse correctly
      never appears in any composed chunk (0 occurrences), exactly as
      designed (no placement consumer yet).
- [x] R105 Convert map generators from Lua to C++ (`generators/lua/map/*.lua`
      → native C++ generators under `src/generators/`), matching the
      migration direction already used for zone/chunk generators (see
      T219-T225, T226-T235) and the revival document's overall "C++ world
      composer" direction (§1).
      **Audited 2026-07-12 (re-scoped, not closed) — the premise was mostly
      wrong.** Before writing any code, all 17 `generators/lua/map/*.lua`
      files were read against `src/Map/ChildGenerator.cpp`/
      `src/generators/map/PlanetGenerator.cpp` (a dedicated audit fork, not
      assumed from analogy). `ChildGenerator.cpp` already runs a
      comprehensive, unit-tested (`ChildGeneratorSettlementsTests.cpp`
      etc.) terrain pipeline at every non-zero level — `MountainRanges::
      generate()`/`apply()`, `Hydrology::trace()`/`carve()`,
      `Coastline::trace()`, multiple `BiomeRefinement::apply*()` passes,
      `Settlements::place()`, `Roads::build()`, `Countries::grow()`/
      `name()` (capitals gated to `kCountryRegionLevel=4`) — genuinely
      superior in several respects (real MST/slope-aware road routing,
      real ridge-polyline mountains) to what the Lua scripts do. **13 of
      17 are confirmed REDUNDANT**: `planet.lua` (own header: "structural
      parity" port of `PlanetGenerator.cpp`), the 7 generic `level*.lua`
      files 1/2/8/10/13/16/17 (each admits in its OWN header "ChildGenerator.
      cpp already does this" — they exist only to preserve content if that
      level ever moves onto the Lua-first path, not because C++ lacks
      anything), `continent.lua` (calls the SAME MountainRanges/Hydrology
      C++ functions via Lua bindings — same algorithm, different call
      path), `trunk_network.lua`/`region.lua`/`metro.lua` (each a strictly
      simpler Lua reimplementation of `Settlements::place()`/
      `Roads::build()`, which the C++ path already does more richly).
      Converting any of these to C++ would be pure duplicate work — no
      value, no gap to close. The genuine, verified gap is narrow: **`Map::
      ZoneCandidate` block/parcel zoning (house_block/apartment_block/
      shop_street/park/square) and terrain-contour-following street-grid
      generation exist ONLY in `city.lua` (level 12, 484 lines, by far the
      largest/most valuable script) and `level14.lua`'s zoning half (level
      11) — zero C++ equivalent anywhere, confirmed by grep. If either
      level ever fell back to C++ (a Lua error, or a future Stage-5
      "demote Lua" step, RR4), all urban zoning/parceling would silently
      disappear.** `district.lua` (level 11's border half) has a smaller,
      secondary gap: its per-quadrant border-drawing has no C++ equivalent
      either, since `Countries::grow()` only produces real borders at
      level 4, not level 11. **R105 is re-scoped to exactly these 2-3
      items** (porting `city.lua`'s `ZoneCandidate` generation +
      street-grid to a new C++ generator, and `district.lua`'s per-quadrant
      borders) rather than the original blanket "convert all 17" reading;
      the other 13 scripts are tracked as Stage-5/R121 "demote, don't
      port" candidates instead (matching RR4's own "premature deletion of
      Lua" caution — demote gradually, don't delete before an equivalent
      exists, but there is no C++ conversion work left to do for them).
      **city.lua's half implemented (2026-07-12), district.lua's half
      still open.** New `Map::CityGenerator`
      (`include/generators/map/CityGenerator.hpp`,
      `src/generators/map/CityGenerator.cpp`) is a direct native port of
      `city.lua`'s algorithm — same elevation/temperature/moisture
      computation `ChildGenerator.cpp` already uses (kept consistent, not
      reinvented), built through `MapBuilder` exactly the way
      `LuaSandbox::executeMap()` drives the Lua path (`setBiomeField()`,
      the 4 `BiomeRefinement` passes, `markUrbanCells()`, `addPark()`,
      `addStreet()`, `setZoneCandidates()`, `addLake()`, `setMetadata()`),
      including a faithful C++ port of `align_grid_line()` (the greedy
      terrain-contour street-bending walk, with the same cross-tile-seam
      pinning fix) and the same weighted zone-candidate roll thresholds.
      Deterministic "randomness" uses `noise::hash2i(index, axis, entropy)`
      (the existing `Settlements.cpp`/`MountainRanges.cpp` convention, a
      distinct axis per kind of roll — park count, park site attempts,
      zone-candidate roll — not a stateful RNG stream) rather than
      literally reproducing Lua's own `ctx.randomInt()` sequence, since
      determinism only needs to hold WITHIN the C++ path (same tile/
      parent/entropy → same output), not bit-for-bit against Lua's
      independent implementation. **Wired into `MapPipeline.cpp`**: a new
      `kCityLevel = 12` constant and a 3-way C++-fallback branch (level 0
      → `PlanetGenerator`, level 12 → `CityGenerator`, everything else →
      the generic `ChildGenerator`) — the per-level dispatch mechanism
      R105's own audit said was needed, mirroring
      `ChunkPipeline::get_generator(zone, region)`'s existing per-zone/
      region dispatch for the chunk layer, just simpler (one special-cased
      level, not a full lookup table, since there is currently only one
      level that needs it). New `tests/CityGeneratorTests.cpp` (16 tests)
      mirrors `CityLuaTests.cpp`'s own fixture/assertion style (moderate
      land / all-ocean / near-sea-level / diagonal-gradient parents) —
      field shapes, urban-cell marking (present over land, absent over
      ocean), zone-candidate grid presence/park-candidate/variety, street
      grid (count, waypoint count, tile-boundary snapping, terrain-
      following bend, cross-sibling-tile seam matching), park placement,
      lake placement, determinism, and culture inheritance — proving the
      C++ path produces the same KIND of output as the Lua path, not
      necessarily byte-identical (independent implementations). Plus one
      new `MapPipelineTest.Level12UsesCppCityGeneratorWithNoLuaRegistered`
      proving the new dispatch actually engages. **district.lua's half
      also implemented (2026-07-12, same session, closing R105):** new
      `Map::DistrictGenerator` (`include/generators/map/
      DistrictGenerator.hpp`, `src/generators/map/DistrictGenerator.cpp`)
      — same elevation/temperature/moisture computation as CityGenerator/
      ChildGenerator, splits the tile into 4 axis-aligned quadrants (NW/
      NE/SW/SE), each with a suitable site becoming a named district
      (`addBorder()` covering exactly that quadrant) + one town
      (`addCity(..., "town")`), matching `district.lua`'s own "quadrant
      split, not a real geometric partition" v1 design. Wired into
      `MapPipeline.cpp` via a new `kDistrictLevel = 11` branch, alongside
      `kCityLevel`. New `tests/DistrictGeneratorTests.cpp` (5 tests,
      mirrors `DistrictLuaTests.cpp`'s own style): field shapes, all 4
      quadrants getting a district+town over solid land, district borders
      being quarter-sized (not whole-tile), never placing over ocean,
      determinism. Plus `MapPipelineTest.
      Level11UsesCppDistrictGeneratorWithNoLuaRegistered`. **1468/1468
      tests pass** (6 more than the city.lua-only 1462; one pre-existing,
      unrelated flaky test — `WorldRendererTests.
      WithMapLayerLoadsChunksWithoutCrashing` — failed once in a
      full-suite run and passed both in isolation and on a full-suite
      rerun immediately after; this is the same test-execution-order-
      dependent `MaterialRegistry` noise R106's own investigation already
      diagnosed as non-blocking, not a new regression, not chased further
      per this project's own "don't chase flaky tests without a fresh
      concrete repro" rule); `apps/mesh-world-app` build verified cleanly
      too. **R105 is now fully closed** — both halves of the audited gap
      (city.lua's zoning/streets, district.lua's per-quadrant borders) are
      implemented; the other 13 of the original 17 map-level Lua scripts
      remain correctly redundant with `ChildGenerator`/`PlanetGenerator`
      per the audit, tracked as Stage-5/R121 "demote, don't port"
      candidates, not further R105 work.
- [x] R106 Global material catalogue: a single place (data file + registry,
      analogous to `data/taxonomy/*.json` + `ObjectDefinitionLibrary`) that
      lists valid material ids (e.g. `wood`, `road`) usable from `mc3.xml`
      by id/reference instead of ad-hoc inline material definitions per
      file. `mc3.xml` definitions/instances select a material by id from
      this global catalogue; unknown ids should be a validation error
      (extend `MC3Validator`, see G5).
      **Investigated 2026-07-12 (not closed — see below):** most of this
      already exists and was never disconnected the way this backlog entry
      implied — `MaterialRegistry`/`MaterialEntry`
      (`include/MaterialRegistry.hpp`/`MaterialEntry.hpp`,
      `src/materials/BuiltinMaterials.cpp`) IS the single global catalogue
      (id + color/roughness/metallic/texture/license), and `MC3Validator`
      (`src/MC3Validator.cpp`) already looks up every `material="..."`
      attribute against it. **One real, narrow gap found and fixed**:
      `generators/lua/zone/park.lua`'s `FLOWER_COLORS` table references
      `flowers_red/yellow/white/purple` (plural), a different naming
      convention than `BuiltinMaterials.cpp`'s pre-existing
      `flower_red/yellow/...` (singular) set — never registered, a real
      content gap, found via the full test suite's own "not registered in
      MaterialRegistry" warning output rather than assumed; all 4 now
      registered. **NOT changed, needs a decision, not made unilaterally**:
      this task's own literal ask ("unknown ids should be a validation
      error") directly conflicts with an existing, deliberate, already-
      tested design decision from the G5 `MC3Validator` rewrite —
      `tests/MC3ValidatorTests.cpp`'s `UnregisteredMaterialIsWarningNotError`
      (name approximate) explicitly asserts `EXPECT_TRUE(result.ok)` with
      the comment "Unregistered material is a WARNING, not an error". Left
      as a warning rather than silently reversing that prior decision;
      flagged here for the user to decide rather than guessed at
      autonomously (this is exactly the kind of behavior-reversing,
      cascading-blast-radius change — every currently-"valid with a
      warning" generator would flip to "invalid" and hit its C++ fallback
      path differently — the standing autonomous-session instructions
      call out as needing a human call, not an engineering judgment call).
      Also observed, not investigated further (noisy but non-blocking):
      running the full `MeshWorldTests` binary still logs ~389
      "not registered" warnings for material ids that ARE genuinely
      registered (e.g. `metal_lamp`, confirmed present in
      `BuiltinMaterials.cpp`) — looks like `MC3ValidatorTests`'s own
      explicit "warning, not error" test already established this is
      inherently non-fatal, and the repeated warnings track which
      individual test file's own fixture happened to call
      `register_builtin_materials()` before exercising a given generator,
      not a real registration gap (confirmed the specific ids above ARE
      registered) — not chased further per this project's own "don't chase
      flaky/noisy test output without a fresh concrete repro" precedent.
      1446/1446 tests still pass with the 4 new registrations.

      **2026-07-13 — closed.** Asked the user directly (not guessed) the
      one real remaining decision this entry had been flagging since
      2026-07-12: warning or hard error for an unregistered material id.
      **Decision: keep as a warning** — `MC3Validator`'s existing,
      already-tested behavior stays exactly as-is, avoiding the large
      cascading blast-radius a hard error would have caused across every
      currently-"valid with a warning" generator.

      While closing this out, chased the ~389-warning noise this entry's
      own 2026-07-12 pass had left uninvestigated — this time with a
      real, fresh, concrete repro (`--gtest_shuffle`), not just a guess.
      Confirmed it is NOT flakiness or coincidence: `MaterialRegistry`
      (`src/MaterialRegistry.cpp`) is a genuine Meyer's singleton (no
      `clear()` exists), so once ANY test in the shared `MeshWorldTests`
      binary calls `register_builtin_materials()`, every subsequent test
      sees the full builtin set for the rest of that process — meaning
      a test file that never calls it itself only "works" (no warnings)
      by accident, whenever some OTHER file happens to run first in
      whatever order gtest picks. Running with `--gtest_shuffle` and a
      range of seeds surfaced 4 real instances of this pattern:
      `BuildingComposerFixture`/`ChunkPipelineComposerIntegration`
      (`tests/BuildingComposerTests.cpp`), `ChunkPipelineDiagnosticsFixture`
      (`tests/ChunkPipelineDiagnosticsTests.cpp`), and 3 tests in
      `tests/WorldRendererTests.cpp` (`T269_UpdateLoadsChunksAroundOrigin`,
      `UnloadOnPlayerMove`, `WithMapLayerLoadsChunksWithoutCrashing`) — all
      fixed by adding their own `register_builtin_materials()` call
      (idempotent, so harmless even where another test already primed it).
      Verified: default test order was already clean before AND after
      (this was purely a shuffled-order latent risk, not a visible
      regression); a `--gtest_shuffle` sweep across dozens of seeds after
      the fix found zero remaining "not registered" warnings for any
      genuinely-registered material id. **1580/1580 tests pass**
      (test-file-only change, no production code touched — no app
      rebuild or `MeshWorldExport` smoke test needed this time).

### R107+ — full-document coverage

R100-R106 above only covered the 5 items the user explicitly dictated in
chat. `mesh_world_revival.md` is a much larger 26-section document; its own
§24 ("Plan Derivation Structure") proposes 14 workstreams (R0-R13,
document's own numbering, distinct from this file's R10x task IDs) which
R107-R122 below map onto directly (one workstream ⇒ one or two tasks here),
so that the whole document — not just the 5 chat-dictated items — eventually
becomes tracked backlog. §23 ("Major Risks") and §25/26 (Definition of
Success / Final Direction) are guiding principles to keep re-checking while
implementing R100-R122, not separate tasks. §15 (Interiors) is deliberately
not added as a new task here — the document itself says exterior city
quality should stabilize first, matching this file's own already-scoped,
still-open G10 (interior-room placement pipeline).

- [x] R107 (workstream R0) Audit & baseline: trace the complete current
      runtime generation path (which chunks/objects go through C++
      generators vs. Lua vs. `ObjectDefinitionLibrary`), record current
      build/test baseline, capture "before" screenshots/metrics of today's
      sparse output for later before/after comparison (§20 Stage 1, §22).
      Implemented as `docs/audit-baseline.md`: both `MapPipeline` and
      `ChunkPipeline` follow the same Lua-first/C++-fallback pattern via
      `LuaGeneratorRegistry`; only 17/many map levels and 4/~19 zones have a
      real Lua override today, the rest already fall back to native C++.
      Recorded build/test baseline: 1442 tests, 1429 passed, 13
      pre-existing `MapPipelineTest` failures (a "child" vs.
      "lua.map.child.level<N>.default" expectation drift, unrelated to
      R100-R106/R123, flagged as a follow-up not fixed here since R107 is
      audit-only). No GPU/screenshot capture was available in this
      session; the code-path trace serves as the "before" reference for
      R114 instead.
- [x] R108 (workstream R0) Generator/asset selection diagnostics: expose,
      per chunk, the generator/composer responsible, selected biome/
      district/style, selected asset ids+variants, fallback reasons, LOD
      level, object/triangle/material/light counts, and validation warnings
      — via metadata, logs, and/or a debug HUD/chunk inspector. Fallback
      must never be silent (§22). Implemented as `ChunkPipeline::get(x, y,
      ChunkDiagnostics*)` (new `ChunkDiagnostics.hpp`): reports source
      (lua/cpp_fallback/cache), the real `generator_id` read back from
      `<metadata>`, a non-empty `fallback_reason` whenever the C++ path
      wins (never silent), zone/region/style/lod, and content stats
      (object/material/light/triangle counts + validation errors/
      warnings) reused from `ValidationResult` (now itself extended with
      those same fields, since `MC3Validator`'s own recursive walk already
      computed — and used to discard — them) plus `Mc3MeshBuilder` for
      triangle counts. A cache hit still fills the struct by re-reading
      the cached content's own metadata (source == Cache), not by
      skipping diagnostics.
- [x] R109 (workstream R1) `mc3.json` semantic format: `Mc3JsonWriter`/
      `Mc3JsonParser` in `mesh-craft/mc3` parsing into the exact same
      `Mc3Document`/`Mc3Object`/`Mc3Material`/`Mc3Transform`/`Mc3Primitive`/
      `Mc3Extrude`/... AST as `Mc3XmlWriter`/`Mc3XmlParser` (one semantic
      model, two surfaces, per §4.2-4.3), wired through new
      `Mc3Document::loadFromJsonFile`/`loadFromJsonString`/
      `saveToJsonFile`. Genuinely semantic JSON (vectors as arrays, nested
      objects for transform/primitive/extrude/uvMapping/csgOperation, no
      `@name`-style mechanical XML mirroring), covering the whole document
      (metadata/meta, environment+fog, lights, cameras, textures incl.
      SVG, materials, embeds, scripts, sounds, music, triggers, scene
      states, definitions/instances incl. variants, primitives, extrude,
      CSG, groups/deform/uv-mapping/per-object metadata+states, actions).
      Uses `nlohmann/json` (new `FetchContent` dependency in
      `mc3/CMakeLists.txt`). Round-trip tests in new
      `mc3/test/json_roundtrip_test.cpp`: a hand-built fixed document
      covering nearly every AST feature, 60 random documents reusing the
      existing `RandomMc3DocumentGenerator.hpp`/`compareDocuments()` (same
      generator the XML `mc3_random_roundtrip_test` uses), and an optional
      cross-format check against the `features.mc3.xml` fixture. A JSON
      Schema file is deferred to a follow-up (not yet added) (§4.3, §21.1).
- [x] R110 (workstream R2) `.mc3lib.json`/`.mc3lib.xml` reusable-library file
      format (distinct from a scene/model file, per §8) + semver + content
      hashing for the R101 import resolver. **Dependency pruning explicitly
      deferred to a follow-up task** (user-approved scope cut, 2026-07-12,
      mirroring R109's own precedent of deferring its JSON Schema file): no
      compiler capable of "used definitions + recursive deps only" exists
      anywhere yet (`mesh-craft/mcb` is a 1:1 binary mirror of one whole
      document, no multi-definition pruning) — building one is real,
      separate scope, blocked on R101 (the `<import>` consumer) existing
      first to even have usage data to prune against. Implemented in
      `mesh-craft/mc3` (same "one Mc3Document AST, two surfaces" pattern as
      R109, not a parallel type hierarchy): new `Mc3LibraryInfo` struct
      (`libraryNamespace`/`version`/`contentHash`) + `std::optional<
      Mc3LibraryInfo> Mc3Document::library`, round-tripped through both
      `Mc3XmlWriter`/`Mc3XmlParser` (a new `<library namespace="..."
      version="..." hash="sha256:..."/>` element) and `Mc3JsonWriter`/
      `Mc3JsonParser` (a `"library": {...}` object) — a library file is
      just an ordinary `Mc3Document` (reusing its existing `definitions`
      map with variants, materials, textures) with `library` set, since
      mesh_world_revival.md §4.4/§4.5's "global libraries of reusable
      definitions with variants" was already modeled by the pre-existing
      AST. New `Mc3Document::saveToLibraryFile`/`saveToLibraryJsonFile`/
      `loadFromLibraryFile`/`loadFromLibraryJsonFile` (throw
      `std::invalid_argument` if `library` is unset — a library file must
      always carry the namespace+version identity `mc3lib://name@version`
      references need) and `computeLibraryContentHash()` (sha256 of the
      canonical mc3.json body, deliberately excluding the `library` block
      itself to avoid self-reference — representation-independent since
      XML/JSON share one AST). New self-contained `Mc3Sha256.{hpp,cpp}`
      (FIPS 180-4 SHA-256, ~100 lines) rather than a new FetchContent
      dependency for one stable, no-moving-parts algorithm. New
      `mc3/test/mc3lib_test.cpp` (registered as ctest `mc3lib`): sha256Hex()
      verified against the standard empty-string/"abc" test vectors,
      save-without-`library` throws on both surfaces, namespace/version/
      contentHash round-trip through both XML and JSON, and
      computeLibraryContentHash() is stable/content-sensitive/excludes
      `library` itself. **All 16 `mesh-craft/mc3` ctest targets pass**
      (verified via a standalone `cmake -S mc3 -B <dir>` configure+build,
      same as R109 — mesh-world's own root build has `BUILD_TESTING=OFF`,
      so `mc3/`'s own ctest suite, including this new one, was never
      reachable through mesh-world's build tree at all; not a regression,
      same pre-existing condition R109 already worked around the same way).
      Mesh-world's own **1446/1446 tests pass** with the updated `mc3/`
      linked in; `apps/mesh-world-app` verified building cleanly too.
- [x] R111 (workstream R3) Full asset metadata schema + registry: category/
      subcategory/semantic+style+region+period tags, nominal size, exact
      bounding box, origin/pivot, front/up orientation, anchor points,
      sockets, material slots, collision proxy, clearance volume, LOD
      refs, triangle/object counts, instancing eligibility, shadow policy,
      max-visibility distance, rarity/selection weight, license/provenance,
      semantic version, content hash, validation status, thumbnail
      reference (§6) — plus socket-alignment and clearance-volume checks in
      `MC3Validator` (§6, §14, §21.3).
      **Scoped to the AUTHORABLE fields only** (user-directive precedent
      from R110's own deferred-pruning cut, applied the same way here
      without re-asking since it's the same class of call): triangle/
      object counts (computable from geometry, not authored — belongs
      with `Mc3MeshBuilder`, mesh-world side), validation status
      (`MC3Validator`/`Mc3Validation.hpp` already produce this per-parse;
      a second authored copy would just go stale or disagree), content
      hash (per-definition hash needs R101's import/dependency tracking to
      be meaningful; the whole-library hash already exists as
      `Mc3LibraryInfo::contentHash`, R110), and thumbnail/preview
      reference (needs a render pipeline, R115's own scope) are explicitly
      NOT included — same "defer what needs tooling that doesn't exist
      yet" precedent R109 (JSON Schema) and R110 (dependency pruning) both
      already used. Socket-alignment/clearance-volume `MC3Validator`
      checks also deferred (need real content using non-trivial sockets/
      clearance to validate against first; nothing does yet since R112's
      content batch hasn't landed). Implemented in `mesh-craft/mc3`: new
      `Mc3AssetMetadata` struct (`Mc3AssetMetadata.hpp`) covering
      category/subcategory, 4 separate tag lists (semantic/style/region/
      period, kept separate per §6's own prose breakdown rather than one
      flattened list), nominal size, bounding box (min/max), facing,
      sockets (a single name→position map — folds "anchor points" and
      "sockets" together, since §6 never actually distinguishes them by
      shape and inventing a redundant near-duplicate concept wasn't
      justified), material slots, a free-form collision-proxy string
      (mirrors `Mc3Object::collision`'s own existing free-form string —
      no shared collision-shape enum exists in this codebase yet),
      clearance volume, LOD tier→definition-id map, instancing-eligible
      flag, a free-form shadow-policy string, max-visibility distance,
      selection weight, license/provenance, source-generator-or-hash, and
      a per-definition semantic version (explicitly distinct from
      `Mc3LibraryInfo::version`, R110, which versions the whole library
      file). Attached as `std::optional<Mc3AssetMetadata>
      Mc3Object::assetMetadata` (definitions only, by convention — nothing
      stops it being set on any object, but nothing currently does).
      Round-tripped through both `Mc3XmlWriter`/`Mc3XmlParser` (a new
      `<assetMetadata>` child element) and `Mc3JsonWriter`/`Mc3JsonParser`
      (`"assetMetadata": {...}`) — same one-AST-two-surfaces pattern as
      R109/R110. New `mc3/test/asset_metadata_test.cpp` (ctest
      `mc3_asset_metadata`): every field round-trips through both XML and
      JSON, and a second definition that never sets `assetMetadata` proves
      it stays genuinely absent after reload (not defaulted-and-written).
      Kept deliberately separate from the shared
      `RandomMc3DocumentGenerator`/`compareDocuments()` harness rather than
      extending it, to keep this task's blast radius to new files only.
      **All 17 `mesh-craft/mc3` ctest targets pass** (standalone build, one
      more than R110's 16). Mesh-world's own 1446/1446 tests pass with the
      updated `mc3/` linked in; `apps/mesh-world-app` build verified too.
- [x] R112 (workstream R4) First urban `mc3lib` content batch built on
      R101/R102/R106: windows, doors, roofs, facade modules, street
      furniture, vehicles, and generic props, each with multiple coherent
      variants and LOD levels (§4.4-4.5, §5.1-5.2, §11) — supersedes/
      absorbs the equivalent still-open Lua object tasks (e.g. T215) as
      the new primary path per R100.
      **2026-07-13 — v1 implemented** (user request: "udělej r112"). Real
      content, genuinely loaded through the actual R101/R102
      `Mc3ImportResolver` mechanism — NOT hand-registered C++ like R113
      v1's `house_gable_default` (that was an explicit "no R112
      dependency" shortcut; this is what real R112 content looks like).
      Scoped as a coherent bounded batch, same v1/v2/v3-style staging
      discipline R113 established: **4 of the 7 categories named in this
      task's own text** — window, door, street furniture (streetlamp),
      prop (mailbox) — 2 coherent variants each + one low-LOD proxy per
      variant (16 `Mc3Object` definitions total). **Roofs, facade
      modules, and vehicles are explicitly deferred to a later batch**,
      not silently dropped. R106 (global material catalogue) is also
      still `[ ]` — worked around for v1 by reusing materials already
      real and registered in `BuiltinMaterials.cpp`
      (`wood_window_frame`/`glass_clear`/`wood_door_panel`/
      `wood_door_frame`/`metal_lamp_ornate`/`metal_dark`/`light_amber`/
      `mailbox_body`/`wood_natural`, all pre-existing, none newly added)
      rather than blocking on R106 first.

      New `src/tools/build_mc3lib_content.cpp` (CMake target
      `MeshWorldBuildMc3Lib`, links `MeshWorldLib`): a real, re-runnable
      content-authoring tool — builds each definition via the exact same
      `box()`/`cyl()`/`icosphere()` "base_y is a BASE elevation"
      local-helper convention `ObjectDefinitionLibrary.cpp`'s own
      `make_house_gable()` uses (deliberately kept identical everywhere
      in this codebase after R113 v1's base-vs-center bug find), attaches
      full `Mc3AssetMetadata` (hand-verified bounds per definition, same
      discipline M327 established), and writes 4 genuine
      `.mc3lib.json` library files via `Mc3Document::defineObject()` +
      `saveToLibraryJsonFile()` — checked into `data/mc3lib/`
      (`urban-windows-1.0.0`, `urban-doors-1.0.0`,
      `urban-street-furniture-1.0.0`, `urban-props-1.0.0`), each a real
      `mc3lib://<name>@<version>` resolvable by `Mc3ImportResolver`.

      `ComposerAssets.cpp`'s `register_composer_assets()` extended with a
      new `register_mc3lib_batch(alias, uri)` helper: constructs a small
      "importer" `Mc3Document` with one `Mc3Import`, resolves it via a
      real `Mc3ImportResolver({"data/mc3lib"})` (never a hard error —
      catches `resolve()`'s exception and skips that batch with a stderr
      warning rather than crashing every tool/test that calls this at
      startup), then registers every resolved definition into
      `ObjectDefinitionLibrary` (so `WorldRenderer::inject_definitions()`
      keeps resolving `<instance definition="...">` unchanged, zero
      renderer-side changes needed) and, for definitions carrying real
      `assetMetadata` (i.e. not the LOD-low proxies, which deliberately
      carry none), into `AssetRegistry` too. **Real bug found and fixed
      during this task**: `resolve()`'s returned map keys are alias-
      qualified (`"<alias>:<definitionId>"`); registering under that
      qualified key broke every `assetMetadata.lods` lookup, which
      references sibling definitions by their bare, unqualified id (the
      tool has no way to know what alias a future importer will choose).
      Fixed by stripping the alias prefix before registering — assets
      end up under the same bare-id convention `house_gable_default`
      already used, e.g. `streetlamp.classic_01`, not
      `street_furniture:streetlamp.classic_01`.

      `BuildingComposer::compose_chunk()` extended to place one
      streetlamp (`AssetRegistry::query("street_furniture", ...)`) and
      one mailbox (`query("prop", ...)`) per parcel, near the parcel's
      own `street_edge_z` — deliberately NOT window/door, which need
      R103's socket-aware facade placement (not yet built) and stay
      registered-but-unplaced for now. Missing street-furniture/prop
      categories are a per-item skip, not a whole-chunk fallback trigger
      (houses remain the primary content either way) — proven by a
      dedicated test with a house-only registry.

      New tests: `tests/BuildingComposerTests.cpp` gained 9 (all 4
      categories resolve with the right counts, style-tag filtering
      selects the right single streetlamp variant, LOD-low proxies land
      in `ObjectDefinitionLibrary` but are NOT independently queryable via
      `AssetRegistry`, `assetMetadata.lods` values actually resolve,
      composed-chunk placement counts, missing-category graceful
      degradation). **1539/1539 tests pass** (1530 + 9 new).
      `apps/mesh-world-app` rebuilt clean. End-to-end verified via
      `MeshWorldExport --validate` with `use_world_composer: true`: 0
      errors/0 warnings across 400 chunks, all 119
      `small_house_block` chunks show real `streetlamp.classic_01`/
      `prop.mailbox.classic_01` instances at the expected parcel-edge
      coordinates, all referenced materials already registered (no new
      `BuiltinMaterials.cpp` warnings).

      **2026-07-13 (same day) — remaining 3 categories added** (user
      request: "udělej zbytek r112" — do the rest of R112), closing
      **all 7 categories this task's own text names**. `build_mc3lib_content
      .cpp` gained 3 more library builders, same helper conventions as
      before: `build_roofs_library()` — `roof.gable_clay_04` (matches
      `house_gable_default`'s own proportions and its `"gable_roof"`
      style tag, so it's a plausible drop-in once something actually
      composes buildings from separate parts) and `roof.flat_modern_01`
      (slab + 4-box parapet ring, same "picture frame of thin boxes"
      pattern `CaveGenerator`'s own entrance breach already established);
      `build_facades_library()` — `facade.residential_bay_window_01`/
      `facade.residential_bay_door_01`, standalone wall "bay" segments
      per §5.2's own "facade bays" wording; `build_vehicles_library()` —
      `car.hatchback.compact_01`/`car.sedan.family_01` (body+cabin+
      windshield+4 wheels; a new `wheel()` helper handles a cylinder
      tipped 90° so its rolling axis is horizontal, a genuinely different
      "resting on the ground" convention from the vertical `cyl()`
      helper). 12 more `Mc3Object` definitions (6 real + 6 LOD-low
      proxies), 3 more `.mc3lib.json` files in `data/mc3lib/`
      (`urban-roofs-1.0.0`, `urban-facades-1.0.0`,
      `urban-vehicles-1.0.0`). `ComposerAssets.cpp` gained 3 more
      `register_mc3lib_batch()` calls (no changes to the helper itself).

      Of the 3 new categories, only **vehicles** got wired into
      `BuildingComposer::compose_chunk()` — one parked car per parcel,
      placed on the street itself beyond `street_edge_z` (no style-tag
      filter, unlike streetlamp/mailbox, since a car model isn't a
      regional-style decision — both variants are real candidates,
      confirmed via a live `MeshWorldExport` run: 113/119 composer chunks
      got a hatchback, 108/119 got a sedan, real variety not a hash bug
      always picking one). Roofs and facade modules stay
      registered-but-unplaced, same honest scope limit as window/door:
      attaching a roof to a building or tiling a facade module along a
      wall both need R103's socket-aware placement, not yet built.

      5 more tests (`tests/BuildingComposerTests.cpp`): roof/facade
      module/vehicle category resolution counts, the gable roof's style
      tag matching the house's own `roofFamily`, composed-chunk vehicle
      placement count + real 2-variant coverage. **1544/1544 tests pass**
      (1539 + 5 new). `apps/mesh-world-app` rebuilt clean. End-to-end
      re-verified via `MeshWorldExport --validate`: 0 errors/0 warnings
      across 400 chunks, all 119 composer chunks show a real
      `car.hatchback.compact_01`/`car.sedan.family_01` instance within
      chunk bounds alongside the house/streetlamp/mailbox.

      **Still open, explicitly deferred, not silently dropped**: deeper
      variant coverage (2 per category, not the "many" `mesh_world_revival
      .md` §4.5 envisions), R106 itself (a real material catalogue —
      this whole batch reuses existing ad hoc ids), R103 (would make
      window/door/roof/facade_module content actually consumable, not
      just registered), and R112's own "supersedes T215" claim (T215
      itself untouched).

      **2026-07-13 (same day) — deeper variant coverage** (user request:
      "udělej hlubší pokrytí variant" — do deeper variant coverage),
      going from 2 to 3-5 variants per category (14 → 29 top-level
      definitions). Every `_02`/`_05`-style id is a genuine same-family
      variant per `mesh_world_revival.md` §4.5's own worked example
      (`window.residential.double.classic_01/02`); every new subcategory
      (`window.residential.single`, `window.shopfront.large`,
      `door.apartment.shared_entry`, `bench`, `prop.trash_bin`,
      `car.van.delivery`) is a deliberately different KIND of thing, not
      just a recolor — also matching worked examples §4.5 explicitly
      names (`window.shopfront.large.urban_03`, `car.van.delivery_02`).
      All built with the SAME proven `box()`/`cyl()`/`icosphere()`/
      `wheel()` primitives already established — no new geometric risk
      introduced (deliberately avoided a hip/pyramid roof idea once
      hand-checking its compound-axis rotation math for correctness
      without a visual renderer turned out to be error-prone; chose a
      second flat-roof variant with a rooftop HVAC unit instead, reusing
      proven box-only geometry).

      **Zero changes needed to `ComposerAssets.cpp` or
      `BuildingComposer.cpp`**: both already generically handle however
      many candidates a category resolves to (`register_mc3lib_batch()`
      iterates the whole resolved map; `compose_chunk()`'s
      `candidates[hash % candidates.size()]` selection scales to any
      pool size) — this deeper batch is purely new content plus
      `MeshWorldBuildMc3Lib` additions, proving the v1 wiring was
      genuinely reusable, not hardcoded to "exactly 2 variants."

      **Real test-design lesson surfaced by this pass**: several existing
      tests had silently assumed "exactly 1 candidate" for a given
      style-tag filter, or indexed `query()[0]` assuming a specific id
      would sort first — both assumptions broke the moment more
      same-tagged variants existed. Fixed by (1) switching the
      style-tag-filter test to a tag ("modern") that still isolates
      exactly one entry after the expansion, and (2) replacing
      placement-check assertions that pinned ONE specific definition id
      with a robust `any_definition_present(xml, {candidate ids...})`
      helper — checking that some valid candidate appears, not a
      specific one a deterministic hash roll might no longer pick once
      the pool grows. This is the CORRECT fix, not a patch: any future
      content addition should use the same robust-to-growth pattern
      rather than pinning exact ids/counts a later variant-coverage pass
      would silently break.

      1 new test (`NewSubcategoriesFromDeeperCoverageExist`, proving each
      genuinely-new subcategory resolved with the right category/
      subcategory) plus the ~10 existing tests reworked per the lesson
      above. **1545/1545 tests pass** (1544 + 1 net new). `apps/mesh-
      world-app` rebuilt clean. Re-verified end-to-end via
      `MeshWorldExport --validate`: 0 errors/0 warnings across 400
      chunks; real variety confirmed for every `central_europe`-tagged
      family (e.g. streetlamp.classic_01 in 99 chunks, classic_02 in 97,
      bench.classic_01 in 90 — genuinely mixed, not always the same
      pick) and all 4 untagged vehicle variants (73-88 chunks each).
      **Honest, non-obvious finding, not a bug**: `streetlamp.modern_01`/
      `prop.mailbox.modern_01` (tagged `{"modern"}` only) never appear in
      ANY composed chunk, because only one `StyleProfile`
      (`central_europe_default`, `facadeFamily="central_europe"`) is
      registered in v1 — the modern variants are real, tested, and
      independently queryable, just never selected by the one style
      profile that exists. A second, modern-facing `StyleProfile` would
      be needed to ever surface them in composed output; out of scope
      for a content-coverage pass (that's deeper STYLE PROFILE coverage,
      a different kind of "depth" than this task's own scope).
      (Investigated one apparently-flaky "15 chunks with
      `rock_snow_covered` warnings" validation result during this pass —
      traced to unrelated alpine/tundra chunks outside the city zone
      entirely, did not reproduce across 3 identical reruns; a
      pre-existing, unrelated flake, not caused by this work, not chased
      further per the project's own established policy.)
- [x] R113 (workstream R5) C++ "world composer": parcel-driven building
      placement selecting a style profile (region/period/wealth/facade/
      windowFamily/roofFamily, extends `StyleRegistry`/G11) and coherent
      asset variants per building, with a street-first generation order
      (road network → intersections → sidewalks/curbs → blocks → parcels →
      building envelopes → facade details → street furniture) (§10.3, §11).
      **2026-07-13 — design written**
      (`docs/world-composer-design.md`, at explicit user request following
      the same "scope it, don't implement yet" pattern G10's own scoping
      section established). Grounded in what already exists (checked, not
      assumed): `Mc3AssetMetadata` (R111) already covers nearly all of
      §6's schema, just unpopulated/unqueried; `Mc3ImportResolver`
      (R101/R102) is the real asset source once R112 content exists;
      `ObjectDefinitionLibrary`'s ~250+ definitions are a usable v1
      population source without waiting for R112; `StyleRegistry`/`Style`
      (G11) is palette-only and needs a NEW, additive `StyleProfile` type
      (region/period/wealth/facade/window/roof family) rather than a
      breaking change; `SmallHouseBlockGenerator.cpp`'s own 4-plot layout
      is the real precedent `Parcel`/`derive_parcels()` generalizes;
      `ContainmentRuleRegistry` (G13) is reused for "how many of this
      prop" rather than inventing a second rule system. v1→v2→v3
      incremental path: v1 proves the mechanism on `small_house_block`
      only, reusing `SmallHouseBlockGenerator`'s own parcel layout and a
      handful of mechanically-ported house definitions (no new content
      authoring, no R112 dependency); v2 needs real R112 content + a real
      street-first block/parcel algorithm; v3 tackles cross-chunk
      continuity (explicitly unsolved by v1/v2, documented as a known
      limitation, not silently ignored — same class of problem R124 fixed
      for terrain elevation, not yet generalized to parcels). Explicit
      fallback behavior spec (no match → `std::nullopt` → fall through to
      the existing chain, never a hard error or placeholder). Explicit
      non-goals: R112/R115/R119/R120 are separate workstreams, not
      R113's own scope.
      **2026-07-13 — v1 implemented**, at explicit user request
      ("ano implementuj jak navrhuješ" — yes, implement as you propose)
      immediately following the design. New types, exactly as sketched:
      `AssetRegistry`/`AssetEntry` (`include/AssetRegistry.hpp`,
      `src/AssetRegistry.cpp` — category+style-tag query, results sorted
      by id for determinism regardless of `unordered_map` bucket order);
      `StyleProfile`/`StyleProfileRegistry` (`include/StyleProfile.hpp`,
      `src/StyleProfile.cpp` — `pick_for(seed)` uses a sorted-by-id vector
      + splitmix64-style seed mix, deliberately decoupled from
      `Map::noise`); `Parcel`/`derive_parcels()` (`include/Parcel.hpp`,
      `src/Parcel.cpp` — v1 returns the same 4-plot layout as
      `SmallHouseBlockGenerator.cpp` for `RegionType::small_house_block`,
      empty otherwise); `BuildingComposer` (`include/BuildingComposer.hpp`,
      `src/BuildingComposer.cpp` — `compose_chunk()`: parcels → style
      profile → `AssetRegistry::query()` → per-parcel deterministic asset
      pick via `Map::noise::hash2i` → `MC3Writer` → `nullopt` if no
      parcels/no matching asset, per the fallback spec). One v1 asset
      registered: `house_gable_default` (`src/ComposerAssets.cpp`'s
      `register_composer_assets()`), a NEW native `ObjectDefinitionLibrary`
      definition (`make_house_gable()` in `src/ObjectDefinitionLibrary.cpp`)
      mechanically ported from `simple_house.lua`'s gable-house geometry,
      with a full `Mc3AssetMetadata` attached (category="house",
      styleTags={"central_europe","gable_roof"}, etc.) — chosen over
      reusing the Lua generator directly, per R100's own direction that
      Lua is a compatibility layer, not the new content's source of truth.
      Wired into `ChunkPipeline::get()` as a new first-choice branch,
      gated by a new `WorldConfig::use_world_composer` flag (default
      `false`, loadable from `world.json`/`world.local.json`) — disabled
      by default, so no existing world/test is affected until opted in.
      `ChunkDiagnostics::Source` gained a `Composer` enumerator so a
      composer-produced chunk is distinguishable from the old C++
      fallback chain without generator_id string-parsing.
      **Incidental finding while porting house geometry**: both
      `simple_house.lua` and `generators/lua/building/house/detached.lua`
      had a real, previously-undiscovered bug — several elements
      (walls/door/windows/gable/foundation) passed CENTER-style y values
      to `Mc3DocumentBuilder::box()`, which actually expects a BASE
      elevation (`position.y = y + sy/2` internally, confirmed by the
      pre-existing `CallGeneratorAppliesPositionOffset` test) — causing
      walls to float roughly half their own height above the ground.
      Fixed in both files (roof-panel elements left deliberately
      unchanged — small magnitude, rotated-panel pivot ambiguity,
      documented in-code). Two new regression tests added
      (`tests/LuaCompositionTests.cpp`:
      `HouseDetachedWallRestsOnFoundationNoFloatingGap`,
      `SimpleHouseWallAndDoorRestOnFloorNoFloatingGap`).
      New `tests/BuildingComposerTests.cpp` (17 tests): `AssetRegistry`
      query/get/clear, `StyleProfileRegistry` get/pick_for/empty,
      `derive_parcels()` layout + bounds, `BuildingComposer::compose_chunk`
      success/nullopt paths, and 3 `ChunkPipeline` integration tests
      proving the flag gates behavior (disabled → unchanged output;
      enabled + `small_house_block` → routes through `Composer` with the
      right generator_id and instance count; enabled + other region →
      unaffected). **1530/1530 tests pass** (1513 + 17 new).
      `apps/mesh-world-app` rebuilt clean. Verified end-to-end via
      `MeshWorldExport --validate` with `use_world_composer: true` set
      through a `world.local.json` override (T242): 0 errors/0 warnings
      across 400 chunks, 119 chunks routed through
      `cpp.chunk.composer.small_house_block` with correct
      `house_gable_default` instance placement/rotation at each parcel;
      confirmed the same coordinate produces the old
      `cpp.chunk.small_house_block` output with the flag off (genuine
      differentiation, not coincidence). v2 (real R112 content + a real
      street-first block/parcel algorithm) and v3 (cross-chunk
      continuity) remain open, per the design doc's own staging.

      **2026-07-13 (same day) — v2 implemented** (user request, in
      response to being asked which R-series item to prioritize next:
      "R113 v2"). By this point R112 (7 real content categories) and
      R103 (a second, modular house) already existed, so v2's own two
      documented prerequisites were both satisfied; this pass is
      specifically `derive_parcels()`'s own literal 4-plot list becoming
      a real, parametric algorithm (`src/Parcel.cpp`).

      **Real bug found while generalizing, not just a refactor**: v1's
      own `x=13`/`x=51` column positions were absolute meter literals
      that only ever produced correct, in-bounds parcels for EXACTLY a
      64m chunk (the one size every existing `world.json` config
      happens to use) — any other `chunk_size_m` would have silently
      placed parcels outside the chunk or overlapped rows. Fixed
      properly: column positions are now computed from a real column
      count (`chunk width ÷ (parcel width + yard gap)`, floored, ≥1) and
      centered on the chunk's actual width; row/street-edge Z positions
      are expressed as FRACTIONS of `chunk_size_m` (each exactly v1's
      own value ÷ 64, so a real 64m chunk reproduces v1's identical
      layout) rather than the absolute offsets v1 used, since an
      absolute offset made the two rows collide on any chunk
      meaningfully smaller than 64m — caught by hand-checking the
      geometry before writing the fix, not discovered by a failing test.
      Object footprint size (10×8m, matching the one real house size
      currently registered) deliberately stays an absolute constant —
      real-world building dimensions don't scale with chunk size, only
      the layout position does.

      Added genuine per-chunk variety (R100's own "seeds drive variety"
      rule): each row's own column count independently rolls one fewer
      than the space allows (new `kParcelColumnRollAxis` constant, same
      `Map::noise::hash2i` convention `BuildingComposer.cpp` already
      uses), so a 64m chunk now yields 8-10 parcels (was always exactly
      4) and different chunks visibly differ from each other while
      staying fully deterministic for a given seed+coordinate. Zero
      changes needed in `BuildingComposer.cpp` itself (it already looped
      `for i in 0..parcels.size()`, no hardcoded count anywhere).

      6 new/updated `tests/BuildingComposerTests.cpp`/`ParcelTest`
      cases: parcel count is deterministic for a given seed, scales
      correctly with `chunk_size_m` (a smaller chunk yields fewer
      parcels than a larger one), and — the real regression guard for
      the row-collision bug — a bounds-safety sweep across 4 chunk sizes
      × 20 seeds (80 combinations), not just the one previously-tested
      default fixture. 3 pre-existing tests that hardcoded "exactly 4
      parcels/houses/streetlamps/mailboxes/vehicles" were fixed to
      compare against `derive_parcels()`'s own actual count instead
      (same "don't pin a count/id a later change can legitimately
      shift" discipline this session has now applied to R112's deeper
      coverage, R103's second house candidate, and this pass alike).
      **1562/1562 tests pass** (1558 + 4 net new). `apps/mesh-world-app`
      rebuilt clean. End-to-end verified via `MeshWorldExport
      --validate`: 0 errors/0 warnings across 400 chunks; composer-
      routed chunks now show a real mix of 8/9/10-parcel blocks (62
      chunks with 9, 29 with 10, 28 with 8, across 119 total) instead of
      always exactly 4 — genuinely denser, more varied urban blocks, not
      just a code refactor with the same visible output. v3 (cross-chunk
      continuity) and a full road-network-aware subdivision (real
      streets between blocks, intersections, non-rectangular blocks)
      remain open, per the design doc's own staging.

      **2026-07-13 (same day) — v3 implemented** (user request: "udělej
      V3 a poté skutečnou síť ulic s krizovatkami" — do v3, then a real
      street network with intersections).

      **Real, pre-existing infrastructure discovered before designing
      anything new** (checked, not assumed): this legacy chunk system
      already has a full road/intersection system —
      `RegionType::road`/`crossroad` are whole-chunk region types with
      their own generators (`RoadGenerator.cpp`/`CrossroadGenerator.cpp`,
      both already exit-aware: `CrossroadGenerator` auto-detects 3-way
      vs 4-way junctions from `ctx.exits`), and `examples/world.json`
      already carves a real road/crossroad grid with intersections
      through the `apartment_block` area. **The actual gap**:
      `WorldMap::build()`'s exits computation
      (`ctx.exits.{north,south,east,west}_road`) was gated to only run
      for chunks that are THEMSELVES road/crossroad — meaning any OTHER
      region type (including `small_house_block`) always saw
      `ctx.exits` as all-false, with zero way to know whether a real
      road actually bordered it. `derive_parcels()` (v1/v2) never
      consulted `ctx.exits` at all, unconditionally drawing a "street"
      to both the north and south of EVERY `small_house_block` chunk
      regardless of what (if anything) was actually adjacent — the real
      cross-chunk continuity bug v1/v2's own staging had flagged as
      unsolved.

      **Fix, in two parts.** (1) `src/WorldMap.cpp`: removed the
      `is_road_region(this chunk)` gate so exits are computed for EVERY
      chunk (existing road/crossroad behavior is unchanged — same
      computation, just no longer skipped for other region types).
      (2) `derive_parcels()` (`src/Parcel.cpp`) is now genuinely
      exits-aware: emits one row of parcels per side with a REAL
      adjacent road (`ctx.exits.north_road`/`south_road`/`east_road`/
      `west_road`), and returns EMPTY if no side has one — an honest
      "nothing to compose here, fall through to the existing generator
      chain" rather than a fabricated street. Generalized to all 4
      orientations (v1/v2 only ever supported north/south): `Parcel`
      gained `street_x`/`street_z` (a real 2D street reference point,
      replacing v2's Z-only `street_edge_z`, which was meaningless for
      an east/west-facing row), `normal_x`/`normal_z` (unit vector from
      the parcel's own center toward its street), and
      `frontage_extent` (the building's own real frontage width, which
      does NOT swap with orientation the way `width`/`depth` do —
      those two ARE swapped for east/west rows, since a 90° yaw makes
      the building's original width span world Z instead of world X).
      Rotation values (`north=0°/south=180°/east=270°/west=90°`) are
      derived so a `"+Z"`-facing definition points away from its own
      center and toward its street — self-consistent, but (like v1/v2's
      own already-shipped 0°/180° convention before it) not
      independently visually verified in this sandboxed, no-GPU
      environment; a pre-existing limitation, not a new one.
      `BuildingComposer.cpp`'s lamp/mailbox/vehicle placement was
      updated to use `street_x`/`street_z` plus a `right` vector derived
      from `normal_x`/`normal_z` (`right = (-normal_z, normal_x)`)
      instead of v1/v2's own X-only/Z-only offset math, which only
      worked for north/south rows.

      New/updated tests: `WorldMapTests`/`WorldConfigTests` — the
      pre-existing `RoadExitsAreConsistentAcrossBorders` test's own
      invariant no longer held under the new (correct, intentionally
      asymmetric at a road/non-road boundary) semantics, so it was
      replaced with two tests checking the actual real invariant
      (`RoadExitsMatchRealNeighborRegionType`,
      `RoadExitsAreSymmetricBetweenTwoRoadChunks`, the latter proving
      the OLD "both-road" symmetric case is still real). New
      `ParcelTest` cases for each single-direction case, all-4-directions,
      the none-adjacent empty case, and a bounds-safety sweep across
      every orientation × 4 chunk sizes × 10 seeds. One pre-existing
      `ChunkPipelineComposerIntegration` test needed updating (it
      constructed a bare `EdgeExits{}` directly, now correctly producing
      no parcels — fixed to pass real exits). **1570/1570 tests pass**
      (1562 + 8 net new). `apps/mesh-world-app` rebuilt clean. End-to-end
      verified via `MeshWorldExport --validate` against the REAL
      `examples/world.json`: 0 errors/0 warnings across 400 chunks;
      composer-routed `small_house_block` chunks dropped from 119 to 48
      (the honest, correct number that actually border a real road in
      the CURRENT world config — the other 71 now correctly fall back to
      the pre-existing generator chain instead of drawing phantom
      streets); hand-verified one real east/west-oriented chunk's exact
      output (a west-facing row, houses at x=13, street at x=2, vehicle
      at x=0.5, lamp/mailbox z-offset ±6 from each house) against the
      new placement formulas by hand -- every value matched exactly.

      Deliberately narrow, upfront scope note: this still places one
      INDEPENDENT row per bordering side (not a single unified block
      layout with corner-aware packing) -- a real road-network-aware
      block subdivision (mesh_world_revival.md §11's full pipeline)
      remains later scope. Since the current `examples/world.json` only
      has real roads within the `apartment_block` area's own footprint,
      the outer `small_house_block` ring is now MOSTLY composer-inactive
      (correctly, but with reduced coverage) -- extending the demo
      world's own road grid to reach that ring is this session's own
      immediate next step, tracked as the "real street network with
      intersections" half of this same user request.

      **Real street network with intersections, same day, same request's
      2nd half.** Computed the ACTUAL current coverage first (a small
      Python script mirroring `WorldMap::build()`'s own override logic,
      not guessed): of 119 `small_house_block` cells, only 48 bordered a
      real road (all clustered right at the `apartment_block`'s own
      inner road grid boundary); the outer ring itself had ZERO internal
      streets of its own. Extended `examples/world.json`'s own `regions`
      array (existing entries untouched, 4 new ones appended) with an
      outer road ring: two full-width horizontal strips (`y=1`, `y=17`,
      `x=2` to `x=18`) plus two vertical connector segments (`x=17`,
      `y=2`-`7` and `y=12`-`16` -- deliberately split around, not through,
      the existing `park` regions at `y=8`-`11` so they stay untouched).
      Deliberately did NOT add explicit `crossroad` markers at the new
      ring's own corners, matching the EXISTING config's own precedent
      (checked first): its inner ring's literal corners, e.g. `(3,3)`,
      are already plain `"road"` with both axes' neighbors also road,
      not marked `crossroad` — `RoadGenerator`/`CrossroadGenerator` (both
      pre-existing, exit-aware) already handle this without a special
      marker.

      Iterated the exact strip placement against the same coverage
      script until reaching a good, honestly-reported result: 74 of the
      former 119 `small_house_block` cells remain that region (32 became
      real road, converting former fictional-street house blocks into
      actual streets); of those 74, 70 (94.6%) now border a real road,
      up from 48/119 (40.3%) before. **4 cells remain honestly
      uncovered** (`x=18, y=8-11` -- directly beside the existing parks,
      where reaching them would have required either overwriting park
      cells or a disproportionate amount of extra road just for 4 cells)
      -- accepted and explicitly documented, not silently claimed as
      100%. New `WorldMap.
      SmallHouseBlockMostlyHasRealStreetAdjacencyAfterOuterRingExtension`
      test makes this a real, automated regression guard (computed via
      `WorldMap` directly, no chunk generation needed) rather than a
      one-off manual check: asserts at least `total-4` cells are
      road-adjacent and that adjacency exceeds 90% overall. **1571/1571
      tests pass** (1570 + 1 new). Verified end-to-end via
      `MeshWorldExport --validate`: 0 errors/0 warnings across all 400
      chunks; composer-routed `small_house_block` chunks rose from 48 to
      70 (matching the coverage script's own prediction exactly); the 4
      new road strips generate correctly via the existing
      `lua.zone.road` generator (the same Lua-first path every other
      road cell in this config already uses — no new generator code
      needed, this was purely a content/config change); the 4 remaining
      fallback chunks are exactly the predicted `(18,8)`-`(18,11)`.

      **2026-07-13 (same day) — size-aware parcel/asset matching**
      (self-selected continuation, following the facade_module-consumption
      entry's own "still open" list above: house.rowhouse.modular_01, at
      15.6m wide, had been deliberately kept OUT of `AssetRegistry`'s
      "house" category up to this point, since v1's `BuildingComposer`
      used one uniform candidate pool per chunk — placing the rowhouse
      wherever a 10m standard parcel landed would have overlapped its
      neighbors). Made the match real instead of leaving it excluded:
      `derive_parcels()` (`src/Parcel.cpp`) now rolls a per-row WIDTH
      CLASS (`kRowWidthClassAxis = 4102`, a new, independent axis constant
      from the existing column-count roll) before computing that row's
      column count — standard (10.0m) roughly 3-in-4 rows, wide (15.6m,
      matching the rowhouse's real footprint) the rest — and
      `BuildingComposer::compose_chunk()` (`src/BuildingComposer.cpp`) now
      filters house candidates PER PARCEL by
      `abs(candidate.nominalSize[0] - parcel.frontage_extent) <
      kHouseWidthTolerance` (1.0m) instead of picking from one pool for
      the whole chunk. `house.rowhouse.modular_01` is now genuinely
      registered into `AssetRegistry`'s "house" category
      (`ComposerAssets.cpp`, `register_in_asset_registry` flipped to
      `true`).

      **Real bug found and fixed, not just new code**: `rowMeta.category`
      in `src/tools/build_mc3lib_content.cpp` was still `"house_row"`
      (a leftover from when the rowhouse was deliberately excluded) —
      flipping the registration flag alone would NOT have made the
      rowhouse a real "house"-category candidate at all, since
      `AssetRegistry::query("house", ...)` matches on `meta.category`
      exactly; fixed to `"house"`, then the content tool was rebuilt and
      re-run to regenerate `data/mc3lib/urban-buildings-1.0.0.mc3lib.json`
      with the corrected category actually baked into the checked-in JSON
      (`ComposerAssets.cpp` loads that file directly — the C++ source fix
      alone does nothing until the data file is regenerated).

      **Second real finding, while chasing test failures — not a bug,
      but a genuine current content-coverage gap, reported rather than
      hidden**: `house.rowhouse.modular_01` has a real FLAT roof
      (`row_roof_slab`, a concrete slab cap — there is no gable-roof
      module at this width yet), so it correctly does NOT carry the
      `"gable_roof"` style tag the way `house_gable_default`/
      `house.gable.modular_01` do. Since the only style profile shipped
      so far (`central_europe_default`) requires `roofFamily =
      "gable_roof"`, the rowhouse is — correctly — never a real candidate
      under it, regardless of a parcel's width class. That means any row
      that rolls wide-class today gets its house instance silently
      skipped (a real, correct per-parcel "no compatible match" fallback,
      same discipline as the existing lamp/mailbox/vehicle per-item
      skips) rather than ever placing the rowhouse — quantified via a
      real `MeshWorldExport --validate` run against the current
      `examples/world.json` (seed 42, `use_world_composer: true`, 400
      chunks): 357 parcel "slots" (counted via streetlamp instance ids,
      placed unconditionally per parcel) vs only 299 real house instances
      — 58 parcels across 23 chunks have a street but no house, and
      `house.rowhouse.modular_01` appears in exactly 0 of the 400 real
      chunks. Left as an honestly-documented follow-up (either a real
      gable-roofed wide house variant, or a second, non-gable-required
      style profile — new content authoring, not this task's own scope),
      not silently patched over.

      This same style-tag gap also meant the default test seed (7) — used
      by nearly every pre-existing `BuildingComposerTests.cpp` case —
      turned out to roll BOTH of its configured rows (north+south) wide,
      leaving zero real house matches for that specific seed/style
      combination; two tests
      (`ComposesRealInstancesForSmallHouseBlock`,
      `MissingStreetFurnitureOrPropDoesNotBlockHousePlacement`) were
      fixed to use seed 0 instead (checked by hand to roll both rows
      standard-width), since neither test was meant to exercise the
      wide/style-incompatible path — that path has its own dedicated
      coverage (`RowhouseIsARealHouseCategoryCandidateWithWideNominalSize`,
      renamed from `RowhouseIsInObjectDefinitionLibraryButNotHouseCategory`,
      now asserting real "house"-category membership instead of deliberate
      exclusion). `DerivesExactlyTenParcelsForDefaultTestSeedAndChunkSize`
      renamed to `DerivesExactlySixParcelsForDefaultTestSeedAndChunkSize`
      (10 → 6 is a real, deterministic consequence of the new width-class
      roll for the default seed — fewer, wider parcels — not a
      regression). **1577/1577 tests pass** (no net new tests this task —
      3 existing tests updated to match the new, correct behavior).
      `apps/mesh-world-app`/`MeshWorldExport` rebuilt clean. End-to-end
      re-verified via `MeshWorldExport --validate` against
      `examples/world.json`: 0 errors/0 warnings across all 400 chunks
      (the 58-parcel style-coverage gap above is an accepted content
      limitation, not an error/warning).

      **2026-07-13 (same session, continued) — wide gable-roofed house
      closes the coverage gap** (self-selected after another bare
      "pokracuj", picking the follow-up item this same entry itself had
      just flagged: a gable-roofed wide house variant, or a second style
      profile). Chose the wide-house-variant option over a second style
      profile: `StyleProfileRegistry::pick_for(seed)` picks ONE profile
      per chunk among ALL registered profiles, so adding a second profile
      would have split EVERY existing small_house_block chunk's style
      pick roughly 50/50 between the two — a much larger, harder-to-fully
      reason-about behavior change than adding one new, purely additive
      content definition.

      New `house.gable.wide_01` (`src/tools/build_mc3lib_content.cpp`):
      a wide (15.6m, matching `kWideParcelWidth` exactly) house, same
      wall/floor/gable-end construction style as `house.gable.modular_01`
      just wider, with 2 windows on each side of a central door (4
      windows total, alternating `window.residential.double.classic_01`/
      `_02`) instead of 1 each. Its own attached Lua script imports a
      NEW roof module, `roof.gable_clay_wide_01` (`build_roofs_library()`
      — same gable-roof construction as `roof.gable_clay_04`, scaled to
      this house's 15.6m×8.6m footprint instead of 10.6m×8.6m, same 2.5m
      ridge height — a real, if architecturally shallower-pitched, gable
      roof, not a placeholder), so `house.gable.wide_01` genuinely and
      honestly carries `styleTags = {"central_europe", "gable_roof"}` —
      unlike `house.rowhouse.modular_01`, which correctly can't (real
      flat roof). Registered into `AssetRegistry`'s "house" category via
      `ComposerAssets.cpp`'s existing `compile_and_register_modular_
      building()` helper, same as the other two modular houses.

      This directly closes the gap the previous entry quantified: under
      `central_europe_default` (the only shipped style profile,
      requiring `roofFamily = "gable_roof"`), a wide-class parcel now has
      a real, style-compatible house candidate regardless of which of
      the two wide houses' widths a given roll needs.

      2 new tests (`WideGableHouseIsARealGableRoofTaggedHouseCandidate`,
      `WideGableHouseHasRealWindowDoorRoofChildrenFromScript` — the
      latter asserting the exact 4 window/1 door/1 roof child instances
      the script places). 2 pre-existing tests updated for the real
      count increase, not a bug:
      `RoofsLibraryResolvesAllVariants` (4→5 roof-category candidates)
      and `GableRoofStyleTagMatchesBothGableVariants` → renamed
      `GableRoofStyleTagMatchesAllThreeGableVariants` (2→3 gable_roof-
      tagged roofs). **1579/1579 tests pass** (1577 + 2 net new).
      `apps/mesh-world-app`/`MeshWorldExport` rebuilt clean. End-to-end
      re-verified via `MeshWorldExport --validate` against
      `examples/world.json`: 0 errors/0 warnings across all 400 chunks;
      the exact same coverage-gap script from the previous entry now
      shows 357 lamp slots vs 357 real house instances (up from 299) —
      **the gap is fully closed**, and `house.gable.wide_01` itself
      appears 58 times (exactly the count of previously-unmatched
      parcels), confirming the fix by direct measurement, not assumption.

      **2026-07-13 (same session, continued) — corner-aware layout fixes
      a real, 100%-reproducible parcel overlap** (self-selected after
      another bare "pokracuj"; the most direct, well-bounded next slice
      of this entry's own recurring "full road-network-aware block
      subdivision" follow-up — not the whole unified-block-layout
      rewrite, just its most concrete, demonstrable defect).

      **Investigated before touching any code**: a standalone throwaway
      program linking `Parcel.cpp` directly, sweeping every 2-of-4-side
      combination × 200 seeds, found that north/south rows (spaced along
      X) and east/west rows (spaced along Z) had always been computed
      completely independently, each spanning the full chunk minus a
      fixed side margin regardless of what else was active — so whenever
      TWO ADJACENT sides both bordered a real road (north+east,
      north+west, south+east, south+west), their rows' parcels
      genuinely, provably overlapped near the shared corner: **100% of
      200 sampled seeds, for every one of the 4 adjacent combinations**.
      Opposite pairs (north+south, east+west) never overlapped (0%) —
      they sit on opposite ends of the chunk, confirming the bug is
      specifically a corner problem, not a general layout flaw. Checked
      the REAL current `examples/world.json` too (same override-mirroring
      Python script established earlier this session): 4 of the 74
      `small_house_block` cells actually have this two-adjacent-road
      condition today (the outer ring's own 4 corners, where a
      horizontal and vertical road strip meet) — a real, if not huge,
      production-visible defect, not just a theoretical one.

      **Fix** (`src/Parcel.cpp`): `max_columns_for()` and `place_row()`
      now take an explicit usable placement RANGE (`usable_start`,
      `usable_end`) instead of always assuming `[kSideMargin, chunk_size_m
      - kSideMargin]`. `derive_parcels()` computes each row's own
      perpendicular-axis position unconditionally (needed for the
      reservation math even when placing the OTHER, perpendicular row),
      then clips north/south rows' shared usable X-range to stop short of
      an ACTIVE east/west row's own real footprint (`kParcelDepth`) plus
      a small gap, and symmetrically clips east/west rows' shared usable
      Z-range against an active north/south row — eliminating the overlap
      by construction instead of detecting/patching it after placement. A
      side with no active perpendicular neighbor gets EXACTLY its old
      range (bit-for-bit unchanged behavior for every single-direction or
      opposite-pair chunk — i.e. every chunk in the current
      `examples/world.json` except those same 4 corner cells).
      `max_columns_for()`/`columns_for()` now return 0 (not clamped to a
      minimum of 1) when the reservation leaves no usable room — an
      honest "no room here" for a heavily-constrained small chunk, not a
      crash or a forced overlap.

      **Verification, not just trust**: re-ran the same standalone sweep
      after the fix — 0 overlapping pairs across all 4 adjacent
      combinations × 200 seeds (was 100%); a broader sweep (4 chunk
      sizes × 500 seeds × all 15 non-empty exit-mask combinations = 30,000
      chunk configurations) found 0 overlaps and 0 out-of-bounds parcels.
      New `ParcelTest.AdjacentActiveRowsNeverProduceOverlappingParcels`
      makes this a permanent, automated regression guard (same sweep
      structure, scaled down to 30 seeds × 4 chunk sizes × 15 masks to
      keep the suite fast) rather than a one-off manual check.
      **1580/1580 tests pass** (1579 + 1 new; zero pre-existing tests
      needed changes — the fix is a pure no-op for every scenario any
      existing test already covered). `apps/mesh-world-app`/
      `MeshWorldExport` rebuilt clean. End-to-end re-verified via
      `MeshWorldExport --validate`: 0 errors/0 warnings across all 400
      chunks; hand-inspected the composed output for the real world's own
      4 corner cells (`(2,16)`, `(16,2)`, `(18,2)`, `(18,16)`) and
      confirmed by direct coordinate comparison that no two houses'
      footprints intersect in any of them.
- [ ] R114 (workstream R6) Deterministic vertical-slice city showcase: one
      3×3/4×4-chunk area, one main street + intersection, sidewalks/curbs,
      a pedestrian crossing, traffic lights, 15-30 buildings from several
      families, parked/moving vehicles, trees/street furniture, one
      landmark, coherent materials, standard pedestrian+overview cameras,
      multiple LOD levels — the required before/after benchmark artifact
      (§19), built on R107's baseline and R113's composer.
      **2026-07-13 — scoped and v1 implemented** (user picked "scope a
      new R114+ workstream" after every R113-adjacent item needed a
      design call/cross-repo buy-in; then approved starting with v1).

      **Investigated first, before writing anything** (fresh, concrete
      findings, not assumed): road/intersection/sidewalks/curbs/
      pedestrian-crossing/traffic-lights ALL already exist and work,
      via the pre-existing `lua.zone.road`/`lua.zone.crossroad`
      generators (crosswalk stripes + 4-corner traffic-light rigs
      confirmed real, from the R121 Lua-port work) — a much smaller gap
      than the spec's length suggested. 4 real house families exist and
      are composer-integrated. Parked vehicles, street furniture, and a
      wide real tree-species catalogue exist. `MaterialRegistry` is real
      (R106, just closed). Real gaps found: NO moving-vehicle/traffic-
      simulation system anywhere; NO landmark concept; only a first-
      person pedestrian camera exists (`apps/mesh-world-app`'s
      `FPCamera`) — no overview/aerial mode; LOD (`WorldRenderer`'s
      `placement_lod_visible_distance_m`/`compute_visible_placement_
      instances`) only applies to MAP11 point-cloud placements, never to
      `BuildingComposer`-placed chunk instances (`Mc3AssetMetadata.lods`
      is authored by `build_mc3lib_content.cpp` but never read anywhere
      at runtime). `docs/audit-baseline.md` (R107) explicitly documents
      this sandboxed environment has no GPU — the "before" reference was
      a code-path trace, not a screenshot, and no screenshot tooling
      exists.

      Staged the remaining work like R113's own v1→v2→v3, agreed with
      the user: **v1** assembles the showcase from existing
      infrastructure only (config + small, bounded generator additions),
      producing a real externally-viewable artifact despite the no-GPU
      constraint; a landmark asset, LOD wiring, and an overview camera
      are later, separately-scoped v2/v3+ items; a full moving-vehicle
      traffic simulation is explicitly OUT of scope (a much bigger,
      separate subsystem, matching §19's own "prioritize visible results
      over new unrelated infrastructure").

      **v1**: new `examples/city_showcase.json` — a real, deterministic
      3×3-chunk area (seed 100), a "+"-shaped road through the middle
      row/column (one real 4-way crossroad at the center), leaving the 4
      corner cells as `small_house_block` — each of which borders the
      road on 2 ADJACENT sides, directly exercising this same session's
      own corner-aware-layout fix on real, intentional content (not just
      a synthetic test sweep). `MeshWorldExport --validate`: 0 errors/0
      warnings across all 9 chunks; 18 houses (within the 15-30 target),
      18 vehicles, 18 streetlamps/mailboxes.

      **Real, bounded addition to `BuildingComposer.cpp`**: one real yard
      tree per parcel (a small fixed candidate list — `tree_oak`/`tree_
      lime`/`tree_birch`/`tree_apple`, ObjectDefinitionLibrary's own
      pre-existing species — referenced directly by id + a `has()`
      check, since these carry no `Mc3AssetMetadata` and so aren't
      `AssetRegistry`-queryable like every other category), placed in
      the back yard (opposite the street, along `-normal`) so it can't
      collide with the house's own facade or the street-side lamp/
      mailbox/vehicle placements. This is the one deliberate scope
      exception to "v1 is config-only" — approved as "a couple small
      generator tweaks if needed" — and closes a real, previously
      unnoticed gap: composer-driven residential blocks had NO trees at
      all, unlike the legacy (non-composer) chain's own per-house garden
      dressing.

      **Real, significant bug found and fixed while producing the
      actual visual artifact, not left as a known limitation**:
      converting the showcase to `.glb` via `MeshWorldGLB` (the only
      viable "deterministic visual artifact" path in a GPU-less sandbox)
      showed EVERY composer-placed instance (houses, vehicles, trees,
      street furniture — anything placed via `w.instance()`) as "unknown
      definition", silently exported as nothing. Root cause: instance-
      reference resolution (walking every `<instance ref="...">` and
      injecting a real `ObjectDefinitionLibrary` entry) had only ever
      existed as `WorldRenderer.cpp`'s own private, renderer-only
      `inject_definitions()` — batch export tools have no live
      `ChunkPipeline`/`WorldRenderer` involved at all, so every instance
      ref was left dangling; only literal box/plane primitives (roads,
      crosswalks, traffic lights) exported correctly. Fixed by
      extracting this into a real, shared, public function,
      `resolve_instance_definitions()` (`ObjectDefinitionLibrary.hpp`/
      `.cpp`), used by both `WorldRenderer.cpp` (replacing its own
      duplicate) and the new `MeshWorldGLB` call site — plus two real
      improvements over the original renderer-only version: (1) a
      fixed-point loop, since injecting a definition (e.g. a compiled
      modular house) can itself introduce NEW instance refs of its own
      (its compiled-in window/door/roof children) that also need
      resolving — the original single-pass version silently missed
      these even in the live renderer, an actual latent bug that simply
      never surfaced there because nobody had looked; (2) a qualified-
      id fallback (strip a leading `"<alias>:"` before retrying), since
      a compiled modular building's own children reference imports by
      their qualified compose-time string (e.g. `"windows:window.
      residential.double.classic_01"`), while `ObjectDefinitionLibrary`
      only ever registers the bare id. **Second bug found and fixed in
      the same pass**: `MeshWorldGLB`'s own material-injection scanned
      for referenced materials BEFORE instance resolution, so it never
      saw materials used only inside a newly-injected definition
      (every vehicle/tree/furniture material) — reordered to resolve
      instances first, then scan for materials.

      Verified by direct, repeated `MeshWorldGLB --stats` runs, not
      assumed: before the fix, every composer instance was "unknown
      definition" (301 objects processed, no real vehicle/tree/house
      geometry); after both fixes, 0 "unknown definition"/"unknown
      material" warnings, only 9 pre-existing, unrelated "ambient light
      has no glTF equivalent" notices (one per chunk, a real, harmless
      mc3togltf limitation) — 1016 objects processed, 116 real materials,
      14656 vertices, 6988 triangles. 5 new tests (4 in
      `ObjectDefinitionLibraryTests.cpp` covering bare-id resolution,
      unknown-id no-op, alias-stripping fallback, and fixed-point
      recursion into a newly-injected definition; 1 in
      `BuildingComposerTests.cpp` proving one real tree per parcel).
      **1585/1585 tests pass** (1580 + 5 new). `apps/mesh-world-app`
      rebuilt clean (touches `WorldRenderer.cpp`, a live-render file).
      The resulting 9 `.glb` files (one per chunk, `examples/
      city_showcase.json`'s own 3×3 grid) were sent directly to the user
      for real external visual inspection (Blender/any glTF viewer) --
      this project's own "visually inspected" requirement (§19) can't be
      satisfied inside this GPU-less sandbox, so the artifact itself was
      handed off rather than a self-verification claimed here.

      **Still open** (separately scoped, not this pass): a landmark
      asset (v2), LOD tier selection wired into composer-placed
      instances (v3), an overview/aerial camera mode, a full moving-
      vehicle traffic simulation (explicitly out of scope, much bigger).

      **2026-07-18 cross-reference (docs-only correction, no history
      rewritten above)**: v2 (landmark asset) and v3 (LOD tier selection
      wired into composer-placed instances) were both closed by R128
      ("City showcase completion (R114 v2/v3)"), and `docs/
      migration-stages.md`'s Stage 4 table was itself corrected the same
      day from a stale "not started" to "done" now that R113/R126/R127/
      R129 cover all 4 common region types. This checkbox stays `[ ]`
      because R114's own remaining bullets — an overview/aerial camera
      and a full moving-vehicle traffic simulation — are still genuinely
      unmet, separately-scoped future work, not because the showcase
      itself is incomplete.
- [ ] R115 (workstream R7) Offline-only AI content-factory pipeline (request
      queue → cheap model first → strict `mc3.json` output → JSON Schema
      validation → `Mc3Document` parse → reference/bounds validation → MCB
      compile → preview render → automated QA → accept/repair-request/
      escalate-to-stronger-model), content-hash-keyed caching (§9.5), and a
      SQLite content store (`model_request`/`model_recipe`/
      `model_definition`/`model_variant`/`model_dependency`/
      `validation_result`/`render_preview`/`generation_attempt`/
      `model_tag`/`material_slot`/`lod_mapping`, §9.6) — explicitly a
      development-time content tool, never a Mesh World runtime dependency
      (consistent with this file's existing "no Claude API at runtime"
      rule and T246-T255's BYOK scoping) (§9).
- [ ] R116 (workstream R8) Nature asset libraries (trees, shrubs, rocks,
      forest floor, stumps, fallen logs) + biome-driven instanced placement
      by slope/altitude/moisture/canopy density, replacing today's
      hardcoded per-biome C++ generators as the content source, plus GPU
      instancing/impostor-billboard support for dense vegetation counts
      (§12, §18).
- [ ] R117 (workstream R9) Cliff/rock-outcrop/ledge/boulder/scree/snow-cap
      module library layered onto existing procedural mountain terrain
      (module + material layering + biome vegetation + LOD, not whole-
      mountain prefabs), with explicit seam/slope rules to avoid visible
      repetition (§13).
- [ ] R118 (workstream R10) Cave/mine module library (straight/curved/
      ascending/descending tunnels, T/four-way junctions, chambers, halls,
      shafts, pits, stalactites/stalagmites/crystals, mine supports/rails/
      torches) + a semantic cave-graph generator realized via these
      modules, with connection-socket, tunnel-alignment, traversability,
      vertical-transition, and clearance validation (§14, §21.3).
- [ ] R119 (workstream R11) `.mc3pack` package format (libraries + textures
      + metadata + previews, §8) + streaming/cache-invalidation/asset-
      versioning for compiled MCB/`mc3pack` runtime output, building on
      R101/R110's dependency-pruned standalone compilation (§7, §11).
- [ ] R120 (workstream R12, cross-repo CNA/NOXNA) PBR material pipeline
      (base color/normal/metallic-roughness/AO/emissive maps, extends the
      R106 material catalogue), shadow system (cascaded shadow maps,
      contact shadows) + SSAO/GTAO, and backend capability tiers (Tier 0
      compatibility/Tier 1 portable/Tier 2 advanced) so the same world
      degrades gracefully across SDL/GLES/Vulkan/bgfx/WebGPU (§16.1-16.4,
      §17) — implemented primarily in CNA/NOXNA/MeshCraft, consumed from
      MeshWorld only through the MC3 material/asset schema.
- [x] R121 (workstream R13) Formal migration-stage tracking mirroring §20's
      Stage 1-7 (audit/baseline → shared `mc3.json`+import foundations →
      global libraries → composed city showcase → demote runtime Lua →
      nature/underground expansion → modern rendering), plus documented
      archival of each Lua generator once its C++/MC3-asset equivalent
      exists and covers it (extends R105's map-generator conversion and
      this file's existing T219-T235 Lua→C++ precedent) (§20, §4.6).
      Implemented as `docs/migration-stages.md` (same living-document
      pattern as `docs/risk-register.md`, R123): §20's 7 stages each
      mapped to the concrete R-task(s) that implement their own bullet
      points, with a status (Stage 1 substantially done modulo the
      human-only visual baseline; Stage 2 done except R109's own
      deferred JSON Schema; Stages 3/4/6/7 not started; Stage 5 in
      progress at the map layer only). §2 tracks Lua-generator archival
      readiness per §4.6: **15 of the 17 map-level scripts are now
      archival-ready** (13 from R105's own audit + `district.lua`/
      `city.lua`, closed this same session by R105's
      `DistrictGenerator`/`CityGenerator`) — explicitly NOT a
      recommendation to delete them now (RR4 caution: that's a real,
      separate, later Stage 5 decision, not automatic). `level14.lua`'s
      zoning half and `neighborhood.lua`'s street-naming half remain
      genuinely uncovered. The zone/chunk-level Lua generators
      (`generators/lua/zone|object|building/*.lua`) are explicitly
      flagged as NOT audited (R105's own scope was map-layer only;
      whether any zone/chunk script is similarly redundant with its
      existing C++ counterpart is a real open question, not assumed) —
      a good candidate follow-up audit, not silently skipped. Docs-only,
      no code/test changes.
- [ ] R122 Extend `MC3Validator`/export tooling with the remaining §21
      checks not covered by R101/R106/R111/R118 above: material-slot and
      texture reference validity, finite transform values, no cyclic
      library dependencies, valid LOD mappings (§21.2); building-road
      intersection, parcel containment, cross-chunk road continuity,
      terrain penetration limits, out-of-bounds detection (§21.3); and
      exported triangle/instance/material/draw-call/shadow-caster/LOD-
      reduction performance metrics (§21.4, overlaps T238's `--validate`
      flag in the Tooling/CLI backlog).
      **First slice done 2026-07-12 (§21.2 "finite transform values"
      only — the rest of §21.2/21.3/21.4 remain open):**
      `rotation`/`scale` attributes on placed objects/definitions
      previously had ZERO validation in `MC3Validator.cpp` (not even the
      malformed-syntax check `position` always got) — now get the same
      `parse_vec3()` check. Investigated adding a separate
      `std::isfinite()` check for a value that parses successfully but is
      NaN/Inf; empirically verified (a standalone test program) that
      libstdc++'s `istringstream >> float` already REJECTS "nan"/"inf"/an
      out-of-float-range literal as an extraction failure (not a
      successful parse producing a non-finite value) — so that scenario
      is already caught by the existing malformed-value check, and a
      separate finite check would be unreachable dead code; not added,
      per this project's own "don't validate scenarios that can't happen"
      rule. 4 new tests in `tests/MC3ValidatorTests.cpp`. 1472/1472 tests
      pass (4 more than R105's 1468); `apps/mesh-world-app` build verified
      cleanly. **Still fully open**: valid material slots/texture
      reference validity, valid LOD mappings, all of §21.3 (spatial:
      building-road intersection, parcel containment, socket alignment,
      cave connection alignment, furniture clearance, terrain
      penetration, cross-chunk road continuity), and all of §21.4
      (performance metrics export). "No cyclic library dependencies"
      (§21.2) does not apply to `MC3Validator`'s own domain (generated
      chunk XML has no `<imports>`) — already covered where it DOES apply,
      by R101's `Mc3ImportResolver` cycle detection in `mesh-craft`.
      **Second slice done 2026-07-18 (R130, this checkbox stays `[ ]`
      since real scope remains open):** R130 closed the rest of §21.2
      (material-slot/texture-reference validity, LOD-mapping validity)
      and the single-chunk-computable subset of §21.3/§21.4 (terrain
      penetration, instance/triangle/draw-call metrics) — see R130's own
      entry below for the full write-up. **Still genuinely open, deferred
      per the user's own Round-2 scope decision, not silently dropped:**
      building-road intersection, parcel containment, cross-chunk road
      continuity, and socket/cave-connection alignment (all §21.3 — would
      need a real multi-chunk/neighbor-context interface change) and
      shadow-caster counts/LOD-reduction-ratio metrics (§21.4 — need
      `assetMetadata` fields not meaningfully computable from a single
      exported chunk document).
- [x] R123 Living risk register + definition-of-success tracking checklist
      for the whole R-series, seeded from §23 ("Major Risks" — scope creep,
      asset-quality inconsistency, performance regressions from AI-authored
      content, Lua-removal breaking existing worlds/saves, tooling/CI
      complexity) and §25-26 ("Definition of Success" / "Final Direction"):
      turn each risk into an owner + mitigation + trigger-to-revisit, and
      each success criterion (e.g. city showcase quality bar, no runtime
      AI dependency, save-compatibility during migration) into a checkable
      condition reviewed at every R107-R121 milestone, instead of leaving
      them as prose-only guidance. Implemented as `docs/risk-register.md`
      (7-row risk register RR1-RR7 + 16-item Definition-of-Success
      checklist DS1-DS16); owners are still `unassigned` and checklist
      items are still unchecked — to be filled in as R107-R121 milestones
      close, per the document's own review cadence.
- [x] R124 Fix the 13 `MapPipelineTest` failures R107's own audit flagged
      (`docs/audit-baseline.md` §3) as a follow-up, and correct that
      section's diagnosis. R107's blanket claim ("all 13 fail the same
      way" — a generator-id string drift) was itself only half right: 10
      of the 13 were exactly that (MAP18 gave almost every map level a
      real, ambient Lua generator, invalidating older tests' "no
      level-specific script is registered" premise — fixed by updating
      expected ids and, where a test's whole point is that absence, adding
      a new `ScopedLuaMapGeneratorRemoved` RAII helper (and hardening
      `ScopedLuaMapGenerator` itself to save/restore rather than blindly
      erase) in `tests/MapPipelineTests.cpp` so the fallback mechanism can
      still be tested regardless of how much real content the repo has
      accumulated). The other 3
      (`SiblingSharedEdgeOrderIndependent[DeepLevel]`,
      `ConcurrentGenerationPreservesSiblingBoundaryConsistency`) were a
      **real, previously undiscovered production bug**, not test drift —
      found by actually reading each failing assertion instead of trusting
      the audit's summary. `MapBuilder::applyParentEdgeConstraints()`
      (`src/MapBuilder.cpp`) selects which HALF of a parent tile's N/S or
      E/W edge array applies to a child using the wrong axis variable (the
      N/S row half used `cy`, which had already picked N-vs-S and carries
      no information about the X sub-range; the E/W column half used `cx`
      the same way) — a copy-paste-shaped bug that only manifests for the
      2 of 4 quadrant combinations where `cx != cy`, which is why ad hoc
      visual QA and half the automated coverage never caught it. Effect:
      every such quadrant's outer-facing edge (row 0/H-1 for N/S, column
      0/W-1 for E/W) got constrained against the WRONG half of its own
      parent's boundary — a real, visible terrain seam at roughly half of
      all tile-tile boundaries throughout the whole map, at every level
      that funnels through `setBiomeField()` (i.e. every generator, C++ or
      Lua). Fixed by swapping the two half-selector variables. **1446/1446
      tests pass** (4 more than R107's 1442 baseline — R108's own
      diagnostics work added tests without updating that count; not
      investigated further, not this task's concern). Root-caused via a
      temporary debug print (removed before commit) proving both sibling
      tiles used the identical real Lua generator id, ruling out a
      Lua-vs-C++-fallback split before looking at `MapBuilder.cpp` itself.
      Also found and worked around, not fixed: two of the newly-"fixed"
      boundary tests kept failing after the production fix landed for an
      unrelated reason — they reuse a FIXED `tmp_dir()` path across runs
      and `PlanetWorld::create_new()` doesn't wipe pre-existing on-disk
      state, so repeated manual test invocations during this session's own
      debugging had left stale, pre-fix persisted tiles on disk; clearing
      `/tmp/meshworld_pipe_test_*` resolved it. Not changed: `tmp_dir()`
      itself, since production code never re-uses a fixed path this way
      (only this test file's own scratch convention does) — flagged here,
      not worth a dedicated task line.
- [x] R125 (workstream R3, §8/§13, `docs/migration-stages.md` Stage 3)
      Curate a bounded, composer-relevant slice of the `object.sqlite3`
      brainstorm database (`object.md`: 7108 object rows, 7337 containment
      edges, built via multi-agent generation waves) into the real,
      git-tracked `data/taxonomy/taxonomy.json` / `data/taxonomy/
      containment.json` — scoped to house/facade/street-furniture/
      vehicle-adjacent categories relevant to R112/R126/R127's current
      composer work, not the whole database (excludes unrelated domains
      like clothing/factory equipment). Append-only: all pre-existing
      entries left byte-for-byte unchanged; curated additions deduplicated
      against them first (confirmed via a one-off script: every existing
      taxonomy id already exists verbatim in `object.sqlite3`, so dedup by
      id is exact, not fuzzy). Investigated `object.sqlite3` directly
      (`sqlite3` CLI + a Python one-off, not guessed): found a genuinely
      useful seam — dozens of real architectural style variants (window/
      door/roof types: bay/casement/gothic-pointed windows, French-double/
      gothic-arched doors, gambrel/saltbox/butterfly roofs, etc.) that a
      prior repair pass in the brainstorm DB had already re-attached under
      generic `architecture.window`/`architecture.door`/`architecture.roof`/
      `architecture.chimney` placeholder parents (`notes: "repair:orphan-
      reattach"`); re-parented these onto the *existing* `object.window`/
      `object.door` taxonomy nodes instead of importing a whole new
      `architecture` kind (added `object.roof` and `object.chimney` as two
      small new parent nodes to keep the same convention, since neither
      existed yet). Also hand-picked 8 real street-furniture items (bike
      rack, bus stop shelter/sign, ornate streetlamp, outdoor trash
      receptacle, etc.) and 8 real vehicle-adjacent items (cargo van,
      3 motorcycle variants, cargo bicycle, scooter, motorcycle parking
      rack, parking-space wheel bumper) out of ~145 substring-matched
      candidates, rejecting workshop tools/office supplies/security-tech
      gadgets/decorative interior items that were false-positive keyword
      matches (e.g. "bench_vise", "card_catalog_cabinet", "signet_ring")
      and collapsing near-duplicate variant spam (multiple near-identical
      "trash receptacle" rows) down to one or two representative entries —
      exactly the drift `object.md` itself warned about. 3 containment
      edges were re-targeted from brainstorm-only parent categories not in
      scope (`room.break_room`, `place.rest_stop`, `zone.road`) onto the
      closest existing real taxonomy parent (`zone.shop_street`,
      `place.parking_lot`, `path.road` respectively) instead of pulling in
      unrelated new parent nodes. **Result:** 99 taxonomy nodes (was 64),
      80 containment rules (was 56), all real probability/min/max/lod_max
      values sourced from the brainstorm DB's own `cancontain` rows, not
      invented. `TaxonomyRegistry`/`ContainmentRuleRegistry` needed no code
      changes (data-only). 4 new regression tests in
      `tests/TaxonomyTests.cpp`: duplicate-id, duplicate-edge, and orphan-
      reference checks that parse both JSON files directly (independent of
      the registry classes' own behavior), plus a spot-check that the
      curated additions actually load and resolve through
      `TaxonomyRegistry`/`ContainmentRuleRegistry`. All 10
      `TaxonomyTests.*` pass. Full suite: 1589 tests total, 1585 pass
      consistently; the remaining 4 (`ChunkPipelineMapContextTest.
      IsCityStaysFalseWithoutAnyCityLuaRegistered`, `MapPipelineTest.
      RealCityLuaAppliesOnlyAtLevel12`, `WorldRendererTests.
      WithMapLayerLoadsChunksWithoutCrashing`, and one of
      `PlanetMapLogicTest.DescendPlanetToChunkColdVsCachedStaysWithinBudget`/
      `MapPipelineTest.CacheHitOnParentProtectsItFromEviction`) were
      re-verified across 3 consecutive full-suite runs to fail as a
      *different, varying* subset each time with no code change in
      between — the signature of pre-existing test-execution-order/timing
      flakiness this file already documents elsewhere (R106's material-
      registration-order gaps, R124's own note on
      `WithMapLayerLoadsChunksWithoutCrashing`), not a regression from this
      task (which touches zero production C++ code — only append-only
      JSON data, docs, and new, isolated tests in a file no other test
      shares state with).
- [x] R126 (workstream R4, §20 Stage 4, `docs/world-composer-design.md`
      v2) Extended `BuildingComposer` to `RegionType::apartment_block`.
      Added one new C++-authored asset, `apartment.block.wide_01`
      (`ObjectDefinitionLibrary.cpp`, following `make_house_gable()`'s own
      box-composition convention exactly): a detached 3-story, flat-roofed
      building (14.6m x 9.6m footprint vs the house's 10.6m x 8.6m), 3
      floors x 3 window columns on the front facade, an off-center street
      entrance (unlike the house's centered door). Registered it in
      `ComposerAssets.cpp` under a new `"apartment"` category, hand-
      registered exactly like `house_gable_default` (not through
      `Mc3ImportResolver` — a single directly-authored definition, not a
      resolved library import). `Parcel.cpp`: `place_row()` now takes
      `parcel_depth`/`kind` as parameters instead of hardcoding
      `kParcelDepth`/`RegionType::small_house_block`, so the same corner-
      aware, exits-gated row-placement algorithm serves both region types
      without duplication; added `kApartmentParcelWidth`/
      `kApartmentParcelDepth` (14.6m/12.0m, one size class — v1 has only
      one apartment asset, so no width-class roll like the house's
      standard/wide split). `derive_parcels()` now branches on
      `small_house_block`/`apartment_block`, every other region still
      returns empty. `BuildingComposer.cpp`: queries `"apartment"` instead
      of `"house"` for that region, and — a real, deliberate asymmetry,
      not an oversight — omits `StyleProfile::roofFamily` from the
      required tags for apartments, since the shipped
      `central_europe_default` profile's `"gable_roof"` tag would
      otherwise make a real, flat-roofed apartment building never match
      (same precedent the street-furniture/prop queries already use).
      `GenerationMetadata`'s `sourceGeneratorOrHash` is now
      `"cpp.chunk.composer." + to_string(ctx.region)` (was a hardcoded
      `"...small_house_block"` literal) so composer output correctly
      self-identifies by region for both paths, not just the original one.
      6 new tests: `ParcelTest.DerivesParcelsForApartmentBlock`/
      `ApartmentBlockParcelsStayWithinChunkBounds` (parcel-shape/bounds),
      `BuildingComposerFixture.RegistryHasApartmentBlockAfterPopulation`/
      `ComposesRealInstancesForApartmentBlock` (real asset selection +
      correct instance count matching `derive_parcels()`'s own output),
      `BuildingComposerUnitTest.
      ApartmentBlockReturnsNulloptWhenNoApartmentAssetRegistered`
      (graceful fallback when the registry has only a house asset, no
      apartment one), and
      `ChunkPipelineComposerIntegration.
      EnabledFlagRoutesThroughComposerForApartmentBlock` (full pipeline
      routing + `diag.generator_id` check). `ParcelTest.
      ReturnsEmptyForOtherRegions` updated (apartment_block moved out of
      the "other regions" list into its own dedicated test; replaced with
      `shop_street`/`square`). All 65 `BuildingComposer*`/`Parcel*`/
      `Taxonomy*`/`*Composer*` tests pass; full suite 1592/1595 pass (the
      3 failures are the same pre-existing, run-order-dependent flaky
      tests R125 already documented — re-confirmed unrelated, since this
      task only touches `Parcel.cpp`/`BuildingComposer.cpp`/
      `ObjectDefinitionLibrary.cpp`/`ComposerAssets.cpp`, none of which
      those 3 flaky tests exercise). Verified via
      `MeshWorldExport --validate --force` against a real 3x3
      `apartment_block` demo world with `use_world_composer: true`: **0
      chunk(s) with errors, 0 chunk(s) with warnings (of 9 total)**,
      output XML directly confirmed to contain real
      `definition="apartment.block.wide_01"` instances (not just a
      passing validator).
- [x] R127 (workstream R4, §20 Stage 4, `docs/world-composer-design.md`
      v2) Extended `BuildingComposer` to `RegionType::shop_street`.
      **Deviation from the original task write-up, found during real
      investigation, not guessed:** `data/mc3lib/urban-facades-1.0.0
      .mc3lib.json` has NO shopfront-tagged facade modules at all
      (checked directly — every one of its 4 facade_module definitions
      carries only `styleTags: ["central_europe"]`, all
      `residential_bay_*` subcategories); the only real shopfront-tagged
      R112 content is `urban-windows-1.0.0.mc3lib.json`'s single
      `window.shopfront.large.urban_01` (category `"window"`, styleTags
      `["modern","urban"]`, nominalSize 2.4x2.0m) — and attaching that
      socket-based window asset to a building at compose time needs R104
      (MeshCraft `<script>` execution), the same explicitly out-of-scope
      cross-repo gap R125/R126 already documented. Resolved the same way
      R126 resolved its own missing-apartment-asset gap: added one new
      C++-authored asset, `shop.building.storefront_01`
      (`ObjectDefinitionLibrary.cpp`, mirrors `make_apartment_block()`'s
      box-composition convention) — a narrow (w=8.0m vs the house's
      10.0m), 2-story, flat-roofed mixed-use building: a tall
      (4.5m) ground-floor shop with a large box-composed glazed
      shopfront (5.4m x 2.0m, deliberately sized to echo
      `window.shopfront.large.urban_01`'s real 2.4x2.0m proportions
      instead of leaving it a silent, undocumented downgrade) plus an
      awning (reusing the exact `"awning_stripe"` material and a matching
      height precedent from `ShopStreetGenerator.cpp`'s own awning
      planes), and one plain residential/office floor above (the common
      Central-European mixed-use pattern). Carries BOTH `"central_europe"`
      (so it still matches `BuildingComposer`'s shared facadeFamily-tag
      query, same as house/apartment) and `"modern"` (the honest style of
      the shopfront glazing it echoes) style tags. Registered under a new
      `"shop"` category in `ComposerAssets.cpp`, hand-registered exactly
      like `house_gable_default`/`apartment.block.wide_01` (not through
      `Mc3ImportResolver`). `Parcel.cpp`: added `kShopParcelWidth`/
      `kShopParcelDepth` (8.6m/9.0m, one narrow size class, no width-class
      roll — same v1 precedent as apartment_block); `derive_parcels()`
      now branches on all 3 region types via a 3-way `row_depth`/
      `width_for()` selection, every other region still returns empty.
      `BuildingComposer.cpp`: queries `"shop"` category for
      `shop_street`, omits `StyleProfile::roofFamily` for both apartment
      AND shop (both are real flat-roofed buildings — the condition is
      now `!is_apartment && !is_shop`), uses a `"cobblestone"` ground
      surface instead of the residential `"grass_garden"`
      (cross-checked against `ShopStreetGenerator.cpp`'s own
      `w.ground("cobblestone")` call — no visual regression on this
      axis), and a `"shop_"` instance-name prefix. `GenerationMetadata`'s
      region-derived `sourceGeneratorOrHash` (already genericized in
      R126) needed no further change. 8 new tests: `ParcelTest.
      DerivesParcelsForShopStreet`/`ShopStreetParcelsStayWithinChunkBounds`,
      `BuildingComposerFixture.RegistryHasShopBuildingAfterPopulation`/
      `ComposesRealInstancesForShopStreet` (asset selection + correct
      instance count + cobblestone-ground assertion),
      `BuildingComposerUnitTest.ShopStreetReturnsNulloptWhenNoShopAssetRegistered`
      (graceful fallback), and `ChunkPipelineComposerIntegration.
      EnabledFlagRoutesThroughComposerForShopStreet` (full pipeline
      routing + `diag.generator_id` check). All 71
      `BuildingComposer*`/`Parcel*`/`Taxonomy*`/`*Composer*` tests pass;
      full suite 1598/1601 pass (the same 3 pre-existing, run-order-
      dependent flaky tests R125/R126 already documented — re-confirmed
      unrelated, since this task only touches
      `Parcel.cpp`/`BuildingComposer.cpp`/`ObjectDefinitionLibrary.cpp`/
      `ComposerAssets.cpp`, none of which those 3 flaky tests exercise).
      Verified via `MeshWorldExport --validate --force` against a real
      3x3 `shop_street` demo world with `use_world_composer: true`: **0
      chunk(s) with errors, 0 chunk(s) with warnings (of 9 total)**,
      output XML directly confirmed to contain real
      `definition="shop.building.storefront_01"` instances AND
      `material="cobblestone"` ground (not just a passing validator).
- [x] R128 (workstream R4, §20 Stage 4, §21.4) City showcase completion
      (R114 v2/v3). **Landmark:** added a new one-off
      `landmark.clocktower_01` definition (`ObjectDefinitionLibrary.cpp`,
      following `house.gable.wide_01`'s precedent of a genuinely new
      asset, but using this file's simpler box/cone-composition
      convention rather than the `<script>`-driven socket variant, since a
      landmark has no window/door/roof-module content to attach) — a
      granite-plinth, plastered-shaft, 4-faced clock, tiled pyramidal
      spire tower. New `WorldConfig::LandmarkPlacement`/`landmarks`
      (parsed in `WorldConfig.cpp`, default empty = no behavior change for
      existing worlds) lets a world config target ONE landmark instance
      at a SPECIFIC chunk coordinate + local (x, z, rotation_y); new
      `ChunkContext::landmark`/`LandmarkInstance` (resolved per-chunk by
      `ChunkPipeline::build_context()`) carries it down to
      `BuildingComposer::compose_chunk()`, which places it unconditionally
      (independent of the parcel loop and its category/width-matching
      query entirely) when `ctx.landmark.definition_id` is non-empty.
      `examples/city_showcase.json` now sets `use_world_composer: true`
      (was missing entirely — the showcase wasn't actually demonstrating
      composer output before this task) and one `landmarks` entry placing
      the clocktower at chunk (0,0)'s center. **LOD wiring:** added
      `BuildingComposer.cpp`'s own `resolve_instance_id()` helper —
      `ctx.lod` (long documented in `docs/lua-generators.md` as an
      existing convention but, per `ChunkGenerator.hpp`'s own prior doc
      comment, never actually backed by a real field or consulted by
      anything) now resolves every composer-placed instance (house/
      apartment/shop AND streetlamp/mailbox/vehicle) to its own authored
      `Mc3AssetMetadata.lods["low"]` proxy id once `ctx.lod` drops to/below
      a threshold, falling back to the full-detail id otherwise (the
      overwhelming default, since only a handful of R112 mc3lib assets,
      e.g. `streetlamp.classic_01`, currently carry a real `"low"` lods
      entry — this task wires the DATA that already existed, it doesn't
      author new low-LOD content). A real distance-based LOD *picker*
      that actually sets `ctx.lod` from live camera distance remains
      future work — this task only makes the field and the data get
      consulted once some future caller sets it. 12 new tests:
      `WorldConfig.DefaultLandmarksAreEmpty`/`LandmarksLoadFromJson`/
      `CityShowcaseHasOneLandmark` (config parsing + the real showcase
      file), `BuildingComposerFixture.
      RegistryHasLandmarkClocktowerAfterPopulation`/
      `ComposesLandmarkInstanceWhenConfigured`/
      `DoesNotPlaceLandmarkWhenNotConfigured` (landmark placement +
      non-placement), and `BuildingComposerFixture.
      FarLodResolvesStreetlampToItsLowLodDefinition`/
      `NearLodUsesFullDetailStreetlamp` (LOD wiring, both directions). All
      81 `BuildingComposer*`/`Parcel*`/`Taxonomy*`/`WorldConfig.*Landmark*`
      tests pass; full suite 1606/1609 pass (the same 3 pre-existing,
      run-order-dependent flaky tests R125/R126/R127 already documented —
      re-confirmed unrelated, since this task only touches
      `WorldConfig.hpp/cpp`, `ChunkGenerator.hpp`, `ChunkPipeline.cpp`,
      `BuildingComposer.cpp`, `ObjectDefinitionLibrary.cpp`,
      `ComposerAssets.cpp`, and `examples/city_showcase.json`, none of
      which those 3 flaky tests exercise). Verified via
      `MeshWorldExport --validate --force` against the real,
      now-composer-enabled `examples/city_showcase.json`: **0 chunk(s)
      with errors, 0 chunk(s) with warnings (of 9 total)**, output XML
      directly confirmed to contain the real
      `id="landmark" position="32 0 32" definition="landmark.clocktower_01"`
      instance in chunk (0,0), alongside real composer-placed houses in
      every other chunk. `docs/risk-register.md`'s DS2/DS4 rows annotated
      (not ticked -- both explicitly require an actual screenshot/video
      review this session's text-only verification cannot substitute for)
      with the wider region-type coverage (R126/R127) and this task's
      landmark/LOD additions, honestly left `[ ]` pending that review.
- [x] R129 (workstream R4, §20 Stage 4) `BuildingComposer` v3:
      `RegionType::square`, the last of the 4 common region types on
      the legacy, non-composer path (`RegionType::square` still fell
      through to `SquareGenerator.cpp` unconditionally after R113/R126/
      R127 — confirmed directly in `BuildingComposer.cpp`, which only
      branched on `apartment_block`/`shop_street`/implicitly
      `small_house_block` via `derive_parcels()`). **Design (user-
      approved, hybrid, not a full asset-driven rewrite):**
      `SquareGenerator.cpp`'s literal fountain/plinth/water-bowl/jet/
      cardinal-paths/conditional-monument geometry is ported UNCHANGED
      into a new `compose_square()` path in `BuildingComposer.cpp` (a
      plaza centerpiece is one-of-a-kind civic content, same reasoning
      R128's landmark already used) — deliberately does NOT call
      `derive_parcels()` at all (`Parcel.cpp`/`Parcel.hpp`: **no
      change** — `derive_parcels()` still returns `{}` for `square`,
      confirmed by `ParcelTest.ReturnsEmptyForOtherRegions`), since a
      square is one whole-chunk composition, not a row of parcels. The
      4 corner lamps, however, are now a real
      `AssetRegistry::query("street_furniture", ...)` pick — the exact
      same category/mechanism `compose_chunk()` already uses for
      house/apartment/shop-block lamps — replacing the legacy
      generator's own hardcoded `w.cylinder(..., "metal_lamp_ornate")`
      raw-primitive calls, so style profiles finally affect squares
      too. Benches (`"bench_stone"`) and corner trees (`kYardTreeIds`)
      stay direct-by-id references, mirroring the existing yard-tree
      convention (no metadata-tagged bench asset exists yet, and
      inventing one was explicitly out of scope). `compose_chunk()`
      gained one early branch: `if (ctx.region == RegionType::square)
      return compose_square(ctx);`, before the `derive_parcels()`
      call every other region still goes through. Returns
      `std::nullopt` (never a partial/broken plaza) when no
      `street_furniture` asset is registered at all, so `ChunkPipeline`
      falls through to the untouched, legacy `SquareGenerator.cpp` —
      same nullopt-means-fall-through contract as every other region;
      no `.lua`/`.cpp` generator deleted (RR4). 8 new tests:
      `BuildingComposerFixture.ComposesRealInstancesForSquare`
      (fountain/lamps/benches/trees/paths all present, confirms
      `"metal_lamp_ornate"` no longer appears at all)/
      `SquareAddsMonumentWhenNearNamedSettlement`,
      `BuildingComposerUnitTest.
      SquareReturnsNulloptWhenNoStreetFurnitureAssetRegistered`
      (graceful fallback, same discipline as R126/R127's own
      apartment/shop unit tests), and `ChunkPipelineComposerIntegration.
      EnabledFlagRoutesThroughComposerForSquare` (full pipeline routing
      + `diag.generator_id == "cpp.chunk.composer.square"` check, no
      `EdgeExits` needed unlike the parcel-based regions). All 76
      `BuildingComposer*`/`Parcel*`/`ChunkPipelineComposerIntegration*`
      tests pass; full suite 1617/1619 pass (the same pre-existing,
      run-order-dependent flaky tests already documented in R125's
      entry — re-confirmed unrelated, since this task only touches
      `BuildingComposer.cpp`/`BuildingComposer.hpp`, neither of which
      those flaky tests exercise). Verified via `MeshWorldExport
      --validate --force` against a real 3x3 `square` demo world with
      `use_world_composer: true`: **0 chunk(s) with errors, 0 chunk(s)
      with warnings (of 9 total)**, output XML directly confirmed to
      contain `id="cpp.chunk.composer.square"` as the generator, real
      `definition="streetlamp.classic_01"`/`streetlamp.classic_02"`
      lamp instances, and zero occurrences of the legacy
      `metal_lamp_ornate` material. **This closes Stage 4
      ("C++ world composer selects assets") completely** — all 4 common
      region types are now composer-covered; `docs/migration-stages.md`
      updated accordingly.
      **Same session, folded into existing entries (not a new
      R-number, per this task's own scoping decision):** root-caused
      and fixed the `"zone"` metadata bug `NEXT.md` §4 had documented as
      "confirmed, not yet root-caused" — `ChunkPipeline::build_context()`
      was unconditionally overwriting `ctx.zone` (correct, flat
      `WorldConfig`-derived) with the map layer's own sampled biome
      ordinal whenever a `MapPipeline` was attached (true for every
      `MeshWorldExport` run). Fixed with a new
      `ChunkContext::authored_zone` field (captured before the
      override; `ctx.zone` itself untouched, still drives real
      generator-routing decisions) that `GenerationMetadata::
      from_chunk_context()` and the 4 Lua zone generators
      (`crossroad`/`forest`/`park`/`road.lua`, each via a new
      `ctx.authored_zone` Lua-exposed field) now report instead. 6 new
      tests (`GenerationMetadataTests.*`, `ChunkPipelineMapContextTest.
      AuthoredZone*`); verified against the exact repro `NEXT.md` §7
      documents — `examples/city_showcase.json` now reports `"city"`
      for every chunk, was `"deep_ocean"`. Also corrected
      `docs/migration-stages.md`'s Stage 4 table (previously stale,
      still said "not started" despite R113/R126/R127/R128 already
      shipping) and added a cross-reference note to this file's own
      R114 entry (not a history rewrite) confirming R128 already closed
      its v2/v3 landmark/LOD gaps, and documented the "ambient light
      has no glTF equivalent" `MeshWorldGLB` warning as
      expected/ignorable with a code comment in
      `src/tools/export_glb.cpp`.
- [x] R130 (closes R122's remaining §21.2/§21.3/§21.4 scope, bounded to a
      single-chunk-computable subset per the user's own Round-2 scope
      decision) `MC3Validator` extensions (R130a+R130b together).
      **R130a (§21.2 remainder):** `MC3Validator.cpp`'s `walk()` gained a
      new `check_materials_and_textures()` pass — every non-empty
      `base_color_texture`/`normal_texture`/`metallic_roughness_texture`/
      `occlusion_texture`/`emissive_texture` on a `<material>` must match a
      declared `<texture id="...">` in the SAME document (mirrors
      `Mc3DocumentBuilder::add_material()`'s own existing self-consistency,
      now enforced as a regression guard against hand-authored/imported
      content, not just mesh-world's own generated output) — plus a new
      `<definition><assetMetadata>` check: `materialSlots` entries must be
      non-empty/non-duplicate, and `lods` entries need a non-empty tier +
      definition id and (when that id resolves to a definition present in
      the SAME document) must not self-reference the owning definition —
      an id resolving to an externally-injected library is deliberately
      NOT flagged, mirroring the existing `<instance definition="...">`
      precedent that already skips resolution for library-external
      targets. **R130b (§21.3 subset + §21.4):** `check_object()`'s
      existing x/z bounds check gained a `y >= -0.05f` terrain-penetration
      check, with an explicit `material=="water"` exemption
      (`RiverBankGenerator`/`BridgeGenerator` intentionally recess water
      planes below y=0 — a real generator convention, not a bug to flag).
      `ValidationResult` (`include/ValidationResult.hpp`) gains 3 new
      fields: `instance_count` (a `<instance>`-only count, split out of
      the existing generic `object_count`), `triangle_count` (reuses the
      EXACT SAME `Mc3MeshBuilder{}.build(xml).total_triangles()` call
      `ChunkPipeline::get()` already makes at `src/ChunkPipeline.cpp:306`,
      so any caller holding only a `ValidationResult` gets it without a
      second mesh-build pass), and `draw_call_estimate` (defined
      conservatively as `== object_count`, documented in-code as a
      placeholder pending R120's GPU-driven batching work, not a real
      batching-aware estimate). `src/tools/validate_mc3.cpp`
      (`MeshWorldValidate`) prints all 3 new fields in its per-file
      output. **Deliberately out of scope this task** (per the user's own
      Round-2 decision — see `docs/migration-stages.md` and this file's
      own R122 entry, which stays open for exactly these items): genuine
      cross-chunk spatial checks (building-road intersection, cross-chunk
      road continuity — would need a new multi-chunk/neighbor-context
      parameter threading through `MC3Validator`/`ChunkPipeline`/
      `MeshWorldExport`/`MeshWorldValidate`, a real interface change, not
      a small addition); socket-alignment/cave-connection-alignment checks
      (no chunk in the codebase today actually composes non-trivial-socket
      content — R112's socket-based window/door/roof/facade-module assets
      remain "registered but unplaced", blocked on the out-of-scope R104
      script engine); and shadow-caster counts/LOD-reduction-ratio metrics
      (need `assetMetadata` fields that live on externally-injected
      library definitions, not meaningfully computable from a single
      exported chunk document's own `<instance>` placements). 15 new tests
      in `tests/MC3ValidatorTests.cpp`
      (`DanglingTextureReferenceFails`/`SelfConsistentMaterialAndTexturePasses`/
      `MultipleDanglingTextureRefTagsAllReported`/`EmptyMaterialSlotEntryFails`/
      `DuplicateMaterialSlotEntryFails`/`ValidMaterialSlotsPass`/
      `SelfReferencingLodEntryFails`/`LodEntryMissingTierOrDefinitionFails`/
      `LodEntryResolvingWithinSameDocumentPasses`/
      `LodEntryReferencingExternalDefinitionIsNotAnErrorHere`/
      `ObjectPenetratingTerrainFails`/
      `ObjectSlightlyBelowZeroWithinEpsilonPasses`/
      `WaterMaterialBelowGroundIsNotTerrainPenetration`/
      `InstanceCountCountsOnlyInstanceElements`/
      `TriangleCountAndDrawCallEstimateArePopulated`). All 34
      `MC3ValidatorTests.*` pass; full suite rebuilt clean and re-run this
      session: 1632/1634 pass (the 2 failing are the same pre-existing,
      run-order-dependent flaky tests already documented in R125's entry —
      `ChunkPipelineMapContextTest.IsCityStaysFalseWithoutAnyCityLuaRegistered`/
      `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12` — re-confirmed
      unrelated, since this task only touches `MC3Validator.cpp`/
      `ValidationResult.hpp`/`validate_mc3.cpp`, none of which those flaky
      tests exercise). Verified via `MeshWorldExport --validate --force`
      against `examples/city_showcase.json`: **0 chunk(s) with errors, 0
      chunk(s) with warnings (of 9 total)**. **Per explicit user
      instruction, R131 (JSON Schema for `mc3.json`, closes Stage 2) and
      R132 (dependency pruning for standalone-compiled assets, closes
      `docs/risk-register.md`'s DS7) are deliberately NOT started this
      session** — both stay listed next in `NEXT.md` §8, embargoed pending
      a future session/explicit approval (see `NEXT.md` §9).
- [x] R114 follow-up (2026-07-24, user-approved visual-showcase scope):
      replaced the old house-only 3×3 `examples/city_showcase.json` with a
      deterministic 7×7 city centered on a crossroad. The app-visible
      radius-3 area now contains real composer-generated
      `small_house_block`, `apartment_block`, `shop_street`, and `square`
      chunks, plus the existing clocktower landmark, roads/crossroad, and
      the composer’s lamps/mailboxes/vehicles/trees. Added
      `WorldConfig.CityShowcaseMixesAllComposerRegionsAroundItsCrossroads`
      to pin the configuration and its streaming-radius contract. Also
      root-caused why the app had not displayed the registered MC3 composer
      models at all: it loaded `ObjectDefinitionLibrary` but never called
      `register_composer_assets()`, leaving `AssetRegistry` empty and
      causing the opt-in composer path to fall through silently. The app
      now registers those assets at startup and offers an explicit
      **Explore City Showcase** menu action; it keeps the independent
      planetary map UI but intentionally does not let its map candidates
      override this curated scene’s authored regions. Focused configuration
      and composer routing tests pass; `MeshWorldExport --validate --force
      --threads 4 examples/city_showcase.json` reports 0 errors/0 warnings
      across all 49 chunks, and direct XML inspection confirms each
      composer generator id plus house/apartment/shop/square/landmark
      instances. `MeshWorldGLB --stats` converted all 49 documents
      successfully (5,410 objects / 106,494 vertices / 47,596 triangles).
      That run exposed the composer apartment's missing `plaster_beige`
      registration; added it to `BuiltinMaterials` with a regression test,
      eliminating all unregistered-material GLB warnings. The remaining 49
      warnings are the expected one-per-chunk ambient-light conversion
      notice. Full `MeshWorldTests` coverage finished at **1634/1635**;
      the only failure was the pre-existing, run-order-dependent
      `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12`, unrelated to this
      configuration/app/material scope. Actual screenshot/video review
      initially remained `needs_human` because the app configuration stalled
      on a first-run dependency. **Follow-up 2026-07-25:** configuration was
      completed using already-built local FetchContent sources plus the
      pinned ImGui download; `MeshWorldApp` built successfully with
      `CCACHE_DISABLE=1 cmake --build ... --parallel 4` (the sandbox's shared
      ccache directory is read-only). A real Xvfb/Mesa GLES 3.2 visual run
      then exposed a serious app-only bug: `PersistentWorldMap` overwrote
      the hand-authored showcase regions with procedural forest/ocean data,
      and `Model3DStreamer` drew unrelated planetary placements above it.
      The curated path now isolates those systems, uses a fresh
      `r114_mixed_chunks` cache, and keeps `WorldRenderer::update()` outside
      the persistent-map guard. The final screenshot visibly contains the
      curated road, building, prop, vehicle, and lamp scene; its 29 loaded
      cache files contain the expected house/apartment/shop/square composer
      IDs. Artistic quality review on a normal GPU/display remains a later
      `needs_human` sign-off, but basic runtime visual execution is now
      verified. **Follow-up 2026-07-25 (ordinary-app adoption):**
      `start_explore()` now explicitly enables `use_world_composer`, so a
      normal app-created world uses the same registered reusable MC3 assets
      wherever the composer has coverage. This intentionally does not change
      `WorldConfig`'s false default, existing library/tool behavior, cached
      chunks, or fallback generation for uncovered region types. The first
      fresh-world smoke run showed that the legacy `PersistentWorldMap`
      supplied no roads, so road-frontage-aware composer regions still
      correctly fell through. Fixed the actual app path instead of claiming
      the flag alone was sufficient: new cells share 4×4 biome planning
      blocks, new city blocks deterministically contain roads/crossroads plus
      house/apartment/shop/square parcels, and `WorldMap::set_info()`
      propagates late road exits without erasing explicit configured exits.
      Existing SQLite rows remain untouched. The app now picks the nearest
      composer-ready parcel in the already-preloaded radius (centre is the
      fallback). Focused persistence/composer/config tests: **18/18 passed**;
      a fresh Xvfb app run cached composer apartment/house/square documents
      and rendered the city scene. The full suite stayed **1634/1635**, with
      only the documented unrelated map-layer flaky failure.
- [x] R131 (2026-07-25, user-approved `mc3.json` schema scope): added
      `schemas/mc3.schema.json`, the strict, MIT-marked Draft 2020-12
      interchange contract deferred from R109. It covers MeshCraft's
      semantic writer shape rather than a raw XML mirror: mandatory
      format/version/model/unit/coordinate-system fields; imports and
      libraries; environment/media/material records; definitions and
      recursive objects; transforms, primitives, extrusion, actions, and
      metadata-tagged assets. Closed records use `additionalProperties:
      false`; maps intentionally designed for dynamic identifiers
      (metadata, sockets, LODs, states) remain open but constrain their
      values. `tests/MC3ValidatorTests.cpp` adds
      `MC3JsonSchemaTests.SchemaAcceptsAllTrackedMc3LibrariesAndRejectsMalformedDocuments`:
      it validates every tracked `data/mc3lib/*.mc3lib.json`, then rejects
      an unknown top-level field, a non-`mc3` format, and a two-component
      transform vector. The test contains a deliberately small evaluator
      for exactly the standard schema keywords used here, so MeshWorld gains
      no production JSON-Schema dependency; real authors/AI tooling should
      run a complete Draft 2020-12 implementation. During validation, real
      street-furniture data established that `primitive.segments: 0` is the
      writer's valid "use default tessellation" sentinel, so the schema
      accepts non-negative primitive segment counts while retaining the
      parser's 4096 safety ceiling. Verified JSON syntax with
      `python3 -m json.tool schemas/mc3.schema.json`; compiled the changed
      existing test translation unit with `-j4`; and ran
      `./build/MeshWorldTests --gtest_filter='MC3JsonSchemaTests.*:MC3ValidatorTests.*'`:
      **36/36 passed**. Final full regression from the project root:
      **1636/1637 passed**, with only the documented, run-order-dependent
      `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12` failure. R131 closes
      Stage 2; R132 remains unstarted and explicitly requires separate user
      approval.
- [x] R132 (2026-07-25, user-approved dependency-pruning scope): added
      `Mc3DependencyPruner` to `MeshWorldLib` and the
      `MeshWorldPruneMc3Lib` CLI. Given an already-resolved mc3lib document
      and one or more root definition IDs, the pruner retains only the
      transitive definition graph reached through ordinary instance,
      variant, and `assetMetadata.lods` references; it independently clones
      the retained object tree, preserves scene basics, and copies only
      document-owned materials plus their bitmap/SVG textures. Library,
      import, and include identity are intentionally cleared, producing an
      ordinary MC3 document that can be saved as XML/JSON or compiled to MCB
      (`--mcb`). Missing definition references fail rather than producing a
      falsely standalone result; material names not declared in the source
      document remain valid external material-registry references, matching
      existing MeshWorld content. The tool loads XML/JSON libraries, resolves
      direct imports from the input directory, and reports retained counts.
      Runtime-script assets are deliberately rejected even when the script
      itself exists: dynamic `def:place()` dependencies cannot be inferred
      safely without running/parsing the separately out-of-scope R104 script
      engine. This prevents the dangerous alternative of silently omitting a
      modular building's window/door/roof dependencies. Two regression tests
      in `ObjectDefinitionLibraryTests.cpp` cover transitive local and
      qualified-variant references, LODs, materials, bitmap/SVG textures,
      deep-copy isolation, empty/missing roots, and runtime-script rejection.
      Focused test: **2/2 passed**. Real CLI smoke compiled static
      `streetlamp.classic_01` from `urban-street-furniture` to a 2-definition
      MCB at `/tmp/r132-lamp.mcb`; `house.gable.modular_01` was correctly
      rejected with the R104 explanation. Standard CMake reconfiguration was
      attempted with `--parallel 4` but is blocked in this sandbox by the
      pre-existing NanoSVG DNS fetch; changed sources were instead compiled
      with the existing strict CMake flags and linked against existing Mc3/Mcb
      artifacts. Final root regression: **1638/1639 passed**, the sole
      failure remaining the documented, run-order-dependent
      `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12`. R132 closes
      `docs/risk-register.md` DS7 for static assets; R104 remains untouched.

## Post-showcase world-coherence roadmap (user-requested, 2026-07-25)

These tasks respond to the observed explorer defects: passing through
instanced props, visually weak parked cars, dead-ending roads, and facade-only
houses. They follow `mesh_world_revival.md` §6/§11/§15/§21.3's ordering:
make the current exterior world spatially coherent first, then add one bounded
enterable interior. They deliberately do **not** introduce a general physics
engine, moving traffic, or R104 script execution.

- [x] R133 (2026-07-25, workstream R5/R6 follow-up): resolved collision
      proxies for rendered MC3 instances without introducing a renderer into
      `MeshWorldLib`. New core-only `Mc3Collision` extracts legacy inline
      `collision="box"` geometry and transformed bounds from resolved
      `Instance` definitions. The explicit policy is `none`/empty = passable,
      `box` = AABB blocker, and any future value = non-fatal structured
      diagnostic; missing definitions and inverted bounds are also diagnosed.
      It handles qualified aliases, uniform/non-uniform scale, root transforms
      and all Euler axes by enclosing all eight transformed bounds corners.
      `WorldRenderer` now only supplies loaded-chunk offsets, while the app
      calls a new core-tested capsule-vs-AABB X/Z slide resolver. To prevent
      the old R112 blanket metadata from making every bench, lamp, mailbox,
      window, roof or facade impassable, the MC3-library generator now emits
      `collisionProxy="none"` by default and explicitly retains `box` for its
      four vehicle assets; the six affected tracked libraries were regenerated
      through `MeshWorldBuildMc3Lib`. New `Mc3CollisionTests` cover legacy
      inline boxes, transformed aliases, `none`, unsupported diagnostics, the
      generated lamp/vehicle metadata contract, vehicle blocking, wall
      sliding, and vertical filtering: **8/8 passed**. The app main and
      renderer translation units compile with their strict app flags. Export
      validation of all 49 `city_showcase` chunks reports **0 errors / 0
      warnings**. The complete root suite, manually linked against the new
      objects because CMake reconfiguration cannot fetch NanoSVG in this
      sandbox, finishes **1646/1647 passed**; only the pre-existing flaky
      `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12` fails. A fresh live
      Xvfb walk-through is `needs_environment`: CMake cannot reconfigure the
      app until its absent NanoSVG dependency is available. MeshCraft was not
      modified.

- [x] R134 (2026-07-25, workstream R5/R6 follow-up): roads now have a
      canonical, physically symmetric edge graph. `WorldMap` keeps authored
      `ChunkInfo::exits` separate from derived data, rebuilds each interior
      edge exactly once, exposes `road_connections()` for road/crossroad
      geometry and `road_frontage()` for parcels, and no longer retains a
      stale OR-ed connection when persistent data replaces a road cell.
      `road_termini` in `WorldConfig` makes intentional `boundary` and
      interior `terminus` ends explicit; `MeshWorldExport --validate` reports
      invalid topology before writing a misleading city. Both shipped demo
      worlds name their legal endings. Road/crossroad generators now emit only
      the backed arms rather than extending strips to every tile boundary.
      The coarse level-15 planet-map crossing signal is retained only as
      `MapContext::has_road_crossing`: it cannot prove a matching 64 m
      neighbour, so R134 deliberately stops it from fabricating a one-sided
      local exit. Existing SQLite `set_info()` rows remain conservative: their
      authored exits are preserved while derived exits are recomputed.
      Tests cover symmetric 7×7 edges, stale persistent replacement,
      unapproved dead ends, single-arm road/crossroad geometry, and the
      map-crossing safety case. Focused coverage: **40/40 passed**;
      `MeshWorldExport --validate --force --threads 4` reports 0 errors/0
      warnings for both `city_showcase.json` (49 chunks) and `world.json`
      (400 chunks). Full regression: **1642/1643 passed**; the sole failure
      remains the unrelated `MapPipelineTest.RealCityLuaAppliesOnlyAtLevel12`.
      A fresh app walk-through remains blocked by NanoSVG's unavailable DNS
      fetch during CMake reconfigure; MeshCraft was not modified.

- [x] R138 (2026-07-25, road showcase adoption and signal-state follow-up):
      made the canonical R134 road graph visibly authoritative in the actual
      Lua-first explorer path. `ChunkPipeline` no longer lets an incidental
      planet-map `ZoneCandidate` replace an explicitly configured or
      persisted `road`/`crossroad` layout; map-derived layout remains the
      default only for a world with no authored zones. Both Lua zone scripts
      now render a centre patch and exactly their declared edge arms, matching
      the C++ fallback instead of extending a road through absent neighbours.
      The app uses versioned, non-destructive chunk-cache directories
      (`r134_road_graph`) so the showcase regenerates rather than displaying
      stale pre-R134 MC3 files. Traffic heads now use large MC3 spheres and
      deterministic red/amber/green state: every approach has exactly one
      bright lens and two dim lenses. This is intentionally a static
      deterministic snapshot, not fake live animation; the explorer does not
      yet advance MC3 object states/actions. Added dim lens materials and
      regression coverage for explicit-layout authority, Lua's no-phantom-arm
      rule, C++ signal states, and the R105 native district fallback that had
      left one stale test expectation. `MeshWorldExport --validate --force
      --threads 4 examples/city_showcase.json` generated all 49 chunks with
      0 errors/0 warnings and its central MC3 has four canonical arms plus
      correct bright/dim signal lenses. Debug `MeshWorldApp` builds with
      `--parallel 4`; full regression is **1645/1645 passed**. MeshCraft was
      not modified.

- [ ] R135 (workstream R4/R6 follow-up) Replace placeholder parked vehicles
      with a bounded coherent vehicle kit and road-aware placement. **Problem:**
      cars are static primitive-composed props with weak silhouette/detail and
      can appear unrelated to usable road frontage. **Deliverable:** three
      style-tagged static assets (compact hatchback, family sedan, delivery
      van) with real wheel/body/window/light hierarchy, authored bounds,
      `collisionProxy="box"`, near/low LODs, and road-parallel parking
      placement that respects curb/parcel clearance and avoids buildings.
      This is intentionally **not** traffic, pathfinding, animation, or NPC
      work; vehicles remain parked scenery. **Dependencies:** R133 supplies
      the collision-policy consumer; R134 supplies stable curb/road edges.
      **Verification:** asset metadata/LOD and placement-clearance tests,
      MC3 validation and MCB-pruning of each asset, triangle/object budget
      report, and before/after standardized showcase screenshots.

- [ ] R136 (workstream R6/G10 v1) Deliver one genuinely enterable house
      interior vertical slice. **Problem:** houses are exterior facades with
      no doorway aperture, interior shell, or interaction/collision contract.
      **Deliverable:** implement the already-scoped G10 v1 only: one selected
      showcase house receives a segmented front wall (left pier + lintel +
      right pier, never fake CSG), an open-door portal, a single room shell,
      basic existing furniture/light assets, and an explicit entrance marker.
      Collision must block the wall but permit the doorway; entering and
      leaving must stream/render correctly without a separate level load.
      Do not start generic multi-room procedural interiors, containment-rule
      selection, loot/gameplay, or a universal building-interior system in
      this task. **Dependencies:** R133; R135/R134 are not required.
      **Verification:** geometry/AABB tests proving the aperture is passable
      and neighbouring wall is not, generated MC3 validation, a focused app
      movement test, and an Xvfb/manual record walking street → doorway → room
      → street.

- [ ] R137 (map-road follow-up, discovered in R134) Materialise planet-map
      roads through their own canonical local edge graph. Level-15 crossings
      are approximately 688 m wide and are useful nearby-road hints, but are
      not proof of a shared 64 m chunk boundary. Preserve R134's safe no-stub
      policy until this task defines an exact downscaling/ownership rule,
      validates both neighbouring local chunks, and proves continuity with
      deterministic map-to-chunk tests. Do not restore the old OR-in logic.

- [ ] R139 (persistent-city topology, needs_human design decision): make a
      newly created persistent world choose city roads from a coherent local
      block graph rather than the current simple grid/random region selector,
      then decide how existing SQLite world layouts migrate. R138 safely stops
      cached or Lua-rendered phantom arms, but it deliberately does not erase
      or rewrite a user's already persisted `regions` rows. Before changing
      those rows, specify whether existing worlds should (a) retain their
      present layout, (b) get an opt-in migration/new-city action, or (c) be
      regenerated automatically. Deliverable after that choice: topology
      ownership/terminus rules, a versioned persistence marker, a safe
      migration or explicit new-world path, symmetry/no-unexpected-terminus
      tests, and a visual city walk-through. **Do not silently mutate existing
      SQLite worlds.**

- [x] R140 (2026-07-25, explorer frame-time containment): removed the three
      confirmed avoidable costs behind the CPU-heavy, rubbery explorer without
      changing MeshCraft. The interactive app now keeps a radius-2 disk (at
      most 13 chunks) rather than radius 3 (at most 29); `WorldRenderer`
      conservatively culls a whole resident chunk before filesystem parsing or
      `SceneRenderer` traversal when its bounding circle is wholly behind,
      outside the horizontal FOV, or past the far plane; and parsed MC3
      documents are evicted once their chunks leave the resident set, bounding
      a long walk's CPU-side cache. `Model3DStreamer::update()` now coalesces
      to a chunk/altitude-band query extent, so the app's per-frame call does
      not re-run the same SQLite proximity query while the player remains in
      one extent. This deliberately leaves per-placement distance filtering
      per-frame, where it is cheap and necessary. CNA already runs its fixed
      60 Hz game timestep, so no redundant sleep/FPS limiter was added.
      New tests prove behind/far chunk rejection and no repeated placement
      recompute within an extent; streamer/visibility coverage is **15/15**,
      Debug `MeshWorldApp` builds with `--parallel 4`, and full regression is
      **1648/1648 passed**. **Validation still needed:** a real hardware
      profiler capture comparing frame time and CPU before/after.

- [ ] R141 (explorer rendering follow-up): profile a representative ordinary
      city on target hardware, publish object/draw-call/frame-time counters,
      then add object-level frustum culling and batching/instancing only where
      the profile proves they dominate. R140 culls whole chunks, but a visible
      chunk still submits all of its own MC3 objects because object-level
      culling/batching belongs in MeshCraft's `SceneRenderer`; do not modify
      that sibling repository without explicit approval. Also consider a
      user-facing quality preset (radius 1/2/3) only after measurements show
      its quality/performance trade-off.

- [x] R142 (2026-07-25, natural-world entry and safe biome-family fallback):
      added the app-visible `examples/biome_showcase.json` and an **Explore
      Biome Showcase** menu action. It is a deterministic 9×9 curated tour
      with a forest spawn and immediately adjacent jungle, mountain, tundra,
      meadow, desert, beach, swamp, and ocean zones; its separate
      `r142_biome_showcase_chunks` cache keeps it isolated from ordinary or
      city-showcase saves. The shared authored-showcase startup path preserves
      the deliberate rule that planet-map candidates and persistent model
      placements may not overwrite curated content. The `ChunkGenerator`
      dispatch no longer sends the 38 MAP16 biome values to `EmptyGenerator`:
      each maps to the closest established generator family (meadow, forest,
      jungle, swamp, desert, tundra, mountain, ocean, or beach). This is an
      explicit compatibility fallback, **not** a claim that these are bespoke
      Stage-6 biome assets. While validating the real showcase, fixed actual
      legacy natural-content defects: beach water no longer penetrates terrain
      and driftwood stays inside the chunk; forest litter reserves its plane
      footprint inside the chunk; all materials used by the displayed natural
      families are registered. Added regression tests for the authored nine
      zones, non-empty dispatch of every MAP16 family, and beach/forest bounds
      across 64 seeds. `MeshWorldExport --validate --force --threads 4
      examples/biome_showcase.json` reports **0 errors / 0 warnings for all
      81 chunks**; Debug `MeshWorldApp` builds with `--parallel 4`; full
      regression is **1651/1651 passed**. MeshCraft was not modified.

- [ ] R143 (Stage 6, nature asset-library vertical slice): replace R142's
      family fallbacks for the first visibly distinct set — temperate forest,
      jungle, desert, alpine/mountain, coast, and wetland — with reusable,
      metadata-tagged MC3 definitions and C++ composition/placement rules.
      Each family needs varied silhouettes (not repeated primitive trees),
      authored bounds/LOD/collision policy, deterministic density and clearings,
      and an explicit material/triangle/object budget. Keep Lua untouched;
      new content belongs in C++ assets/composers. Verify library validation,
      deterministic generation, no out-of-bounds placements, and before/after
      standardized screenshots of the R142 route.
      **R143a completed 2026-07-25:** the first reusable MC3 kits now cover
      temperate forest (broad-canopy oak / layered pine), jungle (buttressed
      canopy / bamboo grove), desert (barrel cactus / agave), mountain
      (spire / outcrop), wetland (cypress), and coast (driftwood). Each is a
      C++-authored `nature.*` definition with `Mc3AssetMetadata`, bounds,
      collision policy, selection tags, and an authored low-LOD proxy; they
      are registered in the same `AssetRegistry` as urban composer assets.
      Forest/jungle/desert/mountain/swamp/beach generators now select the
      appropriate tagged kit deterministically, retaining their old direct
      IDs as an intentionally safe fallback for isolated callers that have
      not initialized the registry. Focused metadata/selection plus 64-seed
      bounds tests pass, as does 81-chunk biome export validation (0 errors/
      0 warnings); full regression is **1652/1652 passed**.
      **R143b completed 2026-07-25:** forest clearings are now real,
      deterministic canopy exclusion zones rather than a grass plane beneath
      the former uniform scatter. A seed-isolated layout stream fixes the
      clearing independently of foliage draw order; all 32 primary trees and
      both five-tree clusters reject its radius plus a 1.5 m canopy margin,
      with bounded deterministic retries and a safe fallback. The regression
      test parses and checks all 42 authored tree positions; the 81-chunk
      showcase export remains 0 errors/0 warnings and full regression is
      **1653/1653 passed**. **Still required before closing R143:** richer
      density/masks for the other families, explicit object and triangle
      budgets, and standardized target-hardware screenshots.
      **R143c completed 2026-07-25:** the former singleton wetland/coast
      slices now have a second tagged silhouette each — a leafless rooted
      swamp snag alongside cypress, and a branched shore root alongside
      driftwood. The same deterministic registry selection now varies
      otherwise repeated swamp trees and shoreline props without Lua changes;
      regression asserts at least two registered assets in both families.

- [ ] R144 (planet-to-ground coupling): make a nearby map tile's elevation,
      slope, water/coast distance, and river data change the actual chunk
      terrain mesh and placement masks, not only generator decoration. Define
      a continuous shared-edge height contract before implementation so
      adjacent chunks cannot crack, and preserve flat deterministic showcase
      configs unless they opt in. Verify neighbouring height continuity,
      water/terrain collision, biome-boundary blending, and a walk from plain
      through mountain to coast. This is the missing bridge between the
      planet-map's rich classification and a visually credible 3D world.

- [ ] R145 (visual-quality review and iCity comparison): capture a fixed
      city and R142 biome route on target hardware, publish screenshots plus
      object/draw-call/frame-time counters, and compare equivalent scenes with
      iCity **once a local path or revision is supplied**. The current
      repository has no iCity source/assets to inspect, so do not invent a
      comparison. Use the evidence to prioritize R143 asset variety, R144
      terrain, or R141 renderer work rather than guessing.

- [x] R146 (interactive cache write bound, completed 2026-07-25): ordinary
      exploration previously retained every generated chunk XML on disk with
      no limit, so a long walk could grow `saves/<world>/chunks/` indefinitely.
      `ChunkCache` now supports an opt-in oldest-entry cap and skips a physical
      rewrite when forced deterministic regeneration produces identical XML.
      The interactive app sets a 256-chunk working-set cap (well above its
      13 resident chunks); generic caches and export tools retain their
      unlimited complete-output behavior. Existing one-argument constructors
      remain as binary-compatible overloads. Focused cache/biome tests, Debug
      app build, 81-chunk biome export (0 errors/0 warnings), and the full
      suite are **1654/1654 passed** using CPU 0–3. Existing saves and build
      directories were deliberately not deleted.

---

## Explicitly not planned

- Billing, Stripe, credits, virtual currency
- Multiplayer or server-authoritative gameplay
- Marketplace for chunks or assets
- Free public AI generation funded by operator
- Characters, NPCs, dialogue
- Full collision or physics engine
- Mobile or web build
- Runtime AI calls without explicit BYOK configuration by the user
