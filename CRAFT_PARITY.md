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
  (`on_key`, `main.c:2199-2201`).
- **cna-craft behavior**: SDL-backed `Keyboard`/`Mouse`. Relative mouse mode is set once in
  `Initialize()` and never toggled. `Keys::Escape` calls `Exit()` directly — **quits the whole
  app** (`CnaCraftGame.cpp:147-150`).
- **Status**: partial
- **Craft files**: `src/main.c` (`create_window`, `on_key`, `on_mouse_button`)
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp:55-116, 147-150`
- **Priority**: medium
- **Verification method**: manual play test (press Escape; Craft releases the cursor, cna-craft
  quits)
- **Notes**: cna-craft's Escape-quits behavior is documented in `README.md` §5 as intentional
  ("Esc | Quit"), so this is a **known, documented divergence**, not an oversight — flagged here
  for completeness but changing it is a control-scheme decision (`needs_human` if pursued, since
  README explicitly documents the current behavior).

### 1.3 Keyboard controls
- **Craft behavior** (`config.h:30-44`, `on_key`/`on_char`/`handle_movement`): W/A/S/D move;
  Space = jump/fly-up; Tab = fly toggle; Shift = hold-zoom (FOV 65→15°); F = hold-ortho; **E/R =
  next/prev item**; **1-9 and 0 = direct item select (10 slots)**; **scroll wheel = item cycle**;
  O/P = multiplayer observe cameras; arrow keys = keyboard look; Enter = chat/command submit,
  Ctrl+Enter = right-click; Ctrl+V = paste; Backspace = edit typing buffer; Esc = cancel
  typing/release cursor; `t`/`/`/`` ` `` = open chat/command/sign text entry.
- **cna-craft behavior** (`CnaCraftGame.cpp:154-220`): W/A/S/D move; Space = jump/fly-up; Left
  Ctrl = fly-down; Tab = fly toggle; arrows = keyboard look; Left Shift = hold-zoom; F =
  hold-ortho; **E = next-slot only (no R/prev)**; **1-9 direct select (no key `0`, so only 9 of
  15 slots reachable directly)**; **no scroll-wheel cycling**; F12 = screenshot (cna-craft-only
  addition, not in Craft). No chat/command/sign typing system at all.
- **Status**: partial
- **Craft files**: `src/config.h:30-44`, `src/main.c` (`on_key`, `on_char`, `handle_movement`)
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp:154-220`
- **Priority**: high
- **Verification method**: code inspection + manual play test
- **Notes**: Missing: `R` (prev-cycle), scroll-wheel cycling, key `0` (10th direct slot — moot
  today since cna-craft only has 9 number keys wired, but relevant once >9 slots need direct
  access), middle-click block-pick, Ctrl+click light-toggle, entire chat/command/sign typing
  system (large, separate feature — see §3).

### 1.4 Mouse look
- **Craft behavior** (`handle_mouse_input`, `main.c:2378-2409`): delta from raw cursor-position
  diff each frame; sensitivity `m = 0.0025`; yaw wraps [0°,360°); pitch clamped to exactly
  ±90°; `INVERT_MOUSE` config flag (default off) flips vertical sign.
- **cna-craft behavior** (`PlayerController.cpp:45-54`, `kMouseSensitivity=0.0025f`): reads an
  already-relative mouse delta (SDL relative mode) each frame, same `0.0025` sensitivity; yaw
  wraps [0, 2π); pitch clamped to ±1.55 rad (≈88.8°, not exactly 90°). No invert-mouse config.
- **Status**: complete
- **Craft files**: `src/main.c:2378-2409`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp:152,164-165`,
  `src/CnaCraft/Worlds/PlayerController.cpp:45-54`
- **Priority**: low
- **Verification method**: manual play test
- **Notes**: Sensitivity matches exactly. Pitch clamp is ~1.2° tighter than Craft's exact ±90° —
  cosmetic, not worth a task on its own.

