# CLAUDE.md — MeshWorld Canonical Handoff

_Last updated: 2026-07-25. This is the canonical entry point for Claude
Code (or any AI coding agent) starting a session on MeshWorld. It stays
deliberately short and does not duplicate history — for the detailed
current-state write-up see `NEXT.md`, for the full dated task-by-task
log see `plan.md` (R100+ is the active series), and for the strategic
vision this whole R-series implements see `mesh_world_revival.md`
(read that first if you need to understand *why*, not just *what*)._

---

## 1. Project summary

**MeshWorld** is an offline-first, procedurally-generated 3D world
explorer written in C++23. It generates city/world content as MC3 XML
(and, increasingly, MC3 JSON) scenes — MeshCraft's scene format, from
the sibling `openeggbert/mesh-craft` repo — and renders them via
MeshCraft's `SceneRenderer` (SDL3 + OpenGL ES 3) in
`apps/mesh-world-app`.

The project is mid-migration, per `mesh_world_revival.md`'s thesis: away
from ad hoc Lua/C++ primitive generators, toward a **C++ world composer**
(`BuildingComposer`) that selects and places reusable, metadata-tagged
MC3 assets from global libraries (`data/mc3lib/*.mc3lib.json`). Lua is
being demoted to a migration/compatibility layer, never deleted
outright. Modern rendering (PBR/shadows/AO) is intentionally deferred
until content and composition are proven — "a single convincing city
block is more valuable than a planet full of sparse boxes."

## 2. Architecture rules (never break)

- **Offline-first.** No Claude/OpenAI API calls at runtime; no API keys
  required by end users. Any AI-assisted content generation is an
  offline/dev-time, BYOK-only tool — never a runtime dependency.
- **`MeshWorldLib` (the core library) must never link SDL3/OpenGL/CNA.**
  Rendering stays isolated in `apps/mesh-world-app`, gated behind the
  `MESH_WORLD_HAS_RENDERER` compile definition.
- **Two subsystems, cleanly separated:** the planetary map layer
  (`namespace MeshWorld::Map`) sits above the legacy chunk/city layer
  (flat `namespace MeshWorld`). The map layer must never depend on
  chunk-layer types.
- **Lua is a compatibility/migration layer, not the long-term content
  source.** Every chunk/map generator tries a Lua script first, falling
  back to a C++ generator — but new generator content should not be
  added in Lua; the C++ world composer is the intended new path. The
  G-series (new Lua generator content) is paused — do not resume it
  without asking.
- **No `.lua` file is deleted just because it becomes "archival-ready."**
  Retiring Lua is a deliberate, later Stage 5 decision once the composer
  path is validated in practice, not automatic (see RR4 in
  `docs/risk-register.md`).
- **Non-reproducible worlds at the planetary/map layer** (persistence via
  SQLite, never a re-derived fixed seed); chunk/city demo configs (e.g.
  `examples/world.json`, `examples/city_showcase.json`) deliberately use
  a fixed `seed` for reproducible demo output — a different, intentional
  convention.
- **Unregistered materials are a warning, not a validation error**
  (deliberate `MC3Validator` behavior — do not flip to a hard error
  without asking).
- **`WorldConfig::use_world_composer` defaults to `false`.** Existing
  worlds/tests must be unaffected unless explicitly opted in.
- C++23, CMake 3.20+, MIT license, `-Wall -Wextra -Werror`, SPDX header
  (`// SPDX-License-Identifier: MIT`) on every new file.
- All MC3 serialization goes through `Mc3Document::saveToFile()` /
  `Mc3SceneBuilder` — never a competing/raw XML builder.

## 3. Current R-series status

Using `mesh_world_revival.md` §20's own 7-stage Migration Strategy as
the yardstick (tracked in detail in `docs/migration-stages.md`):

| Stage | Status |
|---|---|
| 1. Audit & preserve | Done (R107) |
| 2. Shared `mc3.json`/import foundations | Done (R109, R131) |
| 3. Global libraries | Done for a first batch (R112: 7 urban categories) |
| 4. Compose a city showcase | **Fully done (R113/R126/R127/R128/R129)** — `BuildingComposer` covers all 4 common region types (`small_house_block`/`apartment_block`/`shop_street`/`square`) + a landmark + LOD wiring; DS2/DS4 visual review (`docs/risk-register.md`) is still pending |
| 5. Demote runtime Lua | In progress at the map layer only (R105) |
| 6. Nature/underground expansion | Foundation landed (R142 biome tour; R143a-c tagged kits plus forest clearing and wetland/coast variants); other density/budgets and terrain remain R143-R144 |
| 7. Modern rendering | Not started (R120, cross-repo CNA/NOXNA) |

