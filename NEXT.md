# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after
commit `797c67d` (`Re-assess Signs/Chat scope`) on branch `develop`, the
end of a long autonomous multi-task session.

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
   into an ordered, numbered task list (22 items) with the same status
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
links and runs. Verified via headless `--smoke 120` runs and multiple real
interactive screenshots (Xvfb/xdotool/ImageMagick), not just compilation.

**Test status**: `tests/worlds_smoke_test.cpp` — **149 plain-assert checks,
all passing** (up from 103 at the start of this session, 82 two sessions
ago). Builds and runs standalone with `-DCNA_CRAFT_BUILD_GAME=OFF`.

**plan.md §12.1 status as of this session's end** (22 items):
- **15 `completed`**: hotbar 0/R/scroll, wireframe selection outline,
  Bedrock break-protection, player self-intersection placement guard,
  diagonal-movement normalization, non-cubic plant geometry (TallGrass),
  texture-atlas note, dead-Sand-block fix, terminal-velocity clamp,
  collision substepping, fog, sky dome (untextured), middle-click
  eyedropper, Ctrl+left-click-as-place, Flower blocks.
- **4 `needs_human`**: Cloud-placeable-via-hotbar (Craft fidelity vs.
  existing feature trade-off), world persistence, delta storage, chunk
  system redesign (hash-map + streaming).
- **1 `blocked`**: ambient occlusion (needs a custom `ShaderEffect`, only
  real on EASYGL today).
- **2 `pending (large)`**: Signs, Chat/slash-commands — both need a new
  text-input state machine as a shared prerequisite (CNA *does* have a
  usable `TextInputEXT` primitive, checked this session, so this isn't
  engine-blocked — it's genuinely more work than "small, safe, reviewable"
  scope allows in one continuous pass).
- **1 `pending`, explicitly deferred**: multiplayer, per project direction
  (not before local single-player + persistence are stable).

This is a legitimate stop point: every item that was small enough to
implement safely in a continuous session has been implemented, tested, and
verified; everything left needs either a human decision or is large enough
to deserve its own dedicated session.

## 3. Recent changes (this session — a long autonomous run)

All individually committed and pushed to `develop`, in order:

1. `ee05886` — `CRAFT_PARITY.md` audit (5 parallel research passes against
   the real Craft checkout) + `plan.md` §12 ordered task list, plus 6
   immediate fixes: diagonal-movement normalization, terminal-velocity
   clamp, Bedrock break-protection (`BlockDef.breakable`/`World::
   IsBreakable`), player self-intersection placement guard
   (`PlayerController::IntersectsBlock`), hotbar completeness (key `0`,
   `R`, scroll wheel), wireframe selection outline (`Render::
   SelectionOutline`, verified NOT blocked by shader limitations).
2. `6a08ecf` — Fixed the dead `Sand` block: a low-elevation "beach" surface
   rule (`kSandMaxHeight=10`), adapted from Craft's real `h<=12` rule.
3. `6e77628` — Distance fog via `BasicEffect`'s built-in
   `FogEnabled`/`FogColor`/`FogStart`/`FogEnd` — confirmed not blocked by
   shader limitations (CNA has a real cross-backend fog test suite for this
   exact code path).
