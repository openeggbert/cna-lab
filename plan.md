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
- No occlusion/frustum culling in the engine — was acceptable at this prototype's original fixed
  `128×64×128`-block/32-chunk scale; the world is now unbounded and streamed (§12.1 item 19), so
  every loaded column within `kCreateRadius` still draws every frame with no culling at all —
  worth revisiting if render cost at the current radius (6 chunks) ever becomes a problem when
  scaling the radius up further.

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

- `completed` (see §12.1 item 19) — Hash-map-keyed dynamic chunk store + distance-based
  load/unload (§11.1) — the unbounded, streamed world.
- `completed` (moved here from §11.2, see §12.1 item 7) — Non-cubic plant/billboard geometry.
  `completed` (see §12.1 item 12, 2026-07-11) — ambient occlusion (`MeshVertex::shade`);
  `pending` (large) — greedy meshing (§11.2), which still needs its own `ChunkMesher` rework.
- Mostly `completed` shader-free, superseding this entry's old "needs CNA `ShaderEffect`" premise
  (see missing.md's 2026-07-11 correction): textured sky dome + fog shipped as §12.1 item 33,
  block lighting as the item-27 glow pass, ambient occlusion as item 12 — all on stock effects,
  all backends. Still genuinely shader-blocked (EASYGL-only if attempted today): real torch-light
  propagation (`min(1, daylight+light)` doesn't factor into static × uniform) and per-fragment
  elevation fog — and CNA's 3D draw path currently ignores `ShaderEffect` on every backend, so
  even those need engine work first, not just a shader.
- `completed` (see §12.1 item 15) — SQLite-backed delta persistence (§11.5). User decision
  2026-07-10: add SQLite as a dependency.
- `pending`, explicitly deferred — Multiplayer/`Net`-based chunk/block sync (§11.6): per project
  direction, do not start before local single-player gameplay, persistence, and chunk logic are
  stable — persistence and the chunk-system redesign (§11.1) are both now `completed`, but
  multiplayer itself stays deferred per that same direction.
- `completed` (see §12.1 item 16) — Signs (§11.4). `completed` (see §12.1 item 17) — chat/slash
  commands (§11.7): the same text-input state machine Signs uses was generalized into a
  `TypingMode` enum; Craft's real command set (`/cube`, `/sphere`, `/tree`, `/array`, `/copy`,
  `/paste`, `/view`, etc.) is now a full port via new `Worlds/WorldEditor.hpp/.cpp` primitives.

### 11.1 World & terrain

- [x] Replace the fixed `128×64×128` array-of-chunks `World` with a hash-map-keyed, dynamically
      loaded/unloaded chunk store (see §12.1 item 19, CRAFT_PARITY.md §3.1) — X/Z are now
      unbounded (packed-`(cx,cz)`-keyed `unordered_map`); Y stays fixed at `WORLD_CHUNKS_Y=4`,
      matching Craft's own real behavior (Craft never streams Y either). `Chunk` itself stayed a
      dense fixed array rather than adopting Craft's sparse per-chunk hash map — a pure internal
      storage-strategy difference with no player-visible effect.
- [x] Chunk load/unload driven by player distance (see §12.1 item 19, CRAFT_PARITY.md §3.2):
      `kCreateRadius=6`/`kDeleteRadius=9` chebyshev distance, generation+meshing backgrounded via
      `System::Threading::Tasks::TaskT`.
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
- [x] Ambient occlusion baked per-vertex at mesh time (Craft implements
      http://0fps.wordpress.com/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/, encoded
      as a 4th UV component — `uv.z` in `shaders/block_vertex.glsl`'s `fragment_ao`). Done —
      §12.1 item 12 (2026-07-11): `MeshVertex::shade` + vertex-color rendering, no shader work
      needed after all.
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
- [x] Signs: place text on a block face, render as a billboard quad with a small bitmap-font
      texture (Craft: `src/sign.c`, `textures/sign.png`). See §12.1 item 16 for the full
      implementation writeup (data model, text-input state machine, rendering, persistence,
      end-to-end verification).
- [ ] Exact-axis movement keys (Craft's `ZXCVBN`) — low priority, purely a control-scheme nicety.
      **Correction**: `Craft/README.md` documents this ("ZXCVBN to move in exact directions along
      the XYZ axes"), but the local checkout's `src/main.c::handle_movement` does not actually
      implement it (no `glfwGetKey(g->window, 'Z'/'X'/'C'/'V'/'B'/'N')` calls exist) — there's no
      working reference implementation to port from this checkout. Left unimplemented; revisit
      only if this turns out to matter to actual play (still low priority either way).

### 11.5 Persistence

- [x] SQLite-backed delta save/load: a `block(x, y, z, w)` table storing only edits over the
      regenerated procedural terrain, adapted from Craft's schema (`src/db.c`/`src/db.h`) by
      dropping the `p, q` chunk-address columns (no chunk (p,q) addressing in this project's fixed
      grid `World`). See §12.1 item 15 for the full implementation writeup.

### 11.6 Multiplayer

Design pass completed 2026-07-11 — see **`MULTIPLAYER_PLAN.md`** (authoritative; supersedes the
transport suggestion below). Key correction from that research: CNA's ENet `Net` layer is the
WRONG transport for Craft's TCP line protocol — sharp-runtime's existing
`System::Net::Sockets::TcpClient` + `StreamReader::ReadLine()` is the right (and already-built)
tool.

- [ ] Chunk/block sync following Craft's message shapes (`C,p,q,key` chunk requests,
      `B,p,q,x,y,z,w` block updates, `K,p,q,key` cache keys, `P,pid,x,y,z,rx,ry` player position
      streams — `Craft/src/client.c`, `Craft/server.py`) — full protocol table and client/server
      design in `MULTIPLAYER_PLAN.md` §2/§5/§6.
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
   any column at or below `kSandMaxHeight` — a low-elevation "beach" rule adapted from Craft's
   real `if (h <= t) { h = t; w = SAND; }` (t=12). Originally scaled down to 10 since this
   project's old height formula had a much smaller range; **updated to the literal Craft value 12**
   as part of item 23 below, once the height formula itself was re-ported. Deliberately does
   **not** touch cna-craft's layered Stone/Bedrock-under-sand structure — matches cna-craft's own
   pre-existing layered-terrain design (not itself a Craft citation) rather than Craft's
   single-uniform-block-column model. Sandy columns correctly get no trees (`World::GenerateTrees`
   already gates on `BlockType::Grass`, matching Craft's own `if (w == 1)` grass-only trigger). 4
   new unit tests (presence, threshold invariant, determinism, cloud-pass-surface-invariant
   updated for the new valid surface type) — 120 checks total at the time.
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
12. `completed` (2026-07-11) — **Ambient occlusion** (CRAFT_PARITY.md §5.1). Shipped WITHOUT any
    CNA engine work and on ALL backends, superseding both this entry's old "needs a custom vertex
    format + `ShaderEffect`" premise and the user's earlier EASYGL-only scope decision — planning
    research found (a) CNA's 3D draw path ignores `ShaderEffect` entirely (it only works via 2D
    SpriteBatch), so that route was MORE engine work than recorded, and (b) with no torch light
    (item 27's pivot) and Craft's FIXED diffuse direction, Craft's whole block-lighting equation
    factors into `texel × (daylight*0.3+0.2) × (1+df)*aoBrightness` — per-frame scalar × static
    per-vertex value. Implementation in 3 committed phases: (1) `ChunkMesher::ComputeOcclusion`, a
    direct port of Craft's `occlusion()` (27-neighborhood, curve, both-sides rule, 8-block column
    shade, diagonal flip, plant scalar `min_ao`, cloud contrast compression keyed on
    `BlockType::Cloud`), baked into a new `MeshVertex::shade`; lookup cells derived geometrically
    from the mesher's own face table (Craft's tables use a different face/corner order), pinned by
    38 hand-derived known-value checks; occupancy snapshotted per `Build()` into a Craft-style
    padded array filled in chunk-aligned slabs — the worlds suite got FASTER than pre-AO (18.7s vs
    20.4s) because face culling stopped hash-walking `World::IsOpaque`; `World::SetBlock`'s dirty
    rule widened to the `[x-1,x+1]×[y-8,y+1]×[z-1,z+1]` cross product (diagonals + the 8-down
    shade reach). (2) Mesh-job snapshots widened 7→27 chunks and `MarkNeighborColumnsDirty` 4→8
    columns (Craft's own 3×3 `dirty_chunk` shape) so background meshing can't bake border seams.
    (3) Terrain uploads as `VertexPositionColorTexture` carrying `shade/2`; every `Draw` pass sets
    explicit state — terrain unlit + `DiffuseColor = 2*(daylight*0.3+0.2)` (day/night stays a live
    uniform, zero rebaking), sky/glow/outline white diffuse, signs keep the lit rig (its only
    remaining consumer). Verified: 341/341 + 40/40 tests; live Xvfb screenshots of a staged
    `world.db` scene (wall spanning the x=16 chunk border: contact shadow continuous, NO seam;
    overhang underside dark; lone-block contact ring; plant on stone platform fully bright; clouds
    only mildly shaded; glow block bright and daylight-independent). Synthetic keyboard
    movement/typing RECOVERED this session (mouse still dead) — day/night animation over wall time
    and Vulkan were not live-verified in the sandbox (clock too slow / EasyGL-only build).
    **Day/night since USER-VERIFIED on real hardware (2026-07-11): dusk, night, and the full
    cycle confirmed OK ("soumrak a noc stridani dne a noci jsou ok").** Vulkan remains the one
    unverified surface.
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
15. `completed` — **World persistence / delta storage** (CRAFT_PARITY.md §4.1/§4.2). **User
    decision (2026-07-10)**: add SQLite. New `Persistence/WorldStore` (a new layer, deliberately
    kept out of the engine-agnostic `CnaCraftWorlds` library — see its own header comment) wraps a
    `block(x,y,z,w)` table (Craft's real `block(p,q,x,y,z,w)` schema, `src/db.c`, minus the `p,q`
    chunk-address columns since cna-craft's `World` has no per-chunk addressing), unique-indexed on
    `(x,y,z)`, `INSERT OR REPLACE` on save (matching Craft's own single-latest-value-per-coordinate
    delta model). `World` gained plain edit-tracking (`BlockEdit`, `SetBlockAndRecordEdit`,
    `RecordedEdits()`, `ClearRecordedEdits()`) with zero new dependencies — `SetBlock` (used by
    world generation and by loading) still never persists; only player-driven edits recorded via
    `SetBlockAndRecordEdit` (now used by `CnaCraftGame`'s break/place/Ctrl-place paths) do.
    `CnaCraftGame::Initialize` loads `world.db` (if present) right after `World::Generate`, before
    the initial chunk mesh build; edits save synchronously right after each player action (not
    batched/async like Craft's own worker thread — a deliberate simplification given this
    prototype's low single-player edit rate, documented in `WorldStore.hpp`). `world.db` added to
    `.gitignore` (a runtime save file, not source). `find_package(SQLite3 REQUIRED)` added to
    `CMakeLists.txt` (only for the `CnaCraft` executable target, not `CnaCraftWorlds`); confirmed
    available via the system package (`libsqlite3-dev`) with no `FetchContent`/vendoring needed.
    12 new unit tests in a **new, separate test target** `cna_craft_persistence_smoke_test`
    (`tests/persistence_smoke_test.cpp`) — save/load/overwrite/Air-round-trip round-trips against a
    real SQLite file on disk, registered as its own `ctest` (`PersistenceSmokeTest`). Kept separate
    from `worlds_smoke_test`/`WorldsSmokeTest` since it's the only part of this repo's test suite
    that touches real disk I/O. **Verification note**: end-to-end verification via the actual
    graphical game (real mouse click → break → check the `.db` file) turned out to be impractical
    in this sandbox — synthetic mouse clicks into a relative-mouse-mode SDL window are unreliable
    here, the same class of flakiness already documented elsewhere in this project's history for
    mouse-look — so the dedicated `WorldStore`-level test above is the primary verification instead
    (arguably better: it's reliable, repeatable, and became permanent regression coverage rather
    than a one-off manual check). The full game was still verified to build cleanly, start up
    without error, and create `world.db` on launch via a real headless `--smoke` run.
16. `completed` — **Signs** (CRAFT_PARITY.md §4.3). Built as the three pieces identified below,
    plus SQLite persistence (extending item 15's `WorldStore`, decided together with it this
    session): (1) `Worlds::Sign`/`Worlds::SignStore` (`Worlds/Sign.hpp/.cpp`) — an engine-agnostic
    data model keyed by `(x,y,z,face)`, with a deliberately simplified *symmetric* 6-face
    convention (0=+X,1=-X,2=+Y/top,3=-Y/bottom,4=+Z,5=-Z, derived directly from the raycast hit
    normal) rather than Craft's real asymmetric `hit_test_face` (main.c:666-696, 4-way
    player-angle-dependent rotation, no bottom-face support) — documented in Sign.hpp as a reasoned
    difference, not an oversight, since `VoxelRaycast` already provides a normal Craft's own
    hit-test doesn't. (2) A text-input state machine in `CnaCraftGame::Update()`: backtick
    (`Keys::OemTilde`, edge-triggered) opens typing via `TextInputEXT::StartTextInput()`; a
    `TextInputEXT::TextInput` handler registered once in `Initialize()` appends printable-ASCII
    characters (gated on `isTypingSign_`, 64-char cap); Backspace/Enter/Escape handled
    edge-triggered inside the typing branch, which also feeds `PlayerController::Update()` a
    frozen (all-zero) `PlayerInput` with real `dt` (gravity still integrates, matching Craft's own
    `if (!g->typing)` gate on movement-key polling only) and returns early, fully suspending
    WASD/look/click/hotbar input while typing. Enter re-runs `VoxelRaycast::Cast` fresh (not the
    frame-start raycast) before submitting, matching Craft's own Enter-time `hit_test` call
    (main.c:2214-2219). Escape cancels typing without quitting the game; the game-quit Escape
    check was changed from level- to edge-triggered so cancelling doesn't also quit on the next
    frame if the key is still physically held. (3) `Render::SignBillboard`
    (`Render/SignBillboard.hpp/.cpp`) — one dynamically-built 128x32 text texture per sign (reuses
    the now-shared `Render/BitmapFont.hpp` `FontDrawText`, extracted from `Hud.cpp` for this
    purpose) on a quad oriented per the sign's face, offset outward by a small epsilon. Each quad
    is emitted with **both** triangle windings (12 indices, not 6) rather than a single
    Craft-accurate winding — this project's correct winding convention for non-cube geometry has
    needed real-build empirical verification every time so far (see `SkyDome.cpp`'s note), and
    "visible from both sides" is a reasonable simplification for a decal-like sign; the visible
    consequence (confirmed in the screenshot below) is mirrored ghost text bleeding through from
    the reverse-winding triangle when viewed at a grazing angle — accepted as part of the same
    tradeoff. Signs persist via `WorldStore::LoadSignsInto` (bulk load, same as `LoadInto` for
    blocks) plus `UpsertSign`/`DeleteSign`/`DeleteSignsAt` (new `sign` table keyed the same way as
    `SignStore`, adapted from Craft's real `sign(p,q,x,y,z,face,text)` schema by dropping the
    chunk-address columns, same reasoning as item 15's `block` table). **Revised after initial
    landing** (same session, following a direct user request to match Craft more closely): the
    first version of `SaveSigns` did a bulk delete-and-reinsert of the whole sign list on every
    save; re-reading Craft's real `db.c` showed it's actually incremental
    (`db_insert_sign`/`db_delete_sign`/`db_delete_signs`, called from `set_sign`/`unset_sign_face`/
    `unset_sign`), so `WorldStore` was changed to three matching incremental methods instead. This
    also surfaced a real behavior gap versus Craft: `_set_block` (`src/main.c`) calls
    `unset_sign()` whenever a block is set to type 0, so a sign can't outlive the block face it was
    attached to — `CnaCraftGame::Update()`'s break-block branch now calls
    `SignStore::RemoveAllAt`/`WorldStore::DeleteSignsAt` too, closing that gap. `SignStore` gained
    `RemoveAllAt(x,y,z)` (ports `unset_sign`) with 4 new unit tests; `WorldStore`'s incremental sign
    methods have 7 new/revised tests in `persistence_smoke_test.cpp` (upsert-replaces,
    delete-one-face-leaves-others, delete-all-at-leaves-other-cells). Also fixed to match Craft
    exactly: submitting an empty-text sign now deletes any existing sign at that face (Craft's
    `set_sign` routes empty text to `unset_sign_face` unconditionally, not gated on the raycast hit
    existing *and* the buffer being non-empty as the first version had it).
    **Verified end-to-end in a real headless build** (not just unit tests, unlike item 15's
    persistence-only verification): launched the actual `CnaCraft` executable under Xvfb
    (`SDL_VIDEODRIVER=x11` — the default video driver silently created a Wayland surface CNA's SDL
    backend can't see under X11 tooling, wasting an early screenshot attempt before this was
    diagnosed), drove backtick/type/Enter via `xdotool keydown`/`keyup` (plain `xdotool key`'s
    default hold duration was too short for this project's 60fps polling to catch — an update to
    this project's documented keyboard-injection flakiness notes), confirmed via temporary debug
    prints that `TextInput` events, the edge-triggered Enter branch, and the raycast hit all fired
    correctly, confirmed the sign rendered on-screen (including the expected double-winding
    ghost-text artifact described above), confirmed the row landed in `world.db`'s new `sign`
    table via `sqlite3` (both for the original bulk save and, after the revision, the new
    `UpsertSign` call), and confirmed the sign reloaded correctly after killing and relaunching the
    process. Debug prints were removed before the final build. The break-deletes-signs path could
    **not** be verified live via the GUI — synthetic mouse clicks into this project's
    relative-mouse-mode SDL window remain unreliable in this sandbox (same documented flakiness as
    item 15's persistence work); verified instead via the unit tests above plus code review, since
    `CnaCraftGame`'s wiring is a direct, untransformed call into the same
    `SignStore::RemoveAllAt`/`WorldStore::DeleteSignsAt` functions those tests already exercise.
17. `completed` — **Chat/slash commands** (CRAFT_PARITY.md §4.5). Planned via `EnterPlanMode`
    (comparable scope to item 19's chunk redesign, per this project's own "genuine architecture
    change needs a plan first" rule) after reading Craft's real `parse_command` and every function
    it calls directly from the checkout. New `src/CnaCraft/Worlds/WorldEditor.hpp/.cpp` — 7
    engine-agnostic, directly unit-tested geometry primitives (`PaintBlock` = Craft's
    `builder_block`; `FillCuboid` = `cube()`; `FillSphere` = `sphere()`, which already covers both
    the `/sphere` and `/circlex|y|z` command families via its `fx/fy/fz` flatten flags, ported
    verbatim; `FillCylinder` = `cylinder()`, internally a stack of `FillSphere` calls flattened
    along one axis, same as Craft's own implementation; `FillArray` = `array()`; `GrowTree` =
    `tree()`; `PasteRegion` = `paste()`) plus `Worlds::ExecuteCommand`, a direct port of
    `parse_command`'s real dispatch chain covering every world-editing command (`/view`, `/copy`,
    `/paste`, `/tree`, `/array`, `/cube`, `/fcube`, `/sphere`, `/fsphere`, `/circlex|y|z`,
    `/fcirclex|y|z`, `/cylinder`, `/fcylinder`) — the 5 multiplayer-auth-only commands and the
    plain-chat fallback are deliberately not ported (CRAFT_PARITY.md §4.5 already scopes these
    out: no networking exists, and there's no other player to talk to).
    - `CnaCraftGame`'s sign-only typing state machine (`isTypingSign_` bool) generalized to a
      `TypingMode{None,Sign,Command}` enum — backtick still opens Sign typing unchanged, `/`
      (`Keys::OemQuestion`) opens Command typing with the buffer pre-seeded as `"/"`, matching
      Craft's own `g->typing_buffer[0]='/'` exactly.
    - New `mark0_`/`mark1_` members (mirrors Craft's `g->block0`/`g->block1`) updated via a new
      `RecordMark` call at both the break and place sites — every command's anchor points.
    - `radii_` (`Worlds::CommandRadii`) replaces the old compile-time-fixed
      `kCreateRadius`/`kDeleteRadius` as the mutable runtime source of truth `/view` mutates
      (clamped to Craft's exact `1..24`, this project's own `+3` hysteresis margin rather than
      Craft's literal `+4`); fog start/end now derive from `radii_.createRadius` every frame
      instead of a compile-time constant, so `/view` immediately affects the fog distance too, the
      same way it affects streaming.
    - `Render::Hud` gained a real 4-line scrolling message log (`PushMessage`, Craft's own
      `MAX_MESSAGES=4` ring buffer) for command feedback. **User decision (2026-07-10)**: full
      Craft-accurate multi-line log, not a simplified single-line flash message, when asked to
      choose.
    - Test count: 41 new checks (one per `WorldEditor` primitive — filled/hollow cuboid, sphere's
      fill/hollow/flatten-axis variants, cylinder's single-axis requirement, array's per-axis step
      forcing, tree shape, `PaintBlock`'s Bedrock/y-bounds guards, `PasteRegion`'s "reflects live
      state at paste time, not copy time" quirk — a real, faithfully-reproduced Craft
      characteristic, not a bug, confirmed by reading Craft's own identical loop shape — plus a
      broad `ExecuteCommand` dispatch test covering every command string) — 267 checks in
      `worlds_smoke_test.cpp`, up from 226.
    - Verified against a real EasyGL build under Xvfb: `/` opens the typing box correctly,
      `/view 20` updates both the console log and the on-screen message log
      ("Viewing distance set to 20."), an invalid `/view 99` and an unrecognized `/nonsense` both
      produce Craft-accurate feedback that stacks correctly in the message log (oldest on top,
      persists after the typing box closes, matching Craft), and Sign typing (backtick) is
      unaffected by the `TypingMode` generalization. The world-editing paint commands themselves
      could not be verified live end-to-end (they need mouse-driven break/place to set real marks
      first, and synthetic mouse clicks into this sandbox's relative-mouse-mode SDL window remain
      unreliable — the same documented flakiness as items 15/16) — covered instead by the
      exhaustive unit tests above.
18. `completed` (2026-07-11) — **Multiplayer** (CRAFT_PARITY.md §4.6, design in
    `MULTIPLAYER_PLAN.md`, scope decisions in its §11, go-ahead "pust se prosim do implementace
    vsech 4 bodu"). Shipped in 8 independently committed+verified phases:
    - **M0** `Net/Protocol` — shape-identical Craft ASCII line protocol, version-1001 dialect
      (16-block column p/q, BlockType-ordinal w, `W,seed`, auth-less `A,nick`); pure std::, 33
      round-trip/malformed-line checks running in the worlds-only build.
    - **M1** `Net/LineSocket`+`GameClient` — sharp-runtime Socket + Poll(200ms) reader thread +
      ConcurrentQueue inbox; drop → `IsDropped()` instead of Craft's `exit(1)`; loopback CTest
      incl. split-line reassembly.
    - **M2** `Server/GameServer` + `CnaCraftServer` binary — server.py's architecture (accept
      thread, per-client service threads, ONE model thread owning all state), U/E/W handshake,
      P relay, guest nicks, /nick /list /help, version rejection. Two real bugs found by its
      CTest: model-thread id assignment raced service threads (early messages dropped as id 0),
      and close() doesn't wake accept() on Linux (Stop() hung — fixed via shutdown + poke).
    - **M3** world sync — server owns a real `Worlds::World` (validation coherent by
      construction), rowid-keyed incremental chunk responses, Craft's optimistic-edit + revert
      protocol, sign/light sync + break-cleanup; WorldStore grew UpsertBlock/
      LoadColumnEditsSince/raw row reads + the client `key(p,q,key)` cache table (Craft's db.c
      schema); 34-check CTest incl. a full server-restart persistence cycle.
    - **M4** game integration — `--server HOST [PORT]`, pre-store synchronous handshake (seed
      before any column generates; per-server `cache.<host>.<port>.db` so server deltas never
      touch world.db), PollNetworkMessages at both pipeline sites (defer-don't-discard), edit/
      light/sign/position outbound hooks (one RecordedEdits hook covers clicks AND /commands),
      E-driven shared clock, embed-heal on remote blocks. Verified live: a python bot speaking
      the raw protocol built a wall the game rendered, chat flowed to the HUD, 17 P updates
      captured from real key-driven movement.
    - **M5** remote players — `Worlds/RemotePlayer` ports interpolate_player exactly (window
      clamp, saturation, ±π yaw seam; 8 unit checks), `Render/PlayerCube` (Craft's make_player
      cube, new appended atlas tile 59), crosshair nametag as 2D HUD text (Craft's
      SHOW_PLAYER_NAMES; no 3D billboards, matching Craft's own absence). Verified live: the
      'pepa' cube + nametag dead-center in open sky.
    - **M6** chat + mode switching — bare-Enter chat (ONLINE ONLY -- the exact feature NEXT.md
      §9 deferred "until multiplayer actually lands"), unknown-command forwarding (Craft's
      client_talk fallthrough → /list etc. reach the server), `/online HOST [PORT]` +
      `/offline` via SwitchMode (drain in-flight TaskT jobs, unload every column, reconnect,
      right store, respawn through the extracted SpawnPlayerForCurrentMode). Verified live:
      chat round-trip, offline→online cycle with a fresh id in one HUD log.
    - **M7** extras — P-key PIP observation (Draw's pass stack extracted into RenderScene;
      corner viewport + depth-only clear + observed player's interpolated camera; Craft's 'O'
      full-view swap deliberately not ported) and server-side `@nick` PMs (echo to sender,
      readable unknown-nick error, bystander provably excluded — 6 new CTest checks). Verified
      live: one screenshot with the main view, the 'pepa' nametag AND the inset showing pepa's
      own different view.
    Suite totals after item 18: worlds 349, protocol 33, transport 21, server 33, worldsync 34,
    persistence 40 — all green in both build trees.
19. `completed` — **Chunk system redesign: unbounded, streamed world** (CRAFT_PARITY.md
    §3.1/§3.2/§5.2). **User decision (2026-07-10)**: pursue an unbounded world after all
    (supersedes the original `needs_human` framing, which asked whether this was even wanted).
    Planned via `EnterPlanMode` (three parallel research passes + a design-review pass) before any
    code was touched, then implemented in seven phases, each independently built/tested before the
    next started:
    - **Phase 0** — SQLite schema migration: added Craft's real `p,q` chunk-address columns to
      `block`/`sign`, `block`'s unique index becomes `(p,q,x,y,z)`, a new non-unique `(p,q)` index
      on `sign` (mirroring Craft's own asymmetry: `sign` keeps its `(x,y,z,face)` uniqueness).
      Pre-migration `world.db` files fail loudly at open (no silent per-INSERT failures) via the
      existing `CREATE INDEX`-against-missing-columns error path — no migration path, delete and
      regenerate, same precedent as the earlier terrain-formula reset.
    - **Phase 1** — `World` storage rebuilt as a two-level hash map,
      `unordered_map<ColumnKey, array<unique_ptr<Chunk>, WORLD_CHUNKS_Y>>`, keyed by packed
      `(cx,cz)` — unbounded X/Z, Y still fixed (matches Craft's own real behavior: Craft never
      streams Y either). `GenerateColumn`/`IsColumnLoaded`/`UnloadColumn`/`AllocateColumn` new
      public API; `World::Generate(seed)` kept as a legacy whole-region wrapper looping
      `GenerateColumn`, so most of the ~20 pre-existing tests needed zero changes. Ported Craft's
      real tree-canopy chunk-boundary margin check (`world.c`) as a documented, deliberate minor
      regression (a few boundary-adjacent trees that used to spawn no longer do).
    - **Phase 2** — `WorldStore` per-chunk-scoped I/O: `LoadColumnInto`/`LoadColumnSignsInto`/
      `LoadColumnEdits` (`WHERE p=? AND q=?`), matching Craft's own `db_load_blocks`/
      `db_load_signs` exactly.
    - **Phase 3** — `CnaCraftGame` streaming integration, synchronous first (deliberate
      de-risking before adding threading): player `(cx,cz)` tracked every frame,
      `kCreateRadius=6`/`kDeleteRadius=9` chebyshev-distance load/unload with a small per-frame
      budget even pre-threading, spawn moved to world-origin with synchronous force-load
      (mirroring Craft's own `force_chunks`), `chunkRenderers_` became a `ColumnKey`-keyed hash map
      (also fixing a latent correctness bug: the old flat vector was only ever kept in lockstep
      with `World`'s indexing by construction order), signs wired to column load/unload via new
      `SignStore::RemoveAllInColumn`, `PlayerController`'s floor-catch fixed to distinguish "column
      genuinely has no ground" from "column not loaded yet", fog now tracks
      `kCreateRadius * CHUNK_SIZE` instead of a fixed constant (closing the CRAFT_PARITY.md §5.2
      render-radius gap).
    - **Phase 4** — backgrounded generation + meshing via sharp-runtime's
      `System::Threading::Tasks::TaskT` (this project's first use of threading). Discovered
      mid-implementation: `TaskT<T>::getResultProperty()` returns via a `shared_ptr`-reached member
      access, not a named local, so it needs `T` copy-constructible, not just movable — ruled out
      returning a move-only `array<unique_ptr<Chunk>,Y>`; solved via `World::CopyColumn`/
      `AdoptColumnCopy`/`InstallChunkCopy` converting to/from a plain copyable `array<Chunk,Y>`
      snapshot form. Background tasks touch no live `World`/`GraphicsDevice`/`sqlite3*` — only
      plain copyable input/output data, reconstructing a throwaway scratch `World` on the
      background thread. Two real bugs found via real-build Xvfb/xdotool fly-navigation
      verification and fixed before this phase was considered done: (1) completed-but-over-the-
      per-frame-apply-cap jobs were discarded outright instead of deferred, which for meshing
      specifically (dirty flag clears at dispatch time, not completion time) caused permanent
      unfilled rectangular holes in distant terrain; (2) `DispatchMeshingForDirtyChunks` had no
      dispatch cap at all, letting one column's arrival fan out into dozens of concurrent mesh
      tasks. Re-verified via matched-flight-path screenshot comparisons after the fix — clean
      terrain, no holes, thread count bounded, across multiple long fly-out-and-back cycles.
    - **Phase 5** — added the three genuinely new-risk tests this redesign needed (column
      load/unload lifecycle, boundary-remesh correctness — a boundary face is exposed while its
      neighbor column is unloaded and correctly culled once the neighbor loads with a solid block
      across the shared boundary, and a real generate→edit→save→unload→regenerate→reload
      persistence cycle for one column); the ~20 pre-existing `WORLD_SIZE_*`-shaped tests needed no
      changes, already compatible via Phase 1's legacy `Generate` wrapper.
    - **Phase 6** — this documentation pass (CRAFT_PARITY.md §3.1/§3.2/§4.2/§4.3/§5.2, this
      writeup, `README.md`, `NEXT.md`).

    237 checks passing (`cna_craft_worlds_smoke_test` + `cna_craft_persistence_smoke_test`, up from
    225 before this item). Two deliberate, documented player-visible behavior changes: block edits
    and freshly-streamed terrain render 1-2 frames later than the old same-frame synchronous
    rebuild (imperceptible at 60fps), and a handful of trees right at a chunk-column boundary no
    longer spawn (Craft's own real margin-check behavior, previously unreplicated since the whole
    world generated as one unit).
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
23. `completed` — **Terrain-height formula re-port** (CRAFT_PARITY.md §3.3). **User decision
    (2026-07-10)**: re-port Craft's real formula exactly, accepting that it reshapes all existing
    generated terrain. `NoiseGenerator::Height` replaced its old single-additive-`Simplex2` call
    with Craft's actual two-sample multiplicative formula (`f = Simplex2(x*0.01, z*0.01, 4, 0.5,
    2)`, `g = Simplex2(-x*0.01, -z*0.01, 2, 0.9, 2)`, `mh = g*32+16`, `height = f*mh`), verified
    against the checkout. Craft's own `h<=12 -> h=12` sea-level reassignment (which also marks a
    column sand in Craft's source) is folded into `Height()` itself — the height clamp and the
    sand/grass split share one formula in Craft, and keeping `Height()` as a plain `int`-returning
    function (no API signature change) means `World::Generate`'s existing `height <=
    kSandMaxHeight` sand-column check stays correct, since a sandy column's `Height()` now always
    returns exactly the sea level. `World.cpp`'s `kSandMaxHeight` updated from the old scaled-down
    10 to Craft's literal `12`, now that the height range itself matches Craft's scale. No `Worlds/`
    API signature changed, so only the two tests that hardcoded old height-range/threshold numbers
    needed updating (not a wide-reaching test rewrite) — 148 checks total, all still passing.
    Verified visually against a real EasyGL build (terrain, wireframe outline, and a visible tree
    all rendered correctly with the new formula, no corruption or extreme spikes).
24. `completed` — **Minimize remaining Craft-fidelity gaps** (CRAFT_PARITY.md §1.2/§1.4/§1.5/§1.6/
    §1.8/§2.3). **User decision (2026-07-10)**, made after being asked "co dále?" (what's next)
    and given the remaining gap list from CRAFT_PARITY.md: match Craft exactly on every item that
    was previously a pure control-scheme/tuning choice, even where the cna-craft-only alternative
    was arguably more discoverable and already documented as intentional in `README.md`. Five
    changes:
    - **Cursor capture / quit (§1.2)**: Escape now releases the mouse cursor
      (`Mouse::setIsRelativeMouseModeEXTProperty(false)`) instead of calling `Exit()`; left-click
      while released re-captures it instead of breaking/placing/eyedropping on that same click
      (new `cursorCaptured_` member, gates mouse-look and all three mouse buttons in
      `CnaCraftGame::Update`, matching Craft's real `on_mouse_button`/`handle_mouse_input`
      `exclusive` guard exactly). There is no in-game quit key in real Craft at all — confirmed
      `SDL_EVENT_QUIT -> Game::Exit()` is already wired in CNA's `Game::PollEvents` before removing
      the Escape-quit path, so closing the window (Alt+F4/the X button) still works.
    - **Pitch clamp (§1.4)**: `kPitchLimit` changed from an approximate `1.55f` to the exact
      `pi/2`, matching Craft's own `MAX(s->ry, -RADIANS(90))`/`MIN(..., RADIANS(90))` exactly.
    - **Fly speed (§1.5)**: `kFlySpeed` changed from `9.0f` (2x `kMoveSpeed`) to `18.0f` (exactly
      4x), matching Craft's own walk=5/fly=20 ratio.
    - **Pitch-coupled flight (§1.6)**: `PlayerController::Update` now ports Craft's real
      `get_motion_vector` flying branch exactly instead of the prior separate Space/Ctrl vertical
      axis — horizontal speed scales by `cos(pitch)` and gains a `sin(pitch)` vertical component
      (flipped when moving backward) while moving forward/back, full horizontal speed with zero
      vertical component while purely strafing, and Space (`jumpPressed`) unconditionally forces a
      full-speed ascend. `PlayerInput::moveUp` and the Left Ctrl-descend key are gone — **there is
      no dedicated descend key now**, matching Craft (look down + move forward, or look up + move
      backward, to descend). 3 new unit tests (Space-forces-ascend, forward+look-down-descends,
      pure-strafe-has-no-vertical-component).
    - **Reach distance (§2.3)**: `kMaxReach` changed from `6.0f` to `8.0f`, matching Craft's
      hardcoded max hit-test distance exactly.
    Also closed a real gap surfaced while re-reading Craft's `handle_movement` for the flying
    rewrite: Craft's own `if (s->y < 0) s->y = highest_block(s->x, s->z) + 2` floor-catch safety
    net (§1.8) had no cna-craft equivalent at all. New `World::HighestCollidableY(x,z)` (ports
    Craft's own `highest_block`) plus a matching check at the end of
    `PlayerController::Update` close this, including not resetting velocity afterward, matching
    Craft's own (imperfect but faithful) behavior exactly. 1 new unit test (drop a player to
    y=-5 over generated terrain, confirm one `Update()` call snaps them to `HighestCollidableY+2`).
    Total: 5 new unit tests, 169 checks in `worlds_smoke_test.cpp`. **Verified end-to-end against
    a real headless build**: confirmed the process stays alive after Escape (previously it would
    exit), confirmed a clean render with no regressions via screenshot, full test suite re-run (no
    regressions in the substepping/tunneling/jump-height/diagonal-speed physics tests this touches
    adjacent code to). `README.md` §5 updated (Esc behavior, fly controls, mouse-look gating) and
    `CRAFT_PARITY.md` §1.2/§1.3/§1.4/§1.5/§1.6/§1.8/§2.3 updated from their prior stale
    `partial`/`needs_human` statuses to `complete`.
25. `completed` — **Expand the block roster to Craft's full 54-item set** (CRAFT_PARITY.md
    §2.2/§3.7): after the chunk-system redesign (item 19), the two remaining follow-ups were
    Chat/slash-commands (large, needs a design pass) and this — low-value but well-defined content
    additions the item-2/item-22 notes above already deferred as "low value, skipped." User chose
    this over Chat/slash-commands (2026-07-10). Added `Chest` (a plain solid cube, `blocks[CHEST]`
    in Craft's real `item.c` — no special mesh shape, unlike Minecraft's chest) and the 5 flower
    colors beyond the existing `Flower`/yellow (`RedFlower`/`PurpleFlower`/`SunFlower`/
    `WhiteFlower`/`BlueFlower`, reusing `ChunkMesher::EmitPlant`'s existing cross-billboard path
    unchanged), plus all 32 of Craft's `COLOR_00`-`COLOR_31` dye/paint blocks (`Dye00`-`Dye31`,
    plain solid cubes — Craft doesn't name individual dye colors either). New `BlockType` values
    were appended after `Bedrock`, never inserted earlier — `Persistence::WorldStore` persists
    `BlockType` as its raw enum ordinal, so inserting mid-roster would silently reinterpret every
    existing `world.db`'s block types; `Hotbar::kSlots` likewise appends its 38 new slots after the
    existing 16 rather than reordering them, so number keys 1-9 and every existing test assertion
    about specific slots keep meaning exactly what they always have. `Hotbar::SlotCount()` is now
    54, matching Craft's real `item_count` exactly (`item.c`). `TextureAtlas`'s grid grew from 5×5
    to 8×8 tiles to fit the 38 new tiles; the 32 dye-color tiles use real RGB values sampled
    directly from Craft's own shipped `textures/texture.png` (tiles 176-207 there) rather than the
    procedural-placeholder colors every other tile in this atlas uses, since Craft's dye tiles are
    flat swatches with no pattern to speak of anyway — a rare case where matching the real asset
    was easier and more faithful than inventing a placeholder. Also ported Craft's real flower
    color-pick (`world.c`: a second noise sample chooses one of the 6 flower colors,
    `18 + simplex2(x*0.1, z*0.1, 4, 0.8, 2) * 7`), previously skipped when only one flower color
    existed — `World::GenerateFlowersColumn` now scatters all 6 colors through world generation,
    not just placeable via the hotbar. 2 new/revised tests (`TestExpandedRosterBlockDefsMatchExpectedShape`,
    `TestHotbarSelectionAndCycling` rewritten for 54 slots, `TestWorldGeneratesFlowers` rewritten to
    check for any of the 6 colors and confirm more than one color actually appears) — 226 checks in
    `worlds_smoke_test.cpp` (up from 207), persistence suite unchanged at 30. Verified against a
    real EasyGL build under Xvfb: cycled the hotbar through all 54 slots via repeated `E` presses,
    confirmed the HUD correctly showed `#21/54 WhiteFlower` through `#35/54 Dye12`; a screenshot
    during that same session shows naturally-generated orange/yellow and purple flower blooms
    side by side in the world, confirming the color-pick noise actually varies.
26. `completed` — **Player-vs-Craft comparison audit follow-ups, batch 1** (CRAFT_PARITY.md §1.9/
    §3.7/§4.3/§4.1): after item 25, a full player-facing comparison audit against real Craft found
    12 remaining differences; asked back to the user one by one (`AskUserQuestion`, 2026-07-10) for
    a decision on each. Four small/safe items were picked up first, each independently committed:
    - **Arrow-key look speed (§1.9)**: `kArrowLookSpeed`-equivalent rotation rate changed from
      `1.6f * dt` to Craft's own literal `1.0f * dt` (`main.c:2418`, `m = dt * 1.0`), re-verified
      directly against the checkout before changing.
    - **Per-instance plant billboard rotation (§3.7)**: `ChunkMesher::EmitPlant` now takes the
      block's world `x,z` and rotates each quad (and its normal) around the block-local center by
      `Simplex2(seed=0, wx, wz, 4, 0.5, 2) * 360` degrees, porting Craft's real per-instance
      variation (`main.c`) instead of every plant of a given type sharing one fixed orientation.
      The seed is a fixed constant (not the world seed), matching Craft's own non-per-world-seeded
      permutation table for this specific noise use. Verified visually via a real EasyGL build
      (screenshot showing varied grass-blade orientations).
    - **Sign billboard winding (§4.3)**: `SignBillboard` changed from emitting both windings
      (front+back quad, 12 indices) to a single correct winding (6 indices), to remove a
      "ghost text" backface artifact. **Not independently re-verified with a live screenshot** —
      synthetic keyboard text input broke in this sandbox session partway through (confirmed via
      several failed `xdotool type` variants, and confirmed as a real environment regression by
      the fact that even previously-working `/`-command typing failed identically afterward, not
      just this change). Shipped on strong indirect evidence instead (identical winding convention
      already proven correct for every cube face all session) with an honest code comment stating
      it's unverified and a one-line revert path if wrong.
    - **Player-position persistence across sessions (§4.1)**: new `state(x,y,z,rx,ry)` SQLite table
      in `Persistence::WorldStore` (`SavePlayerState`/`LoadPlayerState`, DELETE-then-INSERT
      semantics, matching Craft's real single-row `state` table exactly — `src/db.c`). Craft
      stores eye position directly; cna-craft's `PlayerController` stores feet position
      internally, so `PlayerController::kEyeHeight` (`1.7f`) was made a public constant to convert
      at the load/save boundary in `CnaCraftGame`. On a fresh `world.db` (no saved state), spawn
      behavior is unchanged (deterministic origin-column spawn). Verified end-to-end via a real
      kill-and-relaunch test in Xvfb — screenshot after relaunch shows the exact same resumed view
      — the one verification in this batch that didn't depend on sandbox mouse/keyboard-text
      reliability, since it only needed keyboard movement.
    Commits: `c2fafca` (arrow-key + plant rotation), `a58d456` (sign winding), `eb6cc88`
    (player-position persistence).
27. `completed` — **Light toggle, Ctrl+right-click** (CRAFT_PARITY.md §2.7/§5.1) — the one genuine
    (non-deliberate) gap left from the same 12-item audit as item 26; user chose this as the next
    large subsystem to pick up (2026-07-10, "Přepínání světla"). Research into Craft's real
    implementation (`src/main.c`/`src/db.c`: a per-cell `lights` overlay `Map`, a recursive
    6-directional flood-fill `light_fill` radius 15, blended with ambient occlusion in
    `occlusion()` and baked into two extra per-vertex floats consumed by a custom GLSL shader)
    found that porting it faithfully is **not achievable without new CNA-engine-level work**: all
    three CNA graphics backends (EasyGL/Vulkan/Bgfx) dispatch shader/pipeline selection by
    **hardcoded raw vertex-buffer byte stride**, not by the `VertexDeclaration`'s element list, and
    no existing stride/shader combination in any backend combines `TextureEnabled` +
    normal-based `LightingEnabled` + `VertexColorEnabled` together — the closest proven combo
    (`VertexPositionColorTexture`, stride 24, already used by `SkyDome`/`SelectionOutline`) drops
    normal-based lighting entirely. Presented this tradeoff to the user via `AskUserQuestion`
    rather than silently picking a workaround; user first asked to pause and think, then asked for
    an alternative approach; a proposed **separate additive glow pass** (toggle mechanic +
    persistence 100% Craft-faithful, but the lit block's own faces render via a self-contained
    unlit `VertexPositionColorTexture` pass with a fixed warm-white tint, instead of Craft's real
    light-bleeds-into-neighbors propagation) was accepted ("Ano, tohle zní dobře"). Planned via
    `EnterPlanMode` before implementation, per this project's "genuine architecture change needs a
    plan first" rule. Implementation:
    - `Worlds::Chunk` gained a `lightSources_` boolean overlay array parallel to `blocks_`
      (mirroring Craft's separate `lights` Map, simplified to on/off since this design doesn't
      propagate/attenuate) plus `IsLightSource`/`SetLightSource`, mirroring `GetBlock`/`SetBlock`
      exactly. `Worlds::World` gained matching pass-through accessors (no neighbor-chunk
      dirty-marking, unlike `SetBlock`, since this design's glow doesn't cross chunk boundaries).
    - `Worlds::MeshData` gained a `GlowVertex`/`GlowMeshData` shape (position+UV+tile, no normal)
      and `ChunkMeshData::glow`, a third mesh alongside `opaque`/`transparent`.
      `ChunkMesher::Build` emits a light-source block's exposed faces into `glow` **in addition
      to**, not instead of, its normal `opaque`/`transparent` emission — the block still meshes
      completely normally.
    - `Persistence::WorldStore` gained a `light(p,q,x,y,z,w)` table matching Craft's real schema
      shape exactly (same columns/unique-index pattern as `block`), with `UpsertLight` (stores
      Craft's literal `w=15`/`w=0` values even though only used as a bool here — keeps the
      on-disk schema/data byte-compatible with a real Craft `world.db`) and
      `LoadColumnLightsInto`, wired into both per-column load sites (`LoadColumnSynchronously`,
      `PollGenerationJobs`) alongside the existing block/sign loads.
    - `CnaCraftGame`'s right-click branch gained the missing Ctrl-modified case (Craft's
      `on_light()` vs. plain `on_right_click()`/place split): `Ctrl+right-click` on a breakable
      block toggles its light-source flag and upserts it, gated by the same `IsBreakable` guard
      Craft uses (`is_destructable`).
    - `Render::ChunkRenderer` gained a third vertex/index buffer pair (`glow_`) uploaded via CNA's
      proven `VertexPositionColorTexture` combo, and `DrawGlow`. `CnaCraftGame::Draw` adds a third
      render pass after opaque/transparent: flip `effect_` to `VertexColorEnabled=true`,
      `LightingEnabled=false` (same flip-draw-flip-back pattern as `SkyDome`/`SelectionOutline`),
      draw every chunk's glow mesh with a fixed warm-white tint (full brightness, day/night
      independent), flip back.
    **Deliberate non-goals** (documented as Craft deviations, not gaps): no propagation to
    neighboring faces, no 0-15 intensity falloff (on/off only), no interaction with day/night
    ambient beyond the lit block's own faces — all direct consequences of the engine constraint
    above, out of scope without new engine work.
    21 new checks (`TestChunkAndWorldLightSourceOverlay`, `TestChunkMesherEmitsGlowMeshForLightSourceBlocks`
    in `worlds_smoke_test.cpp`; a lights round in `persistence_smoke_test.cpp`) — 288 checks in
    `worlds_smoke_test` (up from 267), 40 in `persistence_smoke_test` (up from 30), all passing.
    Verified via a clean `build-worlds` + full EasyGL build, and a real Xvfb regression screenshot
    confirming the new (empty, since nothing is toggled yet) glow pass doesn't alter ordinary
    terrain rendering. **Live confirmation that a toggled block actually glows was not obtained**
    — Ctrl+right-click needs a working synthetic mouse click, which (per this project's
    long-documented sandbox limitation) remains unreliable here — coverage relies on the unit
    tests above plus code review, same tradeoff already accepted for other click-gated features
    (e.g. item 17's `/cube`-style commands).
28. `completed` — **Fix invisible window on startup + a glow-pass performance regression**
    (CRAFT_PARITY.md §3.2/§2.7), both user-reported (2026-07-10, "cna craft nabehne neviditelne
    okno proc? je to na easy gl i vulkanu" / "cna craft je navic nejaky zpomaleny") after item 27
    shipped. Investigated by reproducing directly rather than guessing:
    - **Invisible window**: `CnaCraftGame::Initialize()` force-generated+meshed every column
      within `radii_.createRadius` (169 at the default radius of 6) fully synchronously before the
      game loop's first `Update()`/`Draw()` — mirroring Craft's own startup `force_chunks` call
      literally. Timed this directly: ~14 seconds wall-clock in this project (SQLite queries +
      noise generation + CNA-abstraction GPU uploads per column cost far more per-column than
      Craft's raw C/GL path), during which the SDL window existed but had never presented a single
      frame — several window managers/compositors (confirmed reproducible in this project's own
      Xvfb sandbox, not just theoretical) render an unpresented window as blank/black or mark it
      "not responding." Identical on EasyGL and Vulkan since `Initialize()` is 100% engine-agnostic
      (`Worlds::` layer only — the graphics backend never enters into it). **User decision
      (2026-07-10)**, offered a smaller-radius partial fix vs. the full fix: use the
      already-existing backgrounded `TaskT` streaming pipeline (built in item 19 phase 4) from
      frame 1 instead of a special-cased synchronous path. Removed the force-load loop and its two
      now-dead-code-only helpers (`LoadColumnSynchronously`, `RebuildDirtyChunks`) entirely —
      `UpdateStreaming` (already called every `Update()`) discovers the player's spawn column is
      unloaded on frame 1 and dispatches it through the normal pipeline, so `Draw()` presents a
      real frame (sky/fog/HUD) immediately and terrain pops in progressively over about a second.
      Safe without Craft's "never see an ungenerated void" guarantee thanks to the existing
      floor-catch safety net (item 24). Verified by timing the fix the same way as the bug:
      time-to-first-non-black-frame dropped from ~14s to under 1s (real Xvfb screenshots at fixed
      intervals after launch), confirmed via full clean EasyGL build + real-build screenshot
      showing correct terrain/HUD rendering with no regression.
    - **Glow-pass performance regression**: item 27's `Draw()` added an unconditional third full
      pass over every loaded `ChunkRenderer` (map iteration + a `BoundingFrustum::Intersects` test
      per renderer, every frame) on top of the pre-existing opaque+transparent passes, even though
      virtually no world ever has any block actually lit (nobody has toggled a light). New
      `CnaCraftGame::glowChunkCount_` tracks how many loaded chunks currently carry a non-empty
      glow mesh, updated incrementally at the two places that can change it (`PollMeshJobs`'
      `ApplyMesh` call, `UnloadColumn` — the latter needed so unloading a lit chunk doesn't leak
      the count upward forever), and `Draw()` now skips the entire glow pass — not just the
      already-free draw calls, the map iteration and frustum tests too — whenever it's 0. New
      `ChunkRenderer::HasGlow()` accessor backs this.
    - **Investigated but not changed**: `sharp-runtime`'s `TaskT::Run` wraps
      `std::async(std::launch::async, ...)`, which spawns a genuine new OS thread per call rather
      than drawing from a bounded pool (confirmed by reading `sharp-runtime`'s source, not just the
      existing code comments describing this) — up to 10 new threads/frame (2 generation + 8
      meshing dispatch caps) while actively streaming into unexplored territory. This is a
      pre-existing, already-documented architectural characteristic of item 19 phase 4, not
      something either user report's root cause was traced to — left as-is rather than
      speculatively rewritten into a real thread pool without concrete evidence it's a bottleneck.
    No `Worlds/`-layer code touched by either fix, so the existing 288+40 unit tests are unaffected
    (re-ran to confirm: unchanged, all passing). Both fixes are pure `CnaCraftGame`/`Render`-layer
    changes, verified only via real builds (this layer isn't unit-testable, same as every other
    `CnaCraftGame.cpp` change this project has made).
29. `completed` — **Switch flight/look controls from Craft-parity to Minecraft-style**
    (CRAFT_PARITY.md §1.6/§1.9), user decision 2026-07-10 ("uprav ovládání cna craft aby se to
    ovládalo asi jako minecraft, ten craft styl ovládání me hodně sere" — reported right after
    trying item 24's earlier Craft-fidelity flying/arrow-look changes live). A genuine, informed
    reversal of an explicit choice made earlier the same session (item 24), not a bug fix — the
    user tried the Craft-accurate scheme hands-on and found it worse, and this project's own
    stated goal ("port Craft to CNA, ideally no player-visible difference") always deferred to
    what the actual player wants over literal fidelity when the two conflict. Confirmed scope via
    `AskUserQuestion` (multi-select) rather than guessing, since several independent control axes
    were plausibly in play:
    - **Flying (selected)**: `PlayerController`'s pitch-coupled `get_motion_vector` port (item 24)
      replaced with Minecraft's own creative-flight scheme — Space always ascends, new
      `PlayerInput::descendPressed` (Left Shift) always descends, both independent of pitch;
      holding both cancels to zero net vertical movement; horizontal fly speed no longer scales by
      `cos(pitch)` (always full speed). `CnaCraftGame::Draw`'s existing Left-Shift-zoom feature
      (unrelated, predates this) now gates on `!player_->IsFlying()` so descending doesn't also
      zoom the camera in — an unwanted side effect purely from both features sharing one key,
      resolved without being asked since leaving it would have undermined the whole point of the
      change.
    - **Arrow-key look (selected)**: removed entirely. Craft has it as a keyboard alternative to
      mouse-look; Minecraft has no such binding, and the user found it an unwanted extra way to
      nudge the camera. Mouse-look is now the only way to turn.
    - **Hotbar scroll-wheel cycling (selected)**: already implemented (`CnaCraftGame::Update`,
      matches Craft's own `on_scroll` *and* Minecraft's hotbar scroll — both real games already
      agreed on this one) — no code change needed, confirmed by reading the existing wiring rather
      than assuming.
    - **Sprint (not selected)**: left as Craft/cna-craft's existing single walk speed, no
      double-tap-W sprint added.
    4 unit tests rewritten in `TestPlayerControllerFlyingTogglesGravityAndFreeVerticalMovement`
    (Space-ascends, Shift-descends, both-cancel-to-zero, horizontal-speed-independent-of-pitch,
    replacing the old pitch-coupled-specific assertions) — 289 checks in `worlds_smoke_test` (up
    from 288), persistence suite unchanged at 40. `README.md` §5's control table and
    CRAFT_PARITY.md §1.6 updated; §1.6's notes preserve the full same-day back-and-forth history
    (Minecraft-style → Craft-exact → Minecraft-style again) so a future reader sees this was two
    deliberate, informed decisions, not drift. Verified via clean `build-worlds` + full EasyGL
    builds and a real Xvfb screenshot confirming the world still renders correctly; **the actual
    key bindings (Tab/Space/Shift/arrow-key-removal) could not be live-verified this session** —
    synthetic keyboard input, including previously-reliable movement/action keys, stopped reaching
    the app window in this sandbox partway through (same session-wide flakiness already documented
    for text input and mouse clicks, now apparently extending to movement keys too) — coverage
    relies on the rewritten unit tests (which exercise the real `PlayerController::Update` physics
    directly, not a mock) plus code review of the input-wiring glue, which mechanically mirrors
    every other key binding already shipped this session.
30. `completed` — **Fix a real rendering-corruption bug: exact-π/2 pitch clamp caused a degenerate
    view matrix** (CRAFT_PARITY.md §1.4), user-reported immediately after item 29 shipped
    ("cna-craft je zcela rozbity", with a screenshot of terrain rendering shredded into diagonal
    streaks on both EasyGL and Vulkan). Found by reading `Matrix::CreateLookAt`
    (`cna/src/Microsoft/Xna/Framework/Matrix.cpp`) rather than guessing: it computes
    `Cross3(cameraUpVector, forward)` as its first step. At pitch exactly ±π/2,
    `PlayerController::LookDirection()` becomes exactly parallel to `Vector3::Up`, that cross
    product collapses toward the zero vector, and normalizing a near-zero vector produces a
    numerically garbage view basis — corrupting every vertex transform for the whole frame.
    Confirmed numerically (both in a standalone Python check of the same cross-product math and a
    new C++ unit test) that at the literal `kPitchLimit=π/2`, the cross-product magnitude is
    ~6×10⁻¹⁷ (pure float32 noise, not a meaningfully oriented vector), vs. ~0.01 after backing the
    limit off by a small epsilon. This was a **latent bug exposed by two of today's own earlier
    changes compounding**: item 24 tightened the pitch clamp from an approximate 1.55 rad to the
    literal exact π/2 (making the unsafe value trivially reachable — any player tilting the camera
    all the way up/down lands exactly on it, not a rare edge case), and item 29's Minecraft-style
    flying made "look straight up while ascending" — literally the first thing a player tries when
    testing new Space-to-fly controls — a completely natural move, which is almost certainly how
    the user hit it within moments of trying item 29. Craft's own hand-rolled camera matrix never
    goes through an equivalent cross-product step, which is why the exact-π/2 clamp is safe there
    but not here. Fix: `kPitchLimit` is now `π/2 - 0.01f` instead of the literal value — visually
    indistinguishable from looking exactly straight up/down, but keeps `CreateLookAt`'s
    cross-product numerically well-conditioned. This is now a deliberate, permanent deviation from
    Craft's literal pitch-clamp value (CRAFT_PARITY.md §1.4 updated accordingly), not a fidelity
    gap — safety against a real engine-level footgun takes priority over an exact constant match.
    New `TestPlayerControllerPitchClampAvoidsDegenerateLookDirection` (2 checks) asserts
    `LookDirection()`'s horizontal component stays well above float32 noise level at the clamp in
    both directions — 291 checks in `worlds_smoke_test` (up from 289), persistence suite unchanged
    at 40. Verified via clean `build-worlds` + full EasyGL builds; **could not be re-verified live
    against the user's original repro** (their real machine, not this sandbox) — the fix is
    grounded in the exact engine-level mechanism read directly from CNA's `Matrix.cpp` source plus
    independently-confirmed numerics, not a guess, but the user should confirm the shredded-terrain
    symptom is actually gone after pulling this commit.
31. `completed` — **Fix player entombment at spawn (WASD dead, torn rendering) + per-frame SQLite
    fsync stutter** (CRAFT_PARITY.md §3.2/§4.1), user-reported after pulling item 30 ("obraz je
    stale trhany wasd a sipky neumoznuji chuzi" — image still torn, WASD/arrows don't walk). Both
    were regressions from this same day's own work, and they compounded:
    - **Entombment (the WASD/torn-image bug)**: item 28's invisible-window fix removed ALL
      synchronous spawn loading — the player was created and gravity ran from frame 1 while the
      ground under their feet didn't exist yet, so they free-fell through the void; the spawn
      column arrived asynchronously ~a second later (made worse by `UpdateStreaming`'s raster-order
      dispatch, which loaded the FAR CORNERS of the radius-6 square before the player's own column,
      ~85th of 169) and materialized around the falling player, entombing them inside solid blocks.
      Axis-separated collision rejects every move from inside terrain — WASD completely dead — and
      the camera sits inside geometry — torn/shredded rendering. Worst part: item 41's every-frame
      position save persisted the entombed position into `world.db`, so restarting with fixed code
      restored the player still entombed; the bug outlived every code fix until the data itself is
      healed, which is why the user's symptoms survived pulling items 28-30. Three-part fix:
      `Initialize()` synchronously loads exactly ONE column (the player's own, ~80ms, imperceptible
      vs. item 28's 14s for 169 — `LoadColumnSynchronously` re-added after item 28 deleted it);
      `UpdateStreaming` now dispatches nearest-first (chebyshev-sorted candidates) instead of
      raster order (also fixes far-corners-pop-in-first, a visual wart of its own); and new
      `PlayerController::IsEmbedded` + `CnaCraftGame::HealPlayerIfEmbedded` snap an inside-terrain
      player to the surface (preserving yaw/pitch/flying) at the only two places entombment can
      arise — `Initialize()` (heals poisoned `world.db` files from the live regression) and
      `PollGenerationJobs`' column-apply step (a column materializing around a player who
      walked/flew into not-yet-loaded territory at ground level). The existing floor-catch (item
      24) only covers y<0 and structurally cannot catch entombment at y>0 — these are complements,
      not overlap.
    - **Stutter ("trhaný obraz"/earlier "zpomalený")**: `SavePlayerState` ran every frame, and each
      call was a DELETE + INSERT as two separate implicit SQLite transactions — up to 120 disk
      fsyncs per second, a constant frame hitch on ordinary hardware. Invisible in this project's
      own sandbox verification (fast disk), which is why item 41 shipped it unnoticed. Fixed by
      throttling to one save per second (`playerStateSaveAccumulator_`; losing ≤1s of position on a
      crash is nothing) and wrapping the DELETE+INSERT in one explicit transaction (one fsync per
      save, not two).
    4 new checks (`TestPlayerControllerIsEmbeddedDetectsOverlapWithSolidTerrain`:
    buried/standing/unloaded-column cases) — 295 in `worlds_smoke_test` (up from 291), persistence
    unchanged at 40. **Verified end-to-end in the sandbox, including the poisoned-data path**: a
    `world.db` deliberately corrupted via sqlite3 to hold an inside-terrain position (exactly what
    the user's file holds) printed the heal message on launch, stood the player on the surface, and
    self-corrected the saved row within a second; a fresh-world launch screenshot shows a normal
    ground-level first-person view with the targeted-block outline working. The user does NOT need
    to delete `world.db` — the heal is automatic.
32. `completed` — **Port both halves of Craft's cloud interaction guards** (CRAFT_PARITY.md
    §2.5/§2.6), user-reported 2026-07-10 ("proc mohu bloky stavet na mracich?" — why can I build
    blocks on clouds?). Honest history first, since the user asked whether this had been promised:
    clouds were NOT among the 12 differences in this session's player-facing audit and no explicit
    promise about them was made today — but CRAFT_PARITY.md §2.6 (written in a prior session)
    quoted both halves of Craft's placement guard in its Craft-behavior line, then only listed the
    player-overlap half on the cna-craft side, and still said `complete`. That was a genuine audit
    error: the implicit claim "placement matches Craft" was false. Two real gaps, from Craft's
    `item.c`/`main.c` (re-verified against the checkout before changing anything):
    - **Placement (`is_obstacle`, the reported bug)**: Craft's `on_right_click` requires
      `is_obstacle(hw)` — the block being placed AGAINST must be an obstacle, and
      `is_obstacle(CLOUD)==0` (plants too), so nothing can be built on/against clouds or plant
      billboards in real Craft. cna-craft's `tryPlaceBlock` only had the player-overlap half;
      the raycast happily hits a Cloud (solid/hit-testable), so placement against it succeeded.
      Fixed with one guard line: `if (!world_.IsCollidable(hit...)) return;` — `IsCollidable` is
      this project's exact `is_obstacle` equivalent (already false for Cloud and plants), so the
      single check covers clouds AND plants, precisely like Craft's own predicate.
    - **Breaking (`is_destructable`, found while fixing the above)**: `is_destructable(CLOUD)==0`
      in Craft — clouds can't be mined. Cloud's `BlockDef` had wrongly left `breakable` at its
      default `true`. Flipping it to `false` also automatically corrected two more paths that
      route through the same `IsBreakable` predicate: the light toggle (its guard mirrors
      `on_light`'s `is_destructable` check — clouds no longer accept lights, matching Craft) and
      `WorldEditor::PaintBlock` (ports `builder_block`, whose erase is `is_destructable`-gated —
      an Air paint now leaves a cloud in place while a non-Air paint still overwrites it, Craft's
      exact net behavior).
    4 new checks (Cloud unbreakable + Stone-control + both PaintBlock-over-cloud behaviors) — 299
    in `worlds_smoke_test` (up from 295), persistence unchanged at 40. Verified via clean
    `build-worlds` + full EasyGL builds; the placement-guard wiring itself is CnaCraftGame glue
    over the already-unit-tested `IsCollidable` predicate (same verification split as item 21's
    Ctrl+click-as-place), and live click verification remains unavailable in this sandbox (mouse
    clicks unreliable, long-documented).
33. `completed` — **Textured sky dome** (CRAFT_PARITY.md §5.3/§5.2), user decision 2026-07-10
    ("Přidat texturu oblohy", chosen from the remaining audit items when asked what's next).
    Ports Craft's real sky rendering (`render_sky` + `textures/sky.png` +
    `shaders/sky_*.glsl`, all re-read from the checkout before implementing) onto stock
    `BasicEffect`:
    - **New `Render::SkyTexture`**: a 64×16 gradient texture whose colors are sampled directly
      from Craft's own shipped `sky.png` (ImageMagick box-filter downscale, embedded as a
      constants table — the same real-asset-colors approach as item 25's dye blocks, keeping the
      "no binary art assets" convention while matching the real look, dawn/dusk orange bands
      included). Layout matches Craft's sampling exactly: U = time of day, V = elevation
      (`t = 1 - acos(y)/pi`, 0.5 = horizon), `LinearClamp` = Craft's GL_LINEAR+GL_CLAMP_TO_EDGE.
      `SampleSkyColor(u,v)` is the CPU-side bilinear mirror, feeding the clear color and
      `FogColor` from the same gradient's horizon band — so fog now fades toward the *real* sky
      color at the current time of day (Craft samples per fragment at each fragment's elevation;
      BasicEffect's single flat fog color per draw makes the horizon band stand in for all
      elevations, a documented simplification in §5.2).
    - **`Render::SkyDome` rewritten**: full UV sphere (was a hemisphere — below-horizon sky now
      shows Craft's real below-horizon band while flying), `VertexPositionColorTexture` with
      white colors (the proven stride-24 unlit Texture+VertexColor combo, item 27's constraint
      notes apply). Craft passes time-of-day as a shader uniform; `BasicEffect` has no such slot,
      so `Update(device, timeOfDay)` bakes it into every vertex's U and re-uploads per frame —
      the same per-frame upload cost the old vertex-color gradient version already paid.
    - **`Worlds::ComputeTimeOfDay`** split out of `ComputeDaylight` (Craft's `time_of_day()`,
      unit-tested). Found and fixed a real pre-existing mismatch in the process: the degenerate
      `dayLength<=0` branch used to return a literal 0.5 *brightness*; Craft pins the *timer* to
      0.5 (noon) and computes brightness from it (≈1.0, full daylight) — the old test asserting
      0.5 documented behavior that matched nothing in Craft, now corrected.
    - **Game clock now starts at `day_length/3`** (mid-morning), matching Craft's
      `glfwSetTime(g->day_length / 3.0)` (`main.c:2582`) — previously cna-craft started at
      literal midnight, itself an unnoticed parity gap that explains why every screenshot this
      project ever took looked dark.
    8 new/updated checks (`ComputeTimeOfDay` wrap/noon/midnight/degenerate + the corrected
    daylight-at-degenerate-day-length assertions) — 303 in `worlds_smoke_test` (up from 299),
    persistence unchanged at 40. Verified via clean builds + real Xvfb screenshots at two points
    of the time axis: pre-offset launch at t≈0 correctly showed the near-black midnight sky;
    post-offset launch shows a real light-blue day sky immediately (verifying texture path +
    morning start at once). **The dawn/dusk orange bands were not directly screenshot-verified**
    — the sandbox's software renderer runs the fixed-step game clock slower than wall time
    (CNA's `TotalGameTime` advances per executed update; Craft uses wall-clock `glfwGetTime()` —
    a nuance invisible at real 60fps, noted in §5.3), putting dusk ~15+ real minutes out — they
    come from the same numerically-verified color table as the two verified points.
34. `completed` — **F11 fullscreen toggle**, user request 2026-07-10 ("jak v cna-craft udelat
    fullscreen?" — there was previously no way at all). Not a Craft key: real Craft has only a
    compile-time `FULLSCREEN` config flag (`src/config.h`), no runtime toggle — F11 is
    Minecraft's binding, consistent with the item-29 Minecraft-style controls direction. One
    edge-triggered branch in `CnaCraftGame::Update` calling CNA's existing
    `GraphicsDeviceManager::ToggleFullScreen()` (which flips `IsFullScreen` and applies changes
    itself). `README.md` §5 updated. Pure input-wiring glue over an existing engine API (same
    verification class as item 21) — verified via a clean build; the actual mode switch needs a
    real window manager to confirm visually, which the user can do with one keypress.
35. `completed` (2026-07-11) — **Fix upside-down flower sprites**, user report ("ohledne kvetin -
    jsou obracene"). Root cause in `Render/TextureAtlas.cpp`'s `FlowerPattern`: the art code was
    written in image-editor "pixel row 0 = top" orientation, but `MapAtlasUv` maps `localV=0` —
    a quad's BOTTOM vertices per `ChunkMesher`'s `kUv` — to pixel row 0, so low rows render at
    the quad's bottom: blooms sat on the ground with stems pointing up. The flower is the
    atlas's only vertically-directional tile (every cube pattern is vertically symmetric, tall
    grass's blades run full-height), which is why nothing else ever exposed the convention
    mismatch — present since the flower tile was first drawn, NOT an item-12 AO regression
    (AO touched neither UVs nor the atlas). Fix: one mirrored coordinate (`fy = kAtlasTileSize -
    1 - y`) inside `FlowerPattern`, with the V-convention documented at the flip site so the
    next directional tile doesn't repeat this. Verified via a staged-world.db lineup of all six
    flower colors + tall grass before/after under Xvfb: blooms now on top, stems rooted,
    grass unchanged.

36. `completed` (2026-07-11) — **Web build (Emscripten/WebAssembly)**, user request ("zkus udelat
    webovy emscripten build"). The game now builds to wasm and runs in a browser on WebGL 2 via
    CNA's EasyGL backend — terrain streaming, baked AO, the day/night sky dome and the full HUD
    all intact; verified end to end in headless Chrome (WebGL 2.0 / OpenGL ES 3.0 context,
    walked with W, toggled fly with Tab, ascended above the clouds with Space, screenshots at
    each step). Emscripten-specific work, all of it isolated and none of it touching the native
    paths:
    - **Threading**: sharp-runtime's `TaskT` deliberately throws on Emscripten (no `std::async`
      there), so `DispatchColumnGeneration`/`DispatchMeshingForDirtyChunks` take an
      `#ifdef __EMSCRIPTEN__` path that generates/meshes synchronously on the main thread —
      reusing `LoadColumnSynchronously` and `ChunkRenderer::Rebuild`, both already-proven code
      paths — under the same per-frame caps. Terrain pops in chunkier than the native
      background streaming; nothing else changes.
    - **Persistence**: no SQLite on wasm. `Persistence/WorldStoreStub.cpp` implements the same
      interface as no-ops (compiled INSTEAD OF `WorldStore.cpp`), exercising the store's own
      existing open-failure degradation — so game code needs zero platform branches. The one
      semantic that had to be preserved: `SaveEdits` still clears `RecordedEdits`, or they would
      accumulate unboundedly.
    - **Multiplayer**: `CnaCraftServer` and the socket tests are excluded from the wasm build (a
      browser page can't accept TCP connections); the web build is offline single-player. The
      protocol/transport code still compiles — `--server` simply has nothing to connect to.
    - **Two real browser-platform bugs found and fixed**: (1) `-sALLOW_MEMORY_GROWTH` backs the
      wasm heap with a *resizable* ArrayBuffer, and current Chrome rejects views of resizable
      buffers in BOTH `crypto.getRandomValues` (aborted startup before `main()`) and WebGL's
      `texImage2D` (aborted the first texture upload). Fixed by dropping memory growth for a
      fixed 512 MB heap (sidesteps the whole class) plus a `web/pre.js` shim wrapping
      `getRandomValues` for belt-and-braces. (2) The default context was WebGL 1, but EasyGL's
      shaders are `#version 300 es` — fixed with `-sMIN_WEBGL_VERSION=2`.
    - **`web/shell.html`**: a real full-window shell with pointer-lock-on-click (the game runs in
      relative-mouse mode) and a controls legend, replacing the toolchain's 800×480 demo page.
    - CMake: `emcmake` configure works with no extra flags; `embuilder build zlib` is the one
      manual prerequisite (sharp-runtime links zlib). Native builds and all six CTest suites
      verified unchanged afterward.

### 12.2 Deliberately not re-litigated this session

Per CRAFT_PARITY.md, these are `complete` (functionally equivalent to Craft) and need no task:
main game loop (§1.1), mouse look (§1.4), zoom/ortho (§1.9 — its arrow-look half was removed by
item 29, a deliberate Minecraft-style deviation, not still Craft-parity), raycast algorithm (§2.3,
arguably better than Craft's), collision rules for solid/transparent/collidable (§2.8), face
culling (§3.4), mesh generation strategy (§3.5), transparent-block rules (§3.6), trees (§3.8),
clouds (§3.10), day/night lighting (§5.4).

Three former entries here — window/cursor-capture-vs-quit-on-Escape (§1.2) and walking/flying
control scheme (§1.6), resolved by item 24 (2026-07-10 user decision to match Craft exactly on
both, overriding their prior "already documented as intentional" status), and chunk system
being fixed-size rather than infinite (§3.1/§3.2), resolved by item 19 (2026-07-10 user decision
to pursue the unbounded/streamed world after all) — are now fully implemented. None remain open
in this category as of this note.