**R125-R130 roadmap — fully closed (2026-07-18).** All 6 tasks landed,
closing Stage 4 completely and extending `MC3Validator` (see `docs/
migration-stages.md`):
- **R125.** Curated a bounded, composer-relevant slice of the
  `object.sqlite3` brainstorm database (`object.md`) into the real,
  git-tracked `data/taxonomy/taxonomy.json` / `data/taxonomy/
  containment.json` (99 nodes / 80 rules, up from 64/56).
- **R126.** `BuildingComposer` composes real `RegionType::apartment_block`
  content via a new `apartment.block.wide_01` asset and its own parcel
  size class.
- **R127.** `BuildingComposer` composes real `RegionType::shop_street`
  content via a new `shop.building.storefront_01` asset (box-composed
  shopfront/awning, since the real R112 socket-based shopfront window
  asset needs R104 to attach).
- **R128.** One new `landmark.clocktower_01` asset placed via a new
  `WorldConfig::landmarks`/`ctx.landmark` mechanism (used by
  `examples/city_showcase.json`, which now also sets
  `use_world_composer: true`), plus LOD wiring
  (`BuildingComposer::resolve_instance_id()`) so `ctx.lod` now actually
  resolves composer-placed instances to their authored low-LOD proxy.
- **R129.** `BuildingComposer` extended to `RegionType::square` via a new
  `compose_square()` path — closes Stage 4 completely (all 4 common
  region types are now composer-covered). Also root-caused and fixed a
  real `"zone"` metadata bug that was previously an open known-bug item.
- **R130.** `MC3Validator` extended with the remaining single-chunk-
  computable §21.2/§21.3/§21.4 checks: texture-reference/material-slot/
  LOD-mapping validity (R130a), terrain-penetration plus new
  `instance_count`/`triangle_count`/`draw_call_estimate` fields on
  `ValidationResult` (R130b). Genuinely cross-chunk checks (building-
  road intersection, cross-chunk road continuity, socket/cave-
  connection alignment) and shadow-caster/LOD-reduction-ratio metrics
  remain deliberately deferred — see `plan.md`'s R122/R130 entries.

All 6 verified via `MeshWorldExport --validate` (0 errors/0 warnings
each) and the full `MeshWorldTests` suite (**1632/1634 pass** as of
2026-07-18 — the 2 failures are pre-existing, run-order-dependent
flaky tests, not a regression; see `NEXT.md` §2/§5). See `plan.md`'s
R125-R130 entries for full per-task write-ups.

**R131 (2026-07-25) closes Stage 2.** `schemas/mc3.schema.json` is a
strict Draft 2020-12 contract for the semantic MC3 JSON writer output and
the tracked `.mc3lib.json` data. It is verified against every tracked
library and malformed-document cases without adding a MeshWorld runtime
dependency; parser compatibility and `MC3Validator` semantic checks remain
separate concerns.

**R132 (2026-07-25) closes DS7 for static assets.** `MeshWorldPruneMc3Lib`
walks selected definitions' instance/variant/LOD dependencies and emits a
minimal ordinary MC3 or MCB output. It rejects assets with runtime scripts,
whose dynamic placements remain deliberately in R104's separate scope.

**R133 (2026-07-25) closes the immediate explorer-collision gap.** Core-only
MC3 collision extraction now resolves transformed instance bounds from
`assetMetadata.collisionProxy`; the app retains capsule slide movement via a
testable core helper. Regenerated libraries mark decorative props passable and
vehicles solid. Eight focused tests and all 49 showcase export validations
pass; full regression is 1646/1647 with only the known flaky map test. The
app source compiles strictly, while a new Xvfb run is blocked by NanoSVG DNS.
MeshCraft was not modified.

**R134 (2026-07-25) closes canonical configured/persistent road continuity.**
`WorldMap` now exposes a symmetric physical road graph separately from parcel
frontage; config-listed termini and export validation reject unexplained road
ends. Coarse map crossings are context hints only, preventing them from
creating one-sided 64 m road geometry. R137 is the future exact map-road
materialisation follow-up.

**R138 (2026-07-25) makes the fixed graph visible in the actual app.** The
Lua-first road/crossroad generators now obey exact canonical arms, explicit
road/crossroad layout cannot be replaced by incidental map zoning, and the app
uses a non-destructive versioned showcase cache. Traffic heads have a stable
deterministic red/amber/green MC3 snapshot (not animated states). Full
regression is 1645/1645 passed; MeshCraft was not modified.