### 1.5 Player movement (walk speed / diagonal movement)
- **Craft behavior** (`get_motion_vector`, `main.c:204-232`): `strafe = atan2f(sz, sx)` from -1/0/1
  key axes; motion vector is `(cosf(rx+strafe), sinf(rx+strafe))` — **always a unit vector**
  regardless of how many keys are held, so diagonal movement is not faster. Walk speed = 5
  units/s; fly speed = 20 units/s (**exactly 4x** walk speed). No sprint mechanic exists anywhere.
- **cna-craft behavior** (`PlayerController.cpp:13-21, 81-82`): `moveX/moveZ` are the **unnormalized
  sum** of independent forward/right axis contributions — pressing two movement keys at once
  (e.g. W+D) yields a combined vector of magnitude ≈1.41x a single-axis vector, i.e. **diagonal
  movement is ~41% faster than straight movement** — a real bug relative to Craft. `kMoveSpeed=4.5`,
  `kFlySpeed=9.0` — ratio is **2x, not 4x**; the code's own comment at `PlayerController.cpp:14`
  claims "matches Craft's flying speed being 4x" but the actual chosen constants give 2x — the
  comment is factually wrong.
- **Status**: partial (real bug)
- **Craft files**: `src/main.c:204-232, 2411-2469`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.cpp:13-21, 56-104`
- **Priority**: high
- **Verification method**: unit test comparing travel distance for straight vs. diagonal input
  over equal `dt`
- **Notes**: **Diagonal-speed normalization implemented this session** (the actual bug — see §6
  below). The separate `kFlySpeed` 2x-vs-4x ratio mismatch was **left as-is, not changed to 18.0**:
  unlike the diagonal-speed bug (objectively wrong relative to Craft's own math, no judgment
  call), fly speed is a subjective tuning value with no broken mechanic to fix — doubling it is a
  gameplay-feel decision, not a bug fix, so it's `needs_human` if revisited. Only the
  misleading in-code comment was corrected to state the actual 2x ratio and explain why it wasn't
  changed.

### 1.6 Walking vs flying behavior
- **Craft behavior**: Tab toggles `g->flying`. While flying, forward/back movement is
  **pitch-coupled** — `vy = sinf(ry)` contributes vertical motion when moving forward/back while
  looking up/down (classic "look-and-fly" creative flight), and Space directly sets `vy=1`
  (pure hover climb, no gravity/inertia, `dy=0` forced every substep while flying).
- **cna-craft behavior**: Tab toggles `flying_` (zeroes `velocity_.y`). Horizontal movement in
  fly mode uses the **same forward/right basis as walking** (not pitch-coupled); vertical motion
  is a **separate dedicated axis** (Space=+1, Left Ctrl=-1) scaled by `kFlySpeed`, uncollided.
- **Status**: partial
- **Craft files**: `src/main.c:204-232, 2250-2252, 2432-2465`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.cpp:32, 64-79`,
  `src/CnaCraft/CnaCraftGame.cpp:159-163,193-200`
- **Priority**: medium
- **Verification method**: manual play test
- **Notes**: cna-craft's Space/Ctrl vertical-axis scheme is a different (and arguably more
  discoverable/standard) control scheme than Craft's pitch-coupled flight, already documented in
  `README.md` §5 controls table as the current design. Changing to match Craft exactly is a
  control-scheme decision — `needs_human` if pursued, per the "do not change control scheme
  without updating README" rule and since README already documents current behavior as intended.

### 1.7 Collision behavior
- **Craft behavior** (`collide`, `main.c:699-738`): per-substep (`step = MAX(8, estimate)`
  substeps per frame, `main.c:2446`) fractional-offset "padding" clamp — for each of up to
  `height=2` voxel layers, if the player's fractional position within a cell exceeds `pad=0.25`
  toward an obstacle, clamps the coordinate to `n±pad`. Effectively a continuous partial-penetration
  resolver, explicitly substepped to avoid tunneling through thin obstacles at high speed/low
  framerate.
- **cna-craft behavior** (`CollidesAt`, `PlayerController.cpp:27-43`): true AABB-vs-voxel-grid
  test (`kPlayerHalfWidth=0.3`, `kPlayerHeight=1.8`), resolved axis-separated (X, then Z, then Y)
  in a **single whole-step move-or-full-revert per axis per frame** — no substepping.
