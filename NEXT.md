# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after commit
`bd86880` (`Add procedural texture atlas`) on branch `develop`.

## 1. Project summary

CNA Craft is a small first-person voxel-world prototype (a Minecraft-clone-style
game), built entirely on [CNA](https://github.com/openeggbert/cna) — a C++
reimplementation of the XNA 4.0 programming model — and its `sharp-runtime`
utility layer. It is **not** a finished game; it's an active-development
prototype following the roadmap in `plan.md`.

**Main goal**: prove out a walkable, texturable, breakable/placeable chunked
voxel world on top of CNA's `Microsoft::Xna::Framework` API
(`GraphicsDevice`, `BasicEffect`, `VertexBuffer`/`IndexBuffer`, `SpriteBatch`,
`Keyboard`/`Mouse`), then work toward feature parity with
[fogleman/Craft](https://github.com/fogleman/Craft) (MIT-licensed reference
project — architecture/algorithms studied, no code copied verbatim; see
`THIRD_PARTY_NOTICES.md`).

**Current development phase**: past initial-prototype milestones (M0–M6 in
`plan.md` §9 are done); now working through the `plan.md` §11 backlog toward
fuller Craft parity, one item at a time.

**Key architectural decisions**:
- **Two-layer split**: `src/CnaCraft/Worlds/` is engine-agnostic C++23 (no
  CNA/SDL includes at all) — chunk storage, meshing, noise, raycast, player
  physics, hotbar, day/night curve. `src/CnaCraft/Render/` +
  `CnaCraftGame`/`main.cpp` is the CNA-dependent glue layer. This lets the
  entire gameplay/world layer be unit-tested without a GPU or display.
- **Fixed-size world**, not infinite/streamed: `128×64×128` blocks, `8×4×8`
  chunks of `16³`. No chunk load/unload yet.
- **Naive per-face meshing** (not greedy meshing, despite what the README's
  overview paragraph currently says — see §5 "Known bugs and limitations").
- **Backend-agnostic by construction**: all game code is written against
  `Microsoft::Xna::Framework`; the 3D backend (`EASYGL`/`VULKAN`/`BGFX`) is a
  pure CMake configure-time choice (`-DCNA_GRAPHICS_BACKEND=...`), no game
  code branches on it.
- **Two mesh buffers per chunk** (opaque + transparent), split at mesh-build
  time by `BlockDef.transparent`, drawn in two passes (opaque first, then
  transparent with blending on / depth-write off).

## 2. Current status

**Build status**: last verified (this session, EasyGL backend) — clean
configure + build, no warnings-as-errors issues, `CnaCraft` executable links
and runs. Verified via `--smoke N` headless runs and real interactive
screenshots (X11/xdotool), not just compilation.

**Test status**: `tests/worlds_smoke_test.cpp` — 15 test functions, **82
plain-assert checks, all passing** as of the last run this session. Builds and
runs standalone with `-DCNA_CRAFT_BUILD_GAME=OFF` (no CNA/GPU/display
needed).

**What currently works** (verified via real builds + screenshots/tests, not
just code review):
- Chunked world generation (2D Simplex noise heightmap → bedrock/stone/dirt/
  grass/air columns), deterministic per seed.
- Naive per-face chunk meshing with correct cross-chunk-boundary face culling.
- First-person walk/strafe (WASD), jump, gravity, AABB-vs-voxel collision.
- Mouse-look **and** arrow-key look (yaw/pitch) — see blocker note below.
- Fly mode (Tab): no gravity, free vertical movement (Space/Left Ctrl).
- Hold-to-zoom (Left Shift) and orthographic projection (F).
- Block breaking/placing via DDA voxel raycast (left/right click).
- Hotbar: 14 placeable block types, keys 1–9 direct + E cycles all 14.
- Glass (transparent, still solid/collidable) and Cloud (opaque, but **not**
  collidable — inverse of Glass) both implemented and unit-tested.
- Per-chunk frustum culling.
- Day/night cycle (10-minute period, matches Craft's real `DAY_LENGTH=600`)
  driving ambient light + sky clear-color tint.
- Procedural (not flat-color, not authored-art) texture atlas: per-pixel
  speckle/mottle plus dedicated brick/wood-bark/wood-rings/plank/snow
  patterns.
- HUD: crosshair + large "`#4/14 Stone`"-style selected-item text,
  `[FLYING]` indicator.
- F12 screenshot capture to `screenshots/`.

**What does not work / is not implemented yet**:
- Greedy meshing (README's overview paragraph incorrectly implies this exists
  — it doesn't; see §5).
- Infinite/streamed world, chunk load/unload by distance.
- Caves/overhangs (needs 3D Simplex noise — only 2D `Simplex2` exists today).
- Trees/plants, non-cubic billboard geometry, ambient occlusion.
- Textured sky dome, point/block lighting, signs, chat.
- Any persistence (SQLite or otherwise) — world is regenerated fresh every
  run, edits are lost on exit.
- Multiplayer (no networking code at all yet).
- BGFX backend: configures but the first `cmake configure` triggers a real
  network clone of `bgfx.cmake` (unpinned `GIT_TAG master`) that can hang for
  minutes in a sandboxed/offline environment — see `missing.md`.

## 3. Recent changes

Most recent session's commits (newest first; each was individually built,
tested, and pushed):

- `bd86880` — Procedural texture atlas (`Render::BuildPlaceholderAtlas` →
  `BuildProceduralAtlas`; per-pixel patterns instead of flat color).
- `3f42960` — Day/night cycle (`Worlds/DayNightCycle.{hpp,cpp}`, new file;
  `CnaCraftGame::Draw` wiring).
- `847c6f8` — Cloud block (`BlockDef.collidable`, `World::IsCollidable`,
  `PlayerController::CollidesAt` updated). **Caught and fixed a real bug
  during development**: `collidable` defaulted to `true`, and Air's fallback
  `BlockDef` only overrode `solid`, so empty space was briefly collidable and
  froze the player (10 tests failed until `IsCollidable` was changed to
  `solid && collidable`).
- `2127b8c` — Fixed jump height: `kJumpSpeed` 7.0 → 8.0
  (`PlayerController.cpp`). Old value gave `v²/(2g)=0.98` blocks of jump
  height — mathematically could never clear a full 1-block step. User-
  reported ("skakani nefunguje, nemohu preskocit na vyssi blok").
- `1518e06` — Arrow-key look (Left/Right/Up/Down) as an alternative to mouse-
  look, additive with mouse deltas.
- `20faa70` — Fixed player spawn wedging (spawn moved from integer to
  block-center coordinates — `CnaCraftGame::Initialize`) and HUD text size
  (`Render/Hud.cpp` — now shows only the selected item at 3x font scale
  instead of all 14 names on one tiny line). Both user-reported
  ("nelze se pohybovat wasd... nemohu skakat... text dole je moc maly").
- `8e89154` — Glass transparency (`BlockDef.transparent`, `World::IsOpaque`,
  `ChunkMesher` now returns `ChunkMeshData{opaque, transparent}`,
  `ChunkRenderer` has two buffer sets, `CnaCraftGame::Draw` does a two-pass
  draw).
- `e193ded` — Per-chunk frustum culling (`ChunkRenderer::Bounds()` +
  `BoundingFrustum` in `CnaCraftGame::Draw`).
- `5c2f08b` — Swapped terrain noise from a custom value-noise to 2D Simplex
  noise (`Worlds/NoiseGenerator.cpp`), seeded per-world (unlike Craft's own
  fixed/global permutation table).
- Earlier in the same session: F12 screenshot, HUD overlay, hold-to-zoom/
  ortho, fly mode, expanded `BlockType` roster (6→13 types), hotbar.

Tests added this session (all in `tests/worlds_smoke_test.cpp`): glass
occlusion rules, cloud occlusion/collision rules, hotbar slot-count/cycling
for the growing roster, jump apex-height regression test (fails at 7.0,
passes at 8.0 — verified both ways), spawn-block-center regression test,
day/night curve shape (midnight/dawn/midday/dusk/wraparound), simplex noise
seed-determinism/variation.

## 4. Current blocker / main problem

**There is no build-breaking or test-breaking blocker right now.** Last known
state: clean build, 82/82 tests passing.

The closest thing to an open problem:

- **Mouse-look reliability is "verified in principle, not fully confirmed in
  the field."** A real injected relative-mouse-motion event was shown to
  produce exactly the expected yaw change (200px move → 0.5 rad, matching
  `sensitivity=0.0025`), proving the SDL-relative-mode → `InputManager` →
  `Mouse::GetState()` → `PlayerController` pipeline is wired correctly.
  Repeated testing in the sandbox's Xvfb-without-a-window-manager environment
  was inconsistent (worked once, failed on a retry) — most likely an
  environment/focus quirk of that specific sandbox, not a code bug, but this
  has **not** been independently confirmed on the actual user's machine since
  arrow-key look was added as a safety net. If the user reports mouse-look
  still doesn't turn the camera on their real machine, that's the next thing
  to investigate — start by asking whether clicking into the game window
  once after launch changes anything (window-focus/mouse-capture timing is
  the leading suspect).

No failing command or failing test is currently known. If you find one,
that supersedes everything else in this file.

## 5. Known bugs and limitations

- **Confirmed doc inaccuracy**: `README.md` §1 says "...adds the voxel-
  specific layer on top: chunk storage, **greedy meshing**, a block texture
  atlas..." — greedy meshing is **not implemented**. `ChunkMesher` does naive
  per-face meshing only (`plan.md` §4, §11.2 still lists greedy meshing as an
  open backlog item). Nobody has fixed this wording yet.
- **Confirmed limitation**: no persistence. World edits (broken/placed
  blocks) are lost when the process exits; `World::Generate` always starts
  fresh.
- **Confirmed limitation**: fixed-size world only (`128×64×128`), no
  chunk streaming, no load/unload by player distance.
- **Confirmed limitation** (documented in `missing.md`): full Craft parity
  across all three backends is blocked on custom-shader support. `EASYGL`
  has real runtime GLSL compilation; `VULKAN`'s `ShaderEffect` only accepts
  precompiled SPIR-V (no runtime GLSL→SPIR-V path); `BGFX`'s `ShaderEffect`
  is a stub (`Bind()` is a no-op) — needs upstream CNA engine work, out of
  this repo's scope. This blocks ambient occlusion, sky dome, and dynamic
  lighting from working identically on all three backends; `EASYGL` is the
  practical reference target for that work.
- **Confirmed limitation**: `BGFX` backend's first configure does a real,
  unpinned (`GIT_TAG master`) network clone of `bgfx.cmake` — can hang
  minutes in an offline/sandboxed environment. Not a bug, just a real cost to
  know about before choosing BGFX for CI or an offline dev box.
  (`missing.md` has the full writeup.)
- **Needs verification**: mouse-look on the user's actual machine post-fix
  (see §4 above).
- **Incomplete**: `Worlds/NoiseGenerator` only has 2D Simplex noise
  (`Simplex2`/`Height`). Caves/overhangs (backlog item, `plan.md` §11.1)
  needs a 3D variant (`Simplex3`) that doesn't exist yet.
- **Incomplete**: `BlockType` has no Leaves, tall grass, flowers, or Chest.
  Plants specifically need non-cubic (cross-quad billboard) geometry, which
  `ChunkMesher` doesn't support yet (only axis-aligned cube faces).
- **Risky assumption worth knowing**: the `plan.md` backlog was originally
  written from a quick read of Craft's source and turned out to have a few
  factual errors, corrected in-place as they were found during
  implementation (each correction is noted inline in `plan.md` where it
  happened): Craft's `noise.c`/`noise.h` actually live in `Craft/deps/noise/`,
  not inlined in `world.c`; Craft's README documents `ZXCVBN` exact-axis
  movement keys but the actual `src/main.c` doesn't implement them; Craft's
  README "Screenshot" section is just a marketing image, not a documented
  hotkey; `house3d_demo.cpp`'s "glass pass" scaffolding is dead code (
  `glass_builder_` is declared/uploaded but never appended to). If you're
  about to implement a backlog item citing a specific Craft source line,
  re-verify it against the actual checkout at
  `/rv/data/development/github.com/other/Craft` first — don't trust the
  citation blindly.

## 6. Architecture notes

**Main modules** (see `plan.md` §2/§8 for the original design writeup):

- `Worlds/BlockType.hpp` — `enum class BlockType` (14 values) + `BlockDef`
  (`solid`, `topTile`/`sideTile`/`bottomTile`, `transparent`, `collidable`)
  via `constexpr GetBlockDef(BlockType)`. **This is the single source of
  truth for block properties** — adding a new block type means adding an
  enum value + a `GetBlockDef`/`GetBlockName` case, nothing else needs to
  know about it directly.
- `Worlds/Chunk` / `World` — flat fixed-size storage; `World::GetBlock`/
  `SetBlock`/`IsSolid`/`IsOpaque`/`IsCollidable` are the only ways anything
  else touches block data. **Important distinction, easy to get wrong**:
  - `IsSolid` = "occupies space" (meshed, hit-testable) — `def.solid`.
  - `IsOpaque` = "occludes a neighboring face" — `def.solid && !def.transparent`.
  - `IsCollidable` = "physically blocks the player" — `def.solid && def.collidable`.
  - All three are deliberately AND'd with `def.solid` even though only
    `IsSolid` strictly needs to check it — this is defensive: a `BlockDef`
    field with a non-`false` default (like `collidable`) that ISN'T AND'd
    with `solid` will make Air "true" by accident whenever a new block type
    forgets to override that field. This exact bug has been hit and fixed
    once already (`847c6f8`) — do not remove the `solid &&` guard from any
    future `IsX` predicate.
- `Worlds/NoiseGenerator` — `Height(seed, x, z)`, 2D Simplex noise, seeded
  per-world (own permutation table per seed, unlike Craft's fixed one).
- `Worlds/ChunkMesher` — `Build(world, originX, originY, originZ) ->
  ChunkMeshData{opaque, transparent}`. Naive per-face culling: a face is
  emitted if `!world.IsOpaque(neighbor)`.
- `Worlds/PlayerController` — game-mode (gravity/jump/collision) and fly-mode
  physics, `Update(world, input, dt)`. Collision uses `World::IsCollidable`
  only.
- `Worlds/Hotbar` — `kSlots` (14 `BlockType`s, `Bedrock` excluded on
  purpose), `SelectSlot`/`CycleNext`/`Selected()`.
- `Worlds/DayNightCycle` — pure function `ComputeDaylight(elapsedSeconds,
  dayLengthSeconds)`, no state.
- `Render/TextureAtlas` — `BuildProceduralAtlas(device)` builds the whole
  atlas texture in memory (no asset file); `MapAtlasUv` maps tile-local UV →
  atlas UV. Atlas is `kAtlasTilesPerRow=5` (25 slots), 18 currently used.
- `Render/ChunkRenderer` — two `VertexBuffer`/`IndexBuffer` pairs per chunk
  (opaque + transparent), `DrawOpaque`/`DrawTransparent`, `Bounds()` for
  frustum culling.
- `Render/Hud` — crosshair + selected-item text via an embedded 8×8 bitmap
  font (CNA has no runtime `SpriteFont` content pipeline) rendered into a
  CPU-side buffer, uploaded via `Texture2D::SetDataRGBA`.
- `CnaCraftGame` — the only CNA-`Game`-subclass; owns everything, wires
  input → `Worlds/` calls → `Render/` draw calls. `Initialize`/`Update`/
  `Draw` follow the XNA lifecycle.

**Boundaries that should not be broken**:
- `Worlds/` must never `#include` anything CNA/SDL. This is what makes
  `tests/worlds_smoke_test.cpp` buildable/runnable with no GPU (
  `-DCNA_CRAFT_BUILD_GAME=OFF`). Don't leak a CNA type into a `Worlds/`
  header even for convenience.
- Any new `BlockDef` boolean flag with a non-`false` default **must** be
  AND'd with `solid` wherever it's consumed (see §6 note above) — this is
  the single most important invariant to preserve given the bug history.
- Two-mesh-buffer convention (`ChunkMeshData{opaque, transparent}`) — if you
  add a third rendering category (e.g. cutout-alpha for future plants),
  extend this struct rather than overloading `transparent` for a different
  meaning.

**Data flow** (per frame): `CnaCraftGame::Update` reads `Keyboard`/`Mouse` →
builds a `Worlds::PlayerInput` → `PlayerController::Update(world, input, dt)`
→ (on click) `VoxelRaycast::Cast` → `World::SetBlock` → marks chunk(s) dirty →
`RebuildDirtyChunks()` calls `ChunkRenderer::Rebuild` only for dirty chunks.
`CnaCraftGame::Draw` computes `daylight` → sets ambient + clear color →
builds a `BoundingFrustum` → draws all in-frustum chunks' opaque geometry,
then all in-frustum chunks' transparent geometry (blend on/depth-write off),
then the HUD, then handles a pending F12 screenshot.

## 7. Useful commands

Engine-agnostic tests only (no CNA/GPU/display needed):
```bash
cmake -S . -B build-worlds -DCNA_CRAFT_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-worlds -j"$(nproc)"
./build-worlds/cna_craft_worlds_smoke_test
```
Expect: `All checks passed.` (82 `ok:` lines, 0 `FAIL:` lines as of this
writing).

Full graphical game (requires `../cna` and `../sharp-runtime` checked out as
siblings of this repo):
```bash
cmake -S . -B build-easygl -DCNA_GRAPHICS_BACKEND=EASYGL
cmake --build build-easygl --target CnaCraft -j"$(nproc)"
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft          # interactive
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft --smoke 30   # headless CI-style, exits after 30 frames
```
Swap `EASYGL` for `VULKAN` the same way. Avoid `BGFX` unless you're prepared
for a real network fetch on first configure (see §5).

There is no separate lint/format tooling configured in this repo.

Reproduce nothing currently — there is no known failing command. If a new
bug shows up, add its exact repro command to this section when you update
this file.

## 8. Next smallest tasks

Ordered by "smallest/safest first." Pick from the top unless something else
is more urgent (a user-reported bug always jumps the queue).

1. **Fix the README "greedy meshing" inaccuracy.**
   Goal: `README.md` §1 should say "naive per-face meshing" (or just drop the
   claim) instead of implying greedy meshing exists.
   Files: `README.md` only.
   Verify: re-read the paragraph; no build/test needed (docs-only).

2. **Add `Simplex3` (3D Simplex noise) to `Worlds/NoiseGenerator`.**
   Goal: prerequisite for the "Caves/overhangs" backlog item (`plan.md`
   §11.1) — a 3D density-noise term to carve overhangs/caves out of the
   existing heightmap-based terrain. Do this as its own small task (add the
   function + unit tests) before wiring it into `World::Generate`, so it's
   independently testable first.
   Files: `src/CnaCraft/Worlds/NoiseGenerator.{hpp,cpp}`,
   `tests/worlds_smoke_test.cpp`.
   Verify: `cmake --build build-worlds -j$(nproc) && ./build-worlds/cna_craft_worlds_smoke_test`
   — add tests asserting determinism-per-seed and a reasonable output range,
   mirroring the existing `Simplex2`/`Height` tests.

3. **Wire caves/overhangs into `World::Generate` using the new `Simplex3`.**
   Goal: combine the existing 2D heightmap with the 3D density term to carve
   air pockets below the surface, matching Craft's `src/world.c` approach
   (cited in `plan.md` §11.1 — verify against the real Craft checkout before
   copying the exact thresholds).
   Files: `src/CnaCraft/Worlds/World.cpp`, `tests/worlds_smoke_test.cpp`.
   Verify: worlds smoke test (add a determinism test for the new generation
   path) + a real EasyGL build screenshot showing a visible cave/overhang.

4. **Add simple trees during world generation (Wood trunk + Cloud/Leaves-style
   canopy, no billboards needed).**
   Goal: Craft's trees are just placed `WOOD`/`LEAVES` cube clusters (no
   cross-quad billboard geometry required — that's only needed for tall
   grass/flowers, a separate, harder backlog item). This is placeable with
   existing cube-block infrastructure once a `Leaves` block type exists
   (transparent like Glass — reuse that exact pattern).
   Files: `src/CnaCraft/Worlds/BlockType.hpp` (add `Leaves`), `World.cpp`
   (tree placement pass after heightmap generation), `Render/TextureAtlas.cpp`
   (a tile + pattern for Leaves), `Hotbar.hpp` (add to placeable roster),
   `tests/worlds_smoke_test.cpp`.
   Verify: worlds smoke test, then a real EasyGL build screenshot showing
   trees in the generated terrain.

5. **Independently confirm mouse-look on a real (non-sandboxed) machine.**
   Goal: resolve the "needs verification" item in §4/§5. Not a coding task —
   ask the user directly whether mouse-look turns the camera now, and if
   not, whether clicking into the window first changes anything.
   Files: none (diagnostic conversation, not code) — unless it turns up a
   real bug, in which case scope a follow-up task then.

Do not start on SQLite persistence, multiplayer, AO, greedy meshing, or the
sky dome/shader work as a "next smallest task" — they're all real backlog
items but none of them are small (see §9).

## 9. Do not do yet

- **No SQLite integration** without an explicit go-ahead from the user first
  — it's a new external dependency (like BGFX's network-fetch cost), not a
  small drop-in.