4. `377f6b5` — Middle-click eyedropper (`Hotbar::SelectByBlockType`).
5. `b135abe` — Non-cubic plant billboard geometry: `ChunkMesher::EmitPlant`
   (4-quad cross, ported from Craft's `make_plant`), new
   `BlockDef.plant` flag, `BlockType::TallGrass`, `World::
   GenerateGrassDecoration`.
6. `ee71f13` — Collision substepping (`PlayerController::Update` now runs
   `clamp(estimate, 8, 64)` substeps per frame, ported from Craft's own
   `step = MAX(8, estimate)`), preventing tunneling through thin walls at
   large `dt`. Empirically verified meaningful (forced substepping off,
   confirmed the new regression test then failed, restored and re-passed).
7. `974f108` — Plain vertex-colored sky dome (`Render::SkyDome`), replacing
   the flat clear-color sky. **Real bug caught and fixed during
   development**: first winding guess rendered nothing at all; traced to
   CNA's actual default `CullCounterClockwiseFace` rasterizer state via
   reading the source, fixed empirically with debug colors.
8. `dfdff8f` — Ctrl+left-click as place, matching Craft's real
   `on_mouse_button` modifier routing.
9. `abd89f5` — Flower blocks, reusing the TallGrass plant infrastructure.
10. `797c67d` — Re-assessed Signs/Chat scope (confirmed CNA has a usable
    text-input primitive, but the state machine + 3D billboard rendering
    don't exist yet) and documented the reasoning in both `plan.md` and
    `CRAFT_PARITY.md` rather than silently skipping them.

Test count progression: 103 → 117 (first batch of 6 fixes) → 120 (Sand) →
125 (eyedropper) → 138 (plants) → 139 (substepping) → 149 (flowers). Sky
dome and Ctrl+click added no new `Worlds/`-layer tests (pure render glue /
pure input glue respectively, verified via build + smoke run instead).

## 4. Current blocker / main problem

**None.** Clean build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and
`-DCNA_GRAPHICS_BACKEND=EASYGL`, built from scratch), 149/149 tests
passing, zero compiler warnings.

Carried over, still unresolved, still not urgent: mouse-look reliability
confirmation on the user's real (non-sandboxed) machine — `needs_human`,
unchanged for several sessions now (see `plan.md` §11.0/§12.1 for history).

## 5. Known bugs and limitations

See `CRAFT_PARITY.md` for the authoritative, cited, per-feature list — read
it before re-deriving anything from memory or re-reading Craft's source
from scratch. Highlights of what's NOT done yet:

- **`needs_human`**: world persistence (SQLite dependency decision),
  Cloud-placeable-via-hotbar (Craft never allows placing clouds; removing
  it is a fidelity-vs-existing-feature trade-off), terrain-height formula
  (Craft's real dual-multiplicative-simplex2 vs. this project's
  single-additive-simplex2 — changing it would reshape all existing
  terrain), chunk system (hash-map + streaming vs. fixed dense grid — is
  an unbounded world even wanted, given the project's stated fixed-size
  prototype scope in `plan.md` §1?).
- **`blocked`**: ambient occlusion — needs a custom vertex format +
  `ShaderEffect`, only real on EASYGL today (`missing.md`).
- **`pending (large)`**: Signs, Chat/slash-commands (see §2/§3 above for
  the detailed reasoning — both need a new text-input state machine;
  signs additionally need new 3D billboard rendering; chat additionally
  needs Craft's world-editing macro commands).
- **Deliberately deferred**: multiplayer.
- **Low-value, not picked up**: the other 5 of Craft's 6 flower colors,
  Chest, the 32-entry dye-color palette — all would reuse existing
  infrastructure (plant geometry or plain-cube blocks) but are pure
  content-scaling with little gameplay value, explicitly deprioritized per
  "prioritize gameplay parity over decorative additions."

## 6. Architecture notes

**New this session** (see individual commit messages in §3 for full
per-feature detail — this is just the "if you touch this again" summary):

- `Worlds/PlayerController` — `Update()` now runs a substep loop internally
  (`kMinSubsteps=8`, `kMaxSubsteps=64`); yaw/pitch and jump-impulse
  consumption stay once-per-call, only position/velocity integration is
  substepped. Gained public `IntersectsBlock(bx,by,bz)`. Gained
  `kTerminalVelocity=-250.0f`. Move input is normalized before scaling by
  speed. **If you touch this function again**: re-run the full test suite
  (139+ physics-related checks) — this is exactly the kind of load-bearing
  code the project's own rules warn about.
- `Worlds/BlockType` — `BlockDef` gained `breakable` (false only for
  Bedrock) and `plant` (true for `TallGrass`/`Flower`) fields, both
  following the established `solid &&`-guard defensive pattern — **do not
  drop that guard** when adding a new `BlockDef` boolean.
- `Worlds/ChunkMesher` — gained `EmitPlant`, a second mesh-emission path
  (4-quad cross billboard) alongside the original 6-cube-face path, gated
  by `BlockDef.plant`. Adding a new plant type (a flower color, Chest) is
  just a new `BlockType` + atlas tile, not new mesher logic.
- `Worlds/Hotbar` — gained `CyclePrev()` and `SelectByBlockType(type)`.
  `kSlots` grew from 14 → 17 across this and the prior session (Cloud,
  Leaves, TallGrass, Flower), always appended at the end, never reordered.
- `Worlds/World` — `Generate()` pipeline order is now: terrain → trees →
  grass decoration → flowers → clouds. Each later pass's "only place over
  Air" guard relies on this exact order; if you insert a new pass, think
  about where in this sequence it belongs.
- `Render/SelectionOutline`, `Render/SkyDome` (both new files) — both use
  the same "flip `BasicEffect` to `VertexColorEnabled=true`/
  `TextureEnabled=false`/`LightingEnabled=false`, draw, flip back" pattern.
  **If you add a third `VertexPositionColor`-based draw call, follow this
  same save/restore pattern.**
- `Render/SkyDome` specifically: **the triangle winding for non-cube
  geometry is not obvious** — CNA's default `RasterizerState` culls
  `CullCounterClockwiseFace` (confirmed by reading
  `RasterizerState.cpp`), and empirically the correct visible winding for
  inward-facing dome geometry was the *opposite* of the naive "CCW viewed
  from inside" reasoning. If you add another custom mesh (not cube faces,
  not the existing plant cross), verify winding with vivid debug colors in
  a real build before trusting geometric reasoning alone.
- `CnaCraftGame::Update` — the break/place raycast is cast once per frame
  (not once per click) and reused for break, place, Ctrl+place, the
  eyedropper, and the outline. A `tryPlaceBlock` lambda is shared between
  ordinary right-click and Ctrl+left-click.

**Everything else** (module list, boundaries, data flow) — unchanged, see
`plan.md` §2/§6/§8.

**Boundaries that must not be broken** — unchanged, still load-bearing:
`Worlds/` must never `#include` anything CNA/SDL; any new `BlockDef`
boolean flag with a non-`false` default must be AND'd with `solid`
wherever consumed; two-mesh-buffer convention (plants extend `transparent`,
they didn't need a third category).

## 7. Useful commands

Unchanged:

```bash
# Engine-agnostic tests only (no CNA/GPU/display needed):
cmake -S . -B build-worlds -DCNA_CRAFT_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-worlds -j"$(nproc)"
./build-worlds/cna_craft_worlds_smoke_test
```
Expect: `All checks passed.` (149 `ok:` lines, 0 `FAIL:` lines as of this
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

`plan.md` §12.1 is the authoritative ordered priority queue. As of this
session's end, everything genuinely small is done. The real next steps all
require either a human decision or a dedicated multi-part session:

1. **Human decisions needed** (ask the user, don't guess): world
   persistence (SQLite — a new dependency), Cloud-placeable-via-hotbar
   (keep the existing feature or match Craft exactly?), terrain-height
   formula (worth reshaping existing terrain to match Craft's real
   formula?), chunk system (is an unbounded/streamed world even wanted?).
2. **Dedicated follow-up session**: Signs + Chat/commands (share a
   text-input state-machine prerequisite — CNA has the primitive, the
   state machine and 3D billboard rendering don't exist yet). Scope this
   as its own session with its own plan, not a "next smallest task."
3. **Low priority, low value**: the other 5 Craft flower colors, Chest,
   dye-color palette — technically easy (same plant/cube infrastructure)
   but low gameplay value, explicitly deprioritized twice now.

## 9. Do not do yet

Everything from prior sessions still applies (SQLite without go-ahead, no
multiplayer, no AO/greedy-meshing rewrite without a shader-backend
decision, no broad `Worlds/`/`Render/` refactor, no `PlayerController`
public-API changes without re-running the full suite, no `Hotbar::kSlots`
reordering, no fly-speed constant change without a human decision, no
removing `Cloud` from the hotbar without a human decision) — **plus**:

- **No rushed Signs/Chat implementation** — both were deliberately
  re-scoped to "pending (large)" this session after confirming they're not
  simply blocked; doing either properly needs a real design pass for the
  text-input state machine (specifically: how should typing interact with
  WASD/mouse-look while a text box is focused?), not a drive-by addition.
- **No collision-substepping constant changes** (`kMinSubsteps=8`,
  `kMaxSubsteps=64`) without re-running the full test suite AND the manual
  "force substeps to 1, confirm the tunneling test fails, restore" check —
  the tunneling regression test is only meaningful if substepping is
  actually active; a silent regression here wouldn't show up any other way.
- **No `SkyDome`/`SelectionOutline` winding changes** without a real
  EasyGL build + screenshot verification — this geometry's correct winding
  was found empirically, not derived analytically, so don't "clean up"
  the winding based on reasoning alone.

## 10. Resume prompt

```
Read CRAFT_PARITY.md first (the authoritative Craft-vs-cna-craft parity
audit), then plan.md §12.1 (the ordered priority queue derived from it) —
as of the last session, all 22 items are completed/needs_human/blocked/
large-pending-deferred; there is no small pending item left. If the user
hasn't provided a decision on one of the needs_human items (persistence,
Cloud placeability, terrain formula, chunk redesign), ask before
proceeding on any of those. If the user wants Signs/Chat, treat it as a
new dedicated task: design the text-input state machine first (how does
typing interact with WASD/mouse-look?), get that reviewed/confirmed if
it's not obvious, then implement signs before chat (signs are the smaller
of the two). Before implementing anything that cites Craft's source code,
re-verify the citation against the real checkout at
/rv/data/development/github.com/other/Craft — CRAFT_PARITY.md was
carefully audited but could still contain an error, same as every prior
citation pass in this project's history. Make one small, verified
improvement at a time: implement, build (cmake --build build-worlds, or
the full EasyGL build if it touches Render/ or CnaCraftGame), run the
relevant test/smoke command, confirm it actually passes. When finished,
update plan.md §12.1's status, update CRAFT_PARITY.md's corresponding
entry, and update this file's "Current status"/"Recent changes".
```
