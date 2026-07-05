# Analysis: Is it realistic to build at least a Minecraft-clone prototype on CNA?

**Short answer: yes, and it's closer than one might expect — CNA already has exactly the scaffolding a prototype would need.**

## What CNA is

A C++ reimplementation of the XNA 4.0 programming model (FNA/MonoGame-style), built on SDL3, with a pluggable graphics backend layer (`SDL_RENDERER`, `EASYGL`/OpenGL, `BGFX`, `VULKAN` — Vulkan still a work-in-progress scaffold). It is not a finished game, it's a framework/runtime.

## Key finding: `examples/house3d_demo.cpp` already does 80% of what a voxel prototype needs

This isn't just theoretical XNA API compatibility — it's a **working, exercised first-person 3D demo** that already includes:

- `Vector3`/`Matrix`/`Quaternion` math, a `VertexBuffer`/`IndexBuffer`/`BasicEffect` pipeline (real GPU rendering of boxes)
- A first-person camera with yaw/pitch (mouse/keyboard)
- WASD movement + strafe, jump, gravity
- AABB collision against solid boxes (exactly the principle Minecraft uses for player-vs-world collision)
- Game/Fly mode switching

That's essentially a complete player-control loop and render loop for a world made of cubes — the core of a Minecraft-like engine.

The framework also provides the supporting layer: `Texture2D`, `VertexPositionNormalTexture` (textured + lit vertices), `BasicEffect` with texturing and default lighting (exercised in `easygl_basiceffect_default_lighting_test.cpp`, `easygl_basiceffect_fog_test.cpp`), `Ray`/`BoundingBox` (for block-picking raycasts), and full keyboard/mouse/gamepad input.

## What's missing (would need to be written as game code, not engine code)

There are no "voxel"/"chunk"/"minecraft" references anywhere in the repo — nobody has started this yet. Missing pieces:

1. **Voxel data structure** (chunk = 3D array of block IDs) — pure application code
2. **Chunk meshing** (naive per-block quads, or greedy meshing to avoid drawing hidden faces) — new logic on top of `VertexBuffer`/`IndexBuffer`
3. **Texture atlas** for block types — UV mapping; the engine already supports textures/UVs
4. **Terrain generation** (Perlin/Simplex noise) — not in CNA, but a small standalone module (a few dozen lines)
5. **Raycast block breaking/placing** — `Ray`/`BoundingBox` primitives exist; a voxel-grid DDA raycast is new logic
6. **Occlusion/frustum culling for performance** at larger view distances — not needed for a small prototype (a handful of chunks), but scaling to an "infinite" world would require extra work

## Caveats to keep in mind

- **3D currently only works on the EasyGL backend.** Per the comment in `house3d_demo.cpp`, `SDL_RENDERER` and `BGFX` throw "3D not supported" for 3D; the Vulkan backend is generally an incomplete scaffold. A prototype would need to build with `-DCNA_GRAPHICS_BACKEND=EASYGL`.
- This isn't a major limitation — the README lists EasyGL as verified on Linux, Windows (cross-compiled), **Emscripten/web, and Android**, so platform reach stays reasonably broad.
- The known bug in `known_bugs.md` (SpriteBatch multiple Begin/End) is a 2D-layer issue and doesn't affect 3D voxel rendering.

## Verdict

Realistic for a **prototype** (small world, a handful of chunks, basic block breaking/placing, walking/collision) — definitely yes, likely on the order of days to a few weeks of work, since the hardest part (3D render pipeline + camera + movement + collision) already exists and is exercised in `house3d_demo`. A full game (infinite world, performance at large view distances, multiplayer block-sync over the `Net`/ENet layer CNA already has) would be a substantially bigger project, but nothing in the architecture blocks it — it's simply a matter of missing game-logic volume, not missing engine capability.
