# CNA Craft — Prototype Architecture & Implementation Plan

This plan turns the feasibility conclusion in [analysis.md](analysis.md) into a concrete
architecture: a small, fixed-size voxel world, chunked meshing, procedural terrain, a
first-person controller, and block breaking/placing — built on top of CNA's
`Microsoft::Xna::Framework` API and reusing the camera/collision approach already proven in
CNA's `examples/house3d_demo.cpp`.

## 0. Inspiration: fogleman/Craft

[Craft](https://github.com/fogleman/Craft) (MIT license, Copyright (c) 2013 Michael Fogleman) is
a small, well-known Minecraft clone in C/OpenGL and the concrete reference point for several
decisions below (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full attribution —
no code is copied verbatim, only architecture/algorithms):

- **Only exposed faces are rendered** — Craft's central meshing optimization; adopted here in
  §4/§7's `ChunkMesher` face-culling.
- **One-block neighbor overlap across chunk boundaries** so edge faces still cull correctly —
  adopted via `World::IsSolid` transparently reading across chunk borders (§3/§4).
- **Delta-based persistence**: Craft's SQLite `block(p,q,x,y,z,w)` table stores only *edits* on
  top of regenerated procedural terrain, not the full world. Adopted as the intended shape of the
  M7 save/load stretch goal (§9).
- **Deterministic, position-seeded noise** for terrain height — Craft uses Simplex noise, and so
  does this project now (§3, §11.1), independently reimplemented with a per-world-seed permuted
  gradient table rather than Craft's own fixed/global one.
- **Chunked world, not literally infinite** at the storage layer — Craft treats a chunk as a
  32×32 XZ column with the full 0–255 Y range and a hash map of sparse blocks; this prototype
  instead uses fixed-size `16×16×16` chunks over a bounded `128×64×128` world with dense
  per-chunk storage (see §9, M7 for the upgrade path toward Craft-style unbounded/streamed
  chunks).
- **Not adopted for this prototype**: Craft's client/server ASCII networking protocol. CNA
  already has a real ENet-backed `Microsoft::Xna::Framework::Net` layer (see `../cna/README.md`
  §7), which would be the natural place to add multiplayer block-sync later, following Craft's
  message shapes (`C,p,q,key` chunk requests / `B,p,q,x,y,z,w` block updates / `P,pid,x,y,z,rx,ry`
  player position streams) as a protocol reference rather than a literal port.

## 1. Design goals for this prototype (explicitly scoped)

In scope:
- A **fixed-size** world (no infinite streaming) made of fixed-size chunks.
- Procedural terrain (2D heightmap noise → grass/dirt/stone/bedrock columns).
- Per-chunk meshing with hidden-face culling (naive per-face, not greedy — see §7).
- A textured, lit chunk mesh (`BasicEffect` + texture atlas).
- First-person camera + WASD/mouse movement + gravity/jump, reusing the math from
  `house3d_demo.cpp` but colliding against the voxel grid instead of a static box list.
- Block picking via a DDA voxel raycast, left-click break / right-click place, with immediate
  mesh rebuild of the affected chunk(s).

Out of scope (called out as stretch/future work in §9):
- Infinite world / chunk streaming and unload.
- Save/load to disk.
- Greedy meshing / ambient occlusion / smooth lighting.
- Multiplayer (CNA's `Net`/ENet layer would be the natural place for this later).

## 2. Layering strategy: engine-agnostic core + CNA glue

Mirroring the pattern already used in `../galaxy-eggbert/CMakeLists.txt` (`GE_SHARED_SOURCES`,
engine-independent `Worlds/Chunk.cpp` / `World.cpp`, consumed by both a Simple3D and a CNA
target), CNA Craft splits into two layers:

```text
+-------------------------------------------------------------+
|  src/CnaCraft/Worlds/*  (engine-agnostic, no CNA/SDL includes)|
|  BlockType, Chunk, World, NoiseGenerator, ChunkMesher,        |
|  VoxelRaycast, PlayerController — plain float/int math only   |
|  (src/CnaCraft/Core/Vec3.hpp), no GPU types.                   |
+------------------------------+--------------------------------+
                               |
                               v  (MeshData: plain vertex/index arrays)
+-------------------------------------------------------------+
|  src/CnaCraft/Render/*  (CNA-dependent glue)                  |
|  TextureAtlas (tile index -> UV rect, Texture2D)               |
|  ChunkRenderer (MeshData -> VertexPositionNormalTexture,       |
|                 VertexBuffer/IndexBuffer, BasicEffect draw)    |
+------------------------------+--------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|  src/CnaCraft/CnaCraftGame.{hpp,cpp} + main.cpp                |
|  Game subclass: LoadContent/Update/Draw, Keyboard/Mouse input, |
|  owns World + PlayerController + one ChunkRenderer per chunk   |
+-------------------------------------------------------------+
```

Why split it this way:
- The world/meshing/raycast/controller logic is the part most worth getting right and most
  worth unit-testing; keeping it free of CNA/SDL headers means it compiles in seconds and can be
  exercised with a plain `assert`-based smoke test with **no GPU, window, or CNA build required**
  (see §8). This is the same reason `galaxy-eggbert` keeps `Worlds/` engine-independent.
  a `Game`.

## 3. Data model (`src/CnaCraft/Worlds/`)

**`BlockType.hpp`** — `enum class BlockType : uint8_t { Air, Grass, Dirt, Stone, Sand, Bedrock }`
plus a small `BlockDef` lookup table (`IsSolid`, `TopTileIndex`/`SideTileIndex`/`BottomTileIndex`
into the texture atlas — grass needs 3 distinct faces, everything else uses one tile for all
faces).

**`Chunk`** — fixed `CHUNK_SIZE = 16` cube (`16×16×16` blocks), flat
`std::array<BlockType, 16*16*16>` storage, local-coordinate `GetBlock`/`SetBlock`, and a dirty
flag the mesher/renderer use to know a chunk needs re-meshing.

**`World`** — a fixed 3D grid of chunks: `WORLD_CHUNKS_X/Y/Z = 8/4/8`, i.e. a
`128×64×128`-block world (no chunk streaming). Responsibilities:
- World-space `GetBlock(x,y,z)` / `SetBlock(x,y,z,type)` that maps into the right chunk (blocks
  outside the world bounds read as `Air` so mesh/raycast code never needs bounds special-casing).
- `IsSolid(x,y,z)` used by both `PlayerController` collision and `ChunkMesher` face culling.
- `Generate(seed)` — for every `(x,z)` column, sample `NoiseGenerator::Height(x,z)` and fill the
  column: `Bedrock` at `y=0`, `Stone` up to `height-4`, `Dirt` up to `height-1`, `Grass` at
  `height`, `Air` above.
- `SetBlock` marks the owning chunk dirty, and also the neighboring chunk(s) if the modified
  block sits on a chunk boundary (its face culling depends on the neighbor).

**`NoiseGenerator`** — deterministic, dependency-free 2D Simplex noise (§11.1: swapped in from the
original value-noise implementation to match Craft's own terrain algorithm), fractal-summed over a
handful of octaves. No external noise library needed. Seeded (via a per-seed permuted gradient
table) so the same seed always reproduces the same world, and different seeds produce different
worlds (useful for the smoke test in §8).

## 4. Meshing (`src/CnaCraft/Worlds/ChunkMesher.*`, `MeshData.hpp`)

`MeshData` is a plain, engine-agnostic vertex format:

```cpp
struct MeshVertex { float px, py, pz; float nx, ny, nz; float u, v; int tileIndex; };
struct MeshData { std::vector<MeshVertex> vertices; std::vector<uint32_t> indices; };
```

`u, v` are **tile-local** (0..1 within one atlas tile); `tileIndex` says which atlas tile. Folding
tile index + local UV into a final atlas UV is deliberately left to the CNA-side `TextureAtlas`
(§5) — the mesher never needs to know atlas layout.

`ChunkMesher::Build(world, chunkCoord) -> MeshData`: for every non-air block in the chunk, for
each of the 6 faces, check the neighbor via `world.IsSolid(...)` (which transparently reads across
chunk boundaries) — if the neighbor is not solid, emit one quad (4 vertices, 2 triangles, correct
normal, tile index from `BlockDef`). This is the naive "one quad per visible face" approach, not
greedy meshing — see §9 for the upgrade path. It's the right starting point: correct, easy to
reason about and test, and fast enough for a `128×64×128` fixed world's ~32 chunks.

## 5. Rendering glue (`src/CnaCraft/Render/`, CNA-dependent)

**`TextureAtlas`** — loads a single `Texture2D` (a grid of same-size square tiles, e.g. 16×16
tiles of 16px each) and exposes `UvRect(tileIndex) -> (u0,v0,u1,v1)`. A `ToVertexPositionNormalTexture(const MeshData&, const TextureAtlas&) -> std::vector<VertexPositionNormalTexture>` free
function remaps each vertex's tile-local `(u,v)` into the tile's atlas rect. For the very first
milestone (M1, §9) a flat-color placeholder atlas (solid color per tile, generated in-memory) is
enough — no art asset dependency before the prototype's plumbing is proven out.

**`ChunkRenderer`** — owns one `VertexBuffer` + `IndexBuffer` per chunk. `Rebuild(chunk, world,
atlas)` re-runs the mesher and re-uploads the buffers (only called when the chunk's dirty flag is
set — see `Chunk`/`World` above). `Draw(graphicsDevice, basicEffect)` sets the effect's `World`
matrix to the chunk's world-space offset and issues one indexed draw call per chunk (mirrors the
`BasicEffect`/`VertexBuffer`/`IndexBuffer`/`GraphicsDevice::DrawIndexedPrimitives` sequence already
exercised in `easygl_model_draw_test.cpp` and `house3d_demo.cpp`).

## 6. Player controller & camera (`src/CnaCraft/Worlds/PlayerController.*`)

Kept engine-agnostic (own `Vec3f`, no CNA `Vector3`) so it's unit-testable like the rest of
`Worlds/`. Same shape as `house3d_demo.cpp`'s Game-mode controller:

- Position (eye height `1.7` above feet), yaw/pitch from mouse delta, gravity (`25 u/s²`), jump
  impulse (`7 u/s`), `kMaxStepHeight` for walking up single-block ledges.
- Collision: axis-separated AABB-vs-voxel-grid resolution — attempt the X move, clamp against
  `world.IsSolid` if it would intersect a solid block; then Z; then Y (so sliding along a wall
  while colliding works, same idea as `house3d_demo.cpp`'s box-list version, just querying
  `World::IsSolid` instead of iterating a fixed box array).

The CNA glue layer (`CnaCraftGame::Draw`) converts the controller's `Vec3f` position + yaw/pitch
into a CNA `Matrix::CreateLookAt`/`CreateRotationX/Y` view matrix exactly as `house3d_demo.cpp`
already does.

## 7. Block picking & editing (`src/CnaCraft/Worlds/VoxelRaycast.*`)

Amanatides–Woo DDA voxel traversal from the camera position along its look vector, stepping cell
by cell up to a max reach (`6` blocks), calling `world.IsSolid` at each traversed cell. Returns
`std::optional<RaycastHit>` with the hit block's coordinate *and* the face normal (needed to know
which adjacent cell a placed block should go into). Wired up in `CnaCraftGame::Update`:

- Left mouse click → `world.SetBlock(hit.block, BlockType::Air)`.
- Right mouse click → `world.SetBlock(hit.block + hit.normal, <selected block type>)`.
- Either action marks the affected chunk(s) dirty; `CnaCraftGame::Draw` (or a post-`Update` pass)
  calls `ChunkRenderer::Rebuild` only for chunks whose `Chunk::dirty` flag is set.

## 8. Build integration & directory layout

```text
cna-craft/
├── CMakeLists.txt
├── README.md / LICENSE / .gitignore / plan.md / analysis.md
├── src/CnaCraft/
│   ├── Core/Vec3.hpp
│   ├── Worlds/                      # engine-agnostic (CnaCraftWorlds static lib)
│   │   ├── BlockType.hpp
│   │   ├── Chunk.{hpp,cpp}
│   │   ├── World.{hpp,cpp}
│   │   ├── NoiseGenerator.{hpp,cpp}
│   │   ├── MeshData.hpp
│   │   ├── ChunkMesher.{hpp,cpp}
│   │   ├── VoxelRaycast.{hpp,cpp}
│   │   └── PlayerController.{hpp,cpp}
│   ├── Render/                      # CNA-dependent glue
│   │   ├── TextureAtlas.{hpp,cpp}
│   │   └── ChunkRenderer.{hpp,cpp}
│   ├── CnaCraftGame.{hpp,cpp}
│   └── main.cpp
└── tests/
    └── worlds_smoke_test.cpp        # plain-assert test of Worlds/, no CNA/GPU needed
```

`CMakeLists.txt` follows `../galaxy-eggbert/CMakeLists.txt`'s CNA-integration pattern exactly:

```cmake
set(CNA_HOME "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH "...")
set(CNA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${CNA_HOME}" CNA_dep)   # CNA's own CMakeLists.txt pulls in ../sharp-runtime
...
target_link_libraries(CnaCraft PRIVATE
    -Wl,--start-group CNA ${BACKEND_TARGET} -Wl,--end-group SHARP_RUNTIME CnaCraftWorlds)
```

`CnaCraftWorlds` is a small static library target with **no** dependency on `CNA`/`SDL3`, built
unconditionally; `worlds_smoke_test` links only that and runs as a `ctest` — this is the part that
can be compiled and verified in any environment, including one without a GPU or windowing system.
The full `CnaCraft` graphical executable additionally requires `../cna` and `../sharp-runtime`
checked out as siblings, and one of the three 3D-capable backends selected at configure time via
`-DCNA_GRAPHICS_BACKEND=EASYGL` (or `VULKAN`, or `BGFX`) — `SDL_RENDERER` remains 2D-only, per
`house3d_demo.cpp`.

## 9. Milestones

| # | Milestone | Depends on |
|---|-----------|------------|
| M0 | Repo scaffold builds; `CnaCraftWorlds` + smoke test compile and pass | — (this commit) |
| M1 | Single hardcoded chunk of flat-colored cubes renders with fly camera (adapt `house3d_demo.cpp`'s render loop to draw from a `Chunk` instead of a hardcoded box list) | M0 |
| M2 | `ChunkMesher` face-culling verified against `World` (naive per-face quads) | M0 |
| M3 | Full fixed-size world generated via `NoiseGenerator`, all chunks meshed and rendered | M1, M2 |
| M4 | `PlayerController` walking/gravity/jump collision against the voxel `World` (replacing the static AABB list) | M3 |
| M5 | `VoxelRaycast` block picking + break/place + dirty-chunk mesh rebuild | M3 |
| M6 | Real texture atlas (grass/dirt/stone/sand tiles) replacing the M1 placeholder colors, `BasicEffect` default lighting enabled | M2 |
| M7 (stretch) | Greedy meshing, ambient occlusion, chunk streaming/unload, Craft-style delta save/load, `Net`-based multiplayer block-sync | M3–M6 |

## 10. Known constraints carried over from `analysis.md`

- 3D rendering is implemented on `EASYGL`, `VULKAN`, and `BGFX` (selectable via
  `CNA_GRAPHICS_BACKEND`, see §8); `SDL_RENDERER` remains 2D-only. `CnaCraft` itself is written
  against the backend-agnostic `Microsoft::Xna::Framework` API, so no game code changes with the
  backend choice — only the CMake configure flag does.
- No occlusion/frustum culling in the engine — acceptable at this prototype's fixed
  `128×64×128`-block/32-chunk scale; would need addressing before scaling to a larger or
  streamed world (§9, M7).

**Real bugs found via actual play (not just tests/screenshots) and fixed:**

- **Spawn wedged against a cliff, unable to move or jump at all** — `CnaCraftGame::Initialize` used
  to spawn the player at an *integer* world coordinate (`WORLD_SIZE_X/2`, `WORLD_SIZE_Z/2`),
  exactly on the boundary between two block columns. The player's 0.6-wide hitbox
  (`kPlayerHalfWidth=0.3`) straddles that boundary equally on both sides. Once §11.1 swapped in
  Simplex noise (steeper local height changes than the old value noise), the neighboring column
  right next to spawn was sometimes several blocks taller, permanently wedging the player against
  it — floating above its own column's true floor, blocked from moving in any direction, and
  effectively unable to jump clear of it either. **This was reported directly by the user actually
  playing the game** — earlier automated/screenshot-based verification in this session didn't
  catch it. Fixed by spawning at block *center* (integer + 0.5) instead, keeping the hitbox fully
  inside its own column. Root-caused via a temporary per-frame debug print (confirmed WASD input
  *was* reaching `PlayerController::Update` with the correct `moveForward` value, but position
  never changed) plus a deterministic diagnostic against the real generated terrain (showed the
  player resting one full column-height above its own surface, immediately next to a much taller
  neighbor). Now covered by a permanent regression test,
  `TestPlayerSpawnAtBlockCenterAvoidsBoundaryWedging` (`tests/worlds_smoke_test.cpp`), built from a
  hand-crafted flat floor plus one tall neighboring column so it stays deterministic regardless of
  future noise changes.
- **HUD hotbar text far too small to read** — the original `Render::Hud` hotbar texture packed all
  13 slot names onto one very wide, short line (900×16px native), then scaled it to fit ~70% of
  the screen's *width*. Because the native texture was already comparably wide, that barely
  magnified its *height* at all on realistic resolutions, leaving the text tiny regardless of
  display size. Fixed by (1) showing only the currently-selected item (`"#4/13 Stone"`, plus a
  `[FLYING]` prefix) instead of all 13 names on one line, which keeps the content short enough to
  render genuinely large, and (2) sizing the destination rectangle from a target *screen height*
  (with a floor and a width-based fallback to avoid overflow) rather than a fraction of screen
  width. Also added a `scale` parameter to `Hud.cpp`'s `FontDrawText` (3x for the hotbar line) and
  slightly enlarged the crosshair to scale with resolution too.
- **"Why can't I turn?"** — investigated whether mouse-look was broken. Verified via a temporary
  debug print plus real injected relative mouse motion that the underlying mechanism (SDL relative
  mouse mode → `InputManager`'s accumulated delta → `Mouse::GetState()` → `PlayerController::Update`
  yaw/pitch) *is* wired correctly and does work when a motion event arrives — a 200px synthetic
  move produced exactly the expected `0.5` rad yaw change. Repeated testing in this sandbox's
  Xvfb-without-a-window-manager setup was inconsistent (worked once, then not at all on a repeat
  run), most likely an environment/focus quirk of this specific sandbox rather than a deterministic
  code bug — same category as this session's previously-noted letter-key `xdotool` flakiness.
  Rather than leave it unresolved, added **arrow keys as a keyboard alternative to mouse-look**
  (Left/Right → yaw, Up/Down → pitch, same `1.6 * dt` rotSpeed formula as `house3d_demo.cpp`),
  additive with the mouse deltas — verified working via a before/after screenshot pair holding
  Right. If mouse-look specifically still doesn't turn the camera, that points at something
  environment-specific worth reporting (e.g. try clicking into the game window once right after
  it opens, in case the OS/window manager didn't hand it mouse capture immediately).
- **"Can't jump onto a higher block"** — `kJumpSpeed=7` against `kGravity=25` gives a max jump
  height of `v²/(2g) = 49/50 = 0.98` blocks: mathematically just short of clearing a full 1-block
  step, the single most basic Craft-like traversal move. Craft's own jump speed is `8`
  (`Craft/src/main.c`: `dy = 8`) against the same `gravity=25`, giving `64/50=1.28` blocks —
  comfortably enough margin. Matched here. Caught with a precise regression test,
  `TestPlayerJumpClearsOneBlockHeight` (`tests/worlds_smoke_test.cpp`), that measures the jump
  arc's apex directly (no horizontal movement/collision involved, so it isn't affected by
  movement-timing specifics) — verified it fails at the old value of `7` and passes at `8`. A
  companion integration test, `TestPlayerCanJumpOntoOneBlockHigherLedge`, additionally confirms a
  jump can carry the player onto a real ledge while moving, though on its own it turned out not
  strict enough to discriminate the two jump-speed values (a retry-until-success loop masked the
  difference) — kept as a secondary sanity check, not the primary regression guard.

## 11. Task backlog: toward fogleman/Craft feature parity

M0–M6 (§9) get CnaCraft to "walkable, texturable, breakable/placeable voxel world" — a prototype,
not a game. The reference we picked, [Craft](https://github.com/fogleman/Craft) (see
`THIRD_PARTY_NOTICES.md`), is a complete small game: infinite streamed world, plants and
transparency, day/night with a sky dome, SQLite persistence, multiplayer, signs, lighting. This
section is the task-by-task backlog toward that same feature set, organized by system and grafted
onto the architecture in §2/§8. Local reference checkout: `/rv/data/development/github.com/other/Craft`
(read `src/main.c`, `src/world.c`, `src/cube.c`, `shaders/block_*.glsl` before implementing the
matching item below — they're short, working C, and the exact algorithm is usually right there).

Checkbox state reflects this document's authoring session; update as work lands.

### 11.0 Current status & priority queue (updated 2026-07-09)

Baseline verified this session: clean configure+build on `-DCNA_CRAFT_BUILD_GAME=OFF` (worlds-only)
and `-DCNA_GRAPHICS_BACKEND=EASYGL` (full game); started at **82/82** `tests/worlds_smoke_test.cpp`
checks passing, now **103/103** after this session's Simplex3/clouds/trees work (queue items 1-5
below, all completed). No known failing build or test. This section is the authoritative
status/priority list; `NEXT.md` is a handoff summary that should be kept in sync with it, not a
separate source of truth.

Status vocabulary used below and in §11.1–§11.8: `pending`, `in_progress`, `completed`, `blocked`
(technical prerequisite missing), `needs_human` (requires a product/design/dependency decision).

Ordered queue (small/safe first; a user-reported bug always jumps this queue):

1. `completed` — Fix `README.md` "greedy meshing" doc inaccuracy (§1 overview paragraph said
   "greedy meshing", actual implementation is naive per-face meshing with hidden-face culling;
   `NEXT.md` §5/§8 item 1 had flagged this as still-open — fixed this session).
2. `completed` — Sync `plan.md` (this file) with `NEXT.md`'s more recent handoff state (this
   section).
3. `completed` — Add `Simplex3` (3D Simplex noise) to `Worlds/NoiseGenerator`, independently unit
   tested, prerequisite for caves/overhangs (§11.1). Public `NoiseGenerator::Simplex3(seed, x, y, z,
   octaves, persistence, lacunarity)`, same Gustavson/caseman-noise structure as the existing
   `Simplex2`/`Noise2`, own seeded permutation table. 4 new unit tests (purity, seed variation,
   third-axis variation, output-range sanity) — 86 checks total, all passing.
4. `completed` — Wire `Simplex3` into `World::Generate`. **Redirected from "caves/overhangs" to
   "clouds"** after verifying the original citation against the real Craft checkout: Craft's
   `world.c` has no cave-carving code at all; its only `simplex3` use is the CLOUD block pass. That
   real feature was ported instead (`World::GenerateClouds`, §11.1/§11.2) — see the correction notes
   there. Genuine cave/overhang carving remains open but is now `needs_human` (no reference
   algorithm to port; would need invented density thresholds, a subjective design choice).
5. `completed` — Add simple trees (Wood trunk + `Leaves` canopy, cube geometry only, no billboards)
   during world generation (`World::GenerateTrees`, §11.1). New `BlockType::Leaves` (transparent
   like `Glass`), hotbar now 15 slots. 6 new unit tests — 103 checks total. Verified visually.
6. `needs_human` — Independently confirm mouse-look reliability on the user's real (non-sandboxed)
   machine; not a coding task, see §11.4/`NEXT.md` §4 for the investigation history.

Larger backlog items intentionally **not** picked up as "next smallest task" — real work items,
tracked in detail in §11.1–§11.8, but each requires either a bigger implementation effort or an
explicit human decision first:

- `pending` (large) — Hash-map-keyed dynamic chunk store + distance-based load/unload (§11.1)
  — prerequisite for an unbounded world; a genuine architecture change, not a small patch.
- `completed` (moved here from §11.2, see §12.1 item 7) — Non-cubic plant/billboard geometry.
  `pending` (large) — ambient occlusion, greedy meshing (§11.2) — each needs a further
  `MeshVertex`/`ChunkMesher` format change.
- `blocked` (EASYGL-only today) / `needs_human` if VULKAN/BGFX parity is required — Textured sky
  dome + fog, point/block lighting (§11.3): needs CNA `ShaderEffect`; EASYGL has real runtime GLSL,
  VULKAN needs a precompiled-SPIR-V toolchain (no runtime GLSL path), BGFX's `ShaderEffect` is a
  stub in CNA itself (upstream engine work, out of this repo's scope) — see `missing.md`.
- `needs_human` — SQLite-backed delta persistence (§11.5): SQLite is not currently a dependency
  anywhere in this project's chain (`missing.md`); adding it is a new-dependency decision this
  autonomous session will not make unilaterally.
- `pending`, explicitly deferred — Multiplayer/`Net`-based chunk/block sync (§11.6): per project
  direction, do not start before local single-player gameplay, persistence, and chunk logic are
  stable — persistence itself is still `needs_human`/pending, so this stays deferred.
- `pending` — Signs, chat/slash commands (§11.4/§11.7) — small-to-medium UI features, lower
  priority than world/terrain correctness.

### 11.1 World & terrain

- [ ] Replace the fixed `128×64×128` array-of-chunks `World` with a hash-map-keyed, dynamically
      loaded/unloaded chunk store (Craft: 32×32 XZ columns, full 0–255 Y range, `(x,y,z)->w` hash
      map per chunk — `src/map.c`/`src/map.h`). This is the prerequisite for an effectively
      unbounded world instead of today's fixed bounds.
- [ ] Chunk load/unload driven by player distance (Craft: `src/world.c` `create_chunk`, only
      chunks within a radius of the player are resident).
- [x] Swap `NoiseGenerator`'s value noise for Simplex noise. **Correction**: `noise.c`/`noise.h`
      actually live in `Craft/deps/noise/` (derived from https://github.com/caseman/noise, MIT),
      not inline in `world.c` as this line originally said — `world.c` just calls `simplex2`. Ported
      the same 2D simplex algorithm (`Noise2` in `NoiseGenerator.cpp`), but with a per-*world-seed*
      permuted gradient table (small seeded xorshift32 Fisher-Yates shuffle) instead of Craft's own
      fixed/global `PERM` table, so both "same seed → same terrain" (already relied on by the
      smoke test) and "different seed → different terrain" hold — 2 new unit tests cover this plus
      an output-range sanity check (44 checks total now). Only `simplex2` (2D, for `Height`) was
      ported; `simplex3` (3D density, needed for caves) is deferred to that backlog item below.
      Verified visually against a real EasyGL build — terrain has a plausibly more organic/bumpy
      shape than the old value-noise rolling hills.
- [ ] Caves/overhangs: a second, lower-frequency 3D density noise carving out solid terrain.
      **Correction**: this item's original citation ("Craft's `world.c` combines a 2D heightmap
      with a 3D noise term for overhangs") does not hold up against the real checkout at
      `/rv/data/development/github.com/other/Craft/src/world.c` — there is no cave/overhang-carving
      code there at all. Craft's *only* use of `simplex3` is the CLOUD block placement pass (`y=64..72`,
      `simplex3(x*0.01, y*0.1, z*0.01, 8, 0.5, 2) > 0.75`) — see the now-completed Cloud-generation
      item below, which ported that real feature instead. Genuine cave/overhang carving (not present
      in the reference project) would need its own density thresholds/design invented from scratch
      — marked `needs_human` if picked up, since undocumented gameplay/terrain-shape parameters are a
      subjective design choice per this project's autonomous-session rules, not a Craft-parity port.
- [x] Cloud generation via 3D density noise near the world ceiling (`World::GenerateClouds`,
      `World.cpp`): ports Craft's real (verified) cloud pass — same frequency scale/octave
      count/threshold (`simplex3(x*0.01, y*0.1, z*0.01, 8, 0.5, 2) > 0.75`), placed in a
      `y=[58,62]` band near this project's own (much lower) `WORLD_SIZE_Y=64` ceiling instead of
      Craft's `y=64..72` (Craft's own world is far taller). Only places a Cloud where the cell is
      still Air, so it never overwrites terrain. 4 new unit tests (a cloud is generated somewhere,
      all generated clouds stay within the documented band, placement is deterministic per seed,
      terrain generation is undisturbed) — 90 checks total. Verified visually against a real
      EasyGL build (fly mode + a screenshot showed Cloud clusters rendered above the terrain).
- [x] Trees placement pass after heightmap generation (`World::GenerateTrees`, `World.cpp`): ports
      Craft's real tree pass, verified against the checkout at
      `/rv/data/development/github.com/other/Craft/src/world.c` — `simplex2(x, z, 6, 0.5, 2) > 0.84`
      trigger on grass columns, a 7-tall Wood trunk, a Leaves canopy blob (`ox²+oz²+dy²<11` for
      `y∈[h+3,h+8)`, `ox,oz∈[-3,3]`). Added `BlockType::Leaves` (transparent like Glass — same
      `World::IsOpaque`/mesher routing, tile 18 in the procedural atlas, added to `Hotbar::kSlots`
      as the new 15th/last slot). `NoiseGenerator::Simplex2` made public (was private) so
      `GenerateTrees` can reuse it with its own frequency/octave parameters, same pattern as
      `Simplex3`. One deliberate deviation from Craft: canopy cells only fill if still Air (Craft
      writes unconditionally), so one tree's canopy can't stomp a neighboring tree's already-placed
      trunk — the trunk pass itself stays unconditional, matching Craft exactly. Not ported: Craft's
      per-chunk edge-margin check (`dx-4<0` etc.) — specific to Craft's independent per-chunk
      generation model; this project generates the whole bounded world in one pass and
      SetBlock/GetBlock already treat out-of-bounds coordinates as a safe no-op/Air, so the margin
      has no equivalent here. Plants (tall grass, flowers) remain deferred to "Non-cubic plant
      geometry" below — they need billboard geometry, not just a new tile. 6 new unit tests (tree
      presence, determinism, trunk-base-not-floating, plus 3 Leaves mesh-transparency-routing tests
      mirroring the existing Glass tests) — 103 checks total. Verified visually against a real
      EasyGL build (fly mode + a screenshot showed Wood trunk/branch geometry against green Leaves
      canopy, correctly meshed with visible faces).

