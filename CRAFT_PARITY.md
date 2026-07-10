# CRAFT_PARITY.md — cna-craft vs fogleman/Craft feature parity audit

This is a feature-by-feature comparison between [fogleman/Craft](https://github.com/fogleman/Craft)
(the real checkout at `/rv/data/development/github.com/other/Craft`, MIT license, Copyright (c)
2013 Michael Fogleman — see `THIRD_PARTY_NOTICES.md`) and this project (`cna-craft`), produced by
directly reading both codebases' source (not README claims, not training-data assumptions about
Craft). Every "Craft behavior" line below cites a real file/function/line from the actual checkout.

**Purpose**: `plan.md`'s prior backlog (§11) was written from a partial reading of Craft and
contained real citation errors (e.g. a claimed "caves/overhangs" feature that turns out not to
exist in Craft at all — see §3.9 below). This document is the corrected, exhaustive baseline.
`plan.md`'s new "Craft Feature Parity Port" section converts the gaps found here into a concrete,
ordered task list.

**Status legend**: `complete` (functionally equivalent to Craft), `partial` (exists but with a
real behavioral gap/bug relative to Craft), `missing` (does not exist in cna-craft at all),
`blocked` (a technical prerequisite — usually CNA backend shader support — is missing),
`needs_human` (requires a product/design/dependency decision before implementing).

**Audit method**: 5 parallel source-reading passes (this session), each comparing the real Craft
checkout against the current cna-craft `src/` tree, file-by-file and line-by-line where possible.

---

## 1. Core loop, input, and camera

### 1.1 Main game loop structure
- **Craft behavior**: single-threaded `while(1)` in `main()` (`src/main.c:2776-2947`). Each
  iteration: recompute viewport, compute variable `dt` (clamped 0–0.2s), `handle_mouse_input()`,
  `handle_movement(dt)`, network recv, DB-commit/position-send timers, render, `glfwSwapBuffers`,
  `glfwPollEvents`. Input is pull-based (`glfwGetKey`) inside `handle_movement`, plus one-shot
  GLFW key/char/mouse callbacks registered once.
- **cna-craft behavior**: CNA's `Game::Tick()` (fixed-timestep XNA loop, `../cna/src/.../Game.cpp`)
  — `PollEvents()` (SDL), then `Update(gameTime)` at a fixed 60Hz step
  (`CnaCraftGame::CnaCraftGame`, `CnaCraftGame.cpp:45-48`), then `Draw`. `CnaCraftGame::Update`
  polls `Keyboard::GetState()`/`Mouse::GetState()` each call — pull-based, same idea as Craft.
- **Status**: complete
- **Craft files**: `src/main.c:2586-2963`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp`, `src/CnaCraft/main.cpp`,
  `../cna/src/Microsoft/Xna/Framework/Game.cpp`
- **Priority**: low
- **Verification method**: code inspection (done)

### 1.2 Window/input handling setup
- **Craft behavior**: GLFW window; cursor captured (`GLFW_CURSOR_DISABLED`) at startup
  (`main.c:2604`). Left-click when not exclusive **re-captures** the cursor
  (`on_mouse_button`, `main.c:2342-2344`); **Escape releases the cursor without quitting**
  (`on_key`, `main.c:2199-2201`). Mouse buttons (break/place/eyedropper/light-toggle) and
  mouse-look are both inert while the cursor is released (`on_mouse_button`'s `exclusive` guard,
  `handle_mouse_input`'s `exclusive` guard). There is no in-game quit key at all — closing the
  window is the only way to quit.
- **cna-craft behavior** (matched this session, user decision 2026-07-10): `cursorCaptured_`
  (`CnaCraftGame.hpp`) tracks capture state. Escape releases the cursor
  (`Mouse::setIsRelativeMouseModeEXTProperty(false)`) instead of quitting; left-click while
  released re-captures it instead of breaking/placing/eyedropping on that same click; mouse-look
  deltas are only applied while captured (arrow-key look still works either way, matching Craft's
  own `handle_movement`, which isn't gated on `exclusive` at all). Quitting now relies on CNA's
  own `SDL_EVENT_QUIT -> Game::Exit()` (window close / Alt+F4), confirmed already wired in
  `Game::PollEvents` before removing the Escape-quit path.
- **Status**: complete
- **Craft files**: `src/main.c` (`create_window`, `on_key`, `on_mouse_button`, `handle_mouse_input`)
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.{hpp,cpp}`
- **Priority**: medium (done)
- **Verification method**: real headless build under Xvfb — confirmed the process stays alive
  after Escape (previously it would exit); full test suite re-run (no PlayerController-side
  regressions, this section is pure input-wiring glue).
- **Notes**: Previously a **known, documented divergence** (README explicitly documented
  Esc-quits as intentional) — changed after the user explicitly asked to minimize differences
  from Craft, overriding the earlier documented-intentional status. `README.md` §5 updated to
  match.

### 1.3 Keyboard controls
- **Craft behavior** (`config.h:30-44`, `on_key`/`on_char`/`handle_movement`): W/A/S/D move;
  Space = jump/fly-up; Tab = fly toggle; Shift = hold-zoom (FOV 65→15°); F = hold-ortho; **E/R =
  next/prev item**; **1-9 and 0 = direct item select (10 slots)**; **scroll wheel = item cycle**;
  O/P = multiplayer observe cameras; arrow keys = keyboard look; Enter = chat/command submit,
  Ctrl+Enter = right-click; Ctrl+V = paste; Backspace = edit typing buffer; Esc = cancel
  typing/release cursor; `t`/`/`/`` ` `` = open chat/command/sign text entry.
- **cna-craft behavior** (`CnaCraftGame.cpp`): W/A/S/D move; Space = jump / ascend while flying,
  Left Shift = descend while flying (**Minecraft-style independent flight, not Craft's
  pitch-coupled scheme — deliberate deviation, see §1.6**); Tab = fly toggle; no keyboard-look
  binding (arrow-key look was removed the same day it was added, see §1.9); Left Shift also
  hold-zooms, but only outside fly mode (see §1.9); F = hold-ortho; E/R = next/prev item cycle;
  1-9 and `0` = direct item select (10 slots); scroll wheel = item cycle; middle-click =
  eyedropper; backtick/Enter/Backspace/Esc = sign text entry (§4.3); `/` = world-editing command
  entry (§4.5); F12 = screenshot (cna-craft-only addition, not in Craft).
  Only Ctrl+click light-toggle remains unported.
- **Status**: complete (light-toggle tracked separately, §2.7)
- **Craft files**: `src/config.h:30-44`, `src/main.c` (`on_key`, `on_char`, `handle_movement`)
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.{hpp,cpp}`
- **Priority**: high (done)
- **Verification method**: code inspection + manual play test
- **Notes**: `R` (prev-cycle), scroll-wheel cycling, key `0`, middle-click eyedropper, sign text
  entry, and `/`-command entry were all implemented across several sessions (see §2.1/§2.7/§4.3/
  §4.5). Only Ctrl+click light-toggle (needs a whole point-lighting subsystem, §2.7) remains — its
  own larger feature, not a small keyboard-wiring gap, so it's tracked at its own section rather
  than here.

### 1.4 Mouse look
- **Craft behavior** (`handle_mouse_input`, `main.c:2378-2409`): delta from raw cursor-position
  diff each frame; sensitivity `m = 0.0025`; yaw wraps [0°,360°); pitch clamped to exactly
  ±90°; `INVERT_MOUSE` config flag (default off) flips vertical sign.
- **cna-craft behavior** (`PlayerController.cpp`, `kMouseSensitivity=0.0025f`): reads an
  already-relative mouse delta (SDL relative mode) each frame, same `0.0025` sensitivity; yaw
  wraps [0, 2π). Pitch clamp: **exactly ±π/2 briefly (2026-07-10), then backed off by a small
  epsilon the same day (real bug, see Notes)** — `kPitchLimit` is now `π/2 - 0.01f`, not the
  literal `1.57079632679489661923f`. No invert-mouse config.
- **Status**: complete
- **Craft files**: `src/main.c:2378-2409`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp`, `src/CnaCraft/Worlds/PlayerController.cpp`
- **Priority**: low
- **Verification method**: full test suite re-run, plus a new dedicated test
  (`TestPlayerControllerPitchClampAvoidsDegenerateLookDirection`) asserting `LookDirection()`'s
  horizontal component stays well above float32 noise level at the clamp in both directions —
  the actual property `Matrix::CreateLookAt` needs, confirmed numerically outside the test suite
  too (a standalone Python check of the same cross-product math, matching the C++ behavior).
- **Notes**: Sensitivity matches Craft exactly. Pitch clamp does **not** — this is a deliberate,
  permanent deviation, not a fidelity gap. **Real bug found 2026-07-10** (user-reported:
  "cna-craft je zcela rozbity", with a screenshot showing terrain rendering shredded into diagonal
  streaks, reproducible on both EasyGL and Vulkan): at pitch exactly ±π/2, `LookDirection()`
  becomes exactly parallel to `Vector3::Up`. CNA's `Matrix::CreateLookAt`
  (`cna/src/Microsoft/Xna/Framework/Matrix.cpp`) computes `Cross3(cameraUpVector, forward)` as its
  first step — Craft's own hand-rolled camera matrix doesn't go through an equivalent step, which
  is why Craft itself never hits this even with a literal exact-π/2 clamp. When forward and up are
  parallel, that cross product collapses toward the zero vector; normalizing it produces a
  numerically garbage view basis, corrupting every vertex transform for the rest of the frame. The
  exact-π/2 clamp (earlier the same day, this same entry) made this trivially reachable — any
  player tilting the camera all the way up/down lands exactly on the unsafe value, not a rare edge
  case. The new Minecraft-style flying controls (plan.md §12.1 item 29, also this session) made
  "look straight up while ascending" a completely natural first thing to try with the new
  controls, which is almost certainly how the user hit it immediately after trying them.

### 1.5 Player movement (walk speed / diagonal movement)
- **Craft behavior** (`get_motion_vector`, `main.c:204-232`): `strafe = atan2f(sz, sx)` from -1/0/1
  key axes; motion vector is `(cosf(rx+strafe), sinf(rx+strafe))` — **always a unit vector**
  regardless of how many keys are held, so diagonal movement is not faster. Walk speed = 5
  units/s; fly speed = 20 units/s (**exactly 4x** walk speed). No sprint mechanic exists anywhere.
- **cna-craft behavior** (`PlayerController.cpp`): `moveX/moveZ` are now normalized before scaling
  by speed (fixed a prior session), so diagonal movement is not faster. `kMoveSpeed=4.5`,
  `kFlySpeed=18.0` — **exactly 4x**, matching Craft's own ratio (changed from an earlier 9.0f/2x
  value per user decision 2026-07-10).
- **Status**: complete
- **Craft files**: `src/main.c:204-232, 2411-2469`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.cpp`
- **Priority**: high (done)
- **Verification method**: full test suite re-run (no regressions)
- **Notes**: Diagonal-speed normalization and the exact fly-speed ratio are both now Craft-exact.

