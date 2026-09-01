# Migration-Stage Tracking (R121)

This is a **living document** for the R-series revival initiative (see
`plan.md` → "Revival architecture tasks (R-series)" and
`mesh_world_revival.md`). It turns `mesh_world_revival.md` §20's
prose-only "Migration Strategy" (Stage 1-7) into a checkable, trackable
map from each stage's own bullet points to the concrete `plan.md` R-task
that implements it, plus a tracked list of which Lua generators are
ready to be archived once their C++/MC3-asset equivalent exists and
covers them (§4.6).

**Review cadence:** re-visit whenever an R10x/R11x/R12x task closes, same
cadence as `docs/risk-register.md`.

---

## 1. Stage 1-7 tracking

Status values: `done`, `in progress`, `not started`.

### Stage 1 — audit and preserve

| §20 bullet | R-task | Status |
|---|---|---|
| identify active C++ and Lua generators | R107 | done |
| trace the complete runtime generation path | R107 | done |
| identify dead or unused generators | R107 (chunk/zone layer), R105 audit (map layer, 2026-07-12) | done |
| identify current MC3 limitations vs. wrapper limitations | R107 | done |
| preserve working behavior | R107 (audit-only, no code changed) | done |
| establish visual and structural baselines | R107 (structural baseline: 1442 tests, code-path trace); **visual baseline still missing** — no GPU/screenshot capture available in this environment | partial |

**Stage 1: substantially done.** The one open item (visual baseline) needs
a human with a real display — tracked as `needs_human`, not self-startable.

### Stage 2 — introduce shared data foundations

