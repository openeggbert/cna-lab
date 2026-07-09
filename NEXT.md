# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after this
session's work (committed as `<pending>` — see git log for the actual hash)
on top of commit `8bb26e7` (`Add 3D Simplex noise, cloud generation, and
simple trees`) on branch `develop`.

## 1. Project summary

CNA Craft is a first-person voxel-world game/prototype, built entirely on
[CNA](https://github.com/openeggbert/cna) — a C++ reimplementation of the
XNA 4.0 programming model — and its `sharp-runtime` utility layer. The goal
is a faithful CNA-native **port of [fogleman/Craft](https://github.com/fogleman/Craft)**
(MIT-licensed reference project; see `THIRD_PARTY_NOTICES.md`), not merely a
"Craft-inspired" prototype — architecture/algorithms are studied and ported
where practical, no code copied verbatim.

**Current development phase**: past initial-prototype milestones (M0–M6 in
`plan.md` §9). This session did a full, systematic feature-by-feature parity
audit against the real Craft source (not just README claims) — see
[CRAFT_PARITY.md](CRAFT_PARITY.md), the new **source of truth for "does
cna-craft match Craft's real behavior."** `plan.md` §12 ("Craft Feature
Parity Port") converts its gaps into an ordered task list and **supersedes**
§11's older, less systematically-audited backlog for anything the two
disagree on. Read `CRAFT_PARITY.md` and `plan.md` §12 first when resuming.

**Key architectural decisions**: unchanged — two-layer split (`Worlds/`
engine-agnostic, `Render/` + `CnaCraftGame` CNA-dependent), fixed-size world
(`128×64×128`), naive per-face meshing, backend-agnostic game code,
two-mesh-buffer (opaque/transparent) convention. See `plan.md` §2/§6.

## 2. Current status

**Build status**: verified this session (EasyGL backend) — clean configure +
build from scratch, no warnings, `CnaCraft` executable links and runs.
Verified via headless `--smoke 60` runs and real interactive screenshots
(Xvfb/xdotool/ImageMagick), not just compilation.

**Test status**: `tests/worlds_smoke_test.cpp` — **117 plain-assert checks,
all passing** (up from 103 at the start of this session). Builds and runs
standalone with `-DCNA_CRAFT_BUILD_GAME=OFF` (no CNA/GPU/display needed).

**What changed this session**: a full 5-area parallel source audit against
the real Craft checkout (`/rv/data/development/github.com/other/Craft`)
produced `CRAFT_PARITY.md` (~35 feature areas, each with cited Craft
behavior, cited cna-craft behavior, and a status). Six concrete, verified
gaps were then fixed:

1. **Diagonal-movement speed bug** — `PlayerController`'s move input is now
   normalized to a unit vector before scaling by speed, matching Craft's
   `get_motion_vector` (`atan2f`-based, always unit-length). Previously,
   holding two movement keys (e.g. W+D) moved ~41% faster than one key.
2. **Terminal velocity clamp** — fall speed is now capped at Craft's own
   `-250 units/s` (`PlayerController::kTerminalVelocity`); previously
   unbounded.
3. **Bedrock break-protection** — new `BlockDef.breakable` flag (`World::
   IsBreakable`), false only for Bedrock. Bedrock (a cna-craft-only concept —
   Craft itself has no such block) could previously be mined away, despite
   `Hotbar.hpp`'s own comment calling it "not meant to be placed by the
   player." Ported the spirit of Craft's `is_destructable` guard.
4. **Player self-intersection placement guard** — new `PlayerController::
   IntersectsBlock(bx,by,bz)`, wired into right-click placement. Ports
   Craft's real `on_right_click` guard (`!player_intersects_block(...)`).
   Previously a player could place a block that immediately trapped them.
5. **Hotbar completeness** — added key `0` (10th direct slot), `R`
   (reverse-cycle, new `Hotbar::CyclePrev`), and scroll-wheel cycling, all
   verified against Craft's real `on_key`/`on_scroll`. Previously only
   `E`/1-9 existed.
6. **Wireframe selection outline** — new `Render/SelectionOutline.{hpp,cpp}`,
   a 12-edge `LineList` box drawn around the currently-raycast-targeted
   block each frame (ports Craft's `render_wireframe`/`make_cube_wireframe`).
   **Verified this does NOT need a custom shader** — plain `BasicEffect` +
   `VertexPositionColor` works on all three backends, unlike AO/sky-dome.
   Previously the player had zero visual feedback for which block would be
   broken/placed.

All 6 fixes have new unit tests (14 new checks: 103→117) and the wireframe
outline was additionally verified against a real EasyGL build via
screenshot (visible black edge lines around the targeted block).

**What was investigated but deliberately NOT changed**:
- `kFlySpeed` is 2x `kMoveSpeed`, not Craft's 4x — the in-code comment
  claiming 4x was wrong and has been corrected, but the *speed itself* was
  left as-is (a subjective tuning choice, not a bug — see `CRAFT_PARITY.md`
  §1.5). `needs_human` if you want it changed to match Craft's ratio exactly.
- Collision substepping (`CRAFT_PARITY.md` §1.7) — Craft explicitly
  substeps (`step = MAX(8, estimate)`) to avoid tunneling through thin
  obstacles at high velocity; cna-craft does a single whole-step-or-revert
  per axis per frame. Left `pending`, not bundled into this session's
  quick-fix batch — it touches load-bearing physics code that the project's
  own rules say needs the *full* test suite re-verified against every
  constant as its own careful pass, not a drive-by change.
- Removing `Cloud` from the placeable hotbar (Craft never allows placing
  clouds) — `needs_human`, a trade-off between Craft fidelity and
  cna-craft's own existing player-facing feature.
- The dead `Sand` block (defined everywhere, never generated by
  `World::Generate`) — a small, unambiguous fix, left `pending` (not picked
  up this session, see `plan.md` §12.1 item 9).

**What still does not work / is not implemented** (see `CRAFT_PARITY.md` for
the full, cited breakdown): non-cubic plant/billboard geometry, ambient
occlusion (blocked — shader backend limitations), sky dome, fog (NOT
blocked — `BasicEffect` has built-in fog, just unused), any persistence,
signs, chat/commands, multiplayer, hash-map/streamed chunk system.

## 3. Recent changes (this session)

Not yet committed as of this writing (standing permission granted by the
user to commit/push periodically — see memory). Files touched: `README.md`
(N/A this session — no changes), `plan.md`, `NEXT.md` (this file, new),
`CRAFT_PARITY.md` (new), `CMakeLists.txt`,
`src/CnaCraft/Worlds/{BlockType,Hotbar,PlayerController,World}.{hpp,cpp}`,
`src/CnaCraft/CnaCraftGame.{hpp,cpp}`,
`src/CnaCraft/Render/SelectionOutline.{hpp,cpp}` (new),
`tests/worlds_smoke_test.cpp`.

In order:

1. **Dispatched 5 parallel research agents**, each reading the real Craft
   checkout against a specific area of cna-craft's `src/` tree
   (input/movement/camera; hotbar/block-place/raycast; chunk/mesh/terrain;
   persistence/signs/multiplayer; visuals/shaders/atlas), each producing a
   cited, structured comparison report.
2. **Synthesized `CRAFT_PARITY.md`** from the 5 reports — ~35 feature areas,
   each with Craft behavior (cited), cna-craft behavior (cited), status,
   priority, and verification method, plus a summary table.
3. **Added `plan.md` §12 "Craft Feature Parity Port"** — converts
   `CRAFT_PARITY.md`'s gaps into an ordered, prioritized task list with
   explicit statuses, superseding §11 where they disagree.
4. **Implemented the 6 fixes listed in §2 above**, each built and tested
   individually before moving to the next.

## 4. Current blocker / main problem

**There is no build-breaking or test-breaking blocker right now.** Clean
build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and `-DCNA_GRAPHICS_BACKEND=EASYGL`,
built from scratch), 117/117 tests passing.

Carried over, still unresolved, still not urgent: mouse-look reliability
confirmation on the user's real machine (`needs_human`, unchanged for
several sessions now — see `plan.md` §11.0/§12.1).

## 5. Known bugs and limitations

See `CRAFT_PARITY.md` for the authoritative, cited list — do not re-derive
this from memory or re-read Craft's source from scratch without checking
there first, since it's now the maintained single source of truth for
parity gaps. Highlights not yet fixed:

- **High priority, `needs_human`**: no world persistence (SQLite dependency
  decision) — `CRAFT_PARITY.md` §4.1/§4.2.
- **High priority, `pending` (large)**: no non-cubic plant/billboard
  geometry — `CRAFT_PARITY.md` §3.7. Needs a `ChunkMesher`/`MeshData` format
  change (a second emission path), not a quick task.
- **High priority, `blocked`**: no ambient occlusion — needs a custom
  vertex format + `ShaderEffect`, only real on EASYGL today (`missing.md`).
- **Medium priority, `pending`, NOT blocked**: no fog. `BasicEffect` has
  built-in linear distance fog (`FogEnabled`/`FogColor`/`FogStart`/
  `FogEnd`) that needs no custom shader — a genuine low-hanging-fruit gap
  `missing.md` hadn't previously flagged as backend-agnostic.
- **Medium priority, `pending`**: dead `Sand` block (defined, never
  generated) — `CRAFT_PARITY.md` §3.3.
- **Medium priority, `needs_human`**: terrain-height formula structurally
  differs from Craft's real dual-multiplicative-simplex2 composition
  (cna-craft uses a single additive simplex2 call) — changing it would
  visibly reshape all existing terrain, a tuning decision.

## 6. Architecture notes

**New this session**:
- `Worlds/BlockType.hpp` — `BlockDef` gained a `breakable` field (default
  `true`, `false` only for Bedrock). Same defensive `solid &&` AND-guard
  pattern as `transparent`/`collidable` — **do not remove the `solid &&`
  guard from `World::IsBreakable`**, same reasoning as the existing
  `IsOpaque`/`IsCollidable` note below.
- `Worlds/PlayerController` — gained `IntersectsBlock(bx,by,bz)` (public) and
  a `kTerminalVelocity=-250.0f` clamp. The horizontal move-input vector is
  now normalized before scaling by speed (inside `Update`, not part of the
  public API — no signature changes).
- `Worlds/Hotbar` — gained `CyclePrev()`, symmetric with the existing
  `CycleNext()`.
- `Worlds/World` — gained `IsBreakable(x,y,z)`.
- `Render/SelectionOutline` (new file) — a small, self-contained class:
  `Update(device, bx,by,bz)` rebuilds an 8-vertex/24-index `LineList` box
  around a cell, `Draw(device, effect)` issues the draw call. Owns its own
  `VertexBuffer`/`IndexBuffer`, rebuilt (not reallocated) every frame the
  target changes. `CnaCraftGame::Draw` temporarily flips the shared
  `BasicEffect` to `VertexColorEnabled=true`/`TextureEnabled=false`/
  `LightingEnabled=false` around the outline draw call, then restores it —
  if you add another `VertexPositionColor`-based draw call, follow this same
  save/restore pattern rather than leaving the effect in colored-unlit mode.
- `CnaCraftGame::Update` — the break/place raycast is now cast **once per
  frame** (previously once per click), and its result is reused for the
  new outline. If you touch this code, note that `hit` is a `std::optional`
  captured once near the top of the click-handling block, not re-cast per
  branch.

**Everything else** (module list, boundaries, data flow) — unchanged from
prior sessions, see `plan.md` §2/§6/§8.

**Boundaries that should not be broken** — unchanged, still load-bearing:
`Worlds/` must never `#include` anything CNA/SDL; any new `BlockDef` boolean
flag with a non-`false` default must be AND'd with `solid` wherever
consumed (now applies to `breakable` too); two-mesh-buffer convention.

## 7. Useful commands

Unchanged from before:

```bash
# Engine-agnostic tests only (no CNA/GPU/display needed):
cmake -S . -B build-worlds -DCNA_CRAFT_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-worlds -j"$(nproc)"
./build-worlds/cna_craft_worlds_smoke_test
```
Expect: `All checks passed.` (117 `ok:` lines, 0 `FAIL:` lines as of this
writing).

```bash
# Full graphical game (requires ../cna and ../sharp-runtime as siblings):
cmake -S . -B build-easygl -DCNA_GRAPHICS_BACKEND=EASYGL
cmake --build build-easygl --target CnaCraft -j"$(nproc)"
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft          # interactive
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft --smoke 30
```

There is no separate lint/format tooling configured in this repo.

## 8. Next smallest tasks

`plan.md` §12.1 is the authoritative ordered priority queue — read it first.
As of this session, queue items 1, 3, 4, 5, 6, 10 are `completed`; item 2 is
`needs_human`; items 7, 9, 11, 13, 14, 16-19 are `pending`; items 12, 15 are
`blocked`/`needs_human` respectively. A short candidate list for the next
session, smallest/safest first:

1. **Fix the dead `Sand` block** (`CRAFT_PARITY.md` §3.3, `plan.md` §12.1
   item 9) — `World::Generate` never places `BlockType::Sand` despite it
   being fully defined (BlockDef, hotbar roster). Small, unambiguous, no
   design decision needed. Files: `src/CnaCraft/Worlds/World.cpp`.
2. **Fog via `BasicEffect`'s built-in properties** (`CRAFT_PARITY.md` §5.2,
   `plan.md` §12.1 item 13) — genuinely not blocked by shader limitations,
   unlike AO/sky-dome. `FogEnabled`/`FogColor`/`FogStart`/`FogEnd` on the
   existing `effect_` in `CnaCraftGame::Draw`.
