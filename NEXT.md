# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after the
**chunk-system redesign** (plan.md §12.1 item 19: unbounded, streamed world,
7 phases) and, immediately after in the same day, **the block roster expansion
to Craft's full 54-item set** (plan.md §12.1 item 25: Chest, the other 5
flower colors, all 32 dye colors) — both shipped on branch `develop`.

## 1. Project summary

CNA Craft is a first-person voxel-world game/prototype, built entirely on
[CNA](https://github.com/openeggbert/cna) — a C++ reimplementation of the
XNA 4.0 programming model — and its `sharp-runtime` utility layer. The goal
is a faithful CNA-native **port of [fogleman/Craft](https://github.com/fogleman/Craft)**
(MIT-licensed reference project; see `THIRD_PARTY_NOTICES.md`), not merely a
"Craft-inspired" prototype.

**Primary sources of truth, in order**:
1. [CRAFT_PARITY.md](CRAFT_PARITY.md) — the systematic feature-by-feature
   parity audit against the real Craft source. ~35 feature areas, each
   citing exact Craft file/line and exact cna-craft file/line, with a
   status (`complete`/`partial`/`missing`/`blocked`/`needs_human`).
2. `plan.md` §12 ("Craft Feature Parity Port") — converts `CRAFT_PARITY.md`
   into an ordered, numbered task list (24 items) with the same status
   vocabulary. **This supersedes `plan.md` §11's older backlog** wherever
   they disagree.
3. This file — a handoff summary, not an independent source of truth. Keep
   it in sync with the two files above, not the other way around.

**Key architectural decisions, updated this session**: the world is now
**unbounded and chunk-streamed** in X/Z (packed-`(cx,cz)`-keyed hash map of
columns), not the old fixed `128×64×128` array — Y stays fixed
(`WORLD_CHUNKS_Y=4`), matching Craft's own real behavior. Generation and
meshing are backgrounded via `sharp-runtime`'s `System::Threading::Tasks::
TaskT` — this project's first use of threading. Everything else is
unchanged: two-layer split (`Worlds/` engine-agnostic, `Render/` +
`CnaCraftGame` CNA-dependent), naive per-face meshing (plus a cross-billboard
path for plants), backend-agnostic game code, two-mesh-buffer
(opaque/transparent) convention.

## 2. Current status

**Build status**: verified at the end of this session (EasyGL backend) —
clean configure + build from scratch, zero warnings, `CnaCraft` executable
links and runs. Verified via real interactive Xvfb/xdotool/ImageMagick
screenshots for both pieces of work: the chunk redesign (flew far beyond the
old 128×128 boundary in multiple directions, confirmed terrain streams in
cleanly with no permanent visual holes — a real bug found and fixed, see
§3 — flew back toward spawn, confirmed no crashes/leaks and a bounded
thread count throughout) and the roster expansion (cycled the hotbar
through all 54 slots via `E`, confirmed the HUD showed the right name at
each slot, confirmed naturally-generated flowers now render in more than
one color side by side).

**Test status**: `tests/worlds_smoke_test.cpp` — **226 checks, all
passing** (up from 173 at the start of the chunk-redesign session). Plus
`cna_craft_persistence_smoke_test` — **30 checks, all passing** (up from
21). Both build and run standalone with `-DCNA_CRAFT_BUILD_GAME=OFF`.

**plan.md §12.1 status as of this session's end** (25 items):
- **22 `completed`**: everything from before, plus item 19 (chunk-system
  redesign) and item 25 (full 54-item block roster) — see §3 below.
- **1 `blocked`**: ambient occlusion (needs a custom `ShaderEffect`, only
  real on EASYGL today).
- **1 `pending (large)`**: Chat/slash-commands — shares the text-input
  state machine Signs already built, but Craft's real command set needs
  world-editing primitives (`/cube`, `/sphere`, `/tree`, `/array`, `/copy`,
  `/paste`, etc.) that don't exist in this codebase yet.
- **1 `pending`, explicitly deferred**: multiplayer, per project direction
  (all its usual prerequisites — persistence, chunk logic — are now done,
  but multiplayer itself stays deferred).

## 3. Recent changes (this session)

All individually committed and pushed to `develop`. This was a large,
multi-phase architecture change — planned via `EnterPlanMode` (three
research passes + a design-review pass) before any code was touched, then
implemented in 7 phases, each independently built/tested before the next
started (plan.md §12.1 item 19 has the full per-phase writeup):

1. **Phase 0** — SQLite schema migration: added Craft's real `p,q`
   chunk-address columns to `block`/`sign`. Pre-migration `world.db` files
   now fail loudly at open instead of silently misbehaving. No migration
   path — delete `world.db` to upgrade.
2. **Phase 1** — `World`'s storage rebuilt as a two-level hash map keyed by
   packed `(cx,cz)` — unbounded X/Z, Y still fixed. `GenerateColumn`/
   `IsColumnLoaded`/`UnloadColumn` new API; `World::Generate(seed)` kept as
   a legacy whole-region wrapper so most pre-existing tests needed zero
   changes. Ported Craft's real tree-canopy chunk-boundary margin check as
   a documented, deliberate minor regression (a few boundary-adjacent trees
   that used to spawn no longer do).
3. **Phase 2** — `WorldStore` per-chunk-scoped I/O (`LoadColumnInto`/
   `LoadColumnSignsInto`/`LoadColumnEdits`, `WHERE p=? AND q=?`).
4. **Phase 3** — `CnaCraftGame` streaming integration, synchronous first
   (deliberate de-risking before threading): `kCreateRadius=6`/
   `kDeleteRadius=9` chebyshev load/unload, spawn moved to world-origin,
   `chunkRenderers_` became a hash map (also fixed a latent correctness bug
   — the old flat vector was only ever kept in lockstep with `World` by
   construction order), signs wired to column load/unload, floor-catch
   fixed for unloaded columns, fog now tracks `kCreateRadius * CHUNK_SIZE`.
5. **Phase 4** — backgrounded generation + meshing via `TaskT`. Found
   `TaskT<T>::getResultProperty()` requires `T` copy-constructible (a
   `shared_ptr`-reached member return, not a named local — no implicit
   move), which ruled out returning a move-only chunk array; solved via
   `World::CopyColumn`/`AdoptColumnCopy`/`InstallChunkCopy`. **Real bug
   found via visual verification and fixed**: completed-but-over-the-
   per-frame-apply-cap jobs were discarded outright instead of deferred —
   harmless for generation, but for meshing (whose dirty flag clears at
   *dispatch* time) this caused **permanent unfilled rectangular holes** in
   distant terrain. Fixed by deferring instead of discarding, plus added a
   missing dispatch cap on meshing that could otherwise fan out into dozens
   of concurrent tasks per column arrival. Re-verified via matched-
   flight-path screenshot comparisons.
6. **Phase 5** — added the three genuinely new-risk tests: column
   load/unload lifecycle, boundary-remesh correctness (a boundary face is
   exposed while its neighbor column is unloaded, correctly culled once the
   neighbor loads with a solid block across the shared boundary), and a
   real generate→edit→save→unload→regenerate→reload persistence cycle for
   one column.
7. **Phase 6** — this documentation pass: `CRAFT_PARITY.md` §3.1/§3.2/
   §4.2/§4.3/§5.2, `plan.md` §12.1 item 19's writeup, `README.md` (world.db
   reset note, unbounded-world player-visible behavior), this file.

Test count progression: 173 → 199 (phases 1-3, incremental) → 207 (phase 5)
in `worlds_smoke_test`; 21 → 26 (phase 0/2 schema/column tests) → 30
(phase 5) in `persistence_smoke_test`.

8. **Block roster expansion to Craft's full 54 items** (plan.md §12.1 item
   25, CRAFT_PARITY.md §2.2/§3.7), picked up immediately after item 19 in
   the same day, per user choice between it and Chat/slash-commands. Added
   `Chest` (plain solid cube, no special mesh), the other 5 flower colors
   (`Red/Purple/Sun/White/BlueFlower` — reuse the existing plant-billboard
   path unchanged), and all 32 dye colors (`Dye00`-`Dye31`, plain solid
   cubes). **New `BlockType` values appended after `Bedrock`, never
   inserted earlier** — `WorldStore` persists `BlockType` as its raw enum
   ordinal, so inserting mid-roster would silently reinterpret every
   existing `world.db`'s saved blocks; `Hotbar::kSlots` likewise appends
   its 38 new slots after the original 16 so number keys 1-9 and every
   pre-existing test assertion keep meaning exactly what they always have.
   `TextureAtlas`'s grid grew 5×5 → 8×8; the 32 dye tiles use real RGB
   values sampled directly from Craft's own shipped `texture.png` (its dye
   tiles are flat swatches there too, so there was no "procedural pattern"
   to invent — matching the real asset was both easier and more faithful).
   Also ported Craft's real flower color-pick noise (`world.c`) so world
   generation actually scatters all 6 colors, not just the hotbar. Fixed a
   test bug caught by the suite itself while writing this: an early version
   of the new Chest/flower BlockDef test reused one `World` across sub-checks
   and picked up Chest's opaque geometry in a plant's "should be empty"
   mesh assertion — split into isolated `World` instances per sub-check.
   226 checks in `worlds_smoke_test` (up from 207), persistence suite
   unchanged at 30. Verified against a real EasyGL build under Xvfb: cycled
   the hotbar through all 54 slots via `E`, confirmed the HUD showed
   `#21/54 WhiteFlower` through `#35/54 Dye12` correctly, and a screenshot
   showed naturally-generated orange/yellow and purple flower blooms side
   by side, confirming the color-pick noise actually varies.

## 4. Current blocker / main problem

**None.** Clean build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and
`-DCNA_GRAPHICS_BACKEND=EASYGL`, built from scratch), 207/207 + 30/30 tests
passing, zero compiler warnings.

Carried over, still unresolved, still not urgent: mouse-look reliability
confirmation on the user's real (non-sandboxed) machine — `needs_human`,
unchanged for several sessions now (see `plan.md` §11.0/§12.1 for history).

**New, deliberate, documented behavior changes from this session** (not
bugs — read before "fixing"):
- Player-driven block edits and freshly-streamed-in terrain render 1-2
  frames later than the old same-frame synchronous rebuild (backgrounded
  meshing) — imperceptible at 60fps.
- A handful of trees right at a chunk-column boundary no longer spawn
  (Craft's own real per-column margin-check behavior, previously
  unreplicated since the whole world used to generate as one unit).
- No occlusion/frustum culling exists anywhere in the engine, and now the
  world is unbounded — every loaded column within `kCreateRadius` draws
  every frame regardless of visibility. Not a problem at the current
  radius (6 chunks), but worth remembering if the radius is ever increased.

## 5. Known bugs and limitations

See `CRAFT_PARITY.md` for the authoritative, cited, per-feature list — read
it before re-deriving anything from memory or re-reading Craft's source
from scratch. Highlights of what's NOT done yet:

- **`blocked`**: ambient occlusion — needs a custom vertex format +
  `ShaderEffect`, only real on EASYGL today (`missing.md`).
- **`pending (large)`**: Chat/slash-commands — shares Signs' text-input
  state machine (now built and reusable) but additionally needs Craft's
  world-editing macro commands (`copy()/paste()/tree()/array()/cube()/
  sphere()/cylinder()`), none of which exist in this codebase.
- **Deliberately deferred**: multiplayer.
- **Not ported (out of scope this pass)**: Craft's player-position `state`
  table (spawn/camera position persistence) — this project always spawns
  at the deterministic world-origin column; revisit only if that stops
  being desired.
- **Low-value, not picked up**: the other 5 of Craft's 6 flower colors,
  Chest, the 32-entry dye-color palette — all would reuse existing
  infrastructure (plant geometry or plain-cube blocks) but are pure
  content-scaling with little gameplay value, explicitly deprioritized per
  "prioritize gameplay parity over decorative additions."

## 6. Architecture notes

**New this session** (see plan.md §12.1 item 19 for full per-phase detail —
this is just the "if you touch this again" summary):

- `Worlds/World` — storage is now `unordered_map<ColumnKey,
  array<unique_ptr<Chunk>, WORLD_CHUNKS_Y>> columns_`, keyed by
  `World::PackColumnKey(cx,cz)`. `GetBlock`/`SetBlock`/etc. degrade
  gracefully for an unloaded column exactly like they used to for an
  out-of-range coordinate (returns `Air`, writes are no-ops) — same
  pattern, new meaning. `World::Generate(seed)` is a **legacy wrapper**
  looping `GenerateColumn` over the old fixed region — still used by test
  code and any caller that just wants "a whole small world," don't remove
  it without checking callers first.
- `System::Threading::Tasks::TaskT<T>` (sharp-runtime, first use in this
  project) — **`T` must be copy-constructible**, not just movable
  (`getResultProperty()` returns via a `shared_ptr`-reached member, not a
  named local — doesn't qualify for implicit move-on-return). A captured
  lambda passed to `TaskT::Run` must itself be copy-constructible too
  (`std::function` requirement) — never capture a live/move-only `World` by
  value; capture plain copyable snapshot data and reconstruct a throwaway
  `World` **inside** the lambda body instead (see `World::CopyColumn`/
  `AdoptColumnCopy`/`InstallChunkCopy`, and `CnaCraftGame::
  DispatchColumnGeneration`/`DispatchMeshingForDirtyChunks`). Never erase an
  in-flight `TaskT` while `getIsCompletedProperty()` is false — the
  underlying `std::async` future's destructor blocks until the task
  finishes unless already awaited.
- **Apply-cap discipline** (`CnaCraftGame::PollGenerationJobs`/
  `PollMeshJobs`): a completed job that exceeds the per-frame apply cap
  must be **deferred to a later frame, never discarded** — discarding is
  only safe for a job that's genuinely no longer wanted (its column loaded
  some other way first). This is *load-bearing*, not just tidy, for
  meshing specifically: `DispatchMeshingForDirtyChunks` clears the dirty
  flag at *dispatch* time, so a discarded-but-completed mesh result leaves
  that chunk permanently un-meshed with nothing left to ever re-flag it.
  A real bug from exactly this mistake (permanent holes in distant
  terrain) was found and fixed this session — don't reintroduce it.
- `CnaCraftGame::chunkRenderers_` — now `unordered_map<ColumnKey,
  array<unique_ptr<ChunkRenderer>, WORLD_CHUNKS_Y>>`, keyed identically to
  `World`'s own column storage.
- `Persistence::WorldStore` — schema now has Craft's real `p,q` columns.
  **No migration path** from a pre-2026-07-10 `world.db` — it's detected
  and rejected at open (logged, falls back to the no-op store), not
  silently corrupted.
- `Worlds/BlockType` — **new values must always be appended after the last
  one (`Bedrock` as of this note), never inserted earlier.**
  `Persistence::WorldStore` persists `BlockType` as its raw `static_cast
  <int>` ordinal — inserting mid-enum would silently reinterpret every
  existing `world.db`'s saved block types on next load, with no error or
  warning. `Hotbar::kSlots` follows the same append-only convention for the
  same reason (existing slot numbers are load-bearing for muscle memory and
  tests). If you add a new block type, add it at the end of the enum and
  the end of `kSlots`, not wherever feels topically appropriate.

**Everything from before this session** (module list, boundaries, data
flow, signs, cursor-capture, pitch-coupled flight, floor-catch, etc.) is
unchanged — see `plan.md` §2/§6/§8 and prior `NEXT.md` history in git log
for that detail if needed.

**Boundaries that must not be broken** — unchanged, still load-bearing:
`Worlds/` must never `#include` anything CNA/SDL; any new `BlockDef`
boolean flag with a non-`false` default must be AND'd with `solid`
wherever consumed; two-mesh-buffer convention; never erase an in-flight
`TaskT` before it completes (new this session, see above).

## 7. Useful commands

```bash
# Engine-agnostic tests only (no CNA/GPU/display needed):
cmake -S . -B build-worlds -DCNA_CRAFT_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-worlds -j"$(nproc)"
ctest --test-dir build-worlds --output-on-failure
```
Expect: `WorldsSmokeTest` (207 `ok:` lines) and `PersistenceSmokeTest`
(30 checks) both pass.

```bash
# Full graphical game (requires ../cna and ../sharp-runtime as siblings):
cmake -S . -B build-easygl -DCNA_GRAPHICS_BACKEND=EASYGL
cmake --build build-easygl --target CnaCraft -j"$(nproc)"
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft          # interactive
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft --smoke 30
```
Note: `SDL_VIDEODRIVER=x11` matters in sandboxes where `WAYLAND_DISPLAY` is
also set — otherwise SDL silently creates a Wayland surface that X11
screenshot tooling (`import -window root`) can't see, even though the
process runs fine and `DISPLAY` is set correctly. Delete any pre-existing
`world.db` before running this build — see README.md's reset note.

There is no separate lint/format tooling configured in this repo.

## 8. Next smallest tasks

`plan.md` §12.1 is the authoritative ordered priority queue. As of this
session's end, there is exactly **one** remaining real task:

1. **Chat/slash-commands** (CRAFT_PARITY.md §4.5, plan.md item 17). The
   text-input state machine is built and reusable (see
   `CnaCraftGame::Update`'s typing branch) — the remaining scope is Craft's
   world-editing macro commands (`/cube`, `/sphere`, `/tree`, `/array`,
   `/copy`, `/paste`, etc.), which need new world-editing primitives that
   don't exist in this codebase yet. Large enough scope (comparable to the
   chunk redesign in that it needs new primitives, not just a same-shape
   port) that it likely deserves its own `EnterPlanMode` pass before
   diving in, same reasoning as item 19. Scope this as its own session.

The low-value content additions (extra flower colors, Chest, dye palette)
that used to sit here are now done (item 25, see §3). Multiplayer stays
explicitly deferred per project direction (not "next" — just no longer
blocked on anything else).

No other item currently needs a design pass — everything else remaining
(`blocked`/deferred) is waiting on either a human decision (shader-backend
choice for AO) or a deliberate scope decision (multiplayer timing), not on
more design work.

## 9. Do not do yet

Everything from prior sessions still applies (no multiplayer, no AO/
greedy-meshing rewrite without a shader-backend decision, no broad
`Worlds/`/`Render/` refactor, no `PlayerController` public-API changes
without re-running the full suite, no `Hotbar::kSlots` reordering, no
re-adding a dedicated fly-descend key, no rushed Chat/slash-commands
implementation, no `SkyDome`/`SelectionOutline`/`SignBillboard` winding
"cleanup" without real screenshot verification, no `Sign.hpp`
face-convention change to match Craft's asymmetric scheme) — **plus**:

- **No re-running the chunk-system redesign, terrain-formula, fly-speed,
  pitch-clamp, or reach-distance changes** — all already done (see §3);
  don't re-investigate these as if they were still open.
- **Don't discard a completed-but-over-cap background job in
  `PollGenerationJobs`/`PollMeshJobs`** — defer it to a later frame
  instead. See §6's apply-cap note; this exact mistake caused a real,
  hard-to-spot visual bug this session (permanent holes in distant
  terrain) that only showed up under real interactive play, not in any
  unit test.
- **Don't capture a live `World` (or anything move-only) by value in a
  lambda passed to `TaskT::Run`** — it won't compile (`std::function`
  needs the closure copy-constructible), and even if it did, background
  code must never touch the live `World` concurrently with the main
  thread. Capture plain copyable snapshot data instead.
- **Don't increase `kCreateRadius` without also addressing culling** — see
  §4's note; render cost scales with loaded-column count and nothing
  culls off-screen chunks today.
- **Never insert a new `BlockType` value earlier than the last one, and
  never reorder `Hotbar::kSlots`** — see §6's note; both are append-only
  for real persistence-compatibility and test-stability reasons, not just
  tidiness.

## 10. Resume prompt

```
Read CRAFT_PARITY.md first (the authoritative Craft-vs-cna-craft parity
audit), then plan.md §12.1 (the ordered priority queue derived from it) —
as of the last session, 22 of 25 items are completed (including item 19,
the chunk-system redesign to an unbounded/streamed world, and item 25, the
full 54-item block roster), 1 blocked (ambient occlusion), 1 pending-large
(Chat/slash-commands), 1 pending deferred (multiplayer). The one real
remaining task is Chat/slash-commands (reuse the text-input state machine
already built for Signs, CnaCraftGame::Update's typing branch — the
remaining scope is Craft's world-editing macro commands). Unlike the
content-roster addition, this one is comparable in scope to the chunk
redesign (needs new world-editing primitives, not just a same-shape port)
and likely deserves its own EnterPlanMode pass before diving in. Before
implementing anything that cites Craft's source code, re-verify the
citation against the real checkout at
/rv/data/development/github.com/other/Craft — CRAFT_PARITY.md was
carefully audited but could still contain an error, same as every prior
citation pass in this project's history. Make one small, verified
improvement at a time: implement, build (cmake --build build-worlds, or
the full EasyGL build if it touches Render/ or CnaCraftGame), run the
relevant test/smoke command, confirm it actually passes — and for anything
touching CnaCraftGame/Render, verify visually via a real Xvfb/xdotool/
ImageMagick screenshot cycle (remember: SDL_VIDEODRIVER=x11 is required in
this sandbox, and xdotool keydown/keyup — not plain xdotool key — for
reliable key-hold detection). If you touch the chunk-streaming/threading
code (World column storage, CnaCraftGame's Dispatch*/Poll* methods),
re-read §6's TaskT/apply-cap notes first — there's a real, subtle,
already-found-once bug class there (discarding instead of deferring
completed-but-over-cap jobs). When finished, update plan.md §12.1's status,
update CRAFT_PARITY.md's corresponding entry, and update this file's
"Current status"/"Recent changes".
```
