# map.md — Planetary Map Generation (design & analysis)

_Status: **implemented** (MAP0-MAP15 all complete; MAP16-MAP22 — the 2026-07-10
audit-derived backlog covering biome expansion, full quadtree Lua coverage, real
terrain-refinement Lua bindings, 3D placement expansion, the cave biome, and a
polish/correctness pass — also complete; only MAP23 (docs/infra, this pass included)
and MAP24 remain — see `plan.md`'s M-series and `NEXT.md` for exact, current status).
Originally written 2026-06-29 as a design proposal ("Nothing here changes existing code
yet"); reconciled 2026-07-09 (M228) now that the subsystem it proposed is real, with a
further pass 2026-07-10 (M348) correcting §9/§11/§13 item 2 for what MAP19's real Lua
terrain-refinement bindings and MAP18's full 0-18 quadtree coverage actually built
(both had been written before those milestones existed). The architecture and
decisions below describe what was actually built, not a plan — where a number was
marked "recommended," §13 now records what was actually decided. Sections 1-8/10/12/15
are left as originally written where still accurate; §9/§11 were corrected 2026-07-10
(M348) and the open questions in §13-14 needed correcting from the start._

---

## 0. TL;DR (answers to the open questions)

| Question | Short answer |
|---|---|
| Is there a standard map format we can reuse? | Yes. Borrow **slippy/XYZ tiles** (quadtree `z/x/y`) for the LOD scheme, **GeoJSON-style JSON** for vector features (coastlines, borders, rivers, roads, labels), and **MBTiles** (= SQLite-of-tiles) for storage — which maps *directly* onto the existing `map_level{N}.db`. |
| Subdivision factor: ×10 per level, or other? | **Recommend quadtree, factor 2 per level** (each tile → 4 children, edge halves). Not ×10. Reasons in §5. ×10 gives 100 children per tile (too heavy to generate on entry) and never lands on the 64 m chunk cleanly. |
| Planet scale? | Flat square, **side ≈ 22,585 km** → area ≈ 510 M km² (≈ Earth surface). Modeled as a flat (optionally edge-wrapping) plane — no spherical math needed offline. |
| Main-map scale 1:10,000,000? | Scale denominator (1:N) is a **display concern**, derived from viewport size, not a storage parameter. With a typical viewport, 1:10 M lands around the **continent→country** zoom (≈ level 3–5). See §5.4. |
| How many continents? | **Recommend default 5–12, allow 4–20** (config range). Earth has ~7. Fewer = boring, more = fragmented. See §6. |
| Two generator types? | Yes: **Map generators** (new — produce the planet→tile hierarchy: vector features + names + per-tile biome/zone) and **Object/Model generators** (existing — produce MC3 3D geometry). Chunk generators (existing C++) sit between them: they build the 64 m 3D scene *driven by* map data. See §3. |
| Where are 3D models stored? | **SQLite, geo-allocated** — sharded by region (block of chunks) **and indexed by altitude band** so the player streams only the models within a set distance, in any direction. Anything farther (e.g. an object 500 km away — horizontally *or* vertically) is never read. See §10.1. |
| Deterministic seed? | **No fixed seed.** Per-world time entropy (already in `PersistentWorldMap::session_entropy_`). Consistency on revisit comes from **persisting what we generate**, not from a reproducible seed — consistent with `docs/new-direction-offline-first.md`. See §8. |

> **Priority / sequencing.** The **map mechanism is the foundation** and is the focus for the
> foreseeable future (potentially ~6 months): coordinates, LOD pyramid, lazy Lua generation,
> persistence, naming, constraint propagation, and the zoomable map UI (milestones MAP1–MAP10,
> MAP12–MAP14 in `plan.md`). The **3D model store + streaming** (MAP11) is designed here so the
> map carries the right data, but heavy 3D work comes *after* the map LOD is solid.

---

## 1. Purpose & scope

Today a MeshWorld "world" is a **single city-scale flat grid** of 64 m chunks
(`docs/worldgen.md`: 20×20 = 1.28 km × 1.28 km; largest considered 2000×2000 ≈ 128 km).
There is no notion of a planet, continents, countries, or a geographic zoom hierarchy.

This document proposes a **planetary map layer** that sits *above* the chunk grid:

- A brand-new world generates a **whole-planet map** (coarse) at creation time.
- The planet is divided into tiles; each tile is refined into more detailed child
  tiles **only when the player navigates into it** (Level of Detail / lazy generation).
- At the deepest zoom, the map hands off to the **existing chunk system**, which builds
  the actual walkable 3D world (MC3 scenes) using the map as macro context.
- Every new world has **different** continents, oceans, mountains, and **different names**
  (continents, countries, cities, streets, rivers, mountains) — all generated offline,
  no internet, no AI calls.

The planet has the **same total area as Earth** but a different, randomized geography
each time. This is an explicit non-goal of *reproducibility*: same dimensions, different
world, every time.

---

## 2. What exists today (grounding — do not re-invent)

The proposal is a **generalization of existing pieces**, not a from-scratch system:

| Existing piece | What it does today | Role in the map layer |
|---|---|---|
| `WorldMap` (`include/WorldMap.hpp`) | Flat grid; assigns `ZoneType` + `RegionType` + `EdgeExits` per chunk | Becomes the **deepest map level** (the chunk-scale tile layer) |
| `ZoneType` enum | `city, jungle, desert, forest, ocean, mountain, tundra, swamp, cave, meadow, beach` (12 at proposal time; grown to 52 by MAP16, 2026-07-10 — see §6's own update note) | Already the biome vocabulary — reuse for tile biomes |
| `RegionType` enum | chunk role: `road, park, river_bank, small_house_block, …` | Stays chunk-scale; assigned at hand-off |
| `PersistentWorldMap` | SQLite, **non-reproducible** (`session_entropy_`), generate-on-first-visit then persist; **already multi-level: `map_level{N}.db`** | The storage + lazy-generation engine for the whole pyramid |
| `ChunkCoord` + `chunk_seed()` | integer `(x,y)`, FNV seed mixing | Tile addressing + per-tile RNG derivation generalize from this |
| `EdgeExits` / road graph (T005) | road continuity across chunk borders | Generalizes to **constraint propagation** between tiles (§7) |
| Lua `map/` category | design already names `lua.map.city.small_grid`, "map generators (basic_city, …)" | The **map generator** category this doc fleshes out |
| `world.json` | top-level config (seed, dims, style) | Gains planet-level fields (planet size, continent count range, climate) |
| MC3 XML / MCB | per-chunk **3D scene** (source of truth / runtime cache) | **Unchanged.** Maps are *not* MC3 — see §10 |

> The key insight: `map_level{N}.db` and `PersistentWorldMap`'s non-reproducible
> persistence are **already the right primitives**. We are adding levels *above*
> level 0 and a richer per-tile payload, not replacing the engine.

**Code layout convention.** All map-subsystem C++ lives under **`include/Map/`** and
**`src/Map/`** in namespace **`MeshWorld::Map`** (e.g. `MeshWorld::Map::TileCoord`,
included as `"Map/TileCoord.hpp"`). This keeps the new subsystem cleanly separated from the
existing flat `include/` / `src/` layout. Tests remain in `tests/`.

---

## 3. Two generator families (and where chunk generators fit)

The user is right that there are ~2 generator *families*. Precisely:

```
┌─────────────────────────────────────────────────────────────────────┐
│ MAP GENERATORS  (NEW)            lua.map.*  /  cpp.map.*               │
│   Input : a tile (level, x, y) + its parent tile's constraints        │
│   Output: map data — vector features + per-tile fields + names        │
│           (NO 3D geometry)                                            │
│   e.g. planet.lua, continent.lua, country.lua, region.lua             │
└───────────────────────────────┬───────────────────────────────────────┘
                                 │ provides macro context (biome, is-city,
                                 │ road graph, elevation, nearest river…)
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│ CHUNK GENERATORS  (EXISTING, C++)         cpp.chunk.*                  │
│   Input : ChunkContext (now enriched with map context)                │
│   Output: one 64 m MC3 scene (places ground, roads, buildings,        │
│           and INSTANCES of objects)                                   │
│   e.g. ParkGenerator, RoadGenerator, ForestGenerator                  │
└───────────────────────────────┬───────────────────────────────────────┘
                                 │ references object definitions by id
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│ OBJECT / MODEL GENERATORS  (EXISTING)     lua.object.*  + C++ defs     │
│   Input : object parameters / variation                               │
│   Output: a reusable 3D MC3 subtree (tree, bench, lamp, car…)         │
│   e.g. tree.lua, bench.lua, ObjectDefinitionLibrary                   │
└─────────────────────────────────────────────────────────────────────┘
```

- **Map generators** never emit geometry. They emit *where things are and what they are*.
- **Object generators** never decide *where* — they emit a reusable shape.
- **Chunk generators** are the bridge: they read map context, then place object instances.

This cleanly separates "the world's geography" (map) from "the world's stuff" (models),
exactly as the user described.

---

## 4. The planetary model & scale

- **Shape:** a flat square plane (no sphere). Offline-friendly, no projection math.
  Optionally edge-wrapping (toroidal) so there is no hard world boundary — recommended,
  but a hard boundary (ocean ring) is simpler and acceptable for v1.
- **Size:** side `S = 22,585 km = 22,585,000 m` → area ≈ **510 M km² ≈ Earth's surface**.
  (√510,072,000 km² ≈ 22,585 km. The user's 22,583 is the same figure.)
- **Atomic unit:** the existing **chunk = 64 m** (unchanged; floating-origin math depends on it).
- **Chunks across the planet:** 22,585,000 / 64 ≈ **352,900 chunks per side**,
  ≈ **1.24 × 10¹¹ chunks total**. Obviously never pre-generated — purely on-demand.

> Trade-off note: you cannot simultaneously fix (a) chunk = 64 m, (b) area = exactly Earth,
> and (c) a power-of-two pyramid that bottoms out *exactly* on a chunk. We fix (a) and (b);
> the map pyramid and the chunk lattice share an origin and axes, and the map↔chunk overlap
> at the bottom is computed arithmetically (§5.5). If you'd rather have a perfectly clean
> pyramid, set `S = 64 m × 2¹⁹ = 33,554 km` (≈ 2.2× Earth area) — pick neat-pyramid OR
> exact-Earth-area, not both. **Recommendation: keep Earth area (22,585 km).**

---

## 5. The LOD tile pyramid

### 5.1 Scheme: quadtree (factor 2), addressed `level/x/y`

This is the **slippy-map / XYZ-tile** scheme used by every web map (OSM, Google, etc.):

- Level 0 = 1 tile = the whole planet.
- Each level: every tile splits into **2×2 = 4 children**; tile edge **halves**.
- A tile is addressed by `(z, x, y)` with `0 ≤ x,y < 2^z`. Level `z` has `4^z` tiles.

### 5.2 Why factor 2, not ×10

| | factor 2 (quadtree) | ×10 (user's guess) |
|---|---|---|
| children per tile | 4 (cheap to generate on entry) | 100 (heavy; most never visited) |
| levels planet→chunk | ~18 (like web maps z0–19) | ~5.5 (non-integer — never hits 64 m) |
| addressing | trivial bit math, industry standard | awkward |
| LOD streaming | proven (this is *the* map LOD scheme) | non-standard |

The **display scale** (1:10,000,000 etc.) is a *separate* presentation concern (§5.4) —
you can still show the player a "1:N" label per zoom while storing tiles as a quadtree.

### 5.3 Level table (planet side 22,585 km, factor 2)

| z | tile edge | semantic band | what the map generator decides here | example generator |
|---:|---:|---|---|---|
| 0 | 22,585 km | **Planet** | continents, oceans, tectonic mountain belts, climate bands, prevailing winds | `planet.lua` |
| 1 | 11,293 km | planet quadrant | settlement/road parity with the generic C++ fallback (M309, MAP18) | `level1.lua` |
| 2 | 5,646 km | — | settlement/road parity with the generic C++ fallback (M308, MAP18) | `level2.lua` |
| 3 | 2,823 km | **Continent** | continent interior: major rivers, mountain ranges, deserts, coast detail | `continent.lua` |
| 4 | 1,411 km | — | capitals + contested-border growth (`Countries::grow()`, C++ only — see note below) | (generic C++, deliberately not ported to Lua) |
| 5 | 706 km | **Country / large region** | national borders, capital placement, biome refinement | `country.lua` |
| 6 | 353 km | — | road/rail trunk network between cities | `trunk_network.lua` (M306, MAP18) |
| 7 | 176 km | **Region / province** | secondary rivers, town placement, terrain detail | `region.lua` |
| 8 | 88 km | — | settlement/road parity with the generic C++ fallback (M305, MAP18) | `level8.lua` |
| 9 | 44 km | **Metro / county** | city footprint, suburbs, lakes | `metro.lua` |
| 10 | 22 km | — | settlement/road parity with the generic C++ fallback (M303, MAP18) | `level10.lua` |
| 11 | 11 km | — | district layout (quadrant split, M304, MAP18) | `district.lua` |
| 12 | 5.5 km | **City** | street grid, zoning (`ZoneType`), parks, water | `city.lua` |
| 13 | 2.75 km | — | M354 city-zone inheritance only (M298, MAP18) | `level13.lua` |
| 14 | 1.4 km | **Town / borough** | block layout (coarse `ZoneCandidate` grid, M299, MAP18) | `level14.lua` |
| 15 | 688 m | **Neighborhood** | individual streets, named | `neighborhood.lua` |
| 16 | 344 m | block cluster | M354 city-zone inheritance only (M300, MAP18) | `level16.lua` |
| 17 | 172 m | street cluster | per-chunk `RegionType` + `EdgeExits` assignment (finer streets, M301, MAP18) | `level17.lua` |
| 18 | 86 m | **≈ chunk hand-off** | this tile ≈ 1–2 chunks → hand to chunk generators | (hand-off, not a generation level — see note below) |

**Level 4 (2026-07-10, MAP18 M307): deliberately NOT given a dedicated Lua script.**
`ChildGenerator.cpp`'s generic C++ path special-cases level 4 as `kCountryRegionLevel`
— the one level `Countries::grow()`'s real multi-capital contested-border growth is
wired into (see `ChildGenerator.cpp`'s own comment on that constant). Since
`MapPipeline` is Lua-first (a level with a registered Lua script never falls through
to the C++ path), adding a naive Lua script here would silently remove that behavior.
Porting `Countries::grow()` itself to Lua wasn't attempted — it's the same class of
work MAP19 gates behind a `needs_human` decision for `Hydrology`/`MountainRanges`/
`BiomeRefinement`/`FeatureNaming`, and extending that gate to `Countries`/
`Settlements`/`Roads` too is a decision for whoever picks this level up later, not
something to do silently as a side effect of MAP18.

**Level 18 (2026-07-10, MAP18 M302): not a generation level at all.** It's the
hand-off point itself — MAP10's existing `ChunkPipeline`/`ChunkContext.map_context`
machinery already reads level 17's output at this depth and hands control to the
legacy chunk generators (M157/M159). A dedicated "level 18 script" would have nothing
to generate that isn't already level 17's job or the hand-off's own job.

(Bands are illustrative — confirm which `z` levels get a named generator vs. pure
geometric subdivision. Not every level needs its own Lua script; a generator can cover a
*range* of levels.)

### 5.4 Display scale denominator (the "1:N" the user asked about)

The representative fraction is `N = ground_width_shown / screen_width_shown`.
For a ~0.2 m-wide map viewport:

- z0 (whole planet): 22,585,000 m / 0.2 m ≈ **1 : 113,000,000**
- z3 (continent):     2,823,000 / 0.2     ≈ **1 : 14,000,000**
- z5 (country):       706,000 / 0.2       ≈ **1 : 3,500,000**

So the user's **1:10,000,000 corresponds to roughly the continent→country zoom (z3–z4)**.
Because it depends on viewport size, store the *level*, not the scale; compute the 1:N
label for the player on the fly.

### 5.5 Hand-off to the 64 m chunk grid

- The deepest map level `z_max ≈ 18` (tile ≈ 86 m). Below this, the **existing**
  `WorldStreamer` / `ChunkPipeline` (64 m chunks) takes over and builds the 3D world.
- Map tiles and chunks **share world origin and axes**. A z18 tile covers the chunk range
  `[floor(tileMinX/64) … ceil(tileMaxX/64)]` × same in Z — a simple arithmetic mapping,
  no lattice alignment required.
- The chunk generator receives the accumulated map context for its location: biome
  (`ZoneType`), is-this-a-city / road graph (`EdgeExits`), elevation, nearest river, etc.

---

## 6. Continents & macro features (level 0)

At world creation, `planet.lua` produces the level-0 payload:

- **Continents:** place **N landmasses** as vector polygons. **Recommend `N ∈ [5,12]`
  default, configurable range `[4,20]`.** (Earth ≈ 7. <4 feels empty; >20 fragments into
  islands and dilutes naming.) Continent size distribution: a few large + several small,
  not uniform.
- **Oceans:** the complement of land; name the major basins.
- **Tectonic belts:** seed a handful of "plate" boundaries → mountain ranges and coastlines
  follow them (gives believable geography rather than noise blobs).
- **Climate bands:** latitude-based temperature + a moisture field (prevailing wind +
  rain shadow behind mountains) → drives biome assignment at finer levels.
- **Elevation field:** coarse heightmap (e.g. value/Perlin/Worley noise + tectonic uplift).

Finer levels **refine within** these constraints (§7) — they never contradict the parent.
Biomes (`ZoneType`) are derived from (elevation, temperature, moisture), e.g.
hot+dry+low → `desert`; cold+high → `mountain`/`tundra`; temperate+wet+flat → `forest`/`meadow`;
near coast+flat → `beach`; human-settled lowland → `city`.

**2026-07-10 update (MAP16, M235/M236-M275):** `ZoneType` grew from 12 to 52 values —
the same 4-input classification above now resolves to a much finer-grained result,
e.g. hot+dry splits further into `salt_flat`/`dunes`/`rocky_desert`/`badlands`/`mesa`
by exact dryness and elevation, and the underwater case (`elevation < sea_level`)
splits by depth+temperature into `ocean`/`deep_ocean`/`coral_reef`/`kelp_forest`/
`lagoon`/`fjord`. 32 of the 40 new values are reachable from `classify()` itself; the
other 8 need a signal this 3-input model can't provide (slope, coast proximity, river
proximity, or volcanism), tracked individually in `plan.md`'s MAP16 section. Full
taxonomy grouped by climate family: `docs/map-generation.md`'s `--legend` section;
exact thresholds: `src/Map/BiomeClassifier.cpp`.

**2026-07-11 update (M259/M274/M275):** 3 of those 8 (`canyon`, `tidal_flat`,
`sea_cliff`) are now reachable end-to-end, just not from `classify()` itself — two new
`Map::BiomeRefinement` passes (`applyCanyonCarving()`/`applyCoastalReliefRefinement()`,
same neighbor-scan shape as the pre-existing `applyCoastalBeach()`/
`applySwampFlatnessCheck()`) reclassify cells based on local relief/coast-adjacency
right after `classify()`'s own cascade runs.

**2026-07-11 update, same day (M247):** `riparian_forest` is also now reachable — a
third new `BiomeRefinement::applyRiparianForest()` pass, but a genuinely different
shape from the two above: it needs the traced `HydrologyNetwork` itself (a
`RiverPoint` is a continuous world-space coordinate, not grid-aligned, so distance is
computed as true Euclidean distance rather than the coastal passes' Chebyshev-square
neighbor test), so it only runs where a river network was actually traced
(planet/continent/country/region/metro — not city/neighborhood, matching MAP19's own
river scale-gating). Deliberately scoped to the grassland/dry-climate family only for
v1 (savanna/steppe/prairie/chaparral/shrubland), not desert/arid biomes too.

**2026-07-11 update, same day (M265-268) — the last of the 8 originally deferred
biomes:** `volcanic`/`geothermal`/`ash_plain`/`volcanic_island` are now reachable too,
via a genuinely new generator, not another `BiomeRefinement`-only pass: `Map::Volcanism`
(structurally mirrors `MountainRanges` — seed a network, sample its elevation
contribution, uplift a `FieldGrid` in place — but with point/radial hotspot geometry
instead of ridge-line geometry) plus a fourth `BiomeRefinement::applyVolcanicBiomes()`
pass that reclassifies land within a hotspot's reach based on active/dormant state,
distance to center, and coastal adjacency (dormant → ash_plain; active + coastal →
volcanic_island; active + near-center → volcanic; active + periphery → geothermal).
Binding `Volcanism::generate()`/`apply()` to Lua genuinely extends the MAP19
Lua-binding exception (§11 below) to a 5th algorithm family — asked explicitly via
`AskUserQuestion` rather than assumed, answered yes. **All 8 of the originally
deliberately-deferred MAP16 biomes are now reachable end-to-end; none remain.**

---

## 7. Constraint propagation (the central correctness rule)

LOD generation is only believable if **a child tile is consistent with its parent and
its already-generated siblings**. This generalizes the existing chunk `EdgeExits` road
continuity (T005).

Rules:

- **Boundary conditions:** a child tile inherits, on each shared edge, the parent's
  coastline crossing, river crossings, road/rail entry points, and edge elevation. The
  child must honor them (a river entering the north edge must leave somewhere consistent).
- **Field continuity:** elevation/temperature/moisture are interpolated from the parent
  and only *detailed* (add higher-frequency noise) by the child — values at shared
  boundaries must match.
- **Feature inheritance:** a city placed at level 12 must still be a city when its level-15
  children generate streets; a mountain range at level 3 constrains elevations at level 9.
- **Sibling coherence:** the four children of a tile must agree on their shared internal
  edges (generate them as a set, or derive shared edges deterministically from the parent
  tile's stored state so any generation order agrees).

Implementation: each tile stores its **edge descriptors** (like `EdgeExits` but richer:
elevation samples, river/road crossings, biome at edge). A child reads its parent's edge
descriptors as fixed inputs. This is the same pattern the road graph already uses, scaled
to the pyramid.

---

## 8. Non-determinism + persistence (consistency without a fixed seed)

The user wants "no deterministic seed — every new world is different." This is already the
project's direction (`docs/new-direction-offline-first.md`: *"Do NOT promise permanent
generator compatibility based on seed"*; *"generated artifacts are the source of truth once
generated"*) and matches `PersistentWorldMap::session_entropy_` (time-based, non-reproducible).

Model:

1. **World seed = time entropy at creation** (`steady_clock`), stored in the world's DB.
   Two worlds created at different instants differ completely. There is *no* promise that a
   given seed reproduces a given planet across generator versions.
2. **Within a world, generation is persisted, not re-derived.** The first time a tile is
   visited it is generated (using `tileEntropy = mix(worldEntropy, z, x, y)`) and **written
   to `map_level{z}.db`**. Revisiting loads from the DB → always identical. This is exactly
   how `PersistentWorldMap` works today; we add the higher levels.
3. **Children derive RNG from parent state + tile entropy**, so generation order doesn't
   matter and siblings stay coherent (§7).

> Consistency on revisit therefore comes from **storage**, not from reproducible math.
> This is the one place to be careful: do *not* re-introduce "same seed = same world
> forever" language — it conflicts with the offline-first doc.

Storage layout (extends today's `saves/<world>/`):

```
saves/<world>/
  world.json                 # config: planet size, continent range, climate params
  map_level0.db              # planet tile  (1 tile)
  map_level3.db              # continent tiles      ─┐ only levels that carry a generated
  map_level5.db              # country tiles         │ payload need a DB; intermediate
  …                          #                       │ pure-subdivision levels can be
  map_level18.db             # chunk-scale tiles    ─┘ computed on the fly
  chunks/<x>_<y>.mc3.xml     # 3D scenes (existing)
```

(One DB per level keeps each table's row addressing simple — already the convention.)

---

## 9. Procedural naming subsystem (NEW — not in any doc today)

Names (continents, oceans, countries, cities, towns, streets, rivers, mountains) are
generated **offline** by a Lua-callable name generator. No dictionaries of real names
required; no network.

Approach (recommended): **phonotactic / Markov syllable model per "culture."**

- A *culture* = a small set of allowed phonemes, syllable templates (e.g. `CV`, `CVC`),
  and joining rules. A few hand-authored cultures (e.g. "Nordic-ish", "Romance-ish",
  "Desert-ish") give regional flavor.
- Each continent (or country) is assigned a culture at generation → all its place names
  share a consistent sound.
- Feature-type suffixes/patterns: rivers get "-water/-flow" morphemes, mountains
  "-peak/-horn", cities "-burg/-ton", streets "<adjective> <noun> Street/Road/Lane".
- Deterministic *given the tile entropy* so a name is stable once generated, and persisted
  with the feature in the level DB.

**2026-07-10 update (MAP9/MAP22, M082/M083/M340/M341):** the design above (this
section predates any of it) undersold two real, later-built mechanisms and overstated
a third. `NameGenerator::dedupe()` (M082) prevents two features in the same naming
scope from getting an identical generated name — wired into the real pipeline by
M340, so `Settlements::name()`/`Countries::name()` now actually use it (it had existed,
unit-tested standalone, since MAP9, but nothing called it until M340). `NameGenerator::
blend()` (M083) exists too, but the "Neighboring regions can blend cultures at borders"
idea sketched two bullets above is **not** how it was actually built: §13 item 4's own
closed M228 decision says "Borders do not blend between cultures; a region's culture is
inherited from its parent tile" — `MapTilePayload::culture`'s own tile-hierarchy
propagation genuinely never blends. `blend()` was instead wired into `Countries::
name()` (M341) scoped MUCH more narrowly than this section's original sketch implied:
a country's generated NAME (never its stored `culture` field) can read as a hybrid of
its own culture and a close, differently-cultured neighbor's — softening a hard border
in the NAMING output only, exactly the kind of narrow, deliberate exception §13's own
"do not reopen" framing calls for, not a general "regions blend" rule.

Lua API sketch (sandboxed, no I/O) — `seed` is required on every form (each call
derives its own entropy from it; omitting it isn't valid, unlike this section's
original pre-implementation sketch):

```lua
names.culture(seed)                    -- pick/derive a culture for a region
names.continent(culture, seed)         -- "Vorlandia"
names.country(culture, seed)
names.city(culture, seed)
names.river(culture, seed)
names.mountain(culture, seed)
names.lake(culture, seed)              -- added M147; sketch above predates it
names.street(culture, seed)            -- "Old Maple Road"
```

This is its own small generator family (`lua.name.*`) reused by all map generators —
built essentially as sketched (`src/LuaRuntime.cpp`'s `register_names_api()`), a thin
wrapper over the `Naming` class, which itself delegates to the real `NameGenerator`/
`NameCulture`/`NameRegistry` pipeline (MAP5) with a hardcoded-stub fallback if
`data/names/cultures/` can't be loaded from the process's own cwd.

---

## 10. File formats & storage strategy

There is **no map format in the repo today** (the agent confirmed: only `world.json`
config + in-memory `WorldMap`; MC3/MCB are 3D *scene* formats, not maps). We introduce one,
reusing established standards:

| Data | Format | Why |
|---|---|---|
| **Vector features** (coastlines, borders, rivers, roads, lake polygons, place labels) | **GeoJSON-style JSON** (custom, compact) | GeoJSON is the de-facto vector-map standard; human-readable, Git-diffable, trivial to parse. |
| **Per-tile fields** (elevation, temperature, moisture, biome id) | small float/int grids — JSON array for coarse tiles, optional packed binary for fine tiles | compact; fields don't need to be human-edited |
| **Tile storage / index** | **SQLite per level** (`map_level{z}.db`), addressed by `(x,y)` — this is essentially the **MBTiles** pattern (SQLite-of-tiles) | already the project convention; one DB per level; lazy insert-on-generate |
| **World config** | `world.json` (existing) + new planet fields | unchanged tooling |
| **3D chunk scenes** | MC3 XML / MCB (existing) | **maps are NOT MC3** — MC3 describes geometry, maps describe geography |

Why **not** put maps in MC3 XML: MC3 is a 3D scene graph (boxes, instances, transforms).
A map is vector geography + scalar fields + labels — a different domain. Forcing it into MC3
would be square-peg/round-hole. JSON (vector) + SQLite (tiles) is the right tool, and it's
already partly in use.

Per-tile JSON payload (illustrative):

```json
{
  "tile": { "z": 5, "x": 12, "y": 7 },
  "entropy": "0x9f3a…",
  "culture": "nordic",
  "biome_field": { "w": 16, "h": 16, "data": [/* ZoneType ids */] },
  "elevation":   { "w": 16, "h": 16, "data": [/* meters */] },
  "features": [
    { "type": "river",    "name": "Skarnfoss",  "path": [[x,y],[x,y]…], "enters": ["N"], "exits": ["SE"] },
    { "type": "city",     "name": "Vorhavn",    "center": [x,y], "population_hint": 42000 },
    { "type": "border",   "country": "Aeland",  "polygon": [[x,y]…] }
  ],
  "edges": { "N": { "elev": [..], "rivers": [..], "roads": [..] }, "E": {…}, "S": {…}, "W": {…} }
}
```

### 10.1 3D model / scene store — SQLite, geo-allocated, altitude-indexed (DECIDED)

Today the 3D side is split: per-chunk **MC3 XML files** on disk (`saves/<world>/chunks/<x>_<y>.mc3.xml`
via `ChunkCache`) plus shared **object definitions** injected at render time (`ObjectDefinitionLibrary`,
never serialized). At planetary scale (~1.24 × 10¹¹ potential chunks) a file-per-chunk layout is
infeasible (inode exhaustion, slow directory listing, fragmentation). **Decision: the generated 3D
world is stored in SQLite, allocated by geographic location and altitude, and streamed by 3D
proximity to the player.**

**What is "a 3D model" here:**
- **Shared definitions** (tree, bench, lamp, car …) — *not* per-world; stay in code /
  `ObjectDefinitionLibrary` / the content pack, injected at render time. **Never** stored per-world.
- **Placements** — "instance of definition `tree_oak` at world position (x, y, z), rotation, scale,
  LOD" — *this* is what is stored per world, by location.
- **Chunk scene blobs** (optional) — a compiled MC3/MCB blob for a whole chunk, when it is cheaper to
  store the assembled scene than to re-run the chunk generator.

**Geographic allocation + sharding.** One giant DB per world does not scale; instead **shard by
region** (a fixed block of chunks, e.g. a level-9 tile ≈ 44 km, or an N×N chunk block):

```
saves/<world>/
  map_level{z}.db                 # the map LOD pyramid (vector + fields), per level
  models/<rx>_<rz>.db             # 3D placements for region (rx,rz) — sharded by geography
```

Each `models/<rx>_<rz>.db` holds placements whose horizontal chunk falls in that region. A world
touches only the region DBs near the player — the rest stay closed on disk.

**Proximity is the rule: load only what's near the player, in any direction.** The world streams
only models within a set distance of the player — anything farther (the "500 km" figure is just an
example of "too far") is never read. The current `WorldStreamer` already does this **horizontally**
(2D chunk radius); the missing piece is the **vertical axis**, so each placement carries the
**altitude range it occupies** and is indexed by altitude band. That makes the cull a true 3D box
(or sphere) around the player: far horizontally *or* far vertically → not loaded. The store is
therefore genuinely **volumetric (3D)**, not a 2D heightmap — which also supports caves, tall
structures, aircraft, orbit, etc.

```sql
-- inside models/<rx>_<rz>.db
CREATE TABLE placement (
  id          INTEGER PRIMARY KEY,
  chunk_x     INTEGER NOT NULL,        -- horizontal chunk coord (global)
  chunk_z     INTEGER NOT NULL,
  alt_band    INTEGER NOT NULL,        -- floor(y_center / ALT_BAND_M), e.g. 64 m bands
  y_min       REAL    NOT NULL,        -- altitude extent (meters)
  y_max       REAL    NOT NULL,
  pos_x       REAL    NOT NULL,        -- precise world position
  pos_y       REAL    NOT NULL,        -- altitude
  pos_z       REAL    NOT NULL,
  rot_y       REAL    NOT NULL DEFAULT 0,
  scale       REAL    NOT NULL DEFAULT 1,
  lod_min     INTEGER NOT NULL DEFAULT 0,
  definition  TEXT    NOT NULL         -- ObjectDefinitionLibrary / lua.object.* id
);
CREATE INDEX idx_place_xz_alt ON placement (chunk_x, chunk_z, alt_band);
```

**Proximity query (3D streaming).** Loading around the player is a bounded 3D box query, not a scan:

```
horizontal: chunk_x ∈ [px - R_h, px + R_h]  AND  chunk_z ∈ [pz - R_h, pz + R_h]
vertical:   alt_band ∈ [floor((py - R_v)/B), floor((py + R_v)/B)]
```

So only placements inside the player's 3D radius (`R_h` horizontal, `R_v` vertical) are read.
This generalizes the current `WorldStreamer` radius logic to **three axes** (it is 2D today).

**Trade-offs & mitigations:**
- *Inspectability:* SQLite blobs aren't hand-editable like an `.mc3.xml`. → keep a **dev-mode export**
  that dumps a single chunk's placements to `.mc3.xml` for debugging; not used at runtime/scale.
- *DB size:* region sharding bounds each file; closed region DBs cost nothing.
- *Write amplification:* batch placement writes per region in a transaction (WAL is already on).
- *Definitions stay shared:* placements reference definition ids; geometry is never duplicated per row.

**Summary of the storage decision:**

| Data | Store | Keyed/indexed by |
|---|---|---|
| Map LOD pyramid (vector + fields + names) | `map_level{z}.db` (SQLite, per level) | `(x, y)` within level |
| 3D placements (instances of definitions) | `models/<rx>_<rz>.db` (SQLite, region-sharded) | `(chunk_x, chunk_z, alt_band)` — **3D** |
| Optional compiled chunk scene blobs | same region DB (BLOB column) | chunk coord |
| Shared object definitions | code / `ObjectDefinitionLibrary` / content pack | id (not per-world) |
| Dev export of a chunk | `.mc3.xml` file (debug only) | chunk coord |

---

## 11. Lua map-generator API (as built)

Mirrors the existing object/zone generator contract (`M.generate(ctx, map)` with
`M.id/version/category`), but the builder is a **map builder**
(`src/MapBuilder.cpp`/`src/LuaRuntime.cpp`'s `register_map_api()`), not a scene
builder. This section originally sketched a small, illustrative subset before any of
it existed; **2026-07-10 (MAP19, M313-318) update**: the real API is substantially
larger than that sketch, especially after MAP19 exposed 4 of MAP8's real C++
terrain-refinement algorithms to Lua (previously every Lua-scripted level ran a
deliberately simplified v1 reimplementation with no rivers, mountain ranges, or
biome refinement at all — a real content gap the original design didn't anticipate).

```lua
-- generators/lua/map/continent.lua
local M = { id = "lua.map.continent.default", version = "0.1", category = "map" }

function M.generate(ctx, map)
  -- ctx.level, ctx.tile_x, ctx.tile_y, ctx.tile_size_m, ctx.variation (uint64)
  -- ctx.parent   -- read-only: {level, tile_x, tile_y, variation, culture,
  --                elevation, temperature, moisture, biome} (biome added M354,
  --                a categorical BiomeGrid table -- nearest-neighbor sampling
  --                only, never bilinear(), unlike the float fields)
  -- ctx.edges    -- fixed boundary conditions this tile must honor
  -- ctx.random(min,max), ctx.randomInt(min,max), ctx.noise(x,y,...)

  local culture = ctx.parent and ctx.parent.culture or names.culture(ctx.variation)

  -- Base fields (MAP6/M088-103) -- every scripted level calls this first.
  map:setBiomeField(W, H, elevation, temperature, moisture)

  -- MAP19 (M313-317) real terrain-refinement bindings -- scale-gated (see
  -- ChildGenerator.cpp's own per-level cutoff logic mirrored in each script):
  -- mountain ranges only at planet/continent/country scale; rivers additionally
  -- at region/metro; biome refinement (cheap, scale-agnostic) at every level.
  local rivers = map:traceRivers(W, H, elevation)
  map:carveRivers(W, H, elevation, rivers)          -- mutates elevation in place
  map:appendHydrologyFeatures(rivers, culture, ctx.variation)

  local ranges = map:generateMountainRanges(ctx.variation, 3, 1500.0, 4000.0)
  map:applyMountainRanges(W, H, elevation, ranges, 20000.0)  -- mutates elevation
  map:appendMountainRangeFeatures(W, H, elevation, ranges, culture, ctx.variation)

  -- M265-268 (2026-07-11): volcanism -- a 5th algorithm family added to the
  -- Lua-binding exception (asked explicitly, see below); same
  -- "planet/continent/country only" scale-gating as mountain ranges above.
  local volcanoes = map:generateVolcanicHotspots(ctx.variation, 1, 1500.0, 4000.0)
  map:applyVolcanism(W, H, elevation, volcanoes)  -- mutates elevation

  map:applyCoastalBeach()          -- refines THIS builder's own already-set biome grid
  map:applySwampFlatnessCheck()
  map:applyCanyonCarving()             -- M259 (2026-07-11): badlands/mesa/rocky_desert -> canyon
  map:applyCoastalReliefRefinement()   -- M274/M275: coastal cells -> tidal_flat/sea_cliff
  map:applyRiparianForest(rivers)      -- M247: needs `rivers` -- only where traceRivers() ran
                                        -- (planet/continent/country/region/metro, not city/neighborhood)
  map:applyVolcanicBiomes(volcanoes)   -- M265-268: needs `volcanoes` -- runs LAST among the
                                        -- refinement passes (most dramatic natural override)

  -- Vector features / fields (MAP6-era, still the base of every script):
  map:addContinent(names.continent(culture, ctx.variation), x, z)
  map:addRiver(names.river(culture, ctx.variation), { {x, z}, ... })
  map:addMountainRange(names.mountain(culture, ctx.variation), { {x, z}, ... })
  map:addCity(names.city(culture, ctx.variation), x, z, "town")
  map:addBorder(country_name, { {x, z}, ... })
  map:addRoad(names.street(culture, ctx.variation), { {x, z}, ... })
  map:addLake(names.lake(culture, ctx.variation), { {x, z}, ... })
  map:addStreet(names.street(culture, ctx.variation), { {x, z}, ... })
  map:addPark(name, x, z)
  map:markUrbanCells({0, 1, 1, 0, ...})              -- w*h mask -> ZoneType::city
  map:setZoneCandidates({0, 2, 2, 0, ...})           -- w*h mask -> Map::ZoneCandidate
  map:setEdge("S", { elev... })                      -- export to children
  map:setMetadata(M.id, culture)
end

return M
```

Sandbox: same rules as today — `io`, `os`, `debug`, `package`, `dofile`, `loadfile`,
network, filesystem all blocked. The `map:` and `names.` tables are the only new
bindings. `HydrologyNetwork`/`MountainRangeNetwork` (the objects `traceRivers()`/
`generateMountainRanges()` return) are opaque sol2 userdata a script passes straight
through to `carveRivers()`/`appendHydrologyFeatures()`/`applyMountainRanges()`/
`appendMountainRangeFeatures()` — no fields or methods are exposed on them directly;
a script only sees rivers/ranges after they're re-exported as named `MapFeature`
entries via the `append*` calls.

`Countries`/`Settlements`/`Roads` (MAP9's other 3 real C++ algorithms) remain
deliberately un-bound — every scripted level still uses its own independent v1 Lua
reimplementation for those specifically (`country.lua`'s capital+border growth,
`metro.lua`'s settlement placement, etc.); MAP19's own `needs_human` gate answer
scoped the "wire a real C++ algorithm into Lua" exception to exactly `Hydrology`/
`MountainRanges`/`BiomeRefinement`/`FeatureNaming` and does not extend automatically
to these three without asking again.

A C++ `MapBuilder` (analogous to `Mc3SceneBuilder`) collects these calls and serializes to
the JSON payload + writes it into `map_level{z}.db`. Validation analogous to `MC3Validator`
checks edge-consistency before storing.

---

## 12. End-to-end flow

```
New world (time entropy stored)
  └─ planet.lua → level-0 payload (continents, oceans, climate, names) → map_level0.db

Player opens map / moves toward a region
  └─ for each tile entered, descending z:
       load tile from map_level{z}.db
         └─ miss? run map generator for that z, constrained by parent + sibling edges,
                  persist to map_level{z}.db
       (LOD: only tiles the player approaches/zooms are ever generated)

Player enters a location at z_max (~86 m tiles)
  └─ map context (biome, road graph, elevation, nearest river, is-city, names) handed to
     ChunkPipeline → existing C++ chunk generators build 64 m MC3 scenes
       └─ chunk generators place object INSTANCES from ObjectDefinitionLibrary / lua.object.*
```

---

## 13. Open questions — resolved (M228, 2026-07-09)

Originally "open questions to confirm"; each is now a closed decision, reflecting what
was actually built (not a re-opening — see NEXT.md §9's "do not reopen" list for the
milestones these came from):

1. **Edge wrapping:** resolved as a flat, non-wrapping plane — no toroidal/wrap math
   anywhere in `TileCoord`/`PlanetConstants`/the generators. An edge tile's off-planet
   neighbor simply doesn't exist (no special-cased ocean ring either; each generator's
   own coastline/continent placement naturally leaves plenty of ocean near the edges).
2. **Which z levels get a dedicated Lua generator:** **0, 3, 5, 7, 9, 12, 15** —
   `generators/lua/map/{planet,continent,country,region,metro,city,neighborhood}.lua`.
   Level 17 (mentioned as a candidate here) was never implemented; every other level
   falls through to the C++ `ChildGenerator` (§5's "the named bands" — MAP6/M088-103).
   **2026-07-10 update (MAP18, M298-M310):** 10 more levels (1, 2, 6, 8, 10, 11, 13,
   14, 16, 17) got their own dedicated script too — `level17.lua` in particular is
   the one this item's own text once called "never implemented." Only level 4
   (`Countries::grow()`'s C++-only special case, `kCountryRegionLevel`) and level 18
   (not a generation level at all — the chunk hand-off point itself) remain
   deliberately unscripted; see §5.3's own per-level table and notes for the current,
   authoritative state.
3. **Continent default range:** confirmed **5–12** (`WorldConfig::continents_min/max`
   defaults), configurable range 4–20 unchanged.
4. **Number of cultures:** **3** — `data/names/cultures/{desert,nordic,romance}.json`
   (MAP5). Borders do not blend between cultures; a region's culture is inherited from
   its parent tile (`MapTilePayload::culture`, set once at level 0 by `PlanetGenerator`/
   `planet.lua`, propagated down `ChildGenerator`'s recursive descent).
5. **Field resolution per tile:** **64×64** (`GRID_SIZE` in `PlanetGenerator.cpp`/
   `ChildGenerator.cpp`) — not 16×16 or 32×32; chosen to match the existing minimap/
   chunk-grid conventions rather than introduce a new resolution.
6. **Do intermediate (unnamed) levels persist:** resolved as recommended — **only
   named levels persist** (`MapPipeline`'s `named_levels` parameter, M114); other
   levels always regenerate deterministically from the parent chain + world entropy,
   never touching `MapTileStore` for themselves.
7. **Toolless preview:** built as **`MeshWorldPlanet`**, a new CLI (not an extension of
   `MeshWorldMap`, though `MeshWorldMap --planet` was later added too, M208) — `--ascii`
   (M206), `--png` (M205, plus `--png-mode hillshade` M232 and `--png-region` M229),
   `--geojson` (M230), `--mbtiles` (M231). See `docs/map-generation.md` for the full
   flag reference.

---

## 14. Phased implementation plan

> The **granular, numbered task breakdown lives in `plan.md`** as the **M-series**
> (`M001`, `M002`, …, "M" = map), grouped into milestones **MAP1–MAP15**. The summary below
> maps the conceptual phases onto those milestones. Each phase is independently testable; no
> phase changes existing chunk/MC3 code.
>
> **Status as of 2026-07-09 (M228 reconciliation):** MAP0/1-10/12/13/15 are all
> **complete and closed** (see `NEXT.md` §9's "do not reopen" list). **MAP11** (3D
> model store & streaming) is 15/20 done — the remaining rendering-integration tasks
> (M175-177) are blocked on a real build-configuration question, not a design gap; see
> `NEXT.md` §4. **MAP14** (performance, scale & QA) is nearly complete (M215-M227 of
> M215-M228 done as of this reconciliation) — a genuinely later addition than this
> phased-plan sketch anticipated, added once the rest of the map mechanism was already
> solid. **MAP15** (map image/data export — PNG/hillshade/region-stitch/GeoJSON/MBTiles)
> was a user-requested addition (2026-07-07) not anticipated in this original plan at
> all; it slotted in as milestone M229-M234, built directly on M205's PNG renderer.

Milestone groups in `plan.md`: **MAP1** coordinates & tile model · **MAP2** SQLite storage
foundation · **MAP3** planet config & bootstrap · **MAP4** planet generator (level 0) ·
**MAP5** naming subsystem · **MAP6** Lua map-generator binding · **MAP7** LOD descent &
constraint propagation · **MAP8** hydrology/biome refinement · **MAP9** political & settlement
layer · **MAP10** street-level & chunk hand-off · **MAP11** 3D model store (geo + altitude) ·
**MAP12** zoomable map UI · **MAP13** validation & tooling · **MAP14** performance, scale & QA ·
**MAP15** map image & vector data export (added 2026-07-07, not in the original plan).

Conceptual phases (detail in the M-series):

- **MP1 — model & storage:** `TileCoord{z,x,y}` + `tile_entropy()` (generalize `chunk_seed`);
  extend `PersistentWorldMap` to arbitrary levels with a richer payload; `world.json` planet
  fields. Tests: tile addressing, entropy mixing, round-trip persist/load.
- **MP2 — planet generator (C++ first):** a deterministic-given-entropy level-0 generator
  (continents via polygon/noise, climate bands, elevation). Tests: N continents in range,
  land/ocean ratio sane, biome field coherent.
- **MP3 — naming subsystem:** `lua.name.*` + `names.` bindings + a few cultures. Tests:
  names stable given entropy, per-culture phoneme constraints respected.
- **MP4 — Lua map generator binding:** `MapBuilder` + `map:` API + sandbox; port the planet
  generator to `planet.lua`. Tests: sandbox blocks I/O; payload validates.
- **MP5 — LOD descent + constraint propagation:** child generators honor parent edges;
  sibling coherence. Tests: river/road/elevation continuity across tile borders.
- **MP6 — hand-off to chunks:** enrich `ChunkContext` with map context; chunk generators read
  biome/road/elevation from the map instead of from flat `WorldMap`. Tests: existing generator
  tests still pass; chunks reflect map biome.
- **MP7 — map UI:** zoomable map view (reuse the ImGui minimap work); show names; trigger LOD
  generation on pan/zoom. Tests: vertex-budget safe (the 16-bit limit lesson from the minimap).

Foundations already in place that these build on: `PersistentWorldMap` multi-level DBs,
`session_entropy_` non-reproducibility, the `map/` Lua category, `EdgeExits` constraint
pattern, and the `MeshWorldMap` ASCII visualizer.

---

## 15. Relationship to existing rules (sanity check)

- ✅ Offline-first: all generation is local Lua/C++ + noise; no AI/network.
- ✅ Non-reproducible: uses time entropy + persisted artifacts (no "same seed forever").
- ✅ No MC3 API changes: maps are a new JSON+SQLite domain; MC3/MCB untouched.
- ✅ Lua sandbox unchanged: only new *builder* bindings added, same blocked globals.
- ✅ Chunk conventions unchanged: 64 m chunk, `ChunkCoord::from_world` untouched; map sits above.
- ✅ Reuses `map_level{N}.db` (already designed) rather than a new storage engine.
