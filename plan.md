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
- **Deterministic, position-seeded noise** for terrain height — Craft uses Simplex noise; this
  plan uses a self-contained value-noise implementation for the same purpose (§3), with Simplex
  noted as a possible drop-in upgrade.
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

**`NoiseGenerator`** — deterministic, dependency-free 2D fractal value noise (integer hash →
smoothstep-interpolated lattice noise, a handful of octaves summed). No external noise library
needed; the entire prototype's terrain variety comes from this ~40-line file. Seeded so the same
seed always reproduces the same world (useful for the smoke test in §8).

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
checked out as siblings, and the `EASYGL` backend (`-DCNA_GRAPHICS_BACKEND=EASYGL` — 3D is not
implemented on `SDL_RENDERER`/`BGFX` yet, per `house3d_demo.cpp`).

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

- 3D rendering only works on the `EASYGL` backend today; `SDL_RENDERER`/`BGFX` throw "3D not
  supported", `VULKAN` is an incomplete scaffold. This prototype targets `EASYGL` only.
- No occlusion/frustum culling in the engine — acceptable at this prototype's fixed
  `128×64×128`-block/32-chunk scale; would need addressing before scaling to a larger or
  streamed world (§9, M7).
