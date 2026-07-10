# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after a
"minimize remaining Craft-fidelity gaps" batch (plan.md §12.1 item 24) on
branch `develop`, with the chunk-system redesign (item 19) chosen as the
next follow-up but not yet started.

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
placement/persistence/reload cycle and a cursor-release-doesn't-quit check
(process stays alive after Escape, confirmed via `ps` after the change).

**Test status**: `tests/worlds_smoke_test.cpp` — **173 plain-assert checks,
all passing** (up from 149 at the start of this session). Plus a separate
`cna_craft_persistence_smoke_test` target (21 checks, all passing) covering
`Persistence::WorldStore` (blocks + signs) against a real SQLite file. Both
build and run standalone with `-DCNA_CRAFT_BUILD_GAME=OFF`.

**This session resolved 8 open questions total**, all via `AskUserQuestion`
(2026-07-10):
- **Persistence**: add SQLite (full Craft-style delta persistence).
- **Cloud in hotbar**: remove it — match Craft exactly.
- **Terrain formula**: re-port exactly per Craft's real formula.
- **Next step after those three**: Signs (done, then revised to match
  Craft's real incremental `db.c` persistence more closely on request).
- Then, asked "co dále?" (what's next) with the full remaining-gap list
  from `CRAFT_PARITY.md`: **cursor capture** (match Craft — Esc releases
  the cursor, doesn't quit), **fly controls** (match Craft's pitch-coupled
  flight, no dedicated descend key), **fly speed** (exactly 4x, not 2x),
  **chunk system** (pursue the unbounded/streamed world after all).
  Plus two "no real tradeoff" fixes (exact pitch clamp, exact reach
  distance) implemented without asking, same as the terrain-formula
  session's own reasoning for what does/doesn't need a human decision.

**plan.md §12.1 status as of this session's end** (24 items):
- **21 `completed`**: everything from before, plus item 24 (cursor
  capture/quit, pitch clamp, fly speed, pitch-coupled flight, floor-catch
  safety net, reach distance — all now Craft-exact).
- **1 `blocked`**: ambient occlusion (needs a custom `ShaderEffect`, only
  real on EASYGL today).
- **1 `pending (large)`**: Chat/slash-commands — shares the text-input
  state machine Signs now uses, but Craft's real command set needs
  world-editing primitives (`/cube`, `/sphere`, `/tree`, `/array`, `/copy`,
  `/paste`, etc.) that don't exist in this codebase yet.
- **1 `pending`, explicitly deferred**: multiplayer, per project direction.
- **1 `pending (large)`**: chunk system redesign (hash-map + streaming) —
  **user decision 2026-07-10: pursue it**, superseding the prior
  `needs_human` framing. **Not started** — see §8 below; this is the
  actual next task, but needs a real design pass first (new World/Chunk
  storage model, on-demand generation/meshing, load/unload policy,
  persistence-schema impact), not a same-shape port like item 24's fixes.

Item 24's fixes are a legitimate stop point on their own (small, safe,
same-shape ports with no remaining ambiguity, all tested and verified).
The chunk-system redesign is a different kind of task — large enough that
it should get its own design/plan pass (see §8) rather than being rushed
into the same session as item 24's fixes.

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
4. Signs: `Worlds::Sign`/`SignStore` (engine-agnostic data model, symmetric
   6-face convention derived from the raycast normal, deliberately simpler
   than Craft's real asymmetric `hit_test_face`); a text-input state
   machine in `CnaCraftGame::Update()` (backtick opens typing via
   `TextInputEXT`, suspends WASD/look/click while typing but not gravity,
   Enter re-raycasts fresh before submitting, Escape cancels without
   quitting); `Render::SignBillboard` (one dynamically-built text texture
   per sign via the newly-shared `Render/BitmapFont.hpp`, both triangle
   windings emitted per quad to sidestep another empirical-winding debug
   cycle); a new `Hud::SetTyping` on-screen overlay for the in-progress
   typing buffer. **Verified end-to-end against a real headless `CnaCraft`
   build**: `SDL_VIDEODRIVER=x11` was required under Xvfb (the default
   video driver picked Wayland, whose surface X11 screenshot tooling can't
   see — an addition to this project's sandbox quirks); `xdotool keydown`/
   `keyup` (not plain `xdotool key`, whose default hold duration was too
   short for this project's 60fps input polling to catch) drove backtick/
   type/Enter; confirmed the sign rendered on-screen; confirmed the row
   landed in `world.db`'s new `sign` table via `sqlite3`; confirmed the
   sign reloaded correctly after killing and relaunching the process.
5. **Sign persistence revised to match Craft's real `db.c` more closely**,
   following a direct user request after reviewing item 4: the first
   version's `WorldStore::SaveSigns` did a bulk delete-and-reinsert of the
   whole sign list on every save. Re-checked against the real Craft source
   (`src/db.c`) and replaced with three incremental methods —
   `UpsertSign`/`DeleteSign`/`DeleteSignsAt` — matching Craft's own
   `db_insert_sign`/`db_delete_sign`/`db_delete_signs` exactly (same
   `(x,y,z,face)` keying, called from the same places). This re-check also
   surfaced two real gaps versus Craft, both fixed: (a) submitting an
   empty-text sign now deletes any existing sign at that face (Craft's
   `set_sign` does this unconditionally on a raycast hit, not gated on the
   typed buffer already being non-empty as the first version had it); (b)
   breaking a block now deletes any signs on it (`SignStore::RemoveAllAt` +
   `WorldStore::DeleteSignsAt`, matching Craft's `_set_block` →
   `unset_sign` — a sign can't outlive the block face it was attached to).
   New tests: 4 for `SignStore::RemoveAllAt` (`worlds_smoke_test.cpp`, now
   169 checks), 7 revised/new for the incremental sign persistence methods
   (`persistence_smoke_test.cpp`, now 21 checks). Re-verified the sign
   place/reload cycle end-to-end via the same Xvfb/xdotool flow as item 4;
   the break-deletes-signs path could **not** be verified live via the GUI
   (synthetic mouse clicks into this project's relative-mouse-mode SDL
   window remain unreliable here — verified instead via the unit tests
   above plus code review, since `CnaCraftGame`'s wiring calls those same
   tested functions directly).
6. **Minimized remaining Craft-fidelity gaps** (plan.md §12.1 item 24),
   following the user's "co dále?" question and 4 more `AskUserQuestion`
   answers. Five changes, all in `PlayerController`/`CnaCraftGame`: (a)
   cursor capture — Escape now releases the cursor instead of quitting,
   left-click while released re-captures instead of clicking, matching
   Craft's `exclusive` model exactly (new `cursorCaptured_` member);
   confirmed via `ps` that the process survives Escape now, where before
   it would have exited; (b) pitch clamp changed from an approximate
   `1.55f` to the exact `pi/2`; (c) fly speed changed from `9.0f` (2x
   walk speed) to `18.0f` (exactly 4x, matching Craft's walk=5/fly=20
   ratio); (d) flight is now pitch-coupled exactly like Craft's own
   `get_motion_vector` — `PlayerInput::moveUp` and the Left Ctrl-descend
   key are gone, there is genuinely no dedicated descend key anymore
   (look down + move forward, or look up + move backward, to descend);
   (e) `kMaxReach` changed from `6.0f` to `8.0f`, Craft's exact hit-test
   distance. Also closed a real gap surfaced while re-reading Craft's
   `handle_movement` for (d): Craft's own `y<0` floor-catch safety net
   (`highest_block(...)+2`) had no equivalent at all — added
   `World::HighestCollidableY` + a matching check in
   `PlayerController::Update`. 5 new tests in `worlds_smoke_test.cpp` (now
   173 checks): Space-forces-ascend, forward+look-down-descends,
   pure-strafe-has-no-vertical-component, plus 3 for the floor-catch.
   Verified against a real headless build under Xvfb: confirmed the
   process survives Escape, confirmed a clean render (no crash/corruption)
   via screenshot. The re-capture-on-click path could not be verified live
   (same mouse-click flakiness as items 4-5) — verified via code review and
   the fact that `PlayerController`'s side of every change is independently
   unit-tested.

Test count progression: 149 → 149 (Cloud/terrain-formula changes needed no
new `Worlds/`-layer tests beyond existing threshold tests, which were
updated in place) → 164 (Signs' SignStore + edit-recording tests) → 169
(RemoveAllAt) → 173 (item 24's fly/floor-catch tests) in
`worlds_smoke_test`; separately, 0 → 18 → 21 in `persistence_smoke_test`.

## 4. Current blocker / main problem

**None.** Clean build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and
`-DCNA_GRAPHICS_BACKEND=EASYGL`, built from scratch), 173/173 +21/21 tests
passing, zero compiler warnings.

Carried over, still unresolved, still not urgent: mouse-look reliability
confirmation on the user's real (non-sandboxed) machine — `needs_human`,
unchanged for several sessions now (see `plan.md` §11.0/§12.1 for history).

## 5. Known bugs and limitations

See `CRAFT_PARITY.md` for the authoritative, cited, per-feature list — read
it before re-deriving anything from memory or re-reading Craft's source
from scratch. Highlights of what's NOT done yet:

- **`pending (large)`, user-approved, not started**: chunk system redesign
  (hash-map sparse chunks + distance-based streaming, replacing the fixed
  dense grid) — see §8 below, this is the actual next task.
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
- `Worlds/PlayerController` (item 24): flying is now pitch-coupled exactly
  like Craft's own `get_motion_vector` — **`PlayerInput::moveUp` is gone**,
  there is no dedicated descend key. If you're tempted to add one back
  ("it's more discoverable"), don't without asking first — this exact
  tradeoff was already raised and the user explicitly chose Craft-fidelity
  over discoverability. `kPitchLimit` is now the literal `pi/2` and
  `kFlySpeed` is now literally `4 * kMoveSpeed` (`18.0f`) — if you ever
  change `kMoveSpeed`, keep that ratio intentional, don't let it drift.
- `Worlds/World::HighestCollidableY` (new, item 24) + the `y<0` check at
  the end of `PlayerController::Update` — ports Craft's floor-catch safety
  net. Deliberately does NOT reset `velocity_.y` after catching, matching
  Craft's own (slightly odd but faithfully-ported) behavior — don't "fix"
  this to also reset velocity without checking Craft's real code first.
- `CnaCraftGame` (item 24): **Escape no longer quits** — it releases the
  mouse cursor (`cursorCaptured_`), matching Craft exactly. There is no
  in-game quit key in real Craft at all; quitting relies on CNA's own
  `SDL_EVENT_QUIT -> Game::Exit()` (window close / Alt+F4), confirmed
  already wired in `Game::PollEvents`. Left-click while released
  re-captures the cursor instead of breaking/placing/eyedropping —
  mouse-look and all three mouse buttons are gated on `cursorCaptured_` in
  `Update()`; arrow-key look is deliberately NOT gated (matches Craft,
  where arrow-key look lives in `handle_movement`'s `!g->typing` block,
  entirely separate from `handle_mouse_input`'s `exclusive` gate). If you
  add a new mouse-button action, gate it on `cursorCaptured_` too, or it'll
  silently work while the cursor is released, unlike every other click
  action.

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
Expect: `WorldsSmokeTest` (173 `ok:` lines) and `PersistenceSmokeTest`
(21 checks) both pass.

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

1. **The actual next task, user-approved but not started**: chunk system
   redesign (hash-map sparse chunks + distance-based streaming, replacing
   the fixed `128×64×128` dense grid) — CRAFT_PARITY.md §3.1/§3.2,
   plan.md §12.1 item 19. This is a genuine architecture change, not a
   same-shape port like item 24's fixes were — it touches `World`/`Chunk`'s
   core storage model, `ChunkRenderer`'s per-chunk lifecycle, and
   `Persistence::WorldStore`'s schema (Craft's real schema already has
   `p,q` chunk-address columns this project's `block`/`sign` tables
   currently drop — revisit that decision once chunks are actually
   addressed by `(p,q)`). **Needs a real design pass before touching code**
   — use `EnterPlanMode` or equivalent rather than diving straight into
   edits, given the size and risk (this is exactly the kind of task this
   project's own tooling guidance calls out for a plan first). Rough shape
   to design around: chunk storage keyed by `(p,q)` in a hash map instead
   of a fixed array; generate/mesh chunks on demand as the player moves
   (matching Craft's own worker-thread streaming, or a simpler synchronous
   version given this project's existing "no async worker" precedent from
   `WorldStore`); an explicit load/unload radius; decide whether
   `WORLD_SIZE_X/Y/Z` constants and every piece of code that assumes them
   (spawn logic, cloud/tree/flower generation passes, raycast bounds) needs
   to change shape or can stay column-local.
2. **Dedicated follow-up session**: Chat/slash-commands (CRAFT_PARITY.md
   §4.5). The text-input state machine is now built and reusable (see
   `CnaCraftGame::Update`'s typing branch) — the remaining scope is Craft's
   world-editing macro commands (`/cube`, `/sphere`, `/tree`, `/array`,
   `/copy`, `/paste`, etc.), which need new world-editing primitives that
   don't exist in this codebase yet. Scope this as its own session.
3. **Low priority, low value**: the other 5 Craft flower colors, Chest,
   dye-color palette — technically easy (same plant/cube infrastructure)
   but low gameplay value, explicitly deprioritized multiple sessions now.

## 9. Do not do yet

Everything from prior sessions still applies (no multiplayer, no AO/
greedy-meshing rewrite without a shader-backend decision, no broad
`Worlds/`/`Render/` refactor, no `PlayerController` public-API changes
without re-running the full suite, no `Hotbar::kSlots` reordering) —
**plus**:

- **No chunk-system redesign without a design/plan pass first** — it's
  user-approved to pursue, but "approved to pursue" is not the same as
  "approved implementation approach." Don't start editing `World`/`Chunk`
  for this without first proposing a concrete design (storage model,
  streaming policy, persistence-schema impact) and getting it reviewed.
- **No re-adding a dedicated fly-descend key** — this exact tradeoff
  (discoverability vs. Craft-fidelity) was already raised and the user
  chose Craft-fidelity (item 24). Don't second-guess it without asking.
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
- **No re-running the terrain-formula, fly-speed, pitch-clamp, or
  reach-distance changes** — all already done (see §3 items 2 and 6); don't
  re-investigate these as if they were still open.

## 10. Resume prompt

```
Read CRAFT_PARITY.md first (the authoritative Craft-vs-cna-craft parity
audit), then plan.md §12.1 (the ordered priority queue derived from it) —
as of the last session, 21 of 24 items are completed, 1 blocked (ambient
occlusion), 1 pending-large (Chat/slash-commands), 1 pending deferred
(multiplayer), 1 pending-large-user-approved-but-not-started (chunk system
redesign, item 19 — the user chose to pursue an unbounded/streamed world
after all on 2026-07-10, but no implementation has started). The chunk
system redesign is the actual next task, but it's a genuine architecture
change (new World/Chunk storage model, on-demand generation/meshing,
load/unload policy, persistence-schema impact) — do NOT start editing code
for it without a real design pass first (EnterPlanMode or equivalent), per
this project's own "genuine architecture change" handling rule; see §8
above for the rough shape to design around. If the user wants
Chat/slash-commands instead, reuse the text-input state machine already
built for Signs (CnaCraftGame::Update's typing branch) rather than
rebuilding it; the remaining scope is Craft's world-editing macro commands.
Before implementing anything that cites Craft's source code, re-verify the
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
reliable key-hold detection). When finished, update plan.md §12.1's status,
update CRAFT_PARITY.md's corresponding entry, and update this file's
"Current status"/"Recent changes".
```