### 1.6 Walking vs flying behavior
- **Craft behavior**: Tab toggles `g->flying`. While flying, forward/back movement is
  **pitch-coupled** — `vy = sinf(ry)` contributes vertical motion when moving forward/back while
  looking up/down (classic "look-and-fly" creative flight), strafing alone has no vertical
  component, and Space directly sets `vy=1` (pure hover climb, no gravity/inertia, `dy=0` forced
  every substep while flying). There is no dedicated descend key at all.
- **cna-craft behavior** (**revised again 2026-07-10, plan.md §12.1 item 29, user decision** — this
  section's history is a genuine back-and-forth, not indecision on my part, documented in full
  below since both directions were deliberate): Tab still toggles `flying_`. Vertical movement is
  now **Space=ascend / Shift=descend, independent of pitch, at full horizontal speed always** —
  Minecraft's own creative-flight scheme, not Craft's. Holding both Space and Shift cancels out to
  zero net vertical movement. `PlayerInput::descendPressed` (Left Shift) is the new field;
  `jumpPressed` (Space) keeps its existing double duty (jump on the ground, ascend while flying).
- **Status**: complete (as a deliberate departure from Craft, not accidental drift)
- **Craft files**: `src/main.c:204-232, 2250-2252, 2432-2465`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.{hpp,cpp}`, `src/CnaCraft/CnaCraftGame.cpp`
- **Priority**: medium (done)
- **Verification method**: unit tests rewritten for the new scheme (Space-forces-ascend,
  Shift-forces-descend, both-cancel-to-zero, horizontal-speed-independent-of-pitch) plus the full
  suite re-run; a real headless build + Xvfb smoke run confirmed no crash/regression.
- **Notes**: **Full history, because this flip-flopped within the same day**: cna-craft originally
  shipped a Space/Ctrl independent-flight scheme (documented as intentionally more discoverable
  than Craft's). Earlier the same day as this entry, the user asked to minimize differences from
  Craft across several small items at once, and this got ported to Craft's real pitch-coupled
  scheme as part of that batch (see git history / this file's own prior revision for that
  writeup). Once that shipped and the user tried flying with it, they reported the Craft-style
  scheme was genuinely annoying to use and asked for Minecraft-style controls instead — a
  legitimate, informed reversal after hands-on experience with both, not a mistake to "fix" back
  later. **Treat this entry, not the git history, as authoritative for current behavior** if
  they ever seem to disagree. Losing the pitch-coupled behavior is an intentional, permanent
  departure from Craft parity for this one control; `README.md` §5 updated to match.

### 1.7 Collision behavior
- **Craft behavior** (`collide`, `main.c:699-738`): per-substep (`step = MAX(8, estimate)`
  substeps per frame, `main.c:2446`) fractional-offset "padding" clamp — for each of up to
  `height=2` voxel layers, if the player's fractional position within a cell exceeds `pad=0.25`
  toward an obstacle, clamps the coordinate to `n±pad`. Effectively a continuous partial-penetration
  resolver, explicitly substepped to avoid tunneling through thin obstacles at high speed/low
  framerate.
- **cna-craft behavior** (implemented this session): `CollidesAt` (true AABB-vs-voxel-grid test)
  is now called from inside a substep loop in `PlayerController::Update` —
  `step = clamp(estimate, kMinSubsteps=8, kMaxSubsteps=64)`, same distance-based `estimate`
  formula shape as Craft's, resolved axis-separated (X, then Z, then Y) each substep.
  `kMaxSubsteps` is a deliberate addition beyond Craft's own unbounded `step`.
- **Status**: complete
- **Craft files**: `src/main.c:699-738`, called at `main.c:2462`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.cpp`
- **Priority**: high (done)
- **Verification method**: full 139-test physics suite re-run (no regressions) plus a new
  dedicated tunneling regression test, empirically confirmed meaningful by temporarily forcing
  `kMinSubsteps=kMaxSubsteps=1` and observing it correctly fail, then restoring and re-passing.
- **Notes**: cna-craft's AABB approach remains a cleaner algorithm than Craft's point+pad
  approximation; substepping closes the tunneling gap without adopting Craft's fractional-clamp
  resolver itself.

### 1.8 Gravity/jump behavior
- **Craft behavior** (`main.c:2411-2469`): jump sets `dy=8` only `if (dy==0)` (doubles as the
  grounded check) while not flying; gravity `dy -= ut*25` per substep, clamped to a **terminal
  velocity of -250 units/s**; any vertical collision resets `dy=0`; a hard floor-catch:
  `if (s->y < 0) s->y = highest_block(...) + 2`.
- **cna-craft behavior** (`PlayerController.cpp`): explicit `grounded_` flag gates jumping;
  `kJumpSpeed=8.0f` and `kGravity=25.0f` — **both match Craft exactly** (a prior session's
  user-reported jump-height bugfix deliberately matched these). Terminal-velocity clamp
  (`kTerminalVelocity=-250.0f`) matches Craft exactly. `World::HighestCollidableY` +
  `PlayerController::Update`'s post-substep-loop `y<0` check now port Craft's own floor-catch
  fallback exactly, including not resetting velocity afterward.
- **Status**: complete
- **Craft files**: `src/main.c:2411-2469`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.cpp`, `src/CnaCraft/Worlds/World.{hpp,cpp}`
- **Priority**: medium (done)
- **Verification method**: unit tests for both the terminal-velocity clamp and the new floor-catch
  (drop a player to y=-5 over generated terrain, confirm one Update() call snaps them to
  `HighestCollidableY+2`).
- **Notes**: Jump speed, gravity, terminal velocity, and the floor-catch safety net are all now
  exact matches.

### 1.9 Other player-control behavior (zoom/ortho/arrow-look/sprint/crouch)
- **Craft behavior**: no sprint, no crouch, no view bobbing anywhere in `main.c`. Arrow-key look
  exists as a mouse-look alternative at `dt*1.0` rad/s. Shift = hold-zoom (FOV 65→15°). F =
  hold-ortho (`g->ortho=64`). O/P = multiplayer picture-in-picture observe cameras (N/A,
  single-player only).
- **cna-craft behavior**: matches Craft's absence of sprint/crouch/view-bob. **Arrow-key look
  removed entirely 2026-07-10 (plan.md §12.1 item 29, user decision, part of the same
  Minecraft-style controls request as §1.6's flying change)** — it briefly existed at `1.0*dt`
  rad/s (matching Craft's literal `m = dt * 1.0`) after an earlier same-day Craft-fidelity pass,
  but the user found it an unwanted extra way to nudge the camera; mouse-look is now the only way
  to turn, matching Minecraft (which has no keyboard-look binding at all). Zoom and ortho both
  still present; zoom (Left Shift) now also gates on `!IsFlying()` since Shift became the fly
  mode's descend key in the same change (§1.6) — holding Shift to descend no longer also zooms.
- **Status**: complete (arrow-look is now a deliberate deviation from Craft, not parity)
- **Craft files**: `src/main.c:2418-2427, 2268-2273, 2901-2934`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp:29-42, 155-174, 283-289`
- **Priority**: low
- **Verification method**: code inspection

---

## 2. Hotbar, block roster, raycast, and block editing

### 2.1 Hotbar / selected-item switching
- **Craft behavior** (`on_key`, `main.c:2249-2267`; `on_scroll`, `main.c:2310-2324`): keys `1`-`9`
  → `item_index = key-'1'` (slots 0-8); key `0` → slot 9 (**10 direct-key slots**, not 9). `E`/`R`
  cycle next/prev with wraparound through the **entire `items[]` roster** (54 entries). Scroll
  wheel also cycles item_index (debounced via `SCROLL_THRESHOLD`). No empty-hand/eraser slot —
  `EMPTY` never appears in `items[]`; breaking is always via left-click regardless of selection.
- **cna-craft behavior** (`Hotbar.{hpp,cpp}`, `CnaCraftGame.cpp`): `SelectSlot`/`CycleNext`/
  `CyclePrev` all exist. Number keys `1`-`9` map directly, plus key `0` maps to slot 10
  (`kMaxNumberKeySlots=9` + a dedicated `Keys::D0` check — matches Craft's real 10 direct-key
  slots). `E`/`R` cycle next/prev with wraparound through the entire 54-slot roster. Scroll wheel
  also cycles (compares each frame's cumulative `ScrollWheelValue` against the previous frame's).
- **Status**: complete
- **Craft files**: `src/main.c:2249-2267, 2310-2324`; `src/config.h:38-39`
- **cna-craft files**: `src/CnaCraft/Worlds/Hotbar.{hpp,cpp}`, `src/CnaCraft/CnaCraftGame.cpp`
- **Priority**: medium (done)
- **Verification method**: unit tests (`TestHotbarSelectionAndCycling`) + manual play test (press
  `0`/scroll wheel/`R` in-game, confirm they cycle/select as expected)
- **Notes**: R reverse-cycle, key 0, and scroll-wheel cycling were all implemented in an earlier
  session — this "Status" line had drifted stale relative to that fix and the summary table below
  (corrected 2026-07-10).

### 2.2 Block/item roster
- **Craft `items[]`** (`item.c:4-60`, real order, `item_count=54`): Grass, Sand, Stone, Brick,
  Wood, Cement, Dirt, Plank, Snow, Glass, Cobble, LightStone, DarkStone, Chest, Leaves, TallGrass,
  YellowFlower, RedFlower, PurpleFlower, SunFlower, WhiteFlower, BlueFlower, then 32
  `Color00`-`Color31` dye/paint blocks. **`CLOUD` is deliberately excluded from `items[]`** —
  world-gen only, never player-placeable in real Craft.