| §20 bullet | R-task | Status |
|---|---|---|
| formalize `Mc3Document` as the single semantic model | already true going in (R109's own precondition) | done |
| add `mc3.json` parser and writer | R109 | done |
| add JSON Schema | R131 (`schemas/mc3.schema.json`) | done |
| verify XML/JSON semantic round-trip | R109 | done |
| introduce library imports and resolver design | R110 (`.mc3lib` format), R101 (`<imports>` + `Mc3ImportResolver`), R102 (resolved-imports usable via existing instance lookup) | done |

**Stage 2: done.** R131 supplies a strict Draft 2020-12 contract adjacent
to the tracked MC3 libraries. It is exercised against every real
`.mc3lib.json` and representative invalid documents; it intentionally does
not add a JSON-Schema runtime dependency or replace MeshCraft parser
compatibility/`MC3Validator` semantic validation.

### Stage 3 — create global libraries

| §20 bullet | R-task | Status |
|---|---|---|
| windows, doors, roofs, facade modules, street furniture, vehicles, trees, rocks, cave modules, interior furniture | R112 (first urban batch), R116 (nature), R117 (cliff/rock), R118 (cave) | **done (first batch)** |

**Stage 3: done for the first batch (corrected 2026-07-18) — was
previously mis-tracked here as "not started"** despite `plan.md`'s own
R112 entry showing all 7 urban categories (windows/doors/roofs/facade
modules/street furniture/props/vehicles, ~29 definitions with variants)
shipped 2026-07-12/13, loaded through the real `Mc3ImportResolver`
mechanism. **Caveat (not yet closed):** window/door/roof/facade-module
instances are registered in `data/mc3lib` but not yet *placed* into
composed chunks — only house/streetlamp/mailbox/vehicle/yard-tree are
actually composed — because attaching sub-facade modules at compose
time needs R104 (mesh-craft `<script>` engine execution), which stays
out of scope without a separate cross-repo sign-off (`NEXT.md` §9).
Nature (R116), cliff/rock (R117), and cave (R118) library batches remain
not started — Stage 3 is "done" only for the urban batch this covers.
R125 (`object.sqlite3` → `data/taxonomy/*.json` curation) is a related,
data-only follow-up feeding placement rules, not a new library batch.

### Stage 4 — compose a city showcase

| §20 bullet | R-task | Status |
|---|---|---|
| C++ world composer selects assets | R113, R126 (`apartment_block`), R127 (`shop_street`), R129 (`square`) | done |
| existing Lua may be used temporarily where useful | (policy, not a task — no `.lua`/`.cpp` generator deleted; every composer branch falls through to its legacy generator when opted out or when no matching asset is registered) | done |
| validate that active runtime uses the new path | R114, R128 (landmark + LOD wiring) | done |
| generate deterministic screenshots and metrics | R114/R128 (data-level: `MeshWorldExport --validate` output XML inspected); 2026-07-25 Xvfb/Mesa app screenshot establishes execution baseline, while human GPU/display aesthetic review remains `needs_human` | partial |

**Stage 4: done (corrected 2026-07-18)** — was previously mis-tracked
here as "not started" despite `plan.md`'s own R113/R126/R127/R129 entries
showing the composer now covers all 4 common region types
(`small_house_block`, `apartment_block`, `shop_street`, `square`), and
R128 closing the city showcase's landmark/LOD gaps. **2026-07-24:** the
real `examples/city_showcase.json` now mixes all four types in one 7×7
scene and is reachable from `MeshWorldApp`'s **Explore City Showcase**
menu action; the app registers composer assets at startup instead of
silently falling back to legacy generation. All 49 exported chunks have
also been converted to GLB successfully; after registering the apartment's
missing `plaster_beige` material, only one expected ambient-light conversion
warning remains per chunk. The one open item (deterministic screenshots)
now has a first execution-level baseline: on 2026-07-25 the built app ran
under Xvfb/Mesa OpenGL ES 3.2 and rendered the real mixed city scene.
That visual run exposed and fixed the persistent-map/map-placement
overrides that had hidden the authored content. A human GPU/display review
for artistic quality remains `needs_human`, same limitation already
documented for Stage 1.

### Stage 5 — demote runtime Lua

| §20 bullet | R-task | Status |
|---|---|---|
| migrate active generators | R105 (map layer; zone/chunk layer's own T219-235 precedent already covers most of that layer) | in progress (map layer) |
| retain offline conversion tools | (policy — Lua files are kept, not deleted, per RR4) | done (by omission — nothing removed) |
| remove runtime dependencies only after coverage exists | R105 gave `city.lua`/`district.lua` real C++ coverage 2026-07-12; **no Lua file has actually been removed yet** — that is this task's (R121's) own tracking job, done below in §2 | in progress |
| document retired generators | this document, §2 below | in progress |

**Stage 5: in progress at the map layer** (R105 closed), **not started**
at the zone/chunk layer beyond what T219-235 already did historically.

### Stage 6 — expand to nature and underground worlds

| §20 bullet | R-task | Status |
|---|---|---|
| forest placement, cliffs/rock modules, cave graph realization, biome kits, LOD/instancing | R116, R117, R118 | not started |

### Stage 7 — modern rendering

| §20 bullet | R-task | Status |
|---|---|---|
| PBR materials, shadows, AO, post-processing, atmosphere, GPU-driven rendering, backend tiers | R120 (cross-repo: CNA/NOXNA/MeshCraft) | not started |

---

## 2. Lua generator archival tracking (§4.6)

**"Archival-ready" here means: a real, tested, native C++ generator now
covers everything that Lua script does — it is safe to consider
retiring at some future Stage 5 milestone. It does NOT mean the file
should be deleted now.** Per RR4 ("premature deletion of Lua" — see
`docs/risk-register.md`), no `.lua` file is deleted as a *consequence*
of appearing in this list; removal is a deliberate, separate, later
decision once Stage 4's city showcase actually exercises the C++ path
in practice, not automatic the moment a table row flips to "ready."

Sourced from R105's full 17-file audit (2026-07-12, `plan.md`'s own
R105 entry / this repo's git history has the fork's full table) plus
this same session's own R105 implementation (`CityGenerator`,
`DistrictGenerator`) closing 2 of the audit's "genuine gap" rows.

### 2.1 Map-level Lua generators (`generators/lua/map/*.lua`)