- **Status**: partial
- **Craft files**: `src/main.c:699-738`, called at `main.c:2462`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.cpp:27-43, 90-103`
- **Priority**: high
- **Verification method**: unit test placing the player near a thin wall at high velocity /
  low simulated framerate and confirming it doesn't tunnel through
- **Notes**: cna-craft's AABB approach is arguably a cleaner algorithm than Craft's point+pad
  approximation, but the *lack of substepping* is a genuine tunneling risk at high velocity or a
  dropped frame — a real, if currently unobserved, bug relative to Craft's explicit anti-tunneling
  design.

### 1.8 Gravity/jump behavior
- **Craft behavior** (`main.c:2411-2469`): jump sets `dy=8` only `if (dy==0)` (doubles as the
  grounded check) while not flying; gravity `dy -= ut*25` per substep, clamped to a **terminal
  velocity of -250 units/s**; any vertical collision resets `dy=0`; a hard floor-catch:
  `if (s->y < 0) s->y = highest_block(...) + 2`.
- **cna-craft behavior** (`PlayerController.cpp:15-21, 84-103`): explicit `grounded_` flag gates
  jumping; `kJumpSpeed=8.0f` and `kGravity=25.0f` — **both match Craft exactly** (a prior
  session's user-reported jump-height bugfix deliberately matched these). **No terminal-velocity
  clamp** — `velocity_.y` grows unbounded during a long fall. **No `y<0` floor-catch fallback**.
- **Status**: partial
- **Craft files**: `src/main.c:2411-2469`
- **cna-craft files**: `src/CnaCraft/Worlds/PlayerController.cpp:15-21, 84-103`
- **Priority**: medium
- **Verification method**: unit test measuring fall speed after N seconds vs. Craft's -250 cap
- **Notes**: **Implemented this session** (terminal velocity clamp) — see §5 below. Jump speed
  (8) and gravity (25) were already exact matches from a prior session.

### 1.9 Other player-control behavior (zoom/ortho/arrow-look/sprint/crouch)
- **Craft behavior**: no sprint, no crouch, no view bobbing anywhere in `main.c`. Arrow-key look
  exists as a mouse-look alternative at `dt*1.0` rad/s. Shift = hold-zoom (FOV 65→15°). F =
  hold-ortho (`g->ortho=64`). O/P = multiplayer picture-in-picture observe cameras (N/A,
  single-player only).
- **cna-craft behavior**: matches Craft's absence of sprint/crouch/view-bob. Arrow-key look exists
  at `1.6*dt` rad/s (different constant, same idea). Zoom and ortho both present, explicitly
  documented as intentional Craft-matching hold-to-activate behavior.
- **Status**: complete
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
- **cna-craft behavior** (`Hotbar.cpp`, `CnaCraftGame.cpp:204-215`): `SelectSlot`/`CycleNext`
  only. Number keys `1`-`9` map directly (capped at `kMaxNumberKeySlots=9` — 6 of 15 slots
  unreachable by direct key). `E` cycles all 15 slots with wraparound. **No key `0`, no `R`
  (reverse cycle), no scroll-wheel cycling.**
- **Status**: partial
- **Craft files**: `src/main.c:2249-2267, 2310-2324`; `src/config.h:38-39`
- **cna-craft files**: `src/CnaCraft/Worlds/Hotbar.hpp:20-44`, `Hotbar.cpp`,
  `src/CnaCraft/CnaCraftGame.cpp:204-215`
- **Priority**: medium
- **Verification method**: press `0`/scroll wheel/`R` in-game, observe no response (before fix)
- **Notes**: **Implemented this session** (R reverse-cycle, key 0, scroll wheel) — see §5 below.

### 2.2 Block/item roster
- **Craft `items[]`** (`item.c:4-60`, real order, `item_count=54`): Grass, Sand, Stone, Brick,
  Wood, Cement, Dirt, Plank, Snow, Glass, Cobble, LightStone, DarkStone, Chest, Leaves, TallGrass,
  YellowFlower, RedFlower, PurpleFlower, SunFlower, WhiteFlower, BlueFlower, then 32
  `Color00`-`Color31` dye/paint blocks. **`CLOUD` is deliberately excluded from `items[]`** —
  world-gen only, never player-placeable in real Craft.
- **cna-craft `Hotbar::kSlots`** (15 slots): Grass, Dirt, Sand, Stone, Cobblestone, Brick, Plank,
  Wood, Cement, LightStone, DarkStone, Snow, Glass, **Cloud**, Leaves.
- **Missing from cna-craft**: Chest, all 6 plant/flower types (need billboard geometry — see
  §3.7), all 32 dye colors (low value — a flat-color palette, already noted as "skipped" in
  `plan.md`'s original block-roster item).
- **Extra in cna-craft not matching Craft's design**: `Cloud` is directly placeable via the
  hotbar; real Craft never allows this (world-gen-only block).
- **Status**: partial
- **Craft files**: `src/item.h:4-59`, `src/item.c:4-62`
- **cna-craft files**: `src/CnaCraft/Worlds/BlockType.hpp:14-33`,
  `src/CnaCraft/Worlds/Hotbar.hpp:22-27`
- **Priority**: medium
- **Verification method**: diff the two ordered lists (done above)
- **Notes**: Whether to remove `Cloud` from the placeable hotbar to match Craft exactly is a
  **design trade-off between Craft fidelity and cna-craft's own existing player-facing feature**
  (players can currently build with clouds) — marked `needs_human`. Chest/plants/dyes are
  `pending` (large — need new geometry/mesh-format work for plants, low value for dyes).

### 2.3 Raycast / hit-test algorithm
- **Craft behavior** (`_hit_test`/`hit_test`, `main.c:603-664`): fixed-step supersampled march
  (32 substeps/unit), `roundf()`-snapped voxel testing, hardcoded max distance **8**. Face normal
  is not returned directly — derived separately in `hit_test_face` by diffing two hit-test calls,
  used only for sign placement.
- **cna-craft behavior** (`VoxelRaycast::Cast`): true Amanatides-Woo DDA — analytic `tMax`/`tDelta`
  per axis, returns an explicit `RaycastHit{x,y,z,nx,ny,nz}` with face normal computed directly.
  `kMaxReach=6.0f` (vs Craft's hardcoded 8).
- **Status**: complete (algorithmically equivalent-or-better; reach distance differs slightly)
- **Craft files**: `src/main.c:603-664`
- **cna-craft files**: `src/CnaCraft/Worlds/VoxelRaycast.{hpp,cpp}`
- **Priority**: low
- **Verification method**: constant comparison (`kMaxReach=6` vs Craft's `8`)
- **Notes**: cna-craft's DDA is a genuine algorithmic improvement over Craft's march-and-round —
  not a gap. Reach distance (6 vs 8) is a minor numeric mismatch, low priority.

### 2.4 Visible targeted-block behavior (wireframe outline)
- **Craft behavior** (`render_wireframe`, `main.c:1734-1752`; `make_cube_wireframe`,
  `cube.c:191-214`): every frame, hit-tests the targeted block; if it's an obstacle, draws a 3D
  wireframe box (12 edges, `n=0.53` half-size — slightly larger than the unit block) around it
  using a dedicated line shader with `GL_COLOR_LOGIC_OP` (XOR-style highlight).
- **cna-craft behavior**: **no equivalent exists at all** — confirmed via a full-tree grep for
  "wireframe"/"outline"/"selection" (zero matches). The player currently has no visual feedback
  for which block is targeted before breaking/placing.
- **Status**: missing
- **Craft files**: `src/main.c:1734-1752`, `src/cube.c:191-214`, `shaders/line_*.glsl`
- **cna-craft files**: none (would live in `src/CnaCraft/Render/`)
- **Priority**: high
- **Verification method**: manual play test — look at a block, confirm an outline renders around
  exactly that block and updates as the camera moves
- **Notes**: **Does NOT require custom shader support** — a simple line-list primitive drawn with
  stock `BasicEffect` (no `ShaderEffect` needed) is sufficient, so this is unblocked on all three
  backends (EASYGL/VULKAN/BGFX) unlike AO/sky-dome/fog-via-custom-shader. **Implemented this
  session** — see §5 below.

### 2.5 Block breaking
- **Craft behavior** (`on_left_click`, `main.c:2140-2151`; `is_destructable`, `item.c:191-199`):
  guarded by `hy>0 && hy<256 && is_destructable(hw)` — `is_destructable` returns false only for
  `EMPTY` and `CLOUD` (Craft has no bedrock/unbreakable-boundary concept at all). On break, if the
  block directly above is a plant, it's also destroyed (support-chain reaction).
- **cna-craft behavior** (`CnaCraftGame.cpp:222-230`): on left-click, casts a ray; on any hit,
  **unconditionally** calls `SetBlock(..., Air)` — no destructability check at all. Since
  cna-craft (unlike Craft) has a `BlockType::Bedrock` intended as "a world-boundary block, not
  meant to be placed by the player" (per `Hotbar.hpp`'s own comment), **Bedrock can currently be
  mined away**, defeating its own stated purpose.
- **Status**: partial (real bug relative to cna-craft's own documented design intent)
- **Craft files**: `src/main.c:2140-2151`, `src/item.c:191-199`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp:222-230`
- **Priority**: critical
- **Verification method**: unit test — `World::IsBreakable`/equivalent must return false for
  Bedrock; manual test — mine to the world floor, left-click the bottom layer, confirm it no
  longer breaks