- **cna-craft `Hotbar::kSlots`** (54 slots — matches Craft's real `item_count` exactly, added
  2026-07-10): Grass, Dirt, Sand, Stone, Cobblestone, Brick, Plank, Wood, Cement, LightStone,
  DarkStone, Snow, Glass, Leaves, TallGrass, Flower, then Chest, RedFlower, PurpleFlower,
  SunFlower, WhiteFlower, BlueFlower, then `Dye00`-`Dye31` (32 dye colors, Craft's own
  `COLOR_00`-`COLOR_31` — Craft doesn't name individual dye colors either). Same item *set* as
  Craft's `items[]`, different order (the new slots were appended after the original 16 rather
  than interleaved in Craft's exact positions, since `BlockType`'s enum ordinals are persisted
  directly — see `BlockType.hpp`'s doc comment). `Cloud` **removed** (user decision 2026-07-10:
  match Craft exactly) — it's still a real `BlockType` (world-gen only via
  `World::GenerateClouds`), just not in the placeable roster, matching Craft's own `items[]`
  never listing `CLOUD` either.
- **Status**: complete
- **Craft files**: `src/item.h:4-59`, `src/item.c:4-62`
- **cna-craft files**: `src/CnaCraft/Worlds/BlockType.hpp`, `src/CnaCraft/Worlds/Hotbar.hpp`
- **Priority**: medium (done)
- **Verification method**: unit tests (`TestHotbarSelectionAndCycling`,
  `TestExpandedRosterBlockDefsMatchExpectedShape`) + diff the two ordered item sets (done above)
- **Notes**: Removing `Cloud` from the placeable hotbar was a **design trade-off between Craft
  fidelity and cna-craft's own prior player-facing feature** (players could previously build with
  clouds) — resolved in favor of fidelity per user decision 2026-07-10.

### 2.3 Raycast / hit-test algorithm
- **Craft behavior** (`_hit_test`/`hit_test`, `main.c:603-664`): fixed-step supersampled march
  (32 substeps/unit), `roundf()`-snapped voxel testing, hardcoded max distance **8**. Face normal
  is not returned directly — derived separately in `hit_test_face` by diffing two hit-test calls,
  used only for sign placement.
- **cna-craft behavior** (`VoxelRaycast::Cast`): true Amanatides-Woo DDA — analytic `tMax`/`tDelta`
  per axis, returns an explicit `RaycastHit{x,y,z,nx,ny,nz}` with face normal computed directly.
  `kMaxReach=8.0f` (`CnaCraftGame.cpp`) — changed from an earlier 6.0f approximation per user
  decision 2026-07-10, now matching Craft's hardcoded 8 exactly.