3. **Middle-click eyedropper** (`CRAFT_PARITY.md` §2.7) — small,
   independent convenience feature, not blocked on anything larger.

Do not start on: collision substepping (needs its own careful pass, not a
quick task — see §2 above), plants/billboards (large mesh-format change),
AO/sky-dome (blocked/needs_human), persistence/signs/chat/multiplayer (all
`needs_human` or explicitly deferred), chunk-system redesign (large
architecture change, `needs_human` on whether it's even wanted given the
project's stated fixed-world scope).

## 9. Do not do yet

Unchanged from before (SQLite, multiplayer, AO/greedy-meshing rewrite, sky
dome/custom shaders, broad `Worlds/`/`Render/` refactor, `PlayerController`
public-API changes without re-running the full suite, `Hotbar::kSlots`
reordering) — **plus**:

- **No collision-substepping change** as a "quick fix" — it's real,
  identified work (`CRAFT_PARITY.md` §1.7) but explicitly deferred this
  session specifically because it touches load-bearing physics code; do it
  as its own careful pass with the full test suite re-verified.
- **No fly-speed constant change** (`kFlySpeed`) without a human decision —
  the 2x-vs-Craft's-4x ratio was investigated and deliberately left alone
  this session (see §2 above); don't "fix" it as a drive-by.
- **No removing `Cloud` from `Hotbar::kSlots`** without a human decision —
  it's a documented Craft-fidelity-vs-existing-feature trade-off, not a bug.

## 10. Resume prompt

```
Read CRAFT_PARITY.md first (the authoritative Craft-vs-cna-craft parity
audit), then plan.md §12.1 (the ordered priority queue derived from it).
Pick the first pending, non-blocked, non-needs_human item there (or a
user-reported bug if one exists — that always takes priority). Before
implementing anything that cites Craft's source code, re-verify the
citation against the real checkout at
/rv/data/development/github.com/other/Craft rather than trusting
CRAFT_PARITY.md or plan.md blindly — they were carefully audited this
session but could still contain an error, same as every prior citation
pass in this project's history. Inspect only the files the task names —
do not refactor unrelated code, and do not touch anything listed under "Do
not do yet" in either plan.md or this file. Make one small, verified
improvement: implement the task, build it (cmake --build build-worlds, or
the full EasyGL build if it touches Render/ or CnaCraftGame), run the
relevant test/smoke command, and confirm it actually passes before
considering the task done. When finished, update plan.md §12.1's status
for that item, update CRAFT_PARITY.md's corresponding entry if the fix
changes its status from partial/missing to complete, and update this
file's "Current status"/"Recent changes" with what actually changed.
```