- **Notes**: **Implemented this session** — see §5 below. No plant-support chain reaction (moot —
  no plants exist yet in cna-craft, see §3.7).

### 2.6 Block placing
- **Craft behavior** (`on_right_click`, `main.c:2153-2163`): guarded by `hy>0 && hy<256 &&
  is_obstacle(hw)`, **and** `!player_intersects_block(2, s->x,s->y,s->z, hx,hy,hz)` — placement
  is blocked if it would intersect the player's own bounding box (radius 2).
- **cna-craft behavior** (`CnaCraftGame.cpp:231-236`): on right-click, casts a ray; on hit,
  **unconditionally** places the selected block at the hit-normal-offset cell — **no
  player-self-intersection check**, so a player can place a block that then immediately collides
  with (traps) themselves.
- **Status**: partial (real bug)
- **Craft files**: `src/main.c:2153-2163`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp:231-236`
- **Priority**: critical
- **Verification method**: manual test — look straight down at your own feet, right-click,
  confirm placement is blocked (before fix, it silently succeeds and can visibly get the player
  stuck)
- **Notes**: **Implemented this session** — see §5 below.

### 2.7 Special interactions (Ctrl+click, middle-click, light toggle)
- **Craft behavior**: Ctrl+left-click (or Ctrl+Enter) → acts as right-click (place). Ctrl+right-click
  → `on_light()`, toggles a placed point-light source at the targeted block. Middle-click →
  `on_middle_click()`, an "eyedropper" that sets the selected hotbar item to match the targeted
  block's type.
- **cna-craft behavior**: none of the three exist. Left Ctrl is used only for fly-mode descend.
- **Status**: missing
- **Craft files**: `src/main.c:2131-2138, 2153-2175, 2229-2361`
- **cna-craft files**: none
- **Priority**: low
- **Verification method**: `grep -n "middle\|toggle_light" src/CnaCraft/CnaCraftGame.cpp` — no
  results
- **Notes**: The light-toggle half is blocked on a much larger unimplemented subsystem
  (per-block point lighting, §4.3) — not a simple wiring gap. The eyedropper (middle-click) is a
  small, independently implementable convenience feature — `pending`, low priority.

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
- **cna-craft behavior**: dense fixed `std::array<BlockType,16³>` per `Chunk`, dense fixed
  `8×4×8` grid of chunks (`128×64×128` world), allocated once at startup.
- **Status**: partial (deliberate architectural simplification)
- **Craft files**: `src/map.c`, `src/map.h`
- **cna-craft files**: `src/CnaCraft/Worlds/Chunk.{hpp,cpp}`, `World.hpp`
- **Priority**: medium
- **Verification method**: code inspection
- **Notes**: Explicitly documented in cna-craft's own comments as an intentional scoping decision
  (fixed prototype world vs. infinite streamed world), not an oversight. Replacing it is a real
  architecture change — `pending` (large), not a quick task.

### 3.2 Chunk loading/unloading by distance
- **Craft behavior**: `CREATE_CHUNK_RADIUS=10`, `RENDER_CHUNK_RADIUS=10`, `DELETE_CHUNK_RADIUS=14`
  (`config.h`); `force_chunks`/`ensure_chunks` create chunks around the player (worker-threaded),
  `delete_chunks` frees far-away ones every frame.
- **cna-craft behavior**: none — the entire fixed world is generated once at startup, never
  streamed.
- **Status**: missing (deliberate, depends on §3.1)
- **Craft files**: `src/main.c` (`force_chunks` L1307, `ensure_chunks` L1418, `delete_chunks`
  L1225)
- **cna-craft files**: `src/CnaCraft/Worlds/World.cpp` (`World::World`, `World::Generate`)
- **Priority**: low (given the project's stated fixed-world scope)
- **Verification method**: code inspection

### 3.3 Chunk/terrain generation algorithm
- **Craft behavior** (`create_world`, `src/world.c`): `f = simplex2(x*0.01,z*0.01,4,0.5,2)`,
  `g = simplex2(-x*0.01,-z*0.01,2,0.9,2)`, `mh = g*32+16`, `h = f*mh` — **two independent simplex2
  calls combined multiplicatively**. Sea/sand: `h<=12` → `h=12`, block=Sand; else block=Grass.
- **cna-craft behavior** (`NoiseGenerator::Height`, `NoiseGenerator.cpp:199-212`): a **single**
  simplex2 call (`kScale=0.05`, 4 octaves/0.5persist/2lacunarity), `height = base(20) +
  (n-0.5)*2*amplitude(16)`, clamped `[4,56]` — structurally simpler (additive, not
  multiplicative dual-noise composition) with different numeric constants. **`BlockType::Sand`
  is defined, in the `BlockDef` table, and in the hotbar roster, but `World::Generate` never
  places it** — confirmed dead code (no sea-level/beach branch exists at all).
- **Status**: partial
- **Craft files**: `src/world.c` (whole file, ~85 lines)
- **cna-craft files**: `src/CnaCraft/Worlds/World.cpp:81-105`, `NoiseGenerator.cpp:199-212`
- **Priority**: high
- **Verification method**: World-generation unit test asserting Sand appears somewhere in a
  generated world (currently would fail)
- **Notes**: The terrain-height *formula* mismatch is a large, subjective-tuning item — changing
  it would visibly change existing terrain shape (`needs_human`, a gameplay-tuning decision). The
  **dead Sand block** is a small, unambiguous, high-value fix — `pending`, not a design choice.

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
- **cna-craft behavior**: **no non-cubic geometry exists at all** — `ChunkMesher` only emits
  axis-aligned cube faces. No plant `BlockType`s exist.
- **Status**: missing
- **Craft files**: `src/cube.c:100-158`, `src/item.c` (`is_plant`)
- **cna-craft files**: none (would need `ChunkMesher.cpp` + `MeshData.hpp` + `BlockType.hpp`
  changes)
- **Priority**: high (gameplay-visible, but a real mesh-format change — see §7 sequencing notes)
- **Verification method**: visual — plants render as an X-cross, not a cube

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
- **cna-craft behavior**: none — confirmed via full-tree grep (zero real hits; the only matches
  are unrelated noise `persistence` parameter names). `missing.md` independently confirms no
  SQLite anywhere in the CNA/sharp-runtime dependency chain either.
- **Status**: needs_human (new external dependency decision)
- **Craft files**: `src/db.c`, `src/db.h`
- **cna-craft files**: none
- **Priority**: high (gameplay-important) but blocked on a dependency decision
- **Verification method**: `grep -rin sqlite src/` (currently zero real hits)

### 4.2 Delta storage of edits
- **Craft behavior**: `block(p,q,x,y,z,w)` table, unique-indexed on `(p,q,x,y,z)`, `insert or
  replace` per edit, loaded back over regenerated terrain.
- **cna-craft behavior**: none; no "enumerate changed blocks" capability exists in `Chunk`/`World`
  either.
- **Status**: needs_human (blocked on §4.1)
- **Craft files**: `src/db.c:59-151,315-334,404-420`
- **cna-craft files**: none
- **Priority**: high, blocked

### 4.3 Signs / placed text
- **Craft behavior**: `sign.c`/`sign.h` — a `Sign{x,y,z,face,text[64]}` struct in a per-chunk
  `SignList` (not a block type), placed via a typing-buffer flow, persisted in its own `sign`
  table, rendered as a textured billboard quad on the target face.
- **cna-craft behavior**: none.
- **Status**: missing
- **Craft files**: `src/sign.c`, `src/sign.h`, `src/main.c:385-395,756-840`
- **cna-craft files**: none
- **Priority**: medium
- **Notes**: Depends on persistence (§4.1/4.2) for the sign table, and needs a billboard-quad
  render path — but the bitmap-font text-rendering infrastructure already exists
  (`Render/Hud.cpp`'s `FontDrawText`) and is directly reusable.

### 4.4 Player names / on-screen text
- **Craft behavior**: `render_players` draws other players as plain cubes — **no in-world
  nametags exist in real Craft**. `render_text`/`draw_text` are used for a 2D chat/typing overlay
  and message log only.
- **cna-craft behavior**: `Render/Hud.cpp` has a working embedded-bitmap-font text renderer
  (`FontDrawText`), currently used only for the hotbar strip. No chat overlay exists yet.
- **Status**: partial (the reusable building block already exists, just not wired to chat)
- **Craft files**: `src/main.c:1702-1719, 1788-1802, 2870-2895`
- **cna-craft files**: `src/CnaCraft/Render/Hud.cpp`
- **Priority**: low (no nametag gap — Craft has none either); chat-overlay reuse is medium

### 4.5 Chat / slash commands
- **Craft behavior** (`parse_command`, `main.c:2021-2094`): real supported commands are
  `/identity`, `/logout`, `/login`, `/online`, `/offline`, `/view`, `/copy`, `/paste`, `/tree`,
  `/array`, `/fcube`, `/cube`, `/fsphere`, `/sphere`, `/fcirclex|y|z`, `/circlex|y|z`,
  `/fcylinder`, `/cylinder` — mostly world-editing macros, not chat commands in the Minecraft
  sense. Non-`/`-prefixed text is sent as plain chat via `client_talk`.
- **cna-craft behavior**: none.
- **Status**: missing
- **Craft files**: `src/main.c:2021-2094, 2183-2306`
- **cna-craft files**: none
- **Priority**: medium
- **Notes**: A full port would also need `copy()/paste()/tree()/array()/cube()/sphere()/cylinder()`
  world-editing primitives — a larger scope than "add a chat box."

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
  `EASYGL`-only scoped implementation, which is a `needs_human` scope decision).

### 5.2 Fog
- **Craft behavior**: distance+height fog in `block_vertex.glsl`/`block_fragment.glsl`, blending
  toward a sampled sky-texture color by camera distance.
- **cna-craft behavior**: none — confirmed via grep for `FogEnabled`/`FogColor` (zero hits);
  `BasicEffect`'s built-in fog properties are never touched.
- **Status**: missing (but NOT blocked — see notes)
- **Craft files**: `shaders/block_vertex.glsl:32-38`, `block_fragment.glsl:36-37`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp` (Draw)
- **Priority**: medium
- **Notes**: **Does not require custom shaders** — `Microsoft::Xna::Framework::Graphics::BasicEffect`
  has its own built-in linear distance fog (`FogEnabled`/`FogColor`/`FogStart`/`FogEnd`), which
  works identically on all three backends since it's part of the standard XNA `BasicEffect`
  surface, not a custom `ShaderEffect`. This is a genuine low-hanging-fruit gap `missing.md`
  didn't previously call out — `pending`, not `blocked`.

