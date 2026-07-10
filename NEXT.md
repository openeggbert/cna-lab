# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after three
back-to-back sessions on the same day (2026-07-10), all shipped on branch
`develop`: the **chunk-system redesign** (plan.md §12.1 item 19: unbounded,
streamed world, 7 phases), **the block roster expansion** to Craft's full
54-item set (item 25: Chest, the other 5 flower colors, all 32 dye colors),
and **chat/slash world-editing commands** (item 17: `/cube`, `/sphere`,
`/copy`+`/paste`, `/view`, etc.). With item 17 done, `plan.md` §12.1 has
exactly one real remaining item (Chat/slash-commands' own former
placeholder is now filled — the only thing left is `blocked` ambient
occlusion and explicitly-deferred multiplayer; see §8).

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

**Key architectural decisions, updated across these sessions**: the world is
now **unbounded and chunk-streamed** in X/Z (packed-`(cx,cz)`-keyed hash map
of columns), not the old fixed `128×64×128` array — Y stays fixed
(`WORLD_CHUNKS_Y=4`), matching Craft's own real behavior. Generation and
meshing are backgrounded via `sharp-runtime`'s `System::Threading::Tasks::
TaskT` — this project's first use of threading. The block roster is
Craft's full 54-item set. Typing now generalizes to two modes (Sign,
Command via `/`), backed by a new engine-agnostic `Worlds/WorldEditor`
module for world-editing commands. Everything else is unchanged: two-layer
split (`Worlds/` engine-agnostic, `Render/` + `CnaCraftGame` CNA-dependent),
naive per-face meshing (plus a cross-billboard path for plants),
backend-agnostic game code, two-mesh-buffer (opaque/transparent)
convention.

## 2. Current status

**Build status**: verified at the end of this session (EasyGL backend) —
clean configure + build from scratch, zero warnings, `CnaCraft` executable
links and runs. Verified via real interactive Xvfb/xdotool/ImageMagick
screenshots across all three pieces of work: the chunk redesign (flew far
beyond the old 128×128 boundary in multiple directions, confirmed terrain
streams in cleanly with no permanent visual holes — a real bug found and
fixed, see §3 — flew back toward spawn, confirmed no crashes/leaks and a
bounded thread count throughout), the roster expansion (cycled the hotbar
through all 54 slots via `E`, confirmed the HUD showed the right name at
each slot, confirmed naturally-generated flowers now render in more than
one color side by side), and chat/slash-commands (confirmed `/` opens the
Command typing box, confirmed `/view 20` updates both the console and the
new on-screen message log, confirmed an invalid `/view 99` and an
unrecognized `/nonsense` both produce Craft-accurate feedback that stacks
correctly in the message log, confirmed the log persists after the typing
box closes, confirmed Sign typing is unaffected by the `TypingMode`
generalization).

**Test status**: `tests/worlds_smoke_test.cpp` — **267 checks, all
passing** (up from 173 at the start of the chunk-redesign session). Plus
`cna_craft_persistence_smoke_test` — **30 checks, all passing** (up from
21). Both build and run standalone with `-DCNA_CRAFT_BUILD_GAME=OFF`.

**plan.md §12.1 status as of this session's end** (25 items):
- **23 `completed`**: everything from before, plus item 19 (chunk-system
  redesign), item 25 (full 54-item block roster), and item 17
  (chat/slash-commands) — see §3 below.
- **1 `blocked`**: ambient occlusion (needs a custom `ShaderEffect`, only
  real on EASYGL today).
- **1 `pending`, explicitly deferred**: multiplayer, per project direction
  (all its usual prerequisites — persistence, chunk logic, chat — are now
  done, but multiplayer itself stays deferred).

There is no other open `plan.md` §12.1 item — the priority queue this file's
own §8 used to point to is now empty except for the two above. Anything
left is either low-value content (§5) or genuinely out of scope for now.

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

9. **Chat/slash world-editing commands** (plan.md §12.1 item 17,
   CRAFT_PARITY.md §4.5) — picked up immediately after item 25, closing the
   last real gap in the priority queue. Planned via `EnterPlanMode` (same
   scope class as the chunk redesign) after reading Craft's real
   `parse_command` and every function it calls directly from the checkout.
   New `src/CnaCraft/Worlds/WorldEditor.hpp/.cpp`: 7 engine-agnostic
   geometry primitives (`PaintBlock`=`builder_block`, `FillCuboid`=`cube()`,
   `FillSphere`=`sphere()` — covers both `/sphere` and `/circlex|y|z` via
   flatten flags, exactly like Craft reuses it — `FillCylinder`=`cylinder()`
   (a stack of flattened `FillSphere` calls along one axis), `FillArray`=
   `array()`, `GrowTree`=`tree()`, `PasteRegion`=`paste()`) plus
   `Worlds::ExecuteCommand`, a direct port of `parse_command`'s dispatch
   chain for every world-editing command. The 5 multiplayer-auth commands
   and the plain-chat fallback are deliberately not ported (no networking,
   no one to talk to). `CnaCraftGame`'s sign-only `isTypingSign_` bool
   generalized to `TypingMode{None,Sign,Command}`; `/` (`Keys::
   OemQuestion`) opens Command typing pre-seeded with `"/"`. New `mark0_`/
   `mark1_` (mirrors Craft's `g->block0`/`g->block1`) updated via a new
   `RecordMark` at both break and place. `radii_` (`Worlds::CommandRadii`)
   replaces the compile-time `kCreateRadius`/`kDeleteRadius` as the mutable
   value `/view` mutates (clamped `1..24`); fog now derives from
   `radii_.createRadius` every frame instead of a compile-time constant.
   `Render::Hud` gained a real 4-line scrolling message log (`PushMessage`)
   for command feedback — **user decision (2026-07-10)**: full
   Craft-accurate multi-line log over a simplified single-line flash
   message, when asked to choose. Caught and fixed one real test-authoring
   bug via the suite itself: an early `PasteRegion` test accidentally chose
   source/destination coordinates that triggered a genuine, faithfully-
   reproduced Craft quirk (pasting a region whose destination Y range
   overlaps not-yet-read source rows can clobber source data mid-copy,
   since Craft's own `paste()` reads/writes the same live map in one
   ascending-y sweep) — not a bug in the port, fixed by choosing
   non-overlapping test coordinates instead, with the quirk itself now
   documented in `WorldEditor.cpp`. 41 new checks — 267 in
   `worlds_smoke_test` (up from 226), persistence suite unchanged at 30.
   Verified against a real EasyGL build under Xvfb: `/` opens the
   "Command: /_" typing box correctly, `/view 20` updates both the console
   log and the on-screen message log ("Viewing distance set to 20."), an
   invalid `/view 99` and an unrecognized `/nonsense` both produce
   Craft-accurate feedback that stacks correctly (oldest on top, persists
   after the typing box closes), and Sign typing (backtick) is unaffected.
   The world-editing paint commands themselves (`/cube`, `/sphere`, etc.)
   couldn't be verified live end-to-end — they need mouse-driven
   break/place to set real marks first, and synthetic mouse clicks into
   this sandbox's relative-mouse-mode SDL window remain unreliable (same
   documented flakiness as every other click-dependent feature in this
   project's history) — covered instead by the exhaustive `WorldEditor`/
   `ExecuteCommand` unit tests, arguably better regression coverage than a
   one-off manual screenshot would have been anyway.

## 4. Current blocker / main problem

**None.** Clean build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and
`-DCNA_GRAPHICS_BACKEND=EASYGL`, built from scratch), 267/267 + 30/30 tests
passing, zero compiler warnings.

Carried over, still unresolved, still not urgent: mouse-look reliability
confirmation on the user's real (non-sandboxed) machine — `needs_human`,
unchanged for several sessions now (see `plan.md` §11.0/§12.1 for history).
The same underlying sandbox limitation (unreliable synthetic mouse clicks
into a relative-mouse-mode SDL window) is also why the new `/cube`-style
world-editing commands' actual painting couldn't be verified live — see §3
item 9.

**New, deliberate, documented behavior changes from these sessions** (not
bugs — read before "fixing"):
- Player-driven block edits and freshly-streamed-in terrain render 1-2
  frames later than the old same-frame synchronous rebuild (backgrounded
  meshing) — imperceptible at 60fps.
- A handful of trees right at a chunk-column boundary no longer spawn
  (Craft's own real per-column margin-check behavior, previously
  unreplicated since the whole world used to generate as one unit).
- No occlusion/frustum culling exists anywhere in the engine, and now the
  world is unbounded — every loaded column within `radii_.createRadius`
  draws every frame regardless of visibility. Not a problem at the current
  default radius (6 chunks), but worth remembering if `/view` is used to
  increase it a lot, or the default is raised.
- A mismatched-type `/cube`/`/array`/`/cylinder` (marks aren't the same
  block type) now shows a message instead of Craft's own silent no-op — a
  small, deliberate UX improvement, not a fidelity gap.
- `PasteRegion` can clobber source data mid-copy if the destination Y range
  overlaps not-yet-read source rows in the *same* World — a real,
  faithfully-reproduced Craft quirk (same loop shape as Craft's own
  `paste()`), not a bug. Harmless for the common case (pasting somewhere
  that doesn't Y-overlap the source).

## 5. Known bugs and limitations

See `CRAFT_PARITY.md` for the authoritative, cited, per-feature list — read
it before re-deriving anything from memory or re-reading Craft's source
from scratch. Highlights of what's NOT done yet:

- **`blocked`**: ambient occlusion — needs a custom vertex format +
  `ShaderEffect`, only real on EASYGL today (`missing.md`).
- **Deliberately deferred**: multiplayer.
- **Not ported (out of scope)**: Craft's player-position `state` table
  (spawn/camera position persistence) — this project always spawns at the
  deterministic world-origin column; revisit only if that stops being
  desired. Craft's `/identity`/`/login`/`/logout`/`/online`/`/offline`
  commands (multiplayer auth) — no networking exists to authenticate
  against.
- **Nothing else identified**: the block roster (item 25) and chat/commands
  (item 17) categories that used to have "low-value, not picked up" entries
  here are both fully done now.

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
  one (`Dye31` as of this note), never inserted earlier.**
  `Persistence::WorldStore` persists `BlockType` as its raw `static_cast
  <int>` ordinal — inserting mid-enum would silently reinterpret every
  existing `world.db`'s saved block types on next load, with no error or
  warning. `Hotbar::kSlots` follows the same append-only convention for the
  same reason (existing slot numbers are load-bearing for muscle memory and
  tests). If you add a new block type, add it at the end of the enum and
  the end of `kSlots`, not wherever feels topically appropriate.
- `Worlds/WorldEditor` (new, plan.md §12.1 item 17) — engine-agnostic
  world-editing primitives + `Worlds::ExecuteCommand`, the `/`-command
  parser/dispatcher. Pure functions of their inputs (a `World&` plus plain
  coordinates/marks/clipboard/radii) — no CNA dependency, fully
  unit-testable, same pattern as `ChunkMesher`/`VoxelRaycast`/
  `PlayerController`. If you add a new command, add its dispatch branch to
  `ExecuteCommand` (mirrors Craft's real `parse_command` order) and its own
  `WorldEditor::` primitive if it needs new geometry, not inline logic
  inside `CnaCraftGame`.
- `CnaCraftGame::TypingMode` (replaces the old `isTypingSign_` bool) — a
  3-state enum (`None`/`Sign`/`Command`). If you add a third typing trigger
  (Craft's own bare-Enter-opens-empty-chat-buffer, not ported here since
  there's no multiplayer to chat with), add a new enum value, not a second
  bool — a bool can't represent "not typing" vs "typing, but which mode."
- `CnaCraftGame::mark0_`/`mark1_`/`clipboard_`/`radii_` — session state a
  command reads, updated by `RecordMark` (called at both break and place)
  and by `/copy`/`/view` respectively. `radii_` is the actual mutable
  source of truth for streaming/fog now — `kCreateRadius`/`kDeleteRadius`
  (the anonymous-namespace constants) are only ever read once, to seed
  `radii_` in `Initialize()`. Don't read the constants directly anywhere
  else, or a runtime `/view` change won't take effect there.
- `Render::Hud::PushMessage` — Craft's own 4-line message ring buffer
  (`MAX_MESSAGES`). Rebuilds its texture on every call (matches
  `SetTyping`'s "small texture, low-frequency interaction, per-call rebuild
  is cheap enough" reasoning) — don't call it every frame, only when a new
  message actually needs to appear.

**Everything from before these sessions** (module list, boundaries, data
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
Expect: `WorldsSmokeTest` (267 `ok:` lines) and `PersistenceSmokeTest`
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
session's end, **there is no remaining item in the queue at all** —
chat/slash-commands (item 17) was the last one, and it's done. What's left:

- **`blocked`, needs a human decision**: ambient occlusion — needs a
  shader-backend choice (only real on EASYGL today), not more design work.
- **Explicitly deferred, not "next"**: multiplayer, per project direction.
  Every one of its usual prerequisites (persistence, chunk streaming, chat)
  is now done, so if the user ever wants to pick it up, nothing else is
  blocking it — but don't start it without being asked.
- **Possible future polish, not scoped/requested yet**: Craft's real
  per-instance random Y-axis rotation for plant billboards (a cosmetic gap
  noted in §3.7), the player-position `state` table, occlusion/frustum
  culling if `/view`'s radius is ever pushed much higher than the default.
  None of these are "next" — just things a future session might reasonably
  pick up if asked.

If the user asks "what's next" with nothing else specified, the honest
answer is: the ordered queue is empty, ask what they want rather than
guessing at scope.

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
- **Don't add a bare-Enter "chat" typing trigger** — Craft has one
  (opens an empty typing buffer sent as multiplayer chat), deliberately not
  ported since there's no multiplayer to talk to. Don't add it "for
  completeness" without a real reason (e.g. multiplayer actually landing).
- **Don't read `kCreateRadius`/`kDeleteRadius` directly outside
  `Initialize()`** — everything else must read `radii_` (`Worlds::
  CommandRadii`), the actual mutable value `/view` changes at runtime. The
  constants are seed values now, not the live source of truth.

## 10. Resume prompt

```
Read CRAFT_PARITY.md first (the authoritative Craft-vs-cna-craft parity
audit), then plan.md §12.1 (the ordered priority queue derived from it) —
as of the last session, 23 of 25 items are completed (including item 19,
the chunk-system redesign to an unbounded/streamed world; item 25, the
full 54-item block roster; and item 17, chat/slash world-editing
commands), 1 blocked (ambient occlusion, needs a shader-backend decision),
1 pending deferred (multiplayer, per project direction). The ordered
priority queue is now EMPTY — there is no single obvious "next task."
If the user doesn't specify what to work on, ask rather than guessing at
scope; don't assume ambient occlusion or multiplayer are wanted just
because they're the only open items (both are gated on a decision only
the user can make). Before implementing anything that cites Craft's
source code, re-verify the citation against the real checkout at
/rv/data/development/github.com/other/Craft — CRAFT_PARITY.md was
carefully audited but could still contain an error, same as every prior
citation pass in this project's history. Make one small, verified
improvement at a time: implement, build (cmake --build build-worlds, or
the full EasyGL build if it touches Render/ or CnaCraftGame), run the
relevant test/smoke command, confirm it actually passes — and for anything
touching CnaCraftGame/Render, verify visually via a real Xvfb/xdotool/
ImageMagick screenshot cycle (remember: SDL_VIDEODRIVER=x11 is required in
this sandbox, and xdotool keydown/keyup — not plain xdotool key — for
reliable key-hold detection; mouse clicks remain unreliable in this
sandbox, so anything needing a real break/place — including verifying the
`/cube`-style commands' actual painting — should lean on unit tests
instead of a live screenshot). If you touch the chunk-streaming/threading
code (World column storage, CnaCraftGame's Dispatch*/Poll* methods),
re-read §6's TaskT/apply-cap notes first — there's a real, subtle,
already-found-once bug class there (discarding instead of deferring
completed-but-over-cap jobs). If you touch WorldEditor/ExecuteCommand,
re-read §6's PasteRegion note first — a same-World overlapping paste can
clobber source data mid-copy, a real Craft quirk, not a bug to "fix."
When finished, update plan.md §12.1's status, update CRAFT_PARITY.md's
corresponding entry, and update this file's "Current status"/"Recent
changes".
```