- **Status**: complete (algorithmically equivalent-or-better, and now reach-exact too)
- **Craft files**: `src/main.c:603-664`
- **cna-craft files**: `src/CnaCraft/Worlds/VoxelRaycast.{hpp,cpp}`, `src/CnaCraft/CnaCraftGame.cpp`
- **Priority**: low
- **Verification method**: constant comparison (`kMaxReach=8`, matches Craft's `8` exactly);
  full test suite re-run (no regressions — no existing test asserts an exact reach boundary).
- **Notes**: cna-craft's DDA is a genuine algorithmic improvement over Craft's march-and-round —
  not a gap. Reach distance now matches exactly too.

### 2.4 Visible targeted-block behavior (wireframe outline)
- **Craft behavior** (`render_wireframe`, `main.c:1734-1752`; `make_cube_wireframe`,
  `cube.c:191-214`): every frame, hit-tests the targeted block; if it's an obstacle, draws a 3D
  wireframe box (12 edges, `n=0.53` half-size — slightly larger than the unit block) around it
  using a dedicated line shader with `GL_COLOR_LOGIC_OP` (XOR-style highlight).
- **cna-craft behavior** (`Render::SelectionOutline`): every frame, the same raycast used for
  break/place feeds `hasTargetedBlock_`/`targetedBlockX_/Y_/Z_` (`CnaCraftGame.cpp`); when a block
  is targeted, a 12-edge line-list box is drawn around it via stock `BasicEffect`, with
  `kHalfSize=0.53f` matching Craft's own `n=0.53` exactly (slightly larger than the block's true
  0.5 half-size, so the outline sits just outside the block's own faces).
- **Status**: complete
- **Craft files**: `src/main.c:1734-1752`, `src/cube.c:191-214`, `shaders/line_*.glsl`
- **cna-craft files**: `src/CnaCraft/Render/SelectionOutline.{hpp,cpp}`, `src/CnaCraft/CnaCraftGame.cpp`
- **Priority**: high (done)
- **Verification method**: manual play test — look at a block, confirm an outline renders around
  exactly that block and updates as the camera moves
- **Notes**: **Does NOT require custom shader support** — a simple line-list primitive drawn with
  stock `BasicEffect` (no `ShaderEffect` needed) is sufficient, so this is unblocked on all three
  backends (EASYGL/VULKAN/BGFX) unlike AO/sky-dome/fog-via-custom-shader. Implemented in an earlier
  session.

### 2.5 Block breaking
- **Craft behavior** (`on_left_click`, `main.c:2140-2151`; `is_destructable`, `item.c:191-199`):
  guarded by `hy>0 && hy<256 && is_destructable(hw)` — `is_destructable` returns false only for
  `EMPTY` and `CLOUD` (Craft has no bedrock/unbreakable-boundary concept at all). On break, if the
  block directly above is a plant, it's also destroyed (support-chain reaction).
- **cna-craft behavior** (`CnaCraftGame.cpp`): on left-click, casts a ray; only breaks if
  `World::IsBreakable(hit->x,hit->y,hit->z)` is true (`World.cpp`: `solid && breakable`, false for
  Bedrock — a cna-craft-only "world-boundary" block — **and, fixed 2026-07-10 (plan.md §12.1 item
  32), also false for Cloud**, matching `is_destructable(CLOUD)==0`; Cloud's `BlockDef` had wrongly
  left `breakable` at its default `true`, so clouds were minable here while real Craft's never
  are. Since the light-toggle guard (§2.7) and the builder-command erase (`WorldEditor::
  PaintBlock`, §4.5) route through this same predicate, the one flag fix also stopped clouds
  accepting light toggles and being erased by `/cube`-style commands, both matching Craft).
- **Status**: complete
- **Craft files**: `src/main.c:2140-2151`, `src/item.c:191-199`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp`, `src/CnaCraft/Worlds/World.{hpp,cpp}`,
  `src/CnaCraft/Worlds/BlockType.hpp`
- **Priority**: critical (done)
- **Verification method**: unit tests (`World::IsBreakable` false for Bedrock and for Cloud;
  `PaintBlock` leaves a Cloud in place on an Air paint but overwrites it on a non-Air paint,
  builder_block's exact net behavior); manual test — mine to the world floor, left-click the
  bottom layer, confirm it doesn't break
- **Notes**: Implemented in an earlier session. No plant-support chain reaction (moot — Craft's
  version only matters because a plant sitting on a now-broken block would otherwise float;
  cna-craft's own plants aren't currently placed by world-gen directly above ordinary breakable
  terrain in a way that's been observed to float, and this hasn't come up as a reported issue).

### 2.6 Block placing
- **Craft behavior** (`on_right_click`, `main.c:2153-2163`): guarded by `hy>0 && hy<256 &&
  is_obstacle(hw)`, **and** `!player_intersects_block(2, s->x,s->y,s->z, hx,hy,hz)` — placement
  is blocked if it would intersect the player's own bounding box (radius 2).
- **cna-craft behavior** (`CnaCraftGame.cpp`, the shared `tryPlaceBlock` lambda): on right-click
  (or Ctrl+left-click, §2.7), casts a ray; places only if BOTH halves of Craft's guard pass —
  `World::IsCollidable(hit)` on the block being placed against (this project's exact
  `is_obstacle` equivalent: false for Cloud and plants, so nothing can be built on/against a
  cloud or a grass/flower billboard) AND `!player_->IntersectsBlock(px,py,pz)` (rejects a
  placement overlapping the player's own AABB).
- **Status**: complete
- **Craft files**: `src/main.c:2153-2163`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp`, `src/CnaCraft/Worlds/PlayerController.{hpp,cpp}`
- **Priority**: critical (done)
- **Verification method**: unit test (`TestPlayerControllerIntersectsBlockGuardsPlacement`;
  `IsCollidable(Cloud)==false` is unit-tested and is the predicate the new guard consumes —
  the guard wiring itself is CnaCraftGame glue, covered by code review per this project's usual
  split); manual test — look straight down at your own feet, right-click, confirm placement is
  blocked
- **Notes**: **Audit-error correction (2026-07-10, plan.md §12.1 item 32, user-reported "why can
  I build on clouds?")**: this entry's earlier revision quoted both halves of Craft's guard in
  the Craft-behavior line, then listed only the player-overlap half on the cna-craft side — and
  still said `complete`. The `is_obstacle(hw)` half was genuinely missing: the raycast happily
  hits a Cloud (it's solid/hit-testable), and with no obstacle check on the target, blocks could
  be placed on clouds and on plants, both impossible in real Craft. A prior-session audit error
  in the entry itself, not a regression from this session's work.

### 2.7 Special interactions (Ctrl+click, middle-click, light toggle)
- **Craft behavior**: Ctrl+left-click (or Ctrl+Enter) → acts as right-click (place). Ctrl+right-click
  → `on_light()`, toggles a placed point-light source at the targeted block. Middle-click →
  `on_middle_click()`, an "eyedropper" that sets the selected hotbar item to match the targeted
  block's type.
- **cna-craft behavior**: middle-click eyedropper (`Hotbar::SelectByBlockType`) and Ctrl+left-click
  as place (`tryPlaceBlock` lambda shared with ordinary right-click, gated on
  `Keys::LeftControl`/`RightControl`) are implemented in `CnaCraftGame::Update`. Left Ctrl has no
  other binding in cna-craft — flying's own descend is Left Shift (§1.6, Minecraft-style), not
  Ctrl, so there's no modifier-key conflict to speak of.
  Ctrl+right-click light-toggle added 2026-07-10 (plan.md §12.1 item 17 follow-up, user decision):
  `World::IsLightSource`/`SetLightSource` (a per-instance overlay mirroring Craft's own separate
  `lights` Map alongside its block-type `map`, `src/main.c`) toggled on `world_.IsBreakable`-gated
  blocks (matches Craft's own `is_destructable` guard in `on_light`), persisted via a new
  `light(p,q,x,y,z,w)` `WorldStore` table matching Craft's real schema exactly. **Visually,
  this is a deliberate, documented simplification, not a full port** — see §5.1 below for why
  Craft's real light-bleeds-into-neighbors propagation isn't achievable here, and what cna-craft
  does instead.
- **Status**: complete
- **Craft files**: `src/main.c:2131-2138, 2153-2175, 2229-2361`
- **cna-craft files**: `src/CnaCraft/Worlds/{Hotbar,Chunk,World}.{hpp,cpp}`,
  `src/CnaCraft/Persistence/WorldStore.{hpp,cpp}`, `src/CnaCraft/CnaCraftGame.cpp`
- **Priority**: low (done)
- **Verification method**: `TestHotbarSelectionAndCycling`'s `SelectByBlockType` checks (eyedropper,
  unit-tested); Ctrl+click-as-place verified via a clean EasyGL build + headless smoke run (pure
  input-wiring glue over already-tested `IntersectsBlock`/`SetBlock`, no new `Worlds/`-layer logic);
  light-toggle's data model/persistence/glow-mesh-emission are unit-tested
  (`TestChunkAndWorldLightSourceOverlay`, `TestChunkMesherEmitsGlowMeshForLightSourceBlocks`,
  persistence Round 11) — the toggle keybind itself needs a live Ctrl+right-click to verify, which
  (like every other click-dependent feature this session) couldn't be confirmed live in this
  sandbox; a full EasyGL build + real-build screenshot did confirm the new glow render pass adds
  no visual regression to ordinary terrain when no light is toggled.
- **Notes**: The light-toggle mechanic and persistence are 100% Craft-accurate. The *visual*
  result is not — see §4.3's notes for the full reasoning (a CNA engine-level rendering
  constraint, not a scope choice). **Perf fix 2026-07-10 (plan.md §12.1 item 28 follow-up,
  user-reported general slowdown)**: the glow render pass originally iterated every loaded
  `ChunkRenderer` and ran a frustum-intersection test on each, every single frame, unconditionally
  — a real, avoidable per-frame cost added on top of the pre-existing opaque+transparent passes,
  even though almost no world ever has any block actually lit. `CnaCraftGame::glowChunkCount_` now
  tracks how many loaded chunks currently have a non-empty glow mesh (updated incrementally at the
  two places that can change: `PollMeshJobs`' `ApplyMesh` call, `UnloadColumn`), and `Draw()` skips
  the entire glow pass — map iteration and frustum tests included, not just the already-free draw
  calls — whenever it's 0.

### 2.8 Collision rules for solid/transparent/non-collidable blocks
- **Craft behavior**: `is_obstacle(w)` (blocks movement) vs `is_transparent(w)` (occludes
  rendering) are two independent predicates in `item.c` — e.g. Cloud is non-obstacle (walk
  through) but not transparent (still occludes); Glass/Leaves are transparent but still
  obstacles.
- **cna-craft behavior**: `World::IsSolid`/`IsOpaque`/`IsCollidable` (`World.cpp:61-79`) is a
  faithful 3-way reimplementation of exactly this split, already unit-tested (Glass, Cloud, Leaves
  all covered in `tests/worlds_smoke_test.cpp`).
- **Status**: complete
- **Craft files**: `src/item.c` (`is_obstacle`, `is_transparent`)
- **cna-craft files**: `src/CnaCraft/Worlds/World.cpp:61-79`, `src/CnaCraft/Worlds/BlockType.hpp`
- **Priority**: low
- **Verification method**: existing unit tests (already passing)

---

## 3. Chunk system, terrain, and world content

### 3.1 Chunk system
- **Craft behavior**: no fixed 3D array — each `Chunk` (one 32×32 XZ column, full 0–255 Y range)
  holds sparse `(x,y,z)→w` entries in an open-addressed hash map (`src/map.c`/`map.h`), dynamically
  grown. Chunks live in a flat dynamic array keyed by `(p,q)`.
- **cna-craft behavior** (redesigned 2026-07-10, plan.md §12.1 item 19 — user decision: pursue an
  unbounded world after all): `World`'s storage is now a two-level container,
  `std::unordered_map<ColumnKey, std::array<std::unique_ptr<Chunk>, WORLD_CHUNKS_Y>> columns_`,
  keyed by packed `(cx,cz)` — unbounded in X/Z, exactly like Craft's own `(p,q)`-keyed chunk array.
  `Chunk` itself stays a dense fixed `std::array<BlockType,16³>` (not Craft's sparse hash map — a
  pure internal storage-strategy difference with no player-visible effect, since Y never streams
  and 4 chunks of dense height is trivial memory) and `CHUNK_SIZE` stays 16, not Craft's 32 (also
  purely internal). Y remains fixed (`WORLD_CHUNKS_Y=4`), matching Craft's own real behavior —
  Craft never streams Y either.
- **Status**: complete
- **Craft files**: `src/map.c`, `src/map.h`
- **cna-craft files**: `src/CnaCraft/Worlds/Chunk.{hpp,cpp}`, `World.{hpp,cpp}`
- **Priority**: medium (done)
- **Verification method**: unit tests (`cna_craft_worlds_smoke_test`: column pack/unpack,
  generate/load/unload lifecycle, copy round-trips) + real-build Xvfb/xdotool fly-navigation
  verification (confirmed the world extends well past the old 128×128 boundary in every
  direction, with no crashes/corruption over multiple long fly sessions)

### 3.2 Chunk loading/unloading by distance
- **Craft behavior**: `CREATE_CHUNK_RADIUS=10`, `RENDER_CHUNK_RADIUS=10`, `DELETE_CHUNK_RADIUS=14`
  (`config.h`); `force_chunks`/`ensure_chunks` create chunks around the player (worker-threaded),
  `delete_chunks` frees far-away ones every frame.
- **cna-craft behavior** (redesigned 2026-07-10, plan.md §12.1 item 19): `CnaCraftGame::
  UpdateStreaming` runs every frame — loads any column within `kCreateRadius=6` (chebyshev) of the
  player's current `(cx,cz)` that isn't already loaded or in flight, dispatch-capped at
  `kMaxColumnLoadsPerFrame=2` new background jobs/frame; unloads any loaded column beyond
  `kDeleteRadius=9` (hysteresis margin over the create radius, same reasoning as Craft's
  10/10/14). Unlike Craft's fixed 4-worker-thread pool, generation/meshing each dispatch one
  `System::Threading::Tasks::TaskT` per column/chunk — `sharp-runtime`'s `TaskT::Run` is a thin
  wrapper over `std::async(std::launch::async, ...)`, which (per the C++ standard, and confirmed in
  practice on this project's libstdc++ target) spawns a genuine new OS thread per call rather than
  drawing from a real bounded pool — self-throttled only via the dispatch/apply caps above, not a
  true pool. **Revised again 2026-07-10 (plan.md §12.1 item 28, user-reported "invisible window on
  startup" bug)**: `Initialize()` no longer force-generates+meshes the spawn-area columns
  synchronously before placing the player (an earlier version of this note said it still did,
  mirroring Craft's own `force_chunks`-before-`ensure_chunks` split) — that synchronous burst
  (169 columns at the default radius) measured ~14 seconds wall-clock in this project (SQLite +
  noise generation + CNA-abstraction GPU uploads per column cost far more per-column than Craft's
  raw C/GL path), during which the SDL window existed but had never presented a single frame; many
  window managers/compositors render that as a blank/invisible window. `Initialize()` now does
  nothing extra — `UpdateStreaming` naturally discovers the player's spawn column is unloaded on
  the very first `Update()` and dispatches it through the same already-backgrounded pipeline used
  for ordinary runtime streaming, so `Draw()` presents a real frame (sky/fog/HUD) on frame 1 and
  terrain pops in progressively over roughly a second instead of blocking. Deliberately no longer
  matches Craft's "never see an ungenerated void" startup guarantee exactly.
  **Revised a third time 2026-07-10 (plan.md §12.1 item 31, user-reported "WASD doesn't walk,
  image torn" bug)**: item 28's "load nothing synchronously" went one column too far — the player
  free-fell through their own not-yet-loaded spawn ground from frame 1, got entombed inside the
  terrain when the column materialized around them (~a second later; the raster-order dispatch
  loaded far corners first, making it worse), and the then-every-frame position save persisted the
  entombed position into `world.db` so even restarting with fixed code restored them stuck. The
  final arrangement: `Initialize()` synchronously loads exactly ONE column (the player's own,
  ~80ms — imperceptible, unlike 169 of them), `UpdateStreaming` dispatches nearest-first
  (chebyshev-sorted) instead of raster order, and `CnaCraftGame::HealPlayerIfEmbedded` (backed by
  the new `PlayerController::IsEmbedded`) snaps an inside-terrain player to the surface at the two
  places entombment can arise: `Initialize()` (heals `world.db` files poisoned while the
  regression was live) and `PollGenerationJobs`' column-apply step (a column materializing around
  a player who moved into not-yet-loaded territory at ground level). The floor-catch (§1.8) still
  covers the fell-below-the-world case; `IsEmbedded` covers the entombed-above-zero case it
  structurally couldn't.
- **Status**: complete
- **Craft files**: `src/main.c` (`force_chunks` L1307, `ensure_chunks` L1418, `delete_chunks`
  L1225)
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.{hpp,cpp}` (`UpdateStreaming`,
  `DispatchColumnGeneration`/`PollGenerationJobs`, `DispatchMeshingForDirtyChunks`/`PollMeshJobs`)
- **Priority**: low (given the project's original stated fixed-world scope — done anyway per user
  decision to close this gap)
- **Verification method**: unit tests + real-build Xvfb/xdotool verification (fly far from spawn,
  confirm terrain streams in with no permanent holes; fly back, confirm no crashes/leaks; thread
  count stayed bounded, not runaway). Item 28's startup fix specifically verified by timing:
  measured wall-clock from process start to first non-black rendered frame dropped from ~14s
  (synchronous force-load, reproduced and measured directly) to under 1s (background streaming
  from frame 1), via real Xvfb screenshots taken at fixed intervals after launch. Item 31's
  entombment fix verified end-to-end against a deliberately poisoned `world.db` (state row set to
  an inside-terrain position via sqlite3, exactly the state a live-regression `world.db` holds):
  launch printed the heal message, the player stood on the surface, and the saved state
  self-corrected within a second; a fresh-world launch screenshot confirmed a normal ground-level
  view. `IsEmbedded` itself is unit-tested (buried/standing/unloaded-column cases).
- **Notes**: two real bugs were found and fixed during this session's own verification, not left
  as known issues: (1) completed-but-over-the-per-frame-apply-cap background jobs were being
  discarded outright instead of deferred to a later frame, which for meshing specifically (whose
  dirty flag clears at *dispatch* time, not completion time) caused permanent unfilled rectangular
  holes in distant terrain; (2) `DispatchMeshingForDirtyChunks` originally had no dispatch cap at
  all, letting one column's arrival fan out into dozens of uncapped concurrent mesh tasks. Both
  fixed (deferred-not-discarded apply logic, `kMaxMeshDispatchesPerFrame=8` added) and
  re-verified via matched-flight-path screenshot comparisons. Player-driven edits and freshly
  streamed-in terrain both now render 1-2 frames later than same-frame synchronous meshing would
  (backgrounded meshing's one deliberate, documented, imperceptible-at-60fps behavior change).

### 3.3 Chunk/terrain generation algorithm
- **Craft behavior** (`create_world`, `src/world.c`): `f = simplex2(x*0.01,z*0.01,4,0.5,2)`,
  `g = simplex2(-x*0.01,-z*0.01,2,0.9,2)`, `mh = g*32+16`, `h = f*mh` — **two independent simplex2
  calls combined multiplicatively**. Sea/sand: `h<=12` → `h=12`, block=Sand; else block=Grass.
- **cna-craft behavior** (`NoiseGenerator::Height`, `NoiseGenerator.cpp`) — **re-ported exactly
  this session** (user decision 2026-07-10): now uses Craft's real two-sample multiplicative
  formula verbatim (`f = Simplex2(x*0.01, z*0.01, 4, 0.5, 2)`, `g = Simplex2(-x*0.01, -z*0.01, 2,
  0.9, 2)`, `mh = g*32+16`, `height = f*mh`), with Craft's own `h<=12 -> h=12` sea-level clamp
  folded in (unconditional, not probabilistic — guarantees the lower bound exactly). `World.cpp`'s
  `kSandMaxHeight` updated from a scaled-down 10 to Craft's literal `12`, now that the height range
  itself matches Craft's scale.
- **Status**: complete
- **Craft files**: `src/world.c` (whole file, ~85 lines)
- **cna-craft files**: `src/CnaCraft/Worlds/World.cpp`, `NoiseGenerator.cpp`
- **Priority**: high (done)
- **Verification method**: unit tests (determinism, seed-variation, sane-range-with-tolerance,
  Sand-presence/threshold — 2 tests had hardcoded old-range numbers updated, 148 checks total, no
  API signature change so no wide-reaching test rewrite) plus a real EasyGL build screenshot
  (terrain/outline/tree all rendered correctly, no corruption or extreme spikes).
- **Notes**: Does not replicate Craft's whole-column-is-one-material model (Craft has no
  Stone/Bedrock layering at all — every column below `h` is one uniform block type); cna-craft's
  pre-existing layered terrain (Bedrock/Stone/Dirt/Grass-or-Sand) is kept, only the *height
  formula* was re-ported, not the layering model itself (that was never a Craft citation — an
  independent design choice from an earlier session).

### 3.4 Face culling
- **Craft behavior**: `is_transparent`/`is_obstacle` (`item.c`) feed a padded `opaque[]` array in
  `compute_chunk` (`main.c:958-1145`); each face culled against `!opaque[neighbor]`, plus a
  bottom-face-at-y==0 special case.
- **cna-craft behavior**: `World::IsOpaque` = `solid && !transparent`; `ChunkMesher::Build` culls
  each face via `!world.IsOpaque(neighbor)` — functionally equivalent 6-neighbor test.
- **Status**: complete
- **Craft files**: `src/item.c`, `src/main.c:958-1145`
- **cna-craft files**: `src/CnaCraft/Worlds/World.cpp:76-79`, `ChunkMesher.cpp:66-67`
- **Priority**: low

### 3.5 Mesh generation
- **Craft behavior** (`make_cube_faces`, `cube.c:7-98`): naive per-block-per-face — one static
  quad per exposed face, with baked per-vertex AO/light (see §5.1).
- **cna-craft behavior** (`ChunkMesher::Build`): also naive per-block-per-face, structurally
  equivalent, minus AO/lighting (flat-shaded).
- **Status**: complete (meshing strategy); AO gap tracked separately at §5.1
- **Craft files**: `src/cube.c:7-98`
- **cna-craft files**: `src/CnaCraft/Worlds/ChunkMesher.cpp:50-98`
- **Priority**: low

### 3.6 Transparent blocks
- **Craft behavior**: `is_transparent` true only for `EMPTY`, plants, `GLASS`, `LEAVES`.
- **cna-craft behavior**: `BlockDef.transparent` true only for `Glass`, `Leaves` — exact match
  for the two "real" (non-plant) transparent solids.
- **Status**: complete
- **Craft files**: `src/item.c`
- **cna-craft files**: `src/CnaCraft/Worlds/BlockType.hpp`
- **Priority**: low

### 3.7 Plants/flowers/tall grass
- **Craft behavior** (`make_plant`, `cube.c:100-158`): a 2-plane "X" cross billboard (4 quads),
  randomly rotated per-block via noise, for TallGrass + 6 flower colors; non-obstacle,
  transparent, always fully rendered (no face culling test).
- **cna-craft behavior**: `ChunkMesher::EmitPlant` — a 4-quad cross
  billboard (two diagonal planes, each emitted with both windings so it's visible from any angle,
  matching Craft's 4-quad total exactly), gated by a new `BlockDef.plant` flag. `BlockType::
  TallGrass` and all 6 flower colors (`Flower`/`RedFlower`/`PurpleFlower`/`SunFlower`/
  `WhiteFlower`/`BlueFlower` — the other 5 added 2026-07-10, see §2.2) are plant types.
  `World::GenerateGrassDecoration`/`GenerateFlowers` place them using Craft's real triggers
  (`simplex2(-x*0.1, z*0.1, 4, 0.8, 2) > 0.6` for grass, `simplex2(x*0.05, -z*0.05, 4, 0.8, 2) >
  0.7` for flowers). Craft's second noise sample picking one of the 6 flower colors
  (`18 + simplex2(x*0.1, z*0.1, 4, 0.8, 2) * 7`) is now ported too, clamped to always land on a
  real color (Craft's literal formula can overshoot past its own valid range when the noise
  sample drifts slightly above 1.0 — an upstream quirk not worth reproducing). Per-block random
  Y-axis rotation (Craft's `mat_rotate`) is now ported too (added 2026-07-10, user decision):
  `EmitPlant` rotates each plant's cross around its own vertical center axis by
  `simplex2(wx, wz, 4, 0.5, 2) * 360` degrees, matching Craft's own `rotation` formula
  (`main.c`) exactly — a fixed, non-per-world-seeded noise sample (`kPlantRotationSeed`), since
  this is cosmetic per-position variation, not part of the seed-varying terrain shape, matching
  the fact that Craft's own noise here draws from its single global permutation table too.
- **Status**: complete
- **Craft files**: `src/cube.c:100-158`, `src/item.c` (`is_plant`), `src/world.c` (color-pick)
- **cna-craft files**: `src/CnaCraft/Worlds/ChunkMesher.cpp` (`EmitPlant`),
  `src/CnaCraft/Worlds/BlockType.hpp` (`BlockDef.plant`, the 6 flower `BlockType`s),
  `src/CnaCraft/Worlds/World.cpp` (`GenerateGrassDecoration`, `GenerateFlowersColumn`),
  `src/CnaCraft/Render/TextureAtlas.cpp` (tiles 19-20/22-26, `Pattern::GrassBlade`/`Flower`)
- **Priority**: high (done)
- **Verification method**: visual — confirmed via a real EasyGL build screenshot showing
  blade-shaped billboards with transparent gaps growing out of grass terrain (TallGrass; Flower
  reuses the identical, already-verified mesh/generation code path — not independently
  re-screenshotted, see plan.md §12.1 item 22 for the full note); 23 new unit tests total across
  both blocks (mesh shape/never-culled invariant, solid/collidable/transparent/breakable rules,
  generation presence/determinism/surface-height invariant)

### 3.8 Trees
- **Craft behavior**: `simplex2(x,z,6,0.5,2) > 0.84` trigger on grass columns (with a per-chunk
  edge-margin check), 7-tall Wood trunk, Leaves canopy blob (`ox²+oz²+dy²<11`).
- **cna-craft behavior** (`World::GenerateTrees`, added prior session): **all 6 numeric constants
  verified to match Craft exactly** (octaves, persistence, lacunarity, threshold, canopy
  radius/dist², trunk height). One documented deviation: canopy only fills still-Air cells
  (Craft overwrites unconditionally) to avoid cross-tree stomping; trunk placement stays
  unconditional, matching Craft.
- **Status**: complete
- **Craft files**: `src/world.c` (tree block)
- **cna-craft files**: `src/CnaCraft/Worlds/World.cpp:129-159`
- **Priority**: low

### 3.9 Caves/overhangs — CORRECTED
- **Craft behavior**: **does not exist.** A prior session's `plan.md` backlog entry claimed
  "Craft's world.c combines a 2D heightmap with a 3D noise term for overhangs" — this citation is
  **false**. `src/world.c` contains no cave-carving code of any kind. Craft's only 3D-noise use is
  the cloud pass (§3.10).
- **cna-craft behavior**: not implemented (correctly — there is nothing to port).
- **Status**: needs_human (if pursued at all — would be an original design, not a Craft port; no
  reference algorithm/thresholds exist to verify against)
- **Craft files**: none (feature doesn't exist)
- **cna-craft files**: none
- **Priority**: n/a
- **Notes**: Already corrected in `plan.md` §11.1 during a prior session; repeated here so this
  document is self-contained and the correction isn't lost.

### 3.10 Clouds
- **Craft behavior**: `y∈[64,72)`, `simplex3(x*0.01,y*0.1,z*0.01,8,0.5,2) > 0.75`, unconditional
  overwrite.
- **cna-craft behavior** (`World::GenerateClouds`, added prior session): all 5 noise parameters
  match Craft exactly; Y-band adapted to `[58,62]` (documented, since `WORLD_SIZE_Y=64` is far
  shorter than Craft's effectively-unbounded height) with an added "only fill Air" guard (Craft
  doesn't need this since its clouds sit far above all terrain).
- **Status**: complete
- **Craft files**: `src/world.c` (cloud block)
- **cna-craft files**: `src/CnaCraft/Worlds/World.cpp:181-196`
- **Priority**: low

---

## 4. Persistence, signs, chat, multiplayer

### 4.1 World persistence / save-load
- **Craft behavior**: SQLite (`src/db.c`/`db.h`) — `db_init` opens a `.db` file, a dedicated
  writer thread drains a queued ring buffer, periodic + on-exit commits, separate player-position
  `state` table.
- **cna-craft behavior**: `Persistence::WorldStore` — SQLite via
  `find_package(SQLite3 REQUIRED)` (user decision 2026-07-10; confirmed available as a system
  package, `libsqlite3-dev`, no `FetchContent`/vendoring needed). Opens/creates `world.db` in the
  working directory (matching Craft's own simple single-default-file approach, not a save-slot
  system). No dedicated writer thread or ring buffer — `SaveEdits` runs synchronously right after
  each player edit (`CnaCraftGame::Update`), a deliberate simplification for this prototype's low
  single-player edit rate (documented in `WorldStore.hpp`). Player-position `state` table added
  2026-07-10 (plan.md §12.1 item 17 follow-up, user decision): a single-row `state(x,y,z,rx,ry)`
  table matching Craft's own `db_save_state`/`db_load_state` schema exactly (`WorldStore::
  SavePlayerState`/`LoadPlayerState`). One deliberate deviation from Craft's real save-once-at-
  clean-exit behavior: cna-craft saves once per second instead, so a crashed or killed process
  still resumes within a second of where it was. **Revised 2026-07-10 (plan.md §12.1 item 31,
  user-reported stutter/slowdown)**: the first version saved every frame — "cheap single-row
  delete+insert" was wrong in the way that matters, because each save's SQLite commit fsyncs the
  disk, and the DELETE+INSERT pair as two implicit transactions fsynced twice; 60-120 fsyncs/second
  is a real, constant per-frame hitch on ordinary hardware (invisible on this project's own
  fast-disk sandbox, which is why it shipped unnoticed). Now throttled to one save per second in
  `CnaCraftGame::Update` and wrapped in a single explicit transaction in `SavePlayerState`.
  `CnaCraftGame::Initialize` loads the saved state (if any) before placing the player, so a
  returning player spawns where they actually were, not always world-origin (their spawn column is
  loaded synchronously and the restored position is validated/healed against it — see §3.2 item
  31); falls back to the existing block-center/height+2 spawn if nothing was ever saved, matching
  Craft's own `if (!loaded) s->y = highest_block(...) + 2`.
- **Status**: complete
- **Craft files**: `src/db.c`, `src/db.h`
- **cna-craft files**: `src/CnaCraft/Persistence/WorldStore.{hpp,cpp}`,
  `src/CnaCraft/CnaCraftGame.cpp` (Initialize/Update wiring)
- **Priority**: high (done)
- **Verification method**: a new, separate test target `cna_craft_persistence_smoke_test`
  (`tests/persistence_smoke_test.cpp`, 12 checks, registered as ctest `PersistenceSmokeTest`)
  exercises `WorldStore` directly against a real SQLite file on disk — save/load/overwrite/
  Air-round-trip. End-to-end verification via the real graphical game (click to break, check the
  `.db` file) was attempted but synthetic mouse clicks into this sandbox's relative-mouse-mode SDL
  window proved unreliable (same flakiness class already documented for mouse-look elsewhere in
  this project); the direct `WorldStore` test is arguably better anyway — reliable, repeatable,
  permanent regression coverage. The full game was confirmed to build clean, start without error,
  and create `world.db` on launch via a real headless `--smoke` run. Player-position persistence
  specifically (movement doesn't need mouse clicks) *was* verified end-to-end in a real EasyGL
  build under Xvfb: moved away from spawn, killed the process, relaunched, and confirmed via a
  screenshot that the second launch resumed at the exact same view (not back at world-origin).

### 4.2 Delta storage of edits
- **Craft behavior**: `block(p,q,x,y,z,w)` table, unique-indexed on `(p,q,x,y,z)`, `insert or
  replace` per edit, loaded back over regenerated terrain.
- **cna-craft behavior**: `block(p,q,x,y,z,w)` table, unique-indexed on `(p,q,x,y,z)` — matches
  Craft's real schema exactly. Updated 2026-07-10 (plan.md §12.1 item 19) to add the `p,q`
  chunk-address columns once `World` gained real per-column addressing (§3.1); `WorldStore`
  computes `p,q` from `x,z` internally (same chunk-address formula as `World`) before binding, so
  call sites are unchanged. **No migration path from the pre-`p,q` schema** — an old `world.db`
  fails to open (logged, falls back to the harmless no-op store) rather than silently
  misbehaving; delete it to upgrade (same precedent as the earlier terrain-formula reset). `World`
  has the "enumerate only changed blocks" capability `plan.md`'s original persistence note said
  was needed: `BlockEdit`, `SetBlockAndRecordEdit`, `RecordedEdits()`, `ClearRecordedEdits()` — all
  zero-dependency additions to `Worlds/`, kept separate from `WorldStore`'s actual SQLite I/O.
- **Status**: complete
- **Craft files**: `src/db.c:59-151,315-334,404-420`
- **cna-craft files**: `src/CnaCraft/Worlds/World.{hpp,cpp}` (`BlockEdit` and friends),
  `src/CnaCraft/Persistence/WorldStore.cpp` (the actual SQL)
- **Priority**: high (done)

### 4.3 Signs / placed text
- **Craft behavior**: `sign.c`/`sign.h` — a `Sign{x,y,z,face,text[64]}` struct in a per-chunk
  `SignList` (not a block type), placed via a typing-buffer flow, persisted in its own `sign`
  table, rendered as a textured billboard quad on the target face.
- **cna-craft behavior**: `Worlds::Sign`/`SignStore` (`Worlds/Sign.{hpp,cpp}`) — the same
  `(x,y,z,face,text)` shape, keyed by `(x,y,z,face)`. Placed via a text-input state machine in
  `CnaCraftGame::Update()` (backtick opens typing via `TextInputEXT`, Backspace/Enter/Escape
  handled edge-triggered, WASD/look/click suspended while typing but gravity still integrates,
  matching Craft's own `if (!g->typing)` gate). Rendered by `Render::SignBillboard` as a textured
  quad per sign, oriented per face, with a dynamically-built text texture reusing the shared
  `Render/BitmapFont.hpp` `FontDrawText`. Persisted incrementally in `WorldStore`'s `sign` table
  (`UpsertSign`/`DeleteSign`/`DeleteSignsAt`, mirroring Craft's own `db_insert_sign`/
  `db_delete_sign`/`db_delete_signs`), loaded per-column (`WorldStore::LoadColumnSignsInto`)
  alongside that column's block edits as it streams in (plan.md §12.1 item 19), and purged
  (`SignStore::RemoveAllInColumn`) when its column unloads — signs don't leak memory or desync
  across a long streaming session. Breaking the underlying block deletes any signs on it
  (`SignStore::RemoveAllAt` + `WorldStore::DeleteSignsAt`), matching Craft's own `_set_block`
  calling `unset_sign()` when a block is set to type 0 — a sign can't outlive the block face it
  was attached to.
- **Status**: done (deliberate simplifications below)
- **Craft files**: `src/sign.c`, `src/sign.h`, `src/main.c:385-395,756-840,666-696,2214-2219`
- **cna-craft files**: `src/CnaCraft/Worlds/Sign.{hpp,cpp}`, `src/CnaCraft/Render/SignBillboard.{hpp,cpp}`,
  `src/CnaCraft/Render/BitmapFont.{hpp,cpp}`, `src/CnaCraft/Persistence/WorldStore.{hpp,cpp}`,
  `src/CnaCraft/CnaCraftGame.{hpp,cpp}`
- **Priority**: medium (done)
- **Notes**: Two deliberate differences from real Craft, both documented in-code: (1) face
  convention is a *symmetric* 6-face scheme derived directly from the raycast hit normal
  (0=+X,1=-X,2=+Y/top,3=-Y/bottom,4=+Z,5=-Z), not Craft's real asymmetric `hit_test_face`
  (main.c:666-696 — 4-way player-angle-dependent rotation on top faces only, no bottom-face
  support at all) — `VoxelRaycast` already provides a normal Craft's own hit-test doesn't, so
  there was no reason to replicate the asymmetry. (2) each sign quad is emitted with both
  triangle windings (12 indices instead of 6) rather than a single Craft-accurate outward
  winding, since this project's correct winding for non-cube geometry has needed real-build
  empirical verification every time so far (see `Render/SkyDome.cpp`'s note) — the visible
  tradeoff is mirrored ghost text bleeding through at a grazing angle, confirmed in the
  end-to-end verification screenshot (see `plan.md` §12.1 item 16). Verified end-to-end against
  a real headless `CnaCraft` build under Xvfb: typed and submitted a sign via `xdotool`-injected
  keystrokes, confirmed correct on-screen rendering, confirmed the row landed in `world.db`'s
  `sign` table via the new incremental `UpsertSign`, confirmed it reloaded correctly after a
  process restart. The persistence layer was revised after this initial pass, following a direct
  request to match Craft's real `db.c` more closely: it originally did a bulk delete-and-reinsert
  of the whole sign list on every save; re-checked against the real Craft source and changed to
  incremental per-row `UpsertSign`/`DeleteSign`/`DeleteSignsAt`, matching `db_insert_sign`/
  `db_delete_sign`/`db_delete_signs` exactly. That re-check also surfaced (and fixed) two smaller
  gaps: submitting an empty-text sign now deletes any existing sign at that face (Craft's
  `set_sign` does this unconditionally on a hit, not only when the typed text was already
  non-empty), and breaking a block now deletes any signs on it (Craft's `_set_block` →
  `unset_sign`), which the initial pass hadn't ported. The break-deletes-signs path could not be
  verified live via the GUI (synthetic mouse clicks into this project's relative-mouse-mode SDL
  window remain unreliable in this sandbox, same limitation already documented for item 15's
  persistence work) — verified instead via unit tests (`SignStore::RemoveAllAt`,
  `WorldStore::DeleteSign(s)At`) plus code review, since `CnaCraftGame`'s wiring calls those same
  tested functions directly with no transformation in between.

### 4.4 Player names / on-screen text
- **Craft behavior**: `render_players` draws other players as plain cubes — **no in-world
  nametags exist in real Craft** (moot for cna-craft anyway, single-player only). `render_text`/
  `draw_text` are used for a 2D chat/typing overlay and message log.
- **cna-craft behavior** (updated 2026-07-10, plan.md §12.1 item 17): `Render/Hud.cpp`'s
  embedded-bitmap-font text renderer (`FontDrawText`) is now used for the hotbar strip, the
  Sign/Command typing box, and the new 4-line scrolling message log (`Hud::PushMessage`, see
  §4.5) — the "2D chat/typing overlay and message log" role Craft's own `render_text` fills is
  now fully covered. No in-world nametags on other players, matching Craft's own real absence of
  that feature too (and moot regardless — no multiplayer).
- **Status**: complete
- **Craft files**: `src/main.c:1702-1719, 1788-1802, 2870-2895`
- **cna-craft files**: `src/CnaCraft/Render/Hud.{hpp,cpp}`
- **Priority**: low

### 4.5 Chat / slash commands
- **Craft behavior** (`parse_command`, `main.c:2021-2094`): real supported commands are
  `/identity`, `/logout`, `/login`, `/online`, `/offline`, `/view`, `/copy`, `/paste`, `/tree`,
  `/array`, `/fcube`, `/cube`, `/fsphere`, `/sphere`, `/fcirclex|y|z`, `/circlex|y|z`,
  `/fcylinder`, `/cylinder` — mostly world-editing macros, not chat commands in the Minecraft
  sense. Non-`/`-prefixed text is sent as plain chat via `client_talk`.
- **cna-craft behavior** (implemented 2026-07-10, plan.md §12.1 item 17): `Worlds::ExecuteCommand`
  (`WorldEditor.hpp/.cpp`) is a direct, verified port of every world-editing command —
  `/view`, `/copy`, `/paste`, `/tree`, `/array`, `/cube`, `/fcube`, `/sphere`, `/fsphere`,
  `/circlex|y|z`, `/fcirclex|y|z`, `/cylinder`, `/fcylinder` — via 7 new engine-agnostic
  primitives (`PaintBlock`/`FillCuboid`/`FillSphere`/`FillCylinder`/`FillArray`/`GrowTree`/
  `PasteRegion`). The 5 multiplayer-auth-only commands (`/identity`, `/login`, `/logout`,
  `/online`, `/offline`) and the plain-chat fallback are deliberately not ported — no networking
  exists (§4.6) and there's no other player to talk to; an unrecognized command returns an
  "Unknown command" message instead. `CnaCraftGame`'s sign-only typing state machine
  (`isTypingSign_` bool) generalized to a `TypingMode{None,Sign,Command}` enum — backtick still
  opens Sign typing unchanged, `/` (`Keys::OemQuestion`) opens Command typing with the buffer
  pre-seeded as `"/"`, matching Craft's own `g->typing_buffer[0]='/'` exactly. `mark0_`/`mark1_`
  (`CnaCraftGame`, mirrors Craft's `g->block0`/`g->block1`) track the last two edited
  positions+types via a new `RecordMark` call at both the break and place sites — the anchor
  points every command reads. `radii_` (`Worlds::CommandRadii`) replaces the old compile-time-fixed
  `kCreateRadius`/`kDeleteRadius` as the mutable runtime source of truth `/view` mutates (clamped
  to Craft's exact `1..24`); fog start/end are now computed from `radii_.createRadius` every frame
  instead of a compile-time constant, so a `/view` change immediately extends/shrinks the fade
  distance too. `Render::Hud` gained a real 4-line scrolling message log (`PushMessage`,
  Craft's own `MAX_MESSAGES=4` ring buffer, `config.h`) for command feedback — user decision
  2026-07-10: full Craft-accurate multi-line log, not a simplified single-line flash message.
- **Status**: complete
- **Craft files**: `src/main.c:2021-2094, 2183-2306`
- **cna-craft files**: `src/CnaCraft/Worlds/WorldEditor.{hpp,cpp}`, `src/CnaCraft/CnaCraftGame.
  {hpp,cpp}`, `src/CnaCraft/Render/Hud.{hpp,cpp}`
- **Priority**: medium (done)
- **Verification method**: unit tests (`tests/worlds_smoke_test.cpp` — one test per `WorldEditor`
  primitive plus a broad `ExecuteCommand` dispatch test covering every command string, the
  type-mismatch-rejects case, the `/view` clamp, and the unknown-command fallback; 267 checks
  total, up from 226) + a real EasyGL build under Xvfb: confirmed `/` opens the "Command: /_"
  typing box, confirmed `/view 20` both updates the console log and pushes "Viewing distance set
  to 20." into the on-screen message log, confirmed an invalid `/view 99` and an unrecognized
  `/nonsense` both produce Craft-accurate feedback messages that stack correctly (oldest on top,
  newest on bottom, same order as Craft's own render loop), confirmed the message log stays
  visible after the typing box closes (not tied to its lifetime, matching Craft), and confirmed
  Sign typing (backtick) is unaffected by the `TypingMode` generalization. The world-editing
  paint commands themselves (`/cube`, `/sphere`, etc.) could not be verified live end-to-end —
  they need mouse-driven break/place to set real marks first, and synthetic mouse clicks into
  this sandbox's relative-mouse-mode SDL window remain unreliable (the same documented flakiness
  class as every other click-dependent feature in this project's history) — verified instead via
  the exhaustive `WorldEditor`/`ExecuteCommand` unit tests above, which is arguably more reliable
  regression coverage than a one-off manual screenshot anyway.

### 4.6 Multiplayer / server / client networking
- **Craft behavior**: `client.c` sends ASCII line protocol over raw TCP
  (`V,version` / `A,user,token` / `P,x,y,z,rx,ry` / `C,p,q,key` / `B,x,y,z,w` / `L,x,y,z,w` /
  `S,x,y,z,face,text` / talk); `server.py` is a Python `SocketServer`-based authoritative server
  with its own SQLite DB.
- **cna-craft behavior**: none — confirmed via grep (zero hits for net/socket/enet/tcp/udp).
  Already explicitly, deliberately deferred per `plan.md` §0/§9/§11.6 (CNA's own ENet-backed
  `Net` layer is the intended future home, using Craft's message shapes as a protocol
  *reference*, not a literal port).
- **Status**: missing (deliberate, documented scope decision)
- **Craft files**: `src/client.c:69-160`, `server.py`
- **cna-craft files**: none
- **Priority**: low (explicitly deferred until local single-player + persistence are solid, per
  project direction)

---

## 5. Visual features, shaders, atlas

### 5.1 Ambient occlusion
- **Craft behavior**: per-vertex AO baked on the CPU (`occlusion()`, `main.c:879-919`, the
  0fps.wordpress.com corner-lookup algorithm), packed into a 4th vertex UV component, applied in
  `block_fragment.glsl` as a multiplicative darkening term.
- **cna-craft behavior**: none — `VertexPositionNormalTexture` has no AO channel; rendering uses
  stock `BasicEffect`.
- **Status**: blocked
- **Craft files**: `src/main.c:879-919`, `src/cube.c:53-79`, `shaders/block_{vertex,fragment}.glsl`
- **cna-craft files**: `src/CnaCraft/Render/ChunkRenderer.cpp`, `Worlds/ChunkMesher.*`
- **Priority**: high (visual parity), blocked
- **Notes**: Requires a custom vertex format + shader (`ShaderEffect`) — per `missing.md`, only
  `EASYGL` has real runtime shader support today; `VULKAN` needs a precompiled-SPIR-V toolchain;
  `BGFX`'s `ShaderEffect` is a stub in CNA itself. `blocked` pending that CNA-side work (or an
  `EASYGL`-only scoped implementation, which is a `needs_human` scope decision). **Confirmed via a
  deeper research pass 2026-07-10** (while scoping §2.7's light-toggle): the real mechanism is that
  CNA's three graphics backends all dispatch shader/pipeline selection by *hardcoded raw
  vertex-buffer byte stride* (16/20/24/32/52), not by the `VertexDeclaration`'s actual element
  list — no stride/shader combination in any backend combines `TextureEnabled` +
  `LightingEnabled` (normal-based) + `VertexColorEnabled` together, so even a from-scratch custom
  vertex type hits the same wall without new engine-side work in all three backends. One proven,
  already-used exception exists: `VertexPositionColorTexture` (stride 24, `TextureEnabled` +
  `VertexColorEnabled`, `LightingEnabled=false`) — `SkyDome`/`SelectionOutline` already use it, and
  §2.7's light-toggle glow pass reuses it too, but adopting it for *ordinary chunk terrain* (to
  carry baked AO/light) would mean dropping the live per-frame GPU day/night ambient update chunk
  rendering has today in favor of CPU-rebaked vertex colors across every loaded chunk — a much
  larger, separate change than AO itself, not something either the light-toggle work or this note
  is proposing.

### 5.2 Fog
- **Craft behavior**: distance+height fog in `block_vertex.glsl`/`block_fragment.glsl`, blending
  toward a sampled sky-texture color by camera distance.
- **cna-craft behavior**: `CnaCraftGame::Draw` sets `effect_`'s
  `FogEnabled`/`FogColor`/`FogStart`/`FogEnd` every frame — `FogColor` is now sampled from the
  same sky gradient the textured dome uses (`Render::SampleSkyColor(timeOfDay, 0.5)`, the horizon
  band — updated 2026-07-10 with the textured sky, plan.md §12.1 item 33), so fog genuinely fades
  toward the real sky color at the current time of day, dawn/dusk oranges included. One remaining
  simplification vs. Craft: Craft's `block_fragment.glsl` samples the sky texture *per fragment*
  at that fragment's own elevation angle (`vec2(timer, fog_height)`); `BasicEffect`'s
  fixed-function fog has exactly one flat color per draw, so the horizon band (where fog-faded
  geometry actually sits) stands in for all elevations. Fog start/end are computed every frame from
  `radii_.createRadius * CHUNK_SIZE` (`radii_` is the mutable runtime streaming-radius state added
  2026-07-10 for `/view`, plan.md §12.1 item 17 — previously the compile-time-fixed
  `kCreateRadius`, itself updated from the old fixed world's diagonal during the chunk-streaming
  redesign, item 19) — matching Craft's own real `fog_distance = render_radius * CHUNK_SIZE`,
  including tracking a runtime `/view` change immediately. Disabled in ortho mode, matching
  Craft's own `if (bool(ortho)) fog_factor = 0.0`.
- **Status**: complete
- **Craft files**: `shaders/block_vertex.glsl:32-38`, `block_fragment.glsl:36-37`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp` (Draw)
- **Priority**: medium (done)
- **Notes**: **Did not require custom shaders**, as predicted — `BasicEffect`'s built-in linear
  distance fog works identically on all three backends since it's standard XNA surface, not a
  custom `ShaderEffect`; confirmed via CNA's own cross-backend fog test suite
  (`../cna/examples/{easygl,vulkan,bgfx}_basiceffect_lit_fog_test.cpp`), which exercises this
  exact lit+textured `BasicEffect` path. Verified via clean build + 100-frame headless smoke run;
  a visible fade screenshot proved impractical this session (sandboxed X11 fly-navigation
  flakiness, not a code concern — see `plan.md` §12.1 item 13 for the full note).

### 5.3 Sky dome
- **Craft behavior**: a real icosphere mesh (`make_sphere`, `cube.c:346-384`), textured with a
  time-of-day-indexed `sky.png` (`vec2(timer, t)` sampling, `t = 1 - acos(y)/pi`:
  0 nadir / 0.5 horizon / 1 zenith), GL_LINEAR + GL_CLAMP_TO_EDGE, drawn every frame
  (`render_sky`). The clock starts at `day_length/3` (mid-morning, `main.c:2582`), and the block
  shaders' fog samples the same sky texture per fragment at that fragment's elevation.
- **cna-craft behavior** (**textured 2026-07-10, plan.md §12.1 item 33, user decision "Přidat
  texturu oblohy"** — replaced the earlier untextured vertex-color gradient hemisphere): a full
  13-ring/16-segment UV sphere (`VertexPositionColorTexture`, white vertex colors — the proven
  stride-24 Texture+VertexColor unlit combo, same as the light-toggle glow pass), textured with
  `Render::BuildProceduralSkyTexture` — a 64×16 gradient whose colors are sampled directly from
  Craft's own shipped `textures/sky.png` (real dawn/dusk orange bands, real day/night blues —
  same real-asset-colors-embedded-as-constants approach as the dye blocks, item 25), sampled with
  `SamplerState::LinearClamp` (Craft's exact GL_LINEAR+GL_CLAMP_TO_EDGE). V = Craft's exact
  `t = 1 - acos(y)/pi` elevation mapping; U = `Worlds::ComputeTimeOfDay` (Craft's `timer`),
  baked into the vertex UVs and re-uploaded per frame since `BasicEffect` has no custom-uniform
  slot (same per-frame re-upload cost profile the old gradient version already had). The clear
  color and `FogColor` both come from `Render::SampleSkyColor(timeOfDay, 0.5)` — the same
  gradient's horizon band, CPU-side (see §5.2). The game clock also now starts at
  `day_length/3` (mid-morning), matching Craft's `glfwSetTime(g->day_length / 3.0)` — previously
  it started at literal midnight, which is why every earlier screenshot of this project looked
  dark.
- **Status**: complete
- **Craft files**: `src/cube.c:346-384`, `src/main.c:251-253,1721-1732,2582,2636-2644`,
  `shaders/sky_{vertex,fragment}.glsl`
- **cna-craft files**: `src/CnaCraft/Render/SkyDome.{hpp,cpp}`,
  `src/CnaCraft/Render/SkyTexture.{hpp,cpp}`, `src/CnaCraft/Worlds/DayNightCycle.{hpp,cpp}`
  (`ComputeTimeOfDay`), `src/CnaCraft/CnaCraftGame.cpp`
- **Priority**: medium (done)
- **Verification method**: real-build Xvfb screenshots at three points of the U axis — pre-offset
  launch at t≈0 showed the correct near-black midnight sky; post-offset launch shows a real light
  blue day sky immediately (verifying both the texture path and the morning-start offset);
  the dawn/dusk orange bands themselves come from the same sampled color table (verified
  numerically against sky.png during generation) but were **not** directly screenshot-verified —
  the sandbox's software renderer runs the fixed-step game clock slower than wall time, putting
  dusk ~15+ real minutes out, so this relies on the two verified U points plus the table data.
  `ComputeTimeOfDay` is unit-tested (wrap/noon/midnight/degenerate-day-length).
- **Notes**: The old untextured version's empirically-found winding lesson (CNA culls
  CullCounterClockwiseFace; the dome must wind for an inside viewer) carries over unchanged —
  the ring/segment parametrization is the same, extended from a hemisphere to a full sphere so
  looking below the horizon while flying shows Craft's real below-horizon band. One observed
  (sandbox-only) nuance while verifying: CNA's fixed-step `GameTime::TotalGameTime` advances per
  executed update, so under a renderer too slow to hit 60fps the day cycle runs slower than wall
  time — Craft uses wall-clock `glfwGetTime()` and would not. On real hardware at 60fps the two
  agree; noted here rather than "fixed" since it's invisible at full frame rate.

### 5.4 Day/night lighting
- **Craft behavior**: `get_daylight()`/`time_of_day()` (`main.c:163-184`), two logistic sigmoids
  around dawn(0.25)/dusk(0.85), `DAY_LENGTH=600`s.
- **cna-craft behavior** (`DayNightCycle.cpp`, added prior session): formula verified **bit-for-bit
  equivalent** — same sigmoid centers/steepness/branch, same 600s day length.
- **Status**: complete
- **Craft files**: `src/main.c:163-184`, `src/config.h:14`
- **cna-craft files**: `src/CnaCraft/Worlds/DayNightCycle.{hpp,cpp}`
- **Priority**: low

### 5.5 Texture atlas
- **Craft behavior**: a real 256×256 hand-authored `texture.png`, 16×16 grid of 16px tiles,
  `blocks[w][6]` per-face tile-index table.
- **cna-craft behavior**: `BuildProceduralAtlas` — an 8×8 grid (59 tiles used of 64 slots, grown
  from 5×5/25 on 2026-07-10 to fit the expanded block roster's new tiles — see §2.2), mostly
  procedurally patterned (no art asset). One exception: the 32 dye-color tiles (27-58) use real
  RGB values sampled directly from Craft's own shipped `textures/texture.png`, since those tiles
  are flat swatches there too — nothing procedural to invent, so matching the real asset was both
  easier and more faithful for that one block group.
- **Status**: partial (deliberate)
- **Craft files**: `textures/texture.png`, `src/cube.c:54-64,87-92`
- **cna-craft files**: `src/CnaCraft/Render/TextureAtlas.{hpp,cpp}`
- **Priority**: low
- **Notes**: Functionally equivalent (each block gets a distinct texture); not visually
  equivalent to Craft's hand-drawn art for the non-dye tiles. Adopting a real authored texture
  asset for the rest is a `needs_human` decision (new asset dependency).

### 5.6 Crosshair / wireframe selection outline
- See §2.4 above (moved there since it's gameplay-facing, not purely cosmetic) —
  **implemented this session**.

### 5.7 Build/dependency differences
- **Craft behavior**: links GLEW, GLFW, cURL (HTTPS auth), lodepng, SQLite, tinycthread, plus a
  vendored simplex-noise lib — all vendored except cURL.
- **cna-craft behavior**: `CnaCraftWorlds` (pure C++23, zero deps) + `CnaCraft` (links only
  `CNA`/`SHARP_RUNTIME`, no SQLite/cURL/GLFW/GLEW directly — abstracted behind CNA). BGFX backend
  pulls `bgfx.cmake` via an unpinned network `FetchContent` (a dependency-fetch risk Craft
  doesn't have).
- **Status**: partial (expected — no SQLite/cURL, tracked separately at §4.1/§4.6)
- **Craft files**: `CMakeLists.txt`
- **cna-craft files**: `CMakeLists.txt`
- **Priority**: low

---

## 6. Summary table

| # | Feature | Status | Priority |
|---|---|---|---|
| 1.1 | Main game loop | complete | low |
| 1.2 | Window/cursor capture (**fixed this session**) | complete | medium |
| 1.3 | Keyboard controls | complete | high |
| 1.4 | Mouse look (**pitch clamp fixed this session**) | complete | low |
| 1.5 | Player movement (diagonal speed + **fly-speed 4x fixed this session**) | complete | high |
| 1.6 | Walking vs flying (**Minecraft-style Space/Shift flight, deliberate deviation**) | complete | medium |
| 1.7 | Collision (substepping) | complete | high |
| 1.8 | Gravity/jump (terminal velocity + **floor-catch fixed this session**) | complete | medium |
| 1.9 | Zoom/ortho (**arrow-look removed, deliberate deviation**) | complete | low |
| 2.1 | Hotbar switching (0/R/scroll) | **fixed this session** | medium |
| 2.2 | Block roster (**full 54-item roster completed 2026-07-10**) | complete | medium |
| 2.3 | Raycast algorithm (**reach fixed this session, 6->8**) | complete | low |
| 2.4 | Wireframe selection outline | **fixed this session** | high |
| 2.5 | Block breaking (Bedrock protected; **Cloud unbreakable, fixed 2026-07-10**) | complete | critical |
| 2.6 | Block placing (self-intersection; **is_obstacle target guard, fixed 2026-07-10**) | complete | critical |
| 2.7 | Eyedropper + Ctrl-click-as-place + light-toggle (**light-toggle mechanic/persistence completed 2026-07-10, visual simplified — see §5.1**) | complete | low |
| 2.8 | Collision rules (solid/transparent) | complete | low |
| 3.1 | Chunk system (**unbounded column hash-map fixed this session**) | complete | medium (large) |
| 3.2 | Chunk streaming (**background generation/meshing fixed this session**) | complete | low |
| 3.3 | Terrain formula + Sand generation (**both fixed this session**) | complete | high |
| 3.4 | Face culling | complete | low |
| 3.5 | Mesh generation | complete | low |
| 3.6 | Transparent blocks | complete | low |
| 3.7 | Plants/flowers/tall grass (all 6 flower colors **completed 2026-07-10**) | complete | high |
| 3.8 | Trees | complete | low |
| 3.9 | Caves/overhangs | needs_human | n/a (no reference exists) |
| 3.10 | Clouds | complete | low |
| 4.1 | World persistence (**fixed this session**) | complete | high |
| 4.2 | Delta storage (**fixed this session**) | complete | high |
| 4.3 | Signs (**fixed this session**) | complete | medium |
| 4.4 | Player names/text (**chat overlay now wired via message log, 2026-07-10**) | complete | low |
| 4.5 | Chat/commands (**world-editing macros completed 2026-07-10**) | complete | medium |
| 4.6 | Multiplayer | missing | low (deliberate) |
| 5.1 | Ambient occlusion | blocked | high (blocked) |
| 5.2 | Fog (**fades to the real sky gradient, 2026-07-10**) | complete | medium |
| 5.3 | Sky dome (**textured with real sky.png colors, 2026-07-10**) | complete | medium |
| 5.4 | Day/night lighting | complete | low |
| 5.5 | Texture atlas | partial | low (needs_human: asset decision) |
| 5.7 | Build/dependencies | partial | low |

**"Fixed this session"** items are detailed in `plan.md`'s new "Craft Feature Parity Port"
section with exact diffs, tests, and verification.