- **No multiplayer/networking work** — large scope, no networking code
  exists at all yet, needs its own design pass first.
- **No ambient occlusion or greedy meshing rewrite** — both require
  `MeshVertex`/`ChunkMesher` format changes and are real algorithmic work,
  not a small task. Don't start either as a "quick" fix.
- **No sky dome / custom shader work** — blocked on real per-backend
  constraints (see `missing.md`); `EASYGL` is the only backend with working
  runtime shaders today. Don't start this without deciding which backend(s)
  to target first.
- **No broad refactor of `Worlds/` or `Render/`** — the two-layer split and
  the `IsSolid`/`IsOpaque`/`IsCollidable` trio are load-bearing and
  deliberately shaped by a real bug history (see §6). Don't "clean up" the
  `solid &&` guards even though they look redundant.
- **No renaming/removing `BuildProceduralAtlas`, `ChunkMeshData`, or the
  `Hotbar::kSlots` ordering** without checking every call site — several
  tests assert on exact slot indices/counts.
- **No unrelated cleanup** — if you notice something odd outside the task
  you're doing, note it in this file's §5 instead of fixing it inline.
- **No API/behavior changes to `PlayerController`'s public methods**
  (`Update`, `ToggleFlying`, `EyePosition`, `LookDirection`, `Yaw`/`Pitch`,
  `IsGrounded`, `IsFlying`) without re-running the full test suite — several
  tests depend on exact physics constants (`kJumpSpeed=8.0`, `kGravity=25.0`,
  `kMoveSpeed=4.5`, `kFlySpeed=9.0`).

## 10. Resume prompt

```
Read NEXT.md first. Pick the first unstarted task from its "Next smallest
tasks" list (or a user-reported bug if one exists — that always takes
priority). Inspect only the files it names — do not refactor unrelated
code, and do not touch anything listed under "Do not do yet". Make one
small, verified improvement: implement the task, build it
(cmake --build build-worlds, or the full EasyGL build if it touches
Render/ or CnaCraftGame), run the relevant test/smoke command, and confirm
it actually passes before considering the task done. If the task claims
something about Craft's source code, re-verify it against the real
checkout at /rv/data/development/github.com/other/Craft rather than
trusting the citation. When finished, update NEXT.md: move the completed
task out of "Next smallest tasks", update "Current status"/"Recent
changes" with what actually changed, and note any new bugs or open
questions you found under "Known bugs and limitations".
```