**R140 (2026-07-25) contains explorer frame cost.** Interactive streaming is
radius 2 (13 chunks maximum), whole chunks outside the camera are culled
before MC3 traversal, parsed document cache is bounded to resident chunks, and
unchanged placement query extents no longer re-run SQLite every frame. Full
regression is 1648/1648 passed; MeshCraft was not modified.

**R142 (2026-07-25) makes the missing natural environments directly
reachable.** The app now has an authored forest-start biome showcase with
jungle, mountain, tundra, meadow, desert, beach, swamp, and ocean nearby;
all MAP16 biome families route to a closest established natural generator
instead of blank `EmptyGenerator`. It also fixes natural beach/forest bounds
defects and missing material registrations exposed by the full tour. This is
not bespoke Stage-6 asset composition: R143 owns that, while R144 owns actual
planet elevation/hydrology terrain coupling. Export validation is 81/81
chunks with 0 errors/0 warnings; full regression is 1651/1651 passed.

**R143a-c (2026-07-25) starts real natural asset composition.** The first
`nature.*` MC3 definitions cover forest, jungle, desert, mountain, wetland,
and coast, each with metadata/bounds/collision choice/low LOD and deterministic
selection through `AssetRegistry` by the natural C++ generators. R143b also
makes the forest clearing a deterministic canopy exclusion zone for all 32
scattered and 10 clustered trees; R143c gives wetland and coast two real
selection candidates each. R143 remains open for other-family density masks,
visual budgets and screenshots.

**R146 (2026-07-25) contains interactive disk writes.** The app's disposable
chunk-XML cache is now capped at 256 entries and skips byte-identical rewrites;
core/export callers retain their unlimited cache semantics. Existing saves are
not deleted by this policy.

**Next destination:** R143 is the highest-value continuation: actual reusable
nature assets and C++ composition for the visible biome families. R144 then
connects planetary elevation/rivers/coasts to continuous 3D terrain; R145 is
the target-hardware visual review/iCity comparison once a local iCity path is
available. R135/R136/R137 remain independent city alternatives. R139
(persistent-city topology) additionally needs a user decision on existing
SQLite layouts. R141 is profile-led object culling/batching and needs separate
MeshCraft approval. R104 is a separate MeshCraft-repository effort owned by
another agent and remains out of scope here.

**Explicitly out of scope right now** (see `NEXT.md` §9 "Do not do
yet"): R104 (mesh-craft `<script>` engine execution — cross-repo,
needs separate sign-off), R115 (AI content factory), R116-R118
(nature/mountains/caves), and R120 (PBR/rendering).

## 4. Resume prompt

```
Read CLAUDE.md first (this file), then NEXT.md for full current-state
detail (build/test status, known bugs, architecture notes, useful
commands). Both point to plan.md for historical detail and
mesh_world_revival.md for the strategic vision — read that one too if
you need to understand *why* a task is scoped the way it is, not just
*what* to do.

The R125-R132 roadmap is fully closed and R142 is the completed nature-entry
slice: Stage 4 is done, `MC3Validator`
covers all single-chunk-computable checks, `schemas/mc3.schema.json` closes
Stage 2, static dependency pruning closes DS7, R133 closes rendered-instance
collision proxies, R134 closes configured/persistent road continuity, and
R138 makes the Lua-first explorer honour it; R140 contains the first proven
explorer costs. R143 is the next authorized nature task; R144 depends on its
terrain design, and R145 needs target-hardware evidence/iCity source. R135/
R136/R137 require separate direction; R139 needs a persistence/migration
choice, and R141 needs profiling plus MeshCraft approval. Do not begin a
second one in the same session, explore unrelated modules, or refactor
anything outside the task.

For each task:
- Implement the minimal, focused change.
- Add/update tests covering the new behavior (see plan.md's existing
  R1xx entries for the expected test-writing style/rigor).
- Run the exact verification command described for that task (or the
  relevant ./build/MeshWorldTests subset), then the full test suite once
  before considering the task done.
- Update plan.md (append the task's write-up, matching its existing
  per-entry style exactly), NEXT.md (§2/§3/§8 as needed), and this
  file's §3/§4 when finished.

Do not start a second R13x task in the same session unless the first is
fully done and verified. Do not touch anything listed under "Explicitly
out of scope right now" above without asking first.
```
