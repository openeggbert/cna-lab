# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after
adding Signs (this session's 4th and final task) on branch `develop`.

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
   into an ordered, numbered task list (23 items) with the same status
   vocabulary. **This supersedes `plan.md` §11's older backlog** wherever
   they disagree.
3. This file — a handoff summary, not an independent source of truth. Keep
   it in sync with the two files above, not the other way around.

**Key architectural decisions**: unchanged across all sessions — two-layer
split (`Worlds/` engine-agnostic, `Render/` + `CnaCraftGame` CNA-dependent),
fixed-size world (`128×64×128`), naive per-face meshing (now with a second
cross-billboard path for plants), backend-agnostic game code,
two-mesh-buffer (opaque/transparent) convention.

## 2. Current status

**Build status**: verified at the end of this session (EasyGL backend) —
clean configure + build from scratch, zero warnings, `CnaCraft` executable
links and runs. Verified via headless `--smoke` runs and real interactive
Xvfb/xdotool/ImageMagick screenshots, including a full end-to-end sign
placement/persistence/reload cycle (see §3 item 4 below).

**Test status**: `tests/worlds_smoke_test.cpp` — **164 plain-assert checks,
all passing** (up from 149 at the start of this session). Plus a separate
`cna_craft_persistence_smoke_test` target (18 checks, all passing) covering
`Persistence::WorldStore` (blocks + signs) against a real SQLite file. Both
build and run standalone with `-DCNA_CRAFT_BUILD_GAME=OFF`.

**This session resolved all 4 `needs_human` items from the prior session**,
via `AskUserQuestion` — the user's answers (2026-07-10):
- **Persistence**: add SQLite (the "full Craft-style delta persistence"
  option, not an in-memory-only or JSON-file alternative).
- **Cloud in hotbar**: remove it — match Craft exactly (Craft never allows
  placing clouds).
- **Terrain formula**: re-port exactly per Craft's real dual-multiplicative-
  simplex2 formula, accepting that this reshapes all existing terrain.
- **Next step after those three**: Signs (the smaller of Signs/Chat).

**plan.md §12.1 status as of this session's end** (23 items):
- **19 `completed`**: everything from the prior session's 15, plus Cloud
  removed from the hotbar, terrain formula re-ported, SQLite delta
  persistence (blocks), Signs (data model + text-input state machine +
  billboard rendering + persistence).
- **1 `blocked`**: ambient occlusion (needs a custom `ShaderEffect`, only
  real on EASYGL today).
- **1 `pending (large)`**: Chat/slash-commands — shares the text-input
  state machine Signs now uses, but Craft's real command set needs
  world-editing primitives (`/cube`, `/sphere`, `/tree`, `/array`, `/copy`,
  `/paste`, etc.) that don't exist in this codebase yet.
- **1 `pending`, explicitly deferred**: multiplayer, per project direction.
- **1 `needs_human`**: chunk system redesign (hash-map + streaming) — is an
  unbounded/streamed world even wanted, given the project's stated
  fixed-size prototype scope?

This is a legitimate stop point: every item that was small enough to
implement safely, and every item the user has given a decision on, has been
implemented, tested, and verified; everything left needs either a further
human decision or is large enough to deserve its own dedicated session.

## 3. Recent changes (this session)

All individually committed and pushed to `develop`:

1. `6852d83` — Removed `Cloud` from the placeable hotbar (`Worlds/
   Hotbar.hpp`'s `kSlots`, now 16 entries) — clouds are still generated in
   the world (`World::GenerateClouds`) and remain non-collidable, just no
   longer player-placeable, matching Craft exactly.
2. `3f2bbef` — Re-ported `NoiseGenerator::Height` to Craft's real formula:
   `f = Simplex2(x*0.01, z*0.01, octaves=4, persistence=0.5, lacunarity=2)`,
   `g = Simplex2(-x*0.01, -z*0.01, octaves=2, persistence=0.9,
   lacunarity=2)`, `height = f * (g*32+16)`, clamped to Craft's sea level
   `t=12`. Also updated `kSandMaxHeight` from 10 → 12 to match. This
   materially changed the look of generated terrain (verified via
   screenshot — more varied hills, a proper beach at the sand threshold).
3. `406c867` — SQLite delta persistence (`Persistence::WorldStore`,
   `World::SetBlockAndRecordEdit`/`RecordedEdits`/`ClearRecordedEdits`,
   `BlockEdit`). Schema adapted from Craft's real `block(p,q,x,y,z,w)` by
   dropping the `p,q` chunk-address columns (no chunk (p,q) addressing in
   this project's fixed-grid `World`). New standalone
   `cna_craft_persistence_smoke_test` target/ctest (12 checks at the time,
   later 18 after signs were added), since it's the only part of this
   repo's test suite touching real disk I/O. **Verification note**:
   end-to-end verification via real mouse clicks in the graphical game
   turned out to be impractical in this sandbox (synthetic mouse clicks
   into a relative-mouse-mode SDL window are unreliable here, same
   flakiness class already documented for mouse-look) — the dedicated
   `WorldStore`-level test became the primary verification instead.
4. (this commit) — Signs: `Worlds::Sign`/`SignStore` (engine-agnostic data
   model, symmetric 6-face convention derived from the raycast normal,
   deliberately simpler than Craft's real asymmetric `hit_test_face`); a
   text-input state machine in `CnaCraftGame::Update()` (backtick opens
   typing via `TextInputEXT`, suspends WASD/look/click while typing but not
   gravity, Enter re-raycasts fresh before submitting, Escape cancels
   without quitting); `Render::SignBillboard` (one dynamically-built text
   texture per sign via the newly-shared `Render/BitmapFont.hpp`, both
   triangle windings emitted per quad to sidestep another empirical-winding
   debug cycle); `WorldStore::LoadSignsInto`/`SaveSigns` (new `sign` table,
   delete-and-reinsert per save); a new `Hud::SetTyping` on-screen overlay
   for the in-progress typing buffer. **Verified end-to-end against a real
   headless `CnaCraft` build**: `SDL_VIDEODRIVER=x11` was required under
   Xvfb (the default video driver picked Wayland, whose surface X11
   screenshot tooling can't see — an addition to this project's sandbox
   quirks); `xdotool keydown`/`keyup` (not plain `xdotool key`, whose
   default hold duration was too short for this project's 60fps input
   polling to catch) drove backtick/type/Enter; confirmed via temporary
   debug prints (removed before the final build) that `TextInput` events,
   the edge-triggered Enter branch, and the raycast hit all fired
   correctly; confirmed the "HI" sign rendered on-screen; confirmed the row
   landed in `world.db`'s new `sign` table via `sqlite3`; confirmed the
   sign reloaded correctly after killing and relaunching the process.

Test count progression: 149 → 149 (Cloud/terrain-formula changes needed no
new `Worlds/`-layer tests beyond existing threshold tests, which were
updated in place) → 164 (Signs' `SignStore` + edit-recording tests) in
`worlds_smoke_test`; separately, 0 → 18 in the new `persistence_smoke_test`.

## 4. Current blocker / main problem

**None.** Clean build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and
`-DCNA_GRAPHICS_BACKEND=EASYGL`, built from scratch), 164/164 +18/18 tests
passing, zero compiler warnings.

Carried over, still unresolved, still not urgent: mouse-look reliability
confirmation on the user's real (non-sandboxed) machine — `needs_human`,
unchanged for several sessions now (see `plan.md` §11.0/§12.1 for history).

## 5. Known bugs and limitations

See `CRAFT_PARITY.md` for the authoritative, cited, per-feature list — read
it before re-deriving anything from memory or re-reading Craft's source
from scratch. Highlights of what's NOT done yet:

- **`needs_human`**: chunk system (hash-map + streaming vs. fixed dense
  grid — is an unbounded world even wanted, given the project's stated
  fixed-size prototype scope in `plan.md` §1?).
- **`blocked`**: ambient occlusion — needs a custom vertex format +
  `ShaderEffect`, only real on EASYGL today (`missing.md`).
- **`pending (large)`**: Chat/slash-commands — shares Signs' text-input
  state machine (now built and reusable) but additionally needs Craft's
  world-editing macro commands (`copy()/paste()/tree()/array()/cube()/
  sphere()/cylinder()`), none of which exist in this codebase.
- **Deliberately deferred**: multiplayer.
- **Not ported (out of scope this pass)**: Craft's player-position `state`
  table (spawn/camera position persistence) — this project always spawns
  at the deterministic world-center column; revisit only if that stops
  being desired.
- **Low-value, not picked up**: the other 5 of Craft's 6 flower colors,
  Chest, the 32-entry dye-color palette — all would reuse existing
  infrastructure (plant geometry or plain-cube blocks) but are pure
  content-scaling with little gameplay value, explicitly deprioritized per
  "prioritize gameplay parity over decorative additions."

## 6. Architecture notes

**New this session** (see individual commit messages in §3 for full
per-feature detail — this is just the "if you touch this again" summary):

- `Worlds/NoiseGenerator::Height` — now Craft's real two-Simplex2 formula
  (see §3 item 2). **If you touch this again**: changing it reshapes all
  generated terrain; re-verify visually (screenshot), not just via tests,
  since the existing threshold tests (Sand, sea level) only check
  boundaries, not overall shape.
- `Worlds/World` — gained `BlockEdit`, `SetBlockAndRecordEdit`,
  `RecordedEdits()`, `ClearRecordedEdits()`. All player-driven block
  changes in `CnaCraftGame::Update()` must go through
  `SetBlockAndRecordEdit`, not plain `SetBlock`, or they silently won't
  persist.
- `Worlds/Sign`/`SignStore` (new) — face convention is a **cna-craft-only
  simplification**, not a port of Craft's real (asymmetric, top-only,
  4-way-rotated) `hit_test_face`. Don't "fix" this to match Craft's scheme
  without re-reading the doc comment in `Sign.hpp` explaining why it was a
  deliberate choice.
- `Persistence/WorldStore` (new) — synchronous save on every edit (not
  batched/async like Craft's own worker thread) — a deliberate simplicity
  choice at this prototype's edit rate, documented in the class comment.
  Degrades to `db_ = nullptr` (game keeps running, edits just aren't saved)
  on any SQLite failure — never crashes the game over a persistence error.
- `Render/BitmapFont` (new, extracted from `Render/Hud.cpp`) — the shared
  `kFont8x8`/`FontDrawText` used by both the HUD text and `SignBillboard`'s
  sign textures. If you need bitmap text anywhere else, use this, don't
  re-duplicate it.
- `Render/SignBillboard` (new) — **emits both triangle windings per quad**
  (12 indices, not 6) rather than a single correct winding — see `Render/
  SkyDome.cpp`'s note (§ below) for why. This means signs are visible from
  both sides but show mirrored "ghost text" from the back-facing winding at
  a grazing angle — a known, accepted tradeoff, not a bug to fix by
  guessing a single winding.
- `CnaCraftGame::Update` — gained a text-input state machine (backtick/
  Backspace/Enter/Escape) that early-returns before all other input
  handling while `isTypingSign_` is true. The game-quit Escape check
  changed from level-triggered to edge-triggered (so cancelling sign typing
  with Escape doesn't also quit the game on the next frame if the key is
  still held) — if you add other Escape-triggered behavior, keep it
  edge-triggered for the same reason.
- `Render/SkyDome` (from a prior session, still relevant): **the triangle
  winding for non-cube geometry is not obvious** — CNA's default
  `RasterizerState` culls `CullCounterClockwiseFace` (confirmed by reading
  `RasterizerState.cpp`), and empirically the correct visible winding for
  inward-facing dome geometry was the *opposite* of the naive "CCW viewed
  from inside" reasoning. `SignBillboard` sidesteps this by emitting both
  windings instead of re-deriving the correct one; if you add yet another
  custom mesh, either follow that same both-windings shortcut or verify
  with vivid debug colors in a real build before trusting geometric
  reasoning alone.

**Everything else** (module list, boundaries, data flow) — unchanged, see
`plan.md` §2/§6/§8.

**Boundaries that must not be broken** — unchanged, still load-bearing:
`Worlds/` must never `#include` anything CNA/SDL; any new `BlockDef`
boolean flag with a non-`false` default must be AND'd with `solid`
wherever consumed; two-mesh-buffer convention (plants extend `transparent`,
they didn't need a third category).

## 7. Useful commands

Unchanged, plus the persistence test target:

```bash
# Engine-agnostic tests only (no CNA/GPU/display needed):
cmake -S . -B build-worlds -DCNA_CRAFT_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-worlds -j"$(nproc)"
ctest --test-dir build-worlds --output-on-failure
```
Expect: `WorldsSmokeTest` (164 `ok:` lines) and `PersistenceSmokeTest`
(18 checks) both pass.

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
process runs fine and `DISPLAY` is set correctly.

There is no separate lint/format tooling configured in this repo.

## 8. Next smallest tasks

`plan.md` §12.1 is the authoritative ordered priority queue. As of this
session's end:

1. **Dedicated follow-up session**: Chat/slash-commands (CRAFT_PARITY.md
   §4.5). The text-input state machine is now built and reusable (see
   `CnaCraftGame::Update`'s typing branch) — the remaining scope is Craft's
   world-editing macro commands (`/cube`, `/sphere`, `/tree`, `/array`,
   `/copy`, `/paste`, etc.), which need new world-editing primitives that
   don't exist in this codebase yet. Scope this as its own session.
2. **Human decision needed**: chunk system redesign (hash-map + streaming)
   — ask the user whether an unbounded/streamed world is actually wanted
   before starting; it's a genuine architecture change, not a small patch.
3. **Low priority, low value**: the other 5 Craft flower colors, Chest,
   dye-color palette — technically easy (same plant/cube infrastructure)
   but low gameplay value, explicitly deprioritized multiple sessions now.

## 9. Do not do yet

Everything from prior sessions still applies (no multiplayer, no AO/
greedy-meshing rewrite without a shader-backend decision, no broad
`Worlds/`/`Render/` refactor, no `PlayerController` public-API changes
without re-running the full suite, no `Hotbar::kSlots` reordering, no
fly-speed constant change without a human decision) — **plus**:

- **No rushed Chat/slash-commands implementation** — reuse the text-input
  state machine Signs built, but the world-editing macro commands are a
  real design/implementation effort, not a drive-by addition.
- **No collision-substepping constant changes** (`kMinSubsteps=8`,
  `kMaxSubsteps=64`) without re-running the full test suite AND the manual
  "force substeps to 1, confirm the tunneling test fails, restore" check —
  the tunneling regression test is only meaningful if substepping is
  actually active; a silent regression here wouldn't show up any other way.
- **No `SkyDome`/`SelectionOutline`/`SignBillboard` winding "cleanup"**
  without a real EasyGL build + screenshot verification — this geometry's
  correct winding was found empirically (`SkyDome`) or deliberately
  sidestepped (`SignBillboard`'s both-windings choice), not derived
  analytically, so don't "simplify" it based on reasoning alone.
- **No `Sign.hpp` face-convention change** to match Craft's real
  asymmetric `hit_test_face` without re-reading why the current symmetric
  convention was chosen deliberately (see `Sign.hpp`'s doc comment) — it's
  a reasoned difference, not a gap.
- **No re-running the terrain formula change** — it's already done this
  session (`NoiseGenerator::Height` now matches Craft exactly); don't
  re-investigate this as if it were still open.

## 10. Resume prompt

```
Read CRAFT_PARITY.md first (the authoritative Craft-vs-cna-craft parity
audit), then plan.md §12.1 (the ordered priority queue derived from it) —
as of the last session, 19 of 23 items are completed, 1 blocked
(ambient occlusion), 1 pending-large (Chat/slash-commands), 1 pending
deferred (multiplayer), 1 needs_human (chunk system redesign). There is no
small pending item left that doesn't need either a human decision or a
dedicated session. If the user wants Chat/slash-commands, reuse the
text-input state machine already built for Signs (CnaCraftGame::Update's
typing branch) rather than rebuilding it; the remaining scope is Craft's
world-editing macro commands. If the user wants the chunk system
redesigned, ask for confirmation first — it's a genuine architecture
change. Before implementing anything that cites Craft's source code,
re-verify the citation against the real checkout at
/rv/data/development/github.com/other/Craft — CRAFT_PARITY.md was
carefully audited but could still contain an error, same as every prior
citation pass in this project's history. Make one small, verified
improvement at a time: implement, build (cmake --build build-worlds, or
the full EasyGL build if it touches Render/ or CnaCraftGame), run the
relevant test/smoke command, confirm it actually passes — and for anything
touching CnaCraftGame/Render, verify visually via a real Xvfb/xdotool/
ImageMagick screenshot cycle (remember: SDL_VIDEODRIVER=x11 is required in
this sandbox, and xdotool keydown/keyup — not plain xdotool key — for
reliable key-hold detection). When finished, update plan.md §12.1's status,
update CRAFT_PARITY.md's corresponding entry, and update this file's
"Current status"/"Recent changes".
```
