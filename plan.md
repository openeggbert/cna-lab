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
- [ ] Caves/overhangs: a second, lower-frequency 3D density noise carving out solid terrain
      (Craft's `world.c` combines a 2D heightmap with a 3D noise term for overhangs).
- [ ] Trees and plants placement pass after heightmap generation (Craft: `src/world.c` places
      `WOOD`/`LEAVES` clusters and `TALL_GRASS`/flower types on top of grass columns).

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
      tile: Glass (needed transparency — since added, see "Transparency for glass"; `Hotbar` now
      has 13 slots), Leaves (transparent like Glass, but not yet added — no reason left to defer
      other than not having been picked up), Cloud (needs non-solid-but-visible semantics, see
      "Clouds"), Chest/tall grass/flowers (need non-cubic geometry, see "Non-cubic plant
      geometry"), the 32-entry flat-color palette (low value, skipped).
- [ ] Real texture atlas image (replace `Render::BuildPlaceholderAtlas`'s flat-color placeholder
      with an actual authored/generated `texture.png`-style atlas, same layout convention as
      `Craft/textures/texture.png`).
- [ ] Non-cubic "plant" geometry: cross-quad billboards for grass/flowers (Craft: `src/cube.c`
      `make_plant`) — `ChunkMesher` needs a second emission path alongside the cube-face path in
      §4.
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
- [ ] Clouds: thin, non-solid, unlit decorative blocks near the world ceiling (Craft: `CLOUD`
      block type, discarded in orthographic mode per `block_fragment.glsl`).

### 11.3 Sky & lighting

- [ ] Day/night cycle: a `daylight` value driven by a game-time clock, feeding into `BasicEffect`
      lighting (or a custom shader later) the way `block_fragment.glsl`'s `daylight` uniform does.
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