| Script | Level | C++ equivalent | Archival-ready? |
|---|---|---|---|
| `planet.lua` | 0 | `PlanetGenerator` | yes |
| `level1.lua` | 1 | `ChildGenerator` (generic) | yes |
| `level2.lua` | 2 | `ChildGenerator` (generic) | yes |
| `continent.lua` | 3 | `ChildGenerator` (generic — calls the same `MountainRanges`/`Hydrology` C++ functions) | yes |
| `country.lua` | 5 | `ChildGenerator` (generic — capitals/borders mechanism exists via `Countries::grow()`, wired to level 4 not 5; not an absolute capability gap) | yes |
| `trunk_network.lua` | 6 | `ChildGenerator` (generic — real `Settlements::place()`/`Roads::build()` routing is richer than this script's own "naive straight-line" v1) | yes |
| `region.lua` | 7 | `ChildGenerator` (generic) | yes |
| `level8.lua` | 8 | `ChildGenerator` (generic) | yes |
| `metro.lua` | 9 | `ChildGenerator` (generic) | yes |
| `level10.lua` | 10 | `ChildGenerator` (generic) | yes |
| `district.lua` | 11 | **`DistrictGenerator`** (R105, 2026-07-12 — closed the audit's own "minor genuine gap" finding) | yes |
| `city.lua` | 12 | **`CityGenerator`** (R105, 2026-07-12 — closed the audit's own "major genuine gap" finding) | yes |
| `level13.lua` | 13 | `ChildGenerator` (generic) | yes |
| `level14.lua` | 14 | *(none)* — its own `ZoneCandidate`/`setZoneCandidates()` block-zoning call has no C++ equivalent (only `CityGenerator`, level 12, has this today) | **no** |
| `neighborhood.lua` | 15 | *(none)* — its own `addStreet()` (named, terrain-aligned streets) has no C++ equivalent | **no** |
| `level16.lua` | 16 | `ChildGenerator` (generic) | yes |
| `level17.lua` | 17 | `ChildGenerator` (generic) | yes |

**15 of 17 archival-ready** (13 from the original audit + `district.lua`/
`city.lua` closed this session). **2 genuinely not yet covered**:
`level14.lua`'s zoning half and `neighborhood.lua`'s street-naming half —
both would need their own dedicated generator (or a generalization of
`CityGenerator` across levels) to close, not attempted this session
(narrower, lower-priority than `city.lua`/`district.lua` were).

### 2.2 Zone/chunk-level Lua generators (`generators/lua/zone/*.lua`, `generators/lua/object/*.lua`, `generators/lua/building/*.lua`)

**Audited 2026-07-12 (follow-up fork, same day this document was
created)** — confirmed the architectural difference matters: the
verdict here is genuinely mixed, NOT extrapolatable from §2.1's map-layer
conclusion. The chunk layer already has one dedicated C++ class per
zone/region (`ChunkGenerator.cpp`'s `get_generator(zone, region)`
dispatch — `ForestGenerator`, `ParkGenerator`, `DesertGenerator`, etc.),
so a Lua zone script competes against a SPECIFIC class, not a generic
fallback the way map-level scripts did.

Only 4 `generators/lua/zone/*.lua` files exist (`forest.lua`, `park.lua`,
`road.lua`, `crossroad.lua` — `plan.md`'s own T219-223 checkboxes for
`desert.lua`/`jungle.lua`/`beach.lua`/`square.lua`/`shop_street.lua` are
correctly annotated "(superseded — ...)" already; those files were never
meant to exist, not a checkbox/reality gap):

| Script | Verdict | Reason |
|---|---|---|
| `forest.lua` | PARTIAL (Lua weaker) | Inline box-canopy trees, no fallen logs/mushrooms/river-proximity variation/`ModelPlacement` output `ForestGenerator.cpp` has. |
| `park.lua` | PARTIAL (Lua weaker) | Fixed paths regardless of neighbor exits; `ParkGenerator.cpp` is exit-aware, has `StyleRegistry` variation, more benches, collision-avoiding tree placement. |
| `road.lua` | **was a real, live correctness bug — fixed 2026-07-12** | Always built a north-south road regardless of `ctx.exits` (which it never read at all), unlike the exit-aware C++ fallback. Fixed by mirroring `RoadGenerator.cpp`'s own ns/ew/ambiguous-fallback logic; see `tests/RoadLuaTests.cpp`. |
| `crossroad.lua` | **gap closed 2026-07-12** | Had working traffic lights + crosswalk stripes `CrossroadGenerator.cpp` lacked; both ported to C++ (faithfully, including matching the Lua source's own N/S-only crosswalk reality despite its "N,S,E,W" comment). Also found+fixed in passing: `light_red`/`light_amber`/`light_green` were referenced by `crossroad.lua` but never registered in `MaterialRegistry` — same class of gap R106 found for `flowers_*`. |

`generators/lua/object/*.lua` (27 files): **not a "no C++ counterpart"
situation** — `src/ObjectDefinitionLibrary.cpp` has its own independent
native C++ factory functions (`make_bench()`, `make_lamp_post()` — its
own comment says "Mirrors generators/lua/object/lamp.lua" —,
`tree_oak`/`tree_pine`/etc., `make_mushroom()`) building the *same*
conceptual objects, used via `w.instance(...)` by
`ForestGenerator`/`ParkGenerator`/`RoadGenerator`. Two parallel,
independently-authored implementations of the same object types exist
side by side.

**Follow-up check (2026-07-12, same day): confirmed via grep across the
whole codebase — this is not actually a "which one wins" ambiguity at
all.** Nothing in real production code ever calls
`LuaGeneratorRegistry::instance().has("lua.object.` or `.get("lua.object.`
(only comments/docs reference that naming convention) — `w.instance(id,
"tree_oak", ...)`-style calls reference a bare definition id resolved
only through `ObjectDefinitionLibrary::load_all()`, never through the
Lua-first registry lookup the zone/map layers use. **The 27
`generators/lua/object/*.lua` files are genuinely unreachable from the
real generation pipeline today** — not competing with the C++ path, just
never invoked by anything except `MeshWorldPack`'s own generic
`load_from_dir()` archiving (which packs every Lua file it finds,
reachable or not, into SQLite for distribution — it doesn't discriminate).
This is the SAME "built on one side, unreached from the other" pattern
already found repeatedly in this project's history (see the G-series
session's own `scene:callGenerator`/`ctx.containment` findings). **Not
fixed here** — whether the right fix is wiring up real object-level
Lua-first resolution (a genuine new feature, mirroring what the zone/map
layers already have) or simply retiring/repurposing these specific files
is a real direction decision, not a bug fix, and is left for a future
session/explicit decision rather than guessed at.

`generators/lua/building/*.lua` (2 files: `simple_house.lua`,
`house/detached.lua`): **no C++ counterpart exists at all** —
genuinely Lua-only content (the newer G-series `scene:callGenerator`
composition mechanism), not a redundancy candidate.

**Also found, not chased**: `ChunkGenerator::placements()` (the
`ModelPlacement` mechanism `ForestGenerator` uses) is only ever
exercised from tests, never from real production code (`main.cpp`/
`export_chunks.cpp`/the app) — so `forest.lua`'s own gap relative to it
isn't currently visible in practice either way.

**Synthesis: the zone/chunk layer does not need R105-style wholesale
C++ porting** (the dedicated-class structure already exists), but it
is not uniformly healthy either — `road.lua`'s bug was real and live,
and the `object/*.lua` vs `ObjectDefinitionLibrary.cpp` duplication
deserves a dedicated look before assuming either copy is safe to touch
without checking the other.

---

## 3. How to use this document

1. When closing an R10x/R11x/R12x task, update §1's relevant stage row
   and, if the task changes a Lua generator's coverage status, update §2.
2. Do not delete a `.lua` file because it appears "archival-ready" in
   §2 — that decision belongs to a real Stage 5 milestone review, with
   the city showcase (R114) actually exercising the replacement path in
   practice first (see `docs/risk-register.md`'s RR4).
3. This document does not replace `plan.md`'s own per-task descriptions;
   it tracks the cross-cutting migration-stage view `mesh_world_revival.md`
   §20 describes, the same relationship `docs/risk-register.md` has to
   §23/§25-26.
