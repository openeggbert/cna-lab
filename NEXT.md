# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated after this
session's work (not yet committed as of this writing) on top of commit
`1d639fb` (`Add NEXT.md handoff document`) on branch `develop`.

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
fuller Craft parity, one item at a time. `plan.md` §11.0 is the authoritative
status/priority list — keep this file in sync with it, not the other way
around.

**Key architectural decisions**: unchanged from prior sessions — see
`plan.md` §2/§6 below for the two-layer split (`Worlds/` engine-agnostic,
`Render/` + `CnaCraftGame` CNA-dependent), fixed-size world, naive per-face
meshing, backend-agnostic game code, two-mesh-buffer (opaque/transparent)
convention.

## 2. Current status

**Build status**: verified this session (EasyGL backend) — clean configure +
build, no warnings, `CnaCraft` executable links and runs. Verified via a
headless `--smoke 60` run and real interactive screenshots (Xvfb/xdotool/
ImageMagick `import`), not just compilation.

**Test status**: `tests/worlds_smoke_test.cpp` — **103 plain-assert checks,
all passing** as of the last run this session (up from 82 at the start of
this session). Builds and runs standalone with `-DCNA_CRAFT_BUILD_GAME=OFF`
(no CNA/GPU/display needed).

**What currently works** (verified via real builds + screenshots/tests, not
just code review) — everything from the previous session's list, plus this
session's additions:
- Chunked world generation (2D Simplex noise heightmap → bedrock/stone/dirt/
  grass/air columns), deterministic per seed.
- **New this session**: 3D Simplex noise (`NoiseGenerator::Simplex3`),
  independently unit tested.
- **New this session**: Cloud blocks now auto-generate in a `y=[58,62]` band
  near the world ceiling via 3D density noise, matching Craft's real
  `world.c` cloud pass (verified against the actual checkout — see §5 for
  the citation correction this uncovered).
- **New this session**: simple trees (`World::GenerateTrees`) — a 7-tall
  Wood trunk + a spherical-ish Leaves canopy blob on `simplex2(x, z, 6, 0.5,
  2) > 0.84` grass columns, matching Craft's real tree pass exactly. New
  `BlockType::Leaves` (transparent like Glass). Hotbar is now **15** slots
  (was 14).
- Naive per-face chunk meshing with correct cross-chunk-boundary face culling
  (now also correctly routing Leaves through the transparent mesh, same
  rules as Glass).
- First-person walk/strafe (WASD), jump, gravity, AABB-vs-voxel collision.
- Mouse-look **and** arrow-key look (yaw/pitch).
- Fly mode (Tab): no gravity, free vertical movement (Space/Left Ctrl).
- Hold-to-zoom (Left Shift) and orthographic projection (F).
- Block breaking/placing via DDA voxel raycast (left/right click).
- Hotbar: **15** placeable block types, keys 1–9 direct + E cycles all 15.
- Glass and Cloud transparency/collision rules (unchanged this session).
- Per-chunk frustum culling.
- Day/night cycle (10-minute period).
- Procedural texture atlas — now **19** tiles (was 18; tile 18 is Leaves).
- HUD: crosshair + large "`#4/15 Stone`"-style selected-item text (slot count
  auto-follows `Hotbar::SlotCount()`, no hardcoded string to fix).
- F12 screenshot capture to `screenshots/`.

**What does not work / is not implemented yet**:
- Greedy meshing (README's overview paragraph previously incorrectly implied
  this exists — **fixed this session**, see §3).
