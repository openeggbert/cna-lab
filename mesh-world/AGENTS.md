# CLAUDE.md — MeshWorld Canonical Handoff

_Last updated: 2026-07-18. This is the canonical entry point for Claude
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
| 2. Shared `mc3.json`/import foundations | Done except JSON Schema (R109) |
| 3. Global libraries | Done for a first batch (R112: 7 urban categories) |
| 4. Compose a city showcase | Mostly done (R113/R126/R127/R128) — `BuildingComposer` covers `small_house_block`/`apartment_block`/`shop_street` + a landmark + LOD wiring; `square` still legacy-only, and DS2/DS4 visual review (`docs/risk-register.md`) is still pending |
| 5. Demote runtime Lua | In progress at the map layer only (R105) |
| 6. Nature/underground expansion | Not started (R116-R118) |
| 7. Modern rendering | Not started (R120, cross-repo CNA/NOXNA) |

**R125-R128 roadmap — fully closed (2026-07-18).** All 4 tasks landed,
closing most of Stage 4 (see `docs/migration-stages.md`):
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

All 4 verified via `MeshWorldExport --validate` (0 errors/0 warnings
each) and the full `MeshWorldTests` suite. See `plan.md`'s R125-R128
entries for full per-task write-ups.

**Next destination (not yet scoped/started — ask before picking one):**
Stage 4's remaining gap is `square`'s composer coverage plus an actual
visual/screenshot review (`docs/risk-register.md` DS2/DS4 are
data-verified but explicitly NOT visually reviewed yet). Beyond that,
the natural next moves per the risk register's own RR1/RR6 sequencing
are Stage 5 (further Lua demotion), Stage 6 (nature/mountains/caves,
R116-R118), or the cross-repo R104 sign-off that would unblock actually
placing the still-unused window/door/roof/facade-module R112 content.

**Explicitly out of scope right now** (see `NEXT.md` §9 "Do not do
yet"): R104 (mesh-craft `<script>` engine execution — cross-repo,
needs separate sign-off), R115 (AI content factory), R116-R118
(nature/mountains/caves), R120 (PBR/rendering), and the small cosmetic
`"zone"` metadata bug — all correctly sequenced after Stage 4 closes.

## 4. Resume prompt

```
Read CLAUDE.md first (this file), then NEXT.md for full current-state
detail (build/test status, known bugs, architecture notes, useful
commands). Both point to plan.md for historical detail and
mesh_world_revival.md for the strategic vision — read that one too if
you need to understand *why* a task is scoped the way it is, not just
*what* to do.

The R125-R128 roadmap is fully closed. Do NOT invent a new task on your
own — ask the user which of "3. Current R-series status"'s "Next
destination" options (or something else entirely) to scope next, THEN
inspect only the files that task concerns. Do not go exploring unrelated
modules, and do not refactor anything you encounter along the way that
isn't part of the task.

For each task:
- Implement the minimal, focused change.
- Add/update tests covering the new behavior (see plan.md's existing
  R1xx entries for the expected test-writing style/rigor).
- Run the exact verification command described for that task (or the
  relevant ./build/MeshWorldTests subset), then the full test suite once
  before considering the task done.
- Update plan.md (append the task's write-up, matching its existing
  per-entry style exactly) and NEXT.md (§2/§3 as needed) when finished.

Do not start a second R12x task in the same session unless the first is
fully done and verified. Do not touch anything listed under "Explicitly
out of scope right now" above without asking first.
```