### 5.3 Sky dome
- **Craft behavior**: a real icosphere mesh (`make_sphere`, `cube.c:346-384`), textured with a
  time-of-day-indexed `sky.png`, drawn every frame (`render_sky`).
- **cna-craft behavior**: a flat `device.Clear(...)` color, linearly lerped day/night — no
  geometry at all, self-acknowledged in a code comment as "still-flat, no sky dome yet".
- **Status**: missing
- **Craft files**: `src/cube.c:346-384`, `src/main.c:251-253,1721-1732`,
  `shaders/sky_{vertex,fragment}.glsl`
- **cna-craft files**: `src/CnaCraft/CnaCraftGame.cpp` (Draw, sky clear-color)
- **Priority**: medium
- **Notes**: A plain (untextured, unshaded) dome mesh with vertex-colored gradient is achievable
  with stock `BasicEffect` on all backends — full Craft-style time-of-day texture sampling would
  need a custom shader (same backend tiering as §5.1). Scoping which version to build is
  `needs_human`.

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
- **cna-craft behavior**: `BuildProceduralAtlas` — a 5×5 grid (19 tiles used of 25 slots),
  procedurally patterned (no art asset), documented as a deliberate substitution.
- **Status**: partial (deliberate)
- **Craft files**: `textures/texture.png`, `src/cube.c:54-64,87-92`
- **cna-craft files**: `src/CnaCraft/Render/TextureAtlas.{hpp,cpp}`
- **Priority**: low
- **Notes**: Functionally equivalent (each block gets a distinct texture); not visually
  equivalent to Craft's hand-drawn art. Adopting a real authored texture asset is a
  `needs_human` decision (new asset dependency).

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
| 1.2 | Window/cursor capture | partial | medium (needs_human: documented divergence) |
| 1.3 | Keyboard controls | partial | high |
| 1.4 | Mouse look | complete | low |
| 1.5 | Player movement (diagonal speed) | **fixed this session** | high |
| 1.6 | Walking vs flying | partial | medium (needs_human: control-scheme choice) |
| 1.7 | Collision (substepping) | partial | high |
| 1.8 | Gravity/jump (terminal velocity) | **fixed this session** | medium |
| 1.9 | Zoom/ortho/arrow-look | complete | low |
| 2.1 | Hotbar switching (0/R/scroll) | **fixed this session** | medium |
| 2.2 | Block roster | partial | medium (needs_human: Cloud placeability) |
| 2.3 | Raycast algorithm | complete | low |
| 2.4 | Wireframe selection outline | **fixed this session** | high |
| 2.5 | Block breaking (Bedrock protection) | **fixed this session** | critical |
| 2.6 | Block placing (self-intersection) | **fixed this session** | critical |
| 2.7 | Ctrl/middle-click interactions | missing | low |
| 2.8 | Collision rules (solid/transparent) | complete | low |
| 3.1 | Chunk system (hash-map vs fixed) | partial | medium (large) |
| 3.2 | Chunk streaming | missing | low (deliberate) |
| 3.3 | Terrain formula / dead Sand block | partial | high |
| 3.4 | Face culling | complete | low |
| 3.5 | Mesh generation | complete | low |
| 3.6 | Transparent blocks | complete | low |
| 3.7 | Plants/flowers/tall grass | missing | high (large) |
| 3.8 | Trees | complete | low |
| 3.9 | Caves/overhangs | needs_human | n/a (no reference exists) |
| 3.10 | Clouds | complete | low |
| 4.1 | World persistence | needs_human | high (blocked on dependency) |
| 4.2 | Delta storage | needs_human | high (blocked) |
| 4.3 | Signs | missing | medium |
| 4.4 | Player names/text | partial | low |
| 4.5 | Chat/commands | missing | medium |
| 4.6 | Multiplayer | missing | low (deliberate) |
| 5.1 | Ambient occlusion | blocked | high (blocked) |
| 5.2 | Fog | missing | medium (NOT blocked — quick win) |
| 5.3 | Sky dome | missing | medium |
| 5.4 | Day/night lighting | complete | low |
| 5.5 | Texture atlas | partial | low (needs_human: asset decision) |
| 5.7 | Build/dependencies | partial | low |

**"Fixed this session"** items are detailed in `plan.md`'s new "Craft Feature Parity Port"
section with exact diffs, tests, and verification.