- Infinite/streamed world, chunk load/unload by distance.
- **Caves/overhangs**: still not implemented, and now explicitly
  `needs_human` in `plan.md` §11.0/§11.1 — the original backlog citation
  ("Craft's `world.c` combines a 2D heightmap with a 3D noise term for
  overhangs") turned out to be **wrong**: the real Craft checkout has no
  cave-carving code at all. Its only `simplex3` use is the CLOUD pass, which
  this session ported instead (see §5). Genuine cave-carving would need
  invented density thresholds with no reference to port — a subjective
  design choice, not a Craft-parity port, so it's marked `needs_human`
  rather than picked up as a "next smallest task."
- Non-cubic billboard geometry for tall grass/flowers, ambient occlusion.
- Textured sky dome, point/block lighting, signs, chat.
- Any persistence (SQLite or otherwise) — world is regenerated fresh every
  run, edits are lost on exit. `needs_human` per `missing.md` (new
  dependency decision).
- Multiplayer (no networking code at all yet) — explicitly deferred per
  project direction until local single-player + persistence are stable.
- BGFX backend: configures but the first `cmake configure` triggers a real
  network clone of `bgfx.cmake` (unpinned `GIT_TAG master`) — see
  `missing.md`. Not touched this session.

## 3. Recent changes (this session)

Not yet committed as of this writing — working tree changes on top of
`1d639fb`. Files touched: `README.md`, `plan.md`, `NEXT.md` (this file),
`src/CnaCraft/Render/TextureAtlas.cpp`, `src/CnaCraft/Worlds/BlockType.hpp`,
`src/CnaCraft/Worlds/Hotbar.hpp`, `src/CnaCraft/Worlds/NoiseGenerator.{hpp,cpp}`,
`src/CnaCraft/Worlds/World.{hpp,cpp}`, `tests/worlds_smoke_test.cpp`.

In order:

1. **Fixed the README "greedy meshing" doc inaccuracy** (`README.md` §1):
   changed "greedy meshing" to "naive per-face hidden-face-culled meshing" —
   matches what `ChunkMesher` actually does. Docs-only, no build/test needed.
2. **Synced `plan.md` with this handoff file's prior state**: added a new
   `### 11.0 Current status & priority queue` section as the authoritative
   status/priority list (status vocabulary: `pending`/`in_progress`/
   `completed`/`blocked`/`needs_human`), consolidating what was previously
   split across `plan.md`'s per-item checkboxes and this file's "Next
   smallest tasks" list.
3. **Added `NoiseGenerator::Simplex3`** (3D Simplex noise, public API,
   `NoiseGenerator.{hpp,cpp}`) — same Gustavson/caseman-noise structure as
   the existing `Simplex2`, own seeded permutation table. 4 new unit tests
   (purity, seed variation, third-axis variation, output-range sanity).
4. **Wired `Simplex3` into `World::Generate` — as Cloud generation, not
   caves.** Re-verified the "caves/overhangs" backlog citation against the
   real checkout at `/rv/data/development/github.com/other/Craft/src/world.c`
   before implementing anything: **there is no cave-carving code in Craft at
   all.** Craft's only real `simplex3` use is placing CLOUD blocks near its
   world's ceiling (`simplex3(x*0.01, y*0.1, z*0.01, 8, 0.5, 2) > 0.75`).
   Ported that real, verified feature instead (`World::GenerateClouds`,
   `World.cpp`) — same frequency/octave/threshold constants, placed in a
   `y=[58,62]` band near this project's own much-lower `WORLD_SIZE_Y=64`
   ceiling. Only fills still-Air cells, so it never overwrites terrain. 4 new
   unit tests. Verified visually against a real EasyGL build (fly mode +
   screenshot showed Cloud clusters rendered above the terrain).
5. **Added simple trees** (`World::GenerateTrees`, `World.cpp`): Craft's real
   tree pass, also verified against the actual checkout —
   `simplex2(x, z, 6, 0.5, 2) > 0.84` trigger on grass columns, 7-tall Wood
   trunk, Leaves canopy blob (`ox²+oz²+dy²<11` for `y∈[h+3,h+8)`,
   `ox,oz∈[-3,3]`). New `BlockType::Leaves` (transparent like Glass — same
   mesher routing/tile-18 atlas entry). `NoiseGenerator::Simplex2` made
   public (was private) so tree generation can reuse it with its own
   frequency/octave parameters. `Hotbar::kSlots` grew from 14 to 15 (Leaves
   appended at the end — did not reorder or remove anything, per the "don't
   break slot-index tests" rule). One deliberate deviation from Craft: canopy
   cells only fill still-Air, so one tree's canopy can't stomp a
   neighboring tree's already-placed trunk (the trunk write itself stays
   unconditional, matching Craft exactly). 6 new unit tests (tree presence,
   determinism, trunk-base-not-floating, 3 Leaves mesh-transparency-routing
   tests mirroring the existing Glass tests). Verified visually against a
   real EasyGL build (fly mode + a screenshot showed Wood trunk/branch
   geometry against green Leaves canopy, correctly meshed with visible
   faces).

Test count progression this session: 82 → 86 (Simplex3) → 90 (clouds, one
pre-existing test had to be relaxed — see §5) → 103 (trees + Leaves mesher
tests).

## 4. Current blocker / main problem

**There is no build-breaking or test-breaking blocker right now.** Last known
state: clean build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and
`-DCNA_GRAPHICS_BACKEND=EASYGL`), 103/103 tests passing.

