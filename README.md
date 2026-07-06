# CNA Craft

A Minecraft-like voxel world prototype built on [CNA](https://github.com/openeggbert/cna), a C++
reimplementation of the XNA 4.0 programming model.

## 1. Overview

CNA Craft is a small first-person voxel-world prototype: chunked block terrain, procedural
terrain generation, textured/lit cube meshing, and block breaking/placing, all built directly on
CNA's `Microsoft::Xna::Framework` API (`Vector3`/`Matrix`, `VertexBuffer`/`IndexBuffer`,
`BasicEffect`, `Texture2D`, `Keyboard`/`Mouse`).

It reuses the first-person camera, movement, and AABB-collision approach already proven in CNA's
own `examples/house3d_demo.cpp`, and adds the voxel-specific layer on top: chunk storage, greedy
meshing, a block texture atlas, noise-based terrain generation, and a DDA voxel raycast for
picking blocks.

See [plan.md](plan.md) for the detailed architecture and implementation roadmap, and
[analysis.md](analysis.md) for the feasibility analysis this project started from.

Several architectural choices (exposed-face-only meshing, chunk-boundary neighbor overlap,
delta-based world persistence) are consciously modeled on
[fogleman/Craft](https://github.com/fogleman/Craft) (MIT license) — see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full attribution and license text.

## 2. Status

Early prototype / active development. Not a finished game.

## 3. Dependencies

CNA Craft builds against sibling checkouts, following the same convention as
[galaxy-eggbert](https://github.com/openeggbert/galaxy-eggbert):

```text
openeggbert/
├── cna-craft/        (this repo)
├── cna/              (engine — https://github.com/openeggbert/cna)
└── sharp-runtime/     (utility/runtime layer, pulled in automatically by cna's CMakeLists.txt)
```

- CMake 3.21+
- A C++23-capable compiler (GCC 12+ or Clang 15+)
- `../cna` and `../sharp-runtime` present next to this repo (or pass `-DCNA_HOME=<path>`)

3D rendering is implemented on three of CNA's backends — pick any one at configure time:
`EASYGL` (OpenGL), `VULKAN`, or `BGFX`. `SDL_RENDERER` remains 2D-only.

## 4. Build

```bash
git submodule update --init --recursive   # only needed the first time, inside ../cna
cmake -S . -B build -DCNA_GRAPHICS_BACKEND=EASYGL   # or VULKAN, or BGFX
cmake --build build --target CnaCraft
```

## 5. Controls

| Key             | Action                              |
|-----------------|--------------------------------------|
| W / A / S / D   | Move / strafe                        |
| Mouse           | Look (yaw / pitch)                   |
| Space           | Jump                                 |
| Left click      | Break block                          |
| Right click     | Place block (currently selected type)|
| 1 / 2 / 3 / 4   | Select hotbar slot (Grass/Dirt/Stone/Sand) |
| E               | Cycle to next hotbar slot             |
| Esc             | Quit                                  |

Hotbar selection has no on-screen overlay yet (that's a separate task, see
`plan.md` §11.7) — the currently selected block name is printed to the
console when it changes.

## 6. License

CNA Craft is licensed under the [MIT License](LICENSE).
