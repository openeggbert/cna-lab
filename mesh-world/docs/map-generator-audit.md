# Map Generator Audit — 2026-07-10

Status snapshot of MeshWorld's planetary map generation subsystem, written to seed
follow-up planning (new milestones/tasks). Not a design document (see `map.md` for
that) and not a user guide (see `docs/map-generation.md` for the CLI flag reference) —
this is an assessment: what exists, what it can actually do today, what's weak or
missing, and what improving it would look like.

Scope: the **planetary map subsystem** (`namespace MeshWorld::Map`, `include/Map/` +
`src/Map/`, plan.md's M-series). The legacy flat chunk/city system it sits above is
only discussed where the two interact.

---

## 1. Executive summary

**The map generator is fully implemented and functionally complete.** Every milestone
in `plan.md`'s M-series (M001–M234, MAP0 through MAP15) is done, including MAP11's
placement-rendering integration, closed out 2026-07-10. 863/863 automated tests pass;
the build is clean under `-Wall -Wextra -Werror`.

**Yes, it generates PNG maps** — biome-colored, elevation-shaded, single-tile or
multi-tile stitched, plus vector (GeoJSON) and tile-package (MBTiles) export. Details
in §4.

That said, "complete" describes the M-series task list, not "nothing left to improve."
§6 lays out real, specific gaps: most are about **breadth and richness** (how many of
the ~20 chunk generators and how many map levels actually *use* what the map layer
produces) rather than missing infrastructure. §7 turns those into candidate next steps.

---

## 2. Architecture, in brief

```
new world (time-seeded entropy, never a fixed seed)
  → MapPipeline.get(level 0): Lua planet.lua first → C++ PlanetGenerator fallback
    → validate → persist (map_level0.db)
  → player descends/zooms: MapPipeline.get(tile) at deeper levels, same
    Lua-first/C++-fallback/validate/persist pattern, parent-constrained
    (child boundaries must match parent edges — the "constraint propagation"
    invariant, tested at every level and under real concurrency)
  → hand-off level (z=18, ~86 m tiles) → ChunkContext.map_context
    → legacy ChunkPipeline / chunk generators → MC3 3D scene
       (+ optional ModelPlacements → Model3DStreamer → WorldRenderer)
```

- **Quadtree LOD pyramid**, `TileCoord{level, x, y}`, levels 0–18, factor-2 subdivision
  (slippy-map/XYZ scheme). Planet is a flat 22,585 km square (~Earth's surface area).
- **Non-reproducible by design**: entropy is `steady_clock`-derived at world creation,
  never a fixed seed. Consistency on revisit comes from SQLite persistence
  (`map_level{N}.db` per level), not from re-deriving the same seed. Regenerating a
  *deleted* tile of an *existing* world is still fully deterministic (same world
  entropy + parent chain) — verified directly (M222).
- **Lua-first, C++-fallback**: every level tries a registered Lua generator
  (`LuaGeneratorRegistry` → sandboxed execution → structural validation) before
  falling back to a C++ generator. Only 7 of the 19 levels have a Lua script (§6.1).
- **MapValidator** structurally checks every generated payload before persistence
  (field shapes, biome ordinal range, feature bounds, edge-length consistency) —
  reusable standalone via `MeshWorldPlanet --validate` against an already-persisted
  world too.
- **Separate from but consumed by the legacy chunk system**: the map layer never
  depends on chunk types; `ChunkContext.map_context` is populated additively at the
  hand-off level and read by chunk generators that opt in (§6.2).
- **3D model placements** (trees, props — MAP11) are a second, independent SQLite
  store, region-sharded and altitude-indexed (`models/<rx>_<rz>.db`), streamed by
  `Model3DStreamer` based on the player's true 3D position (not just the 2D chunk
  radius the legacy `WorldStreamer` uses) and rendered via `WorldRenderer::
  render_placements()` (closed out 2026-07-10, see §6.6 for what's still unverified).

---

## 3. What it can generate, today

Per level, a tile carries: elevation/temperature/moisture fields (64×64 grids),
a biome classification grid (`ZoneType` per cell: city/jungle/desert/forest/ocean/
mountain/tundra/swamp/cave/meadow/beach/empty), vector features (rivers, lakes,
mountain ranges, coastlines, borders, roads, named settlements), and place labels.

- **Terrain**: continents via noise-based coastlines, elevation (fBm detail + uplift),
  climate bands (latitude + elevation lapse rate), moisture, biome classification.
- **Hydrology**: rivers traced downhill to the sea, carved into elevation; lakes.
- **Mountain ranges**: tectonic ridge lines, peak elevation within a configured band.
- **Biome refinement**: coastal beach reclassification, swamp-on-steep-terrain demotion.
- **Political layer**: countries grown from capitals (border tracing biased toward
  rivers/ridges), settlements placed by suitability (water/flat/coastal), a routed
  road network (MST + redundant links, slope-aware, river-bridging).
- **Naming**: 3 cultures (desert/nordic/romance), procedural names for continents,
  countries, cities, towns, rivers, mountain ranges, streets — culture is inherited
  down the tile hierarchy from wherever it was first chosen (level 0).
- **3D object placements** (MAP11): currently only `ForestGenerator` emits these
  (trees) — see §6.5.

---

## 4. PNG / image export — yes, several modes

All via the `MeshWorldPlanet` CLI (`include/PlanetMapLogic.hpp`, vendored
`stb_image_write.h` for encoding — the project's one and only image dependency):

| Flag | Produces |
|---|---|
| `--png out.png` | Single tile: biome-color grid + feature/label overlay (rivers/roads/borders/coastlines as lines, place labels as color-coded markers — no font rendering, so labels are dots/markers, not literal text). |
| `--png-mode hillshade` | Same tile, grayscale directional shaded-relief instead of biome color — driven purely by the elevation field's local slope/normal. |
| `--png-region out.png --region-tiles N` | An N×N block of sibling tiles stitched into one larger image (for levels > 0, where a single tile is only part of a continent/region). |
| `--mbtiles out.mbtiles` | Every already-generated tile at a level packaged into a standard MBTiles-style SQLite file (real spec, TMS row convention) — usable in external map viewers (QGIS, etc.), not just this project's own tools. |
| `--geojson out.geojson` | Vector features/labels as a GeoJSON-style `FeatureCollection` (world-space meters, not lon/lat — a custom format for an in-game planet, not a georeferenced real-world map). |

All verified against real generated worlds with pixel-level checks (Pillow), not just
"doesn't crash." `docs/map-generation.md` has the full worked examples.

**What PNG export does *not* do**: render literal text labels (no font dependency by
design), render 3D placements (trees/props — those are a separate, chunk/render-time
system, not part of the 2D map export), or export at a resolution higher than the
tile's own field resolution × an upscale factor (i.e., it's a data visualization, not
a hand-authored cartographic render).

---

## 5. Test / build status (2026-07-10)

- **863/863 tests pass**, full clean rebuild, `-Wall -Wextra -Werror`, zero warnings
  beyond one pre-existing, unrelated vendored-Lua linker warning.
- `apps/mesh-world-app` (the interactive 3D app, a separate CMake project) also builds
  cleanly end-to-end. **Never visually run** in this environment — no GPU/display here
  (`SDL_VIDEODRIVER=dummy` reaches "no OpenGL context" every time). This is the single
  biggest "unknown" for the whole subsystem: automated tests confirm correctness of
  data/logic, not that the map/placements actually *look* right on screen. A human
  with a real display has not yet confirmed this.
- No CI currently wired up — a `.github/workflows/build.yml` existed untracked but was
  deleted by explicit user decision (2026-07-09); nothing replaced it. Tests only run
  when a human/agent invokes them locally.
- Real machine-performance figures exist (`plan.md`'s MAP14 section, `NEXT.md`
  §5 #26/#28): level-0 generation ~3-6 ms, a full 18-level descent to hand-off
  ~20-200 ms depending on cache state, both with wide safety margins. Profiling found
  hydrology tracing and the per-cell noise/bilinear loop are the two dominant costs in
  child-tile generation (roughly tied, ~90%+ of a call's own cost) — both doing
  necessary work, no found algorithmic bug, no SIMD/parallelism attempted yet.

---

## 6. Known gaps and limitations

Ordered roughly by how much they'd change what a player actually experiences.

### 6.1 Most quadtree levels have no dedicated generator

Only 7 of 19 levels (0, 3, 5, 7, 9, 12, 15 — plan.md's "named bands", `map.md` §5.3)
have a Lua script. Every other level (1, 2, 4, 6, 8, 10, 11, 13, 14, 16, 17, 18) falls
through to the generic C++ `ChildGenerator` — structurally valid (boundary-matching
etc. all still hold) but with no level-specific character (no "this is a district,
this is a block cluster" flavor). This is by design for now (`map.md` explicitly says
"not every level needs its own script"), but it means roughly 12 of 19 zoom steps are
visually/structurally generic refinement rather than deliberate content.

### 6.2 Only 4 of ~20 legacy chunk generators read the map layer

`ForestGenerator`, `OceanGenerator`, `MeadowGenerator`, `MountainGenerator` read
`ChunkContext.map_context` (biome/elevation/roads from the map layer). The other ~16
(city/house/street generators etc.) still only use the older flat `WorldMap` zone
system. This means for most chunk types, the map subsystem's elevation/biome/road data
has **no visible effect** even though it's correctly computed and handed off. This is
the single largest gap between "the map generator works" and "the map generator
visibly shapes the playable world."

### 6.3 MAP8's terrain algorithms are C++-only, not ported to Lua

`Hydrology`, `MountainRanges`, `BiomeRefinement`, `FeatureNaming` exist only as C++
generators. `country.lua`/`metro.lua`/`city.lua`/`region.lua`/`continent.lua` are
deliberately independent, simplified reimplementations (by explicit project rule —
not wrappers around the C++ classes) that don't include rivers, mountain ranges, or
biome refinement passes. So a tile generated via a Lua script (any of the 7 named
levels) gets none of MAP8's terrain detail; only the C++-fallback path does. This is a
real content gap, not just an implementation-tidiness one.

### 6.4 Cave biome exists in name only

`ZoneType::cave` is a valid enum value and appears in biome grids, but there's no
actual cave *placement* — no generator seeds cave systems or 3D geometry for them
(`plan.md` M131, explicitly deferred pending MAP11's placement infrastructure, which
is now built but never used this way).

### 6.5 3D model placements: one data source, unverified visually

`Model3DStreamer`/`WorldRenderer::render_placements()` (just closed out) is fully
wired and unit-tested, but exactly **one** chunk generator (`ForestGenerator`) emits
`ModelPlacement`s today. Everything else that could plausibly place a streamed 3D
object (rocks, ruins, wildlife, urban props) doesn't. And since this environment has
no GPU, the actual on-screen appearance of what *does* get placed has never been seen.

### 6.6 Placeholder/approximate pieces, listed together

These are all deliberate, documented placeholders — not being "hidden," but worth
flagging together as things that would need real work before this looks/feels finished:
- Placeholder textures are 4×4 solid-color PNGs, not real texture art.
- `object_height_m()` (drives placement altitude extent) is a hardcoded per-definition
  lookup table, not a real geometry-derived bounding box — no bounding-box
  infrastructure exists at all.
- `tree.lua` uses a box canopy; the C++ tree definitions use an icosphere — the two
  generation paths render visually different trees for the same concept.
- `NameGenerator::dedupe()`/`blend()` are implemented and tested standalone but never
  called from a real generator (so name collisions/blending between adjacent
  regions aren't actually prevented in practice yet).
- `MC3Writer::sphere()`/`cone()` are unused by any generator.
- Planet size (22,585 km) isn't a power-of-two multiple of the 64 m chunk — the
  map↔chunk overlap at the deepest level is arithmetic, not lattice-aligned (a
  deliberate, accepted trade-off per `map.md` §5.5, not a bug, but worth knowing).

### 6.7 Documentation debt (small, mostly closed this session)

`map.md` (the original design doc) said "not yet implemented" until this session
corrected it (M228). `docs/map-generation.md` didn't mention `--validate`/`--db-sizes`
until this session either. Both are now current. `plan.md`'s own checkboxes for MAP11
M167-174/178-186 are still unticked despite that work being done — cosmetic, but worth
a five-minute sync pass.

---

## 7. Recommendations — candidate next steps

Not a committed plan, just what an "improve this" pass would plausibly contain, grouped
by theme. A human should prioritize/scope these before they become real tasks.

**Biggest leverage, smallest individual scope (do these first):**
1. Wire more of the ~16 map-layer-unaware chunk generators to read `ChunkContext.
   map_context` (§6.2) — each one is an independent, small, testable change (the 4
   that already do it are the template). This is the single highest-leverage
   improvement: it's the difference between "the map exists" and "the map matters."
2. A human visually running `apps/mesh-world-app` at least once (§5) — cheap, and the
   single biggest source of *unknown* risk right now (everything is tested except
   "does it look right").
3. Sync `plan.md`'s M167-174/178-186 checkboxes (§6.7) — trivial, no code.

**Medium scope, real content value:**
4. Port MAP8's terrain algorithms (or a simplified version) into the Lua generators
   (§6.3), or accept the gap explicitly and document *why* Lua tiles skip rivers/
   mountains rather than leaving it as an implicit side effect.
5. Add Lua/C++ generators for more of the 12 currently-generic quadtree levels (§6.1),
   starting with whichever ones are most visually reachable during normal play
   (probably the levels between "City" (12) and hand-off (18), since that's the range
   a player actually walks through).
6. Have more chunk generators emit `ModelPlacement`s (§6.5) — rocks, ruins, wildlife —
   now that the streaming+rendering pipeline actually exists end-to-end.

**Smaller polish items, pick up opportunistically:**
7. Real bounding-box computation to replace `object_height_m()`'s hardcoded table.
8. Reconcile `tree.lua`'s box canopy with the C++ icosphere tree look.
9. Wire `NameGenerator::dedupe()`/`blend()` into a real generator.
10. Cave biome placement (§6.4) — now technically unblocked (MAP11 exists), still needs
    the actual generation logic.

**Infrastructure, lower urgency:**
11. Decide whether to restore CI (a `.github/` workflow was deleted this session by
    request) — without it, nothing catches a regression except a human/agent running
    tests locally.
12. If generation performance ever becomes a real bottleneck (not observed yet — see
    §5's benchmark figures), the profiling groundwork (M226) already identified where
    to look first (hydrology tracing, the per-cell noise loop).

---

*Written 2026-07-10 as a snapshot for planning purposes — treat as a point-in-time
assessment, not a live document. Cross-check against `NEXT.md`/`plan.md` for anything
time-sensitive before acting on it.*