Nothing outstanding from this session. Carried over from before (still
unresolved, still not urgent):

- **Mouse-look reliability** is still only "verified in principle" — not
  independently confirmed on the user's real (non-sandboxed) machine since
  arrow-key look was added as a fallback. Marked `needs_human` in `plan.md`
  §11.0 item 6. Not a coding task — next time you talk to the user, ask
  whether mouse-look turns the camera, and if not, whether clicking into the
  game window once after launch changes anything.

No failing command or failing test is currently known. If you find one, that
supersedes everything else in this file.

## 5. Known bugs and limitations

- **Fixed this session**: README "greedy meshing" doc inaccuracy (§3 item 1).
- **New this session — citation correction**: the "caves/overhangs" backlog
  item's citation ("Craft's `world.c` combines a 2D heightmap with a 3D noise
  term for overhangs") does not hold up against the real Craft checkout —
  there is no cave-carving code there. Corrected in `plan.md` §11.1 (the
  checkbox item now documents this and is marked `needs_human` if picked up
  again, since it would need invented, undocumented density thresholds — a
  subjective design choice, not a verifiable Craft port). Don't re-attempt
  caves as a "next smallest task" without a human decision on the design
  parameters first.
- **Fixed this session** (test-only, not a real bug): `TestWorldGeneratesClouds`'s
  "terrain unaffected" assertion originally required the surface block at
  every sampled column to be exactly `Grass` — this broke once trees were
  added, since a tree trunk legitimately overwrites the surface Grass cell
  with Wood (same as Craft's own behavior, `func(x, h, z, 5, arg)`). Relaxed
  the assertion to accept Grass OR Wood at the surface cell; this is correct
  behavior, not a bug.
- **Confirmed limitation**: no persistence. World edits (broken/placed
  blocks) are lost when the process exits; `World::Generate` always starts
  fresh.
- **Confirmed limitation**: fixed-size world only (`128×64×128`), no chunk
  streaming, no load/unload by player distance.
- **Confirmed limitation** (documented in `missing.md`): full Craft parity
  across all three backends is blocked on custom-shader support — unchanged
  this session, see `missing.md` for the full writeup. `EASYGL` remains the
  practical reference target.
- **Confirmed limitation**: `BGFX` backend's first configure does a real,
  unpinned network clone of `bgfx.cmake` — unchanged this session.
- **Needs verification**: mouse-look on the user's actual machine (§4).
- **Incomplete**: `BlockType` still has no tall grass, flowers, or Chest —
  those need non-cubic (cross-quad billboard) geometry, which `ChunkMesher`
  doesn't support yet (only axis-aligned cube faces). Leaves (cube geometry)
  is now done; billboard plants are not.
- **Risky assumption worth knowing** (carried over, reinforced this
  session): always re-verify a `plan.md`/`NEXT.md` citation about Craft's
  source against the real checkout at
  `/rv/data/development/github.com/other/Craft` before implementing —
  this session found a second wrong citation (caves/overhangs) on top of the
  ones already documented from prior sessions (`noise.c` location, `ZXCVBN`
  keys, the screenshot key, `house3d_demo.cpp`'s dead glass-pass code).

## 6. Architecture notes

**Main modules** (see `plan.md` §2/§8 for the original design writeup; only
what changed this session is detailed here — everything else is unchanged
from before):

- `Worlds/BlockType.hpp` — now **15** `BlockType` values (added `Leaves`,
  transparent like Glass, tile index 18). Still the single source of truth
  for block properties.
- `Worlds/World` — `Generate(seed)` now runs three passes in order: (1) the
  original terrain heightmap pass, (2) `GenerateTrees(seed)` (new, private),
  (3) `GenerateClouds(seed)` (new, private). Order matters: clouds only fill
  still-Air cells, so running clouds *after* trees means a rare tall tree
  reaching into the cloud band's `y=[58,62]` won't have its canopy
  overwritten by a cloud.
- `Worlds/NoiseGenerator` — `Simplex2` is now **public** (was private; still
  used internally by `Height()`, now also reused directly by
  `World::GenerateTrees`). New public `Simplex3(seed, x, y, z, octaves,
  persistence, lacunarity)` — 3D density noise, used by
  `World::GenerateClouds`.
- `Worlds/Hotbar` — `kSlots` is now **15** entries (`Leaves` appended at the
  end, index 14 / slot 15). `SlotCount()`/`kMaxNumberKeySlots` logic is
  unchanged and derives from `kSlots.size()` automatically — no hardcoded
  counts to update elsewhere in game code (`CnaCraftGame.cpp` already reads
  `Hotbar::kSlots.size()`/`SlotCount()` dynamically).
- `Render/TextureAtlas` — 19 tiles now (was 18); tile 18 is Leaves (mottled
  green, alpha ~215/255, same `Pattern::Mottle` technique as other tiles,
  just a punchier 0.30 strength for a foliage-clump look). Still well within
  the 25-slot (`kAtlasTilesPerRow=5`) grid.

**Boundaries that should not be broken** — unchanged from before, still
load-bearing:
- `Worlds/` must never `#include` anything CNA/SDL.
- Any new `BlockDef` boolean flag with a non-`false` default must be AND'd
  with `solid` wherever consumed.
- Two-mesh-buffer convention (`ChunkMeshData{opaque, transparent}`) — Leaves
  extends the existing `transparent` bucket (same as Glass), it did not need
  a third category.

**New invariant from this session**: `World::GenerateTrees` and
`World::GenerateClouds` are both private helpers called only from
`Generate()`, in that fixed order (terrain → trees → clouds). If you add
another generation pass, think about whether it should run before or after
these two — passes later in the sequence can rely on earlier passes' output
(e.g. clouds' "only fill Air" check relies on trees having already run).

**Data flow**: unchanged from before — see `plan.md` §6/`NEXT.md` (prior
session) for the full per-frame `Update`/`Draw` flow; nothing about it
changed this session, only what `World::Generate` itself does at startup.

## 7. Useful commands

Unchanged from before:

Engine-agnostic tests only (no CNA/GPU/display needed):
```bash
cmake -S . -B build-worlds -DCNA_CRAFT_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-worlds -j"$(nproc)"
./build-worlds/cna_craft_worlds_smoke_test
```
Expect: `All checks passed.` (103 `ok:` lines, 0 `FAIL:` lines as of this
writing — up from 82 at the start of this session).

Full graphical game (requires `../cna` and `../sharp-runtime` checked out as
siblings of this repo):
```bash
cmake -S . -B build-easygl -DCNA_GRAPHICS_BACKEND=EASYGL
cmake --build build-easygl --target CnaCraft -j"$(nproc)"
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft          # interactive
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft --smoke 30   # headless CI-style, exits after 30 frames
```
Swap `EASYGL` for `VULKAN` the same way. Avoid `BGFX` unless you're prepared
for a real network fetch on first configure (see `missing.md`).

There is no separate lint/format tooling configured in this repo.

Reproduce nothing currently — there is no known failing command.

## 8. Next smallest tasks

`plan.md` §11.0 is now the authoritative ordered priority queue with explicit
statuses — read it first. As of this session, queue items 1-5 there are all
`completed`, item 6 (mouse-look confirmation) is `needs_human`. The
"Larger backlog items intentionally not picked up" list in `plan.md` §11.0
has the next tier of real work; picking from there means accepting a bigger
scope than a single small task. A short candidate list, smallest/safest
first:

1. **Non-cubic plant/billboard geometry prerequisite work**: still large
   (needs a `ChunkMesher` emission-path change), not a "next smallest task"
   on its own — but a good *next* backlog item to scope down into something
   smaller (e.g. just the `MeshData`/`ChunkMesher` plumbing for a
   cross-quad billboard shape, without wiring actual plant placement yet).
2. **Extend `tests/worlds_smoke_test.cpp` test coverage** for existing
   untested edge cases as they're noticed (plan.md §11.8) — always safe,
   always small, no design decision required.
3. Anything else in `plan.md` §11.0's "larger backlog items" list — each
   needs either a bigger implementation effort or a human decision first;
   don't start without re-reading that section's specific status tag.

Do not start on SQLite persistence, multiplayer, AO, greedy meshing, caves
(now `needs_human` — see §5), or the sky dome/shader work as a "next
smallest task" — they're all real backlog items but none of them are small,
and several need an explicit human decision first (see `plan.md` §11.0).

## 9. Do not do yet

Unchanged from before, still applies — **plus**:

- **No genuine cave/overhang carving** without a human decision on the
  density-threshold/shape parameters first (§5 — the original "port Craft's
  algorithm" framing turned out to have no real algorithm to port).
- **No further `Hotbar::kSlots` reordering** — it's now 15 entries; `Leaves`
  was appended at the end specifically so no existing slot index shifted.
  Follow the same pattern (append, don't insert) for any future block type
  added to the hotbar.
- **No renaming/removing `NoiseGenerator::Simplex2`'s new public visibility**
  without checking `World::GenerateTrees`'s call site — it was private
  before this session, now public and directly depended on outside
  `NoiseGenerator` itself.

All the pre-existing items still apply: no SQLite without go-ahead, no
multiplayer/networking work, no AO/greedy-meshing rewrite, no sky dome/
custom shader work, no broad `Worlds/`/`Render/` refactor, no
`PlayerController` public-API changes without re-running the full suite.

## 10. Resume prompt

```
Read NEXT.md first, then plan.md §11.0 (the authoritative status/priority
queue). Pick the first pending, non-blocked, non-needs_human task there (or
a user-reported bug if one exists — that always takes priority). Before
implementing anything that cites Craft's source code, re-verify the
citation against the real checkout at
/rv/data/development/github.com/other/Craft rather than trusting it blindly
— this session found a second wrong citation (caves/overhangs) on top of
several from before. Inspect only the files the task names — do not
refactor unrelated code, and do not touch anything listed under "Do not do
yet" in either plan.md or this file. Make one small, verified improvement:
implement the task, build it (cmake --build build-worlds, or the full
EasyGL build if it touches Render/ or CnaCraftGame), run the relevant
test/smoke command, and confirm it actually passes before considering the
task done. When finished, update plan.md §11.0's status for that item, and
update this file's "Current status"/"Recent changes" with what actually
changed, noting any new bugs or open questions under "Known bugs and
limitations".
```