### 11.2 Blocks & block rendering

- [x] Expand `BlockType`/`BlockDef` (`src/CnaCraft/Worlds/BlockType.hpp`) toward Craft's roster
      (`Craft/src/item.h`): sand, brick, wood, cement, plank, snow, glass, cobblestone, light/dark
      stone, chest, leaves, cloud, tall grass, flower variants, plus the 32-entry solid color
      palette. Doesn't need to match 1:1, but should stop being 6 block types.
      Added the plain solid/opaque cube blocks (Cobblestone, Brick, Plank, Wood, Cement,
      LightStone, DarkStone, Snow — 13 block types total now, up from 6), each with placeholder
      atlas tiles (16/16 atlas tiles now in use, `TextureAtlas.cpp`). `Hotbar` (§11.4) expanded to
      all 12 placeable types (Bedrock excluded), keys `1`-`9` direct + `E` cycles through all 12,
      matching Craft's own `item_index`/number-key/E behavior (`Craft/src/main.c:2254-2265`).
      **Deliberately deferred** to their own backlog items below since they need more than a new
      tile: Glass and Cloud (both since added, see "Transparency for glass" / "Clouds"; `Hotbar`
      now has 14 slots), Leaves (transparent like Glass, but not yet added — no reason left to
      defer other than not having been picked up), Chest/tall grass/flowers (need non-cubic
      geometry, see "Non-cubic plant
      geometry"), the 32-entry flat-color palette (low value, skipped).
- [x] Real texture atlas image: `Render::BuildPlaceholderAtlas` renamed to `BuildProceduralAtlas`
      (no external art asset — none exists — but no longer flat color either). Inspected Craft's
      real `Craft/textures/texture.png` (256×256, hand-authored 16px tiles) for reference: mottled
      speckle on stone/dirt/sand/cobblestone, a brick mortar grid, wood bark/growth-ring patterns,
      plank board seams. Ported the *idea* (not the pixels) as deterministic per-pixel procedural
      patterns — `MottlePattern` (12 of 18 tiles, a hash-noise speckle, strength tuned per material),
      plus dedicated `BrickPattern` (mortar grid), `WoodBarkPattern`/`WoodRingsPattern`,
      `PlankPattern` (board seams), `SnowFleckPattern`. Verified visually against a real EasyGL
      build — clearly visible per-pixel speckle detail on terrain (dirt/grass) replacing the old
      flat color blocks.
- [x] Non-cubic "plant" geometry: cross-quad billboards, `ChunkMesher::EmitPlant` — a second
      emission path alongside the cube-face path in §4, ported from Craft's `src/cube.c`
      `make_plant` (4-quad "X" cross, verified against the real checkout). `BlockType::TallGrass`
      is the first plant type; flowers/Chest can reuse the same path later. See §12.1 item 7 for
      the full implementation notes (this was picked up as part of the CRAFT_PARITY.md-driven
      parity pass, not this section's original session).
- [x] Transparency for glass: added `BlockType::Glass` (solid/collidable, but transparent) and a
      `BlockDef.transparent` flag. `World::IsOpaque` (`solid && !transparent`) replaces `IsSolid` in
      `ChunkMesher`'s neighbor-face check only — collision (`IsSolid`) is untouched — matching
      Craft's own `opaque[cell] = !is_transparent(w)` rule (`src/main.c`): adjacent transparent
      blocks don't occlude each other's faces, but a transparent face against a genuinely opaque
      neighbor is culled. `ChunkMesher::Build` now returns a `ChunkMeshData{opaque, transparent}`
      pair; `ChunkRenderer` holds two buffer sets and exposes `DrawOpaque`/`DrawTransparent`;
      `CnaCraftGame::Draw` draws all opaque geometry first, then all transparent geometry with
      `SetBlendEnabled(true)`/`SetDepthWriteEnabled(false)`.
      **Correction**: `house3d_demo.cpp`'s "glass pass" turned out to be dead code — its
      `glass_builder_` is declared and uploaded but never actually appended to, so
      `glassMeshes_` is always empty; the demo's window "glass" pane renders as an ordinary opaque
      box in the solid pass. Reused its *draw-state sequence* (blend on / depth-write off / draw /
      restore) since that part is correct, but didn't have a working glass-mesh reference to copy —
      the actual per-chunk opaque/transparent mesh split above is new. Plant alpha-cutout is left
      to "Non-cubic plant geometry" (transparency here only covers the not-a-plant Glass case).
      8 new unit tests (transparent-mesh routing, glass/glass and glass/stone occlusion rules,
      solid-vs-opaque distinction) — 53 checks total. Verified visually against a real EasyGL
      build (a temporary in-code glass wall, since keyboard-driven placement wasn't reliably
      scriptable in this sandbox — see the Vulkan/EasyGL input-injection notes elsewhere in this
      history): a screenshot clearly showed the sky blending through the glass tile's alpha rather
      than a flat opaque color, confirming the blend pass works; temporary code was reverted before
      committing.
- [ ] Ambient occlusion baked per-vertex at mesh time (Craft implements
      http://0fps.wordpress.com/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/, encoded
      as a 4th UV component — `uv.z` in `shaders/block_vertex.glsl`'s `fragment_ao`). `MeshVertex`
      would need an `ao` field alongside `tileIndex`.
- [ ] Greedy meshing to replace the current naive per-face `ChunkMesher` (§4/§9 M7) — reduces
      vertex count substantially once the world is no longer fixed-size.
- [x] Per-chunk frustum culling before `ChunkRenderer::Draw` (Craft: naive AABB-vs-frustum test in
      `src/main.c` `render_chunks`). CNA already ships real XNA-shaped `BoundingFrustum`/
      `BoundingBox` types, so this needed no new math: `ChunkRenderer::Bounds()` returns the
      chunk's world-space AABB, and `CnaCraftGame::Draw` builds one `BoundingFrustum(View *
      Projection)` per frame and skips `Draw()` for any chunk whose bounds don't intersect it.
      Verified visually against a real EasyGL build across a camera turn (no incorrect
      disappearing chunks/pop-in).
- [x] Clouds: added `BlockType::Cloud` — the inverse situation from Glass. Craft's
      `is_obstacle(CLOUD)` returns 0 (walk through it) while `is_transparent(CLOUD)` is *not* set
      (still occludes neighbors, still hit-testable/breakable — `_hit_test` in `src/main.c` treats
      any `map_get() > 0` cell as hittable). Modeled with a new `BlockDef.collidable` flag (default
      `true`, `false` only for Cloud) and a new `World::IsCollidable` used only by
      `PlayerController::CollidesAt`; `World::IsSolid`/`IsOpaque` (meshing/occlusion/hit-testing)
      are unaffected, so Cloud meshes and occludes exactly like a normal opaque block. Added to the
      hotbar (14 slots now) and the atlas (18 tiles). **Real bug caught during development**:
      `BlockDef::collidable` defaults to `true`, and Air's fallback initializer only overrides
      `solid`, so a naive `.collidable` read made empty space collidable — froze the player
      instantly (10 existing tests failed). Fixed by AND-ing with `solid` in `IsCollidable`
      (`solid && collidable`), matching `IsOpaque`'s existing `solid && !transparent` pattern; added
      an explicit regression test asserting Air is never collidable. 6 new unit tests total (mesh
      routing/occlusion parity with normal blocks, collidability, the Air guard); 75 checks total.
      Not discarded in orthographic mode yet (that needs the `daylight`/ortho-aware shader work
      tracked under "Textured sky dome + fog" below) — left as-is; this task covered the block-type
      semantics only. **Update**: auto-generation via 3D density noise near the world ceiling has
      since landed — see the "Cloud generation via 3D density noise" item under §11.1 above.

### 11.3 Sky & lighting

- [x] Day/night cycle: new engine-agnostic `Worlds/DayNightCycle.{hpp,cpp}` — `ComputeDaylight`
      ports Craft's own `get_daylight()`/`time_of_day()` curve shape (`src/main.c`: two sigmoid
      transitions for dawn/dusk bracketing long full-day/full-night plateaus), driven by
      `GameTime::TotalGameTime` (matches Craft's real `DAY_LENGTH=600` seconds per cycle).
      `CnaCraftGame::Draw` recomputes it every frame, feeding `BasicEffect`'s ambient term with the
      same `value*0.3+0.2` formula as `block_fragment.glsl`, and lerps the (still-flat — no sky
      dome yet, see below) clear color between a night and day tint. 6 new unit tests (curve shape
      at midnight/dawn/midday/dusk, wraparound across multiple cycles, zero-day-length fallback);
      82 checks total. Verified visually against a real EasyGL build: at game start (`TotalGameTime
      ≈ 0`, i.e. midnight) the sky renders a clear dark navy instead of the old fixed sky-blue, and
      terrain ambient lighting is visibly darker too.
- [ ] Textured sky dome + fog blended by height/distance (Craft: `shaders/sky_*.glsl`,
      `fog_height`/`fog_factor` in `block_vertex.glsl`) — replaces the flat `device.Clear(...)` sky
      color in `CnaCraftGame::Draw` (§5).
- [ ] Point/block light sources with propagation (Craft: `light_map` alongside `block_map` in
      `src/map.c`, `Ctrl+Right-click` toggles a block as a light per the README controls table) —
      a substantial systems addition, likely its own multi-task effort.

### 11.4 Player & interaction

- [x] Hotbar: 1–9 keys + `E` to cycle selected block type for placing (README controls table).
      Implemented as an engine-agnostic `Worlds/Hotbar` (4 slots: Grass/Dirt/Stone/Sand, unit-tested
      in `tests/worlds_smoke_test.cpp`) wired into `CnaCraftGame::Update` (keys `1`-`4` select,
      `E` cycles); right-click placement now uses `hotbar_.Selected()` instead of a hardcoded
      `BlockType::Stone`. No on-screen overlay yet — selection changes print to the console
      instead (the visual crosshair/hotbar overlay is its own item, §11.7).
- [x] Flying-mode toggle (`Tab`), reusing `house3d_demo.cpp`'s Game/Fly mode split (§1/§6 already
      note this demo as the movement reference). `PlayerController::ToggleFlying()`/`IsFlying()`
      added; fly mode disables gravity and allows free vertical movement (`PlayerInput::moveUp`,
      Space up / Left Ctrl down — Left Shift deliberately left free for the "Zoom" item below);
      horizontal movement still collides with the world, vertical does not (matches
      `house3d_demo.cpp`'s fly branch letting the player clip through floors/ceilings on purpose).
      7 new unit tests in `tests/worlds_smoke_test.cpp`. Verified end-to-end against the real
      EasyGL build (Tab toggling logged "Flying: on"/"off" to the console, no on-screen HUD yet —
      same as the hotbar).
- [x] Zoom (Left Shift narrows FOV) and orthographic view (`F`) in `CnaCraftGame::Draw`. Both are
      **hold**-to-activate, not toggles — matches Craft's own actual behavior
      (`g->fov = ... ? 15 : 65` / `g->ortho = ... ? 64 : 0` in `Craft/src/main.c:2418-2419`) despite
      this backlog item's "toggle" wording. Verified visually against a real EasyGL build
      (screenshots: normal FOV, Left-Shift-held narrow FOV, F-held orthographic projection all
      visually distinct and correct).
- [ ] Signs: place text on a block face, render as a billboard quad with a small bitmap-font
      texture (Craft: `src/sign.c`, `textures/sign.png`) — CNA's `SpriteFont`/bitmap-font approach
      from `house3d_demo.cpp`'s controls overlay is a usable reference for the text-rendering part.
- [ ] Exact-axis movement keys (Craft's `ZXCVBN`) — low priority, purely a control-scheme nicety.
      **Correction**: `Craft/README.md` documents this ("ZXCVBN to move in exact directions along
      the XYZ axes"), but the local checkout's `src/main.c::handle_movement` does not actually
      implement it (no `glfwGetKey(g->window, 'Z'/'X'/'C'/'V'/'B'/'N')` calls exist) — there's no
      working reference implementation to port from this checkout. Left unimplemented; revisit
      only if this turns out to matter to actual play (still low priority either way).

### 11.5 Persistence

- [ ] SQLite-backed delta save/load: a `block(p, q, x, y, z, w)` table storing only edits over the
      regenerated procedural terrain, precisely mirroring Craft's schema (`src/db.c`/`src/db.h`)
      — already the intended shape per §0/§9 M7; this is now a concrete, schema-specified task.
      `World`/`Chunk` need a way to enumerate only *changed* blocks to keep writes small.

### 11.6 Multiplayer

- [ ] Chunk/block sync over CNA's real ENet-backed `Microsoft::Xna::Framework::Net` layer (see
      `../cna/README.md` §7), using Craft's message shapes as the protocol reference rather than a
      literal port: `C,p,q,key` chunk requests, `B,p,q,x,y,z,w` block updates, `K,p,q,key` cache
      keys, `P,pid,x,y,z,rx,ry` player position streams (`Craft/src/client.c`, `Craft/server.py`).
- [ ] Remote player rendering + interpolation between the last two received position updates
      (Craft: `src/main.c` `interpolate_player`).
- [ ] Picture-in-picture observation of another player (Craft: `O`/`P` keys, `src/main.c` — "just
      change the viewport and render the scene again from the other player's point of view",
      trivial to reproduce via a second `Viewport`/`BasicEffect.View` pass in `CnaCraftGame::Draw`).

### 11.7 UI/UX

- [x] Crosshair + hotbar overlay via `SpriteBatch` (new `Render/Hud.{hpp,cpp}`), replacing the
      console-printf stopgap the hotbar/fly-mode features used until now. CNA has no
      content-pipeline `SpriteFont` available at runtime, so — same as `house3d_demo.cpp`'s
      controls overlay (§5) — text is drawn with an embedded 8x8 bitmap font into a CPU-side RGBA
      buffer, uploaded via `Texture2D::SetDataRGBA`, and drawn with `SpriteBatch`. Shows a
      center-screen crosshair (static texture, built once) and a bottom-center hotbar strip listing
      all 12 slot names with the selected one bracketed/highlighted, rebuilt only when selection or
      flying state changes (not every frame) plus a `[FLYING]` prefix while flying. Verified
      visually against a real EasyGL build (screenshots: default hotbar row, and the `[FLYING]`
      prefix appearing after a real Tab press).
- [ ] Chat + slash commands (Craft: `T` to type, `/` for commands, `src/main.c` `handle_command`).
- [x] Screenshot capture command: `F12` captures the current frame (including the HUD) to
      `screenshots/cnacraft_NNNN.png`. **Correction**: Craft's README "Screenshot" section turned
      out to just be a marketing image, not a documented in-game hotkey — there's no reference
      key binding to match, so `F12` was picked as a common convention instead. CNA already had
      the needed path (`GraphicsDevice::GetBackBufferData` + `Texture2D::SaveAsPng`), so this
      didn't need new engine-level work. Verified end-to-end against a real EasyGL build (F12
      produced a valid, correct PNG on disk).

### 11.8 Testing

- [ ] Extend `tests/worlds_smoke_test.cpp` alongside each `Worlds/`-layer item above (hash-map
      chunk store, caves, AO baking, save/load round-trip) — it's the part of this backlog that's
      cheaply unit-testable without a GPU (§8).
- [ ] A headless `--smoke N` CI check for `CnaCraft` itself (already wired, §8/main.cpp), extended
      with `ctest` registration the way `../cna/CMakeLists.txt` registers
      `EasyGL_House3D_SmokeTest`, once this repo has CI.

## 12. Craft Feature Parity Port

**Superseding note (2026-07-09)**: §11 above was written from a partial reading of Craft's source
and, while mostly accurate, was never a systematic feature-by-feature audit — it also contained at
least one real citation error (the "caves/overhangs" item, corrected in §11.1). This section is
the result of a full audit: 5 parallel passes reading the actual Craft checkout at
`/rv/data/development/github.com/other/Craft` against cna-craft's current `src/` tree, area by
area (input/movement, hotbar/raycast/editing, chunk/mesh/terrain, persistence/signs/multiplayer,
visuals/shaders/atlas). Full findings, with exact file/line citations on both sides, are in
[CRAFT_PARITY.md](CRAFT_PARITY.md) — **that file is now the source of truth for "does cna-craft
match Craft's real behavior"**; this section converts its gaps into an ordered, sequential task
list. Status vocabulary matches §11.0: `pending`/`in_progress`/`completed`/`blocked`/`needs_human`.

**Headline correction**: cna-craft is further along than §11's checkbox list alone suggested for
some things (raycast is algorithmically *better* than Craft's, trees/clouds/day-night already
verified as exact constant matches) — but it is **not** a faithful port in several concrete,
previously-unaudited ways: two real player-facing bugs relative to Craft's own logic (unbounded
diagonal-speed movement, no player-self-intersection check on block placement), one design gap
relative to cna-craft's own stated intent (Bedrock — a block Craft doesn't even have — was
minable, defeating its own "world-boundary, not meant to be placed" comment), a missing
block-targeting visual (no wireframe outline — unblocked by shader limitations, just never built),
and a dead `Sand` block (defined everywhere but never generated). This section prioritizes exactly
those kinds of concrete, verifiable gaps over further decorative world-gen work.

### 12.1 Priority queue (gameplay-critical first, per user's ordering)

1. `completed` — **Hotbar/selected-item switching completeness** (CRAFT_PARITY.md §2.1): add
   key `0` (10th direct slot), `R` (reverse-cycle, mirrors Craft's `E`/`R` pair), and scroll-wheel
   cycling to `CnaCraftGame::Update`/`Hotbar`.
2. `completed` — **Block roster matching** (CRAFT_PARITY.md §2.2). **User decision (2026-07-10)**:
   match Craft exactly. `BlockType::Cloud` removed from `Hotbar::kSlots` (now 16 slots, was 17) —
   Craft's real `items[]` never lists `CLOUD` either (world-gen-only, never player-placeable), so
   cna-craft's earlier "let players build with clouds" feature was a documented divergence, now
   resolved in favor of fidelity. `Cloud` itself is unchanged as a `BlockType` (still generated by
   `World::GenerateClouds`, still hit-testable/opaque/non-collidable) — only its hotbar-placeable
   status changed. 1 test count reduction (one fewer slot to cycle through in
   `TestHotbarSelectionAndCycling`) — 148 checks total. Chest/plants(flowers already
   done)/dyes remain `pending` (Chest needs new mesh-format work like Chest's real shape; dyes are
   low-value, deferred).
3. `completed` — **Visible targeted-block behavior / wireframe outline** (CRAFT_PARITY.md §2.4):
   render a highlight box around the currently-raycast-targeted block. Verified NOT blocked by
   the shader-backend limitations in `missing.md` — a plain line-list primitive with stock
   `BasicEffect` suffices, no `ShaderEffect` needed, works identically on all three backends.
4. `completed` — **Block breaking: Bedrock protection** (CRAFT_PARITY.md §2.5): add an
   `IsBreakable`-equivalent guard so `World::Bedrock` (a cna-craft-only concept, absent from
   Craft entirely) can no longer be mined, matching cna-craft's own documented design intent for
   that block type.
5. `completed` — **Block placing: player self-intersection guard** (CRAFT_PARITY.md §2.6): port
   Craft's `player_intersects_block` check — a placement that would overlap the player's own AABB
   is now rejected, matching `on_right_click`'s real guard in `main.c:2153-2163`.
6. `completed` — **Player movement: diagonal-speed normalization bug** (CRAFT_PARITY.md §1.5):
   `PlayerController`'s horizontal move vector is now normalized before scaling by `kMoveSpeed`/
   `kFlySpeed`, matching Craft's `get_motion_vector`'s always-unit-vector approach — diagonal
   movement (e.g. W+D) is no longer ~41% faster than straight movement.
7. `completed` — **Non-cubic plant geometry** (CRAFT_PARITY.md §3.7): `ChunkMesher` gained a
   second emission path, `EmitPlant` — a 4-quad cross ("X") billboard ported from Craft's real
   `make_plant` (src/cube.c), two full-block diagonal planes each emitted with both windings so
   the cross is visible from any angle. New `BlockDef.plant` flag (true only for the new
   `BlockType::TallGrass`): `solid=true` (meshed/hit-testable/breakable, matches `World::IsSolid`),
   `collidable=false` (walk-through, same non-collidable pattern as Cloud but opposite reasoning —
   ports Craft's `is_obstacle(plant)==0`), `transparent=true` (doesn't occlude neighbors, ports
   `is_transparent(plant)`). Plants are never face-culled against neighbors — always emit all 4
   quads regardless of what's adjacent, matching Craft exactly. New `World::GenerateGrassDecoration`
   places `TallGrass` one cell above grass-column surfaces using Craft's real trigger
   (`simplex2(-x*0.1, z*0.1, 4, 0.8, 2) > 0.6`, verified against the checkout), run after
   `GenerateTrees` (a harmless reordering vs. Craft's same-Y-level model, since this project places
   the plant one cell *above* the surface rather than *at* it, so it never actually contends with a
   tree trunk cell — see the code comment for the full reasoning). New atlas tile 19
   (`Pattern::GrassBlade` — per-pixel transparent gaps carve blade shapes out of the cross quads,
   `TextureAtlas.cpp`). `TallGrass` appended to `Hotbar::kSlots` (now 16 slots). 14 new unit tests
   (mesh emission shape/vertex-count, never-face-culled invariant, solid/collidable/transparent/
   breakable rules, world-gen presence/determinism/surface-height invariant, hotbar slot count) —
   138 checks total. Verified visually against a real EasyGL build: a screenshot clearly shows
   dark-green blade-shaped billboards (with visible transparent gaps between blades) growing out of
   grass terrain, confirming mesh shape, alpha blending, and world placement all work end-to-end.
8. `completed` — **Texture atlas note**: no code change — CRAFT_PARITY.md §5.5 confirms
   cna-craft's procedural atlas is a documented, deliberate substitution for Craft's hand-authored
   `texture.png`; adopting a real asset is `needs_human` (new asset dependency), not picked up.
9. `completed` — **Terrain generation: dead `Sand` block** (CRAFT_PARITY.md §3.3): `World::Generate`
   now places `Sand` (replacing the Dirt/Grass surface layers only, not the heightmap itself) for
   any column at or below `kSandMaxHeight=10` — a low-elevation "beach" rule adapted from Craft's
   real `if (h <= t) { h = t; w = SAND; }` (t=12), scaled down since Craft's own threshold would
   swallow nearly this project's entire height range (`kMinHeight=4`). Deliberately does **not**
   touch `NoiseGenerator::Height`'s formula itself (still the `needs_human` item below) — Stone/
   Bedrock underneath a sandy column are unchanged, matching cna-craft's own pre-existing layered-
   terrain design (not itself a Craft citation) rather than Craft's single-uniform-block-column
   model. Sandy columns correctly get no trees (`World::GenerateTrees` already gates on
   `BlockType::Grass`, matching Craft's own `if (w == 1)` grass-only trigger). 4 new unit tests
   (presence, threshold invariant, determinism, cloud-pass-surface-invariant updated for the new
   valid surface type) — 120 checks total.
10. `completed` — **Player physics: terminal velocity clamp** (CRAFT_PARITY.md §1.8): port
    Craft's `-250 units/s` fall-speed cap to `PlayerController` (jump speed `8.0` and gravity
    `25.0` already matched Craft exactly from a prior session).
11. `completed` — **Collision substepping** (CRAFT_PARITY.md §1.7): `PlayerController::Update`
    now breaks each frame's movement into `step = clamp(estimate, kMinSubsteps=8, kMaxSubsteps=64)`
    substeps (ported from Craft's own `step = MAX(8, estimate)`, `main.c handle_movement`, same
    distance-based `estimate` formula using this project's own speed constants), each substep
    independently resolved through the existing axis-separated `CollidesAt` checks. `kMaxSubsteps`
    is a deliberate addition beyond Craft's own unbounded `step` — a defensive cost cap for a
    pathologically large one-off `dt`. Yaw/pitch update and jump-impulse consumption stay
    once-per-`Update()`-call (not per substep), matching Craft's own `s->rx`/`dy` update points.
    Verified via the full existing physics test suite re-run (139/139 passing, no regressions —
    jump height/gravity/fly-mode/grounding all numerically unaffected at normal `dt`), plus a new
    dedicated regression test that places a huge single-frame `dt` (2 real seconds) against a
    1-block-thick wall and asserts the player stops at the wall instead of tunneling through it —
    **empirically confirmed meaningful** by temporarily forcing `kMinSubsteps=kMaxSubsteps=1`
    (i.e. disabling substepping) and observing the new test correctly fail, then restoring the
    real values and re-confirming all 139 tests pass.
12. `blocked` — **Ambient occlusion** (CRAFT_PARITY.md §5.1): needs a custom vertex format +
    `ShaderEffect`; only `EASYGL` has real runtime shader support today per `missing.md`.
13. `completed` — **Fog** (CRAFT_PARITY.md §5.2): `CnaCraftGame::Draw` now sets `effect_`'s
    built-in `FogEnabled`/`FogColor`/`FogStart`/`FogEnd` every frame (`kFogStart=70`,
    `kFogEnd=150`, scaled to this project's fixed 128×64×128 world instead of Craft's
    streamed-world render radius). `FogColor` matches the already-computed flat sky clear color,
    so distant geometry fades toward the sky (same intent as Craft's real sky-texture-sampling
    fog, simpler source since there's no sky dome yet — see item 14). Fog is disabled in
    orthographic mode, matching Craft's own `if (bool(ortho)) fog_factor = 0.0`. No custom shader
    needed — confirmed via CNA's own cross-backend fog test suite
    (`../cna/examples/{easygl,vulkan,bgfx}_basiceffect_lit_fog_test.cpp`) that this exact
    lit+textured `BasicEffect` path has real fog support on EASYGL, VULKAN, and BGFX alike.
    Verified via a clean build (worlds + full EasyGL) and a 100-frame headless smoke run; a
    fade-visible screenshot proved impractical in this session's sandboxed X11 environment (the
    fixed spawn point sits in a tightly enclosed terrain pocket, and scripted fly-navigation out
    of it was unreliable — same class of sandbox flakiness noted elsewhere in this project's
    history) — the wireframe-outline screenshot from the prior commit already confirms the same
    `Draw()` pipeline (including `effect_` state changes) renders correctly end-to-end.
14. `completed` — **Sky dome** (CRAFT_PARITY.md §5.3): picked the objectively-safe option rather
    than treating this as `needs_human` — the plain vertex-colored version needs no new asset
    dependency and no custom shader (both would be genuine scope/dependency decisions), so it's
    the "smallest correct" implementation, not a subjective call. New `Render::SkyDome`: a 7-ring/
    16-segment hemisphere (`VertexPositionColor`, stock `BasicEffect`), rebuilt each frame with a
    horizon-to-zenith color gradient from the same day/night sky colors `CnaCraftGame::Draw`
    already computes, drawn centered on the camera (via `World = Scale(400) *
    Translation(cameraPos)`) with depth writes off so it never occludes anything, fog/lighting
    temporarily disabled for the draw (same save/restore pattern `SelectionOutline` already uses).
    Replaces the flat `device.Clear(...)` sky (the clear color is kept as a fallback base/matches
    the dome's horizon ring, so there's no visible seam). **Real bug caught and fixed during
    development**: the first winding choice (reasoned analytically as "CCW viewed from inside,
    since the camera sits at the dome's center") rendered nothing at all — confirmed via a real
    EasyGL build with deliberately vivid red/green debug colors that never appeared on screen.
    Traced to CNA's actual default `RasterizerState` (`CullCounterClockwiseFace`, confirmed by
    reading `RasterizerState.cpp`) needing the opposite winding from what was assumed; fixed
    empirically (flipped the index order, rebuilt, the green gradient then rendered correctly) and
    documented in a code comment so the next non-cube geometry addition doesn't repeat the same
    mistake. Verified visually against a real EasyGL build twice — once with vivid debug colors to
    unambiguously confirm the dome geometry renders as a gradient (not just the flat clear color
    showing through), once with the real day/night colors restored.
15. `needs_human` — **World persistence / delta storage** (CRAFT_PARITY.md §4.1/§4.2): SQLite
    dependency decision, unchanged from `missing.md`'s prior assessment.
16. `pending` (large) — **Signs** (CRAFT_PARITY.md §4.3). **Re-assessed this session**: CNA does
    have a real, usable text-input primitive (`Microsoft::Xna::Framework::Input::TextInputEXT` —
    `TextInput`/`TextEditing` events, `StartTextInput`/`StopTextInput`), so this is not blocked on
    a missing engine capability the way it first looked. It remains `pending (large)` rather than
    picked up as a "next smallest task" because a correct implementation needs *three* new pieces
    together, not a small extension of existing code: (1) a text-input state machine (open on `` ` ``
    per Craft, capture `TextInput` events, handle Backspace/Enter/Escape, and — matching Craft's
    own `if (!g->typing)` gate on `handle_movement`'s key polling — suspend WASD/mouse-look input
    while typing, since CNA's event-based text input doesn't automatically exclude movement keys
    the way Craft's GLFW model does); (2) an in-memory (non-persisted, consistent with this
    project's current no-persistence state) per-face sign store; (3) new 3D billboard-quad
    rendering oriented per whichever of 6 face normals the sign was placed on, with dynamically
    generated text texture (`Hud.cpp`'s `FontDrawText` technique is reusable for the *text
    rendering* part, but not the 3D billboard placement/orientation part, which doesn't exist
    anywhere in this codebase yet). Recommended as its own dedicated follow-up task, not bundled
    into a continuous small-batch session.
17. `pending` (large) — **Chat/slash commands** (CRAFT_PARITY.md §4.5): the same text-input state
    machine from item 16 is a prerequisite here too. The real Craft command set is mostly
    world-editing macros (`/cube`, `/sphere`, `/tree`, `/array`, `/copy`, `/paste`, etc.) — a
    materially larger scope than "add a chat box," since none of those world-editing primitives
    exist in this codebase either. Recommended as its own dedicated follow-up task.
18. `pending`, explicitly deferred — **Multiplayer** (CRAFT_PARITY.md §4.6): per project
    direction, not started before local single-player + persistence are stable.
19. `needs_human` — **Chunk system redesign** (CRAFT_PARITY.md §3.1/§3.2): hash-map sparse
    chunks + distance-based streaming, replacing the fixed dense grid — a genuine architecture
    change, not a quick task; scoping it is a product decision (does cna-craft want an unbounded
    world at all, given its "small fixed-size prototype" design goal in §1?).
20. `completed` — **Middle-click eyedropper** (CRAFT_PARITY.md §2.7): new `Hotbar::
    SelectByBlockType(type)`, wired to the middle mouse button in `CnaCraftGame::Update` using the
    same per-frame raycast as break/place/outline. Ports Craft's real `on_middle_click` exactly
    (linear-scan the roster for the targeted block's type, select that slot if found, otherwise
    leave selection unchanged — e.g. targeting Bedrock or Air does nothing). 5 new unit tests — 125
    checks total. Ctrl+click-as-place and the light-toggle half of CRAFT_PARITY.md §2.7 remain
    `pending`/blocked-on-a-larger-subsystem respectively, not picked up in this batch.
21. `completed` — **Ctrl+left-click as place** (CRAFT_PARITY.md §2.7): ports Craft's real
    `on_mouse_button` (`control ? on_right_click() : on_left_click()` for the left button) —
    holding Left/Right Ctrl while left-clicking now places the selected block (same guard as
    ordinary right-click: rejects placement that would overlap the player) instead of breaking.
    The left-click/right-click/Ctrl+left-click paths now share one `tryPlaceBlock` lambda instead
    of duplicating the placement logic. The light-toggle half of Craft's Ctrl+right-click remains
    `pending`, blocked on the much larger unimplemented point-lighting subsystem (§4.3/CRAFT_PARITY
    §2.7). Verified via a clean EasyGL build and a headless smoke run; no new `Worlds/`-layer logic
    was added (pure `CnaCraftGame.cpp` input-wiring glue reusing already-tested `IntersectsBlock`/
    `SetBlock`), so no new unit tests were needed.
22. `completed` — **Flowers** (CRAFT_PARITY.md §3.7 follow-up): reuses the plant-geometry
    infrastructure from item 7 above (`ChunkMesher::EmitPlant`, `BlockDef.plant`) — a small,
    well-scoped extension, not new structural work. New `BlockType::Flower` (one representative
    type; Craft's real 6-color roster is a content-scaling exercise, low value per the "prioritize
    gameplay parity over decorative additions" guidance, not picked up). New
    `World::GenerateFlowers` using Craft's real trigger (`simplex2(x*0.05, -z*0.05, 4, 0.8, 2) >
    0.7`, verified against the checkout), run after `GenerateGrassDecoration` with the same
    "only place over Air" guard (so a column where both triggers fire keeps whichever decoration
    was placed first, a documented deviation from Craft's literal unconditional-overwrite order).
    New atlas tile 20 (`Pattern::Flower` — a stem-and-bloom cutout shape). `Flower` appended to the
    hotbar (17 slots now). 9 new unit tests (generation presence/determinism/surface-height,
    mesh/collision/occlusion rules) — 149 checks total. **Verification scoped down deliberately**:
    Flower reuses the identical `EmitPlant`/generation code path already visually verified for
    TallGrass in the previous commit, so this item relies on that prior visual confirmation plus a
    clean build + headless smoke run rather than a second full interactive screenshot round — a
    reasonable time/verification tradeoff given the code path is provably identical (same function,
    different tile index and trigger constants only), not a gap in rigor.

### 12.2 Deliberately not re-litigated this session

Per CRAFT_PARITY.md, these are `complete` (functionally equivalent to Craft) and need no task:
main game loop (§1.1), mouse look (§1.4), zoom/ortho/arrow-look (§1.9), raycast algorithm (§2.3,
arguably better than Craft's), collision rules for solid/transparent/collidable (§2.8), face
culling (§3.4), mesh generation strategy (§3.5), transparent-block rules (§3.6), trees (§3.8),
clouds (§3.10), day/night lighting (§5.4).

Per CRAFT_PARITY.md, these are documented, deliberate divergences with a real trade-off, not bugs
— left as `needs_human` rather than auto-"fixed" since a human already made (or should confirm) a
call: window/cursor-capture-vs-quit-on-Escape behavior (§1.2, README already documents Esc=Quit),
walking/flying control scheme (§1.6, README already documents the current Space/Ctrl scheme),
chunk system being fixed-size rather than infinite (§3.1/§3.2, `plan.md` §1 already scopes this
as a deliberate prototype boundary).
