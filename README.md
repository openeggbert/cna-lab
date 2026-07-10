# CNA Craft

A Minecraft-like voxel world prototype built on [CNA](https://github.com/openeggbert/cna), a C++
reimplementation of the XNA 4.0 programming model.

## 1. Overview

CNA Craft is a small first-person voxel-world prototype: chunked block terrain, procedural
terrain generation, textured/lit cube meshing, and block breaking/placing, all built directly on
CNA's `Microsoft::Xna::Framework` API (`Vector3`/`Matrix`, `VertexBuffer`/`IndexBuffer`,
`BasicEffect`, `Texture2D`, `Keyboard`/`Mouse`).

It reuses the first-person camera, movement, and AABB-collision approach already proven in CNA's
own `examples/house3d_demo.cpp`, and adds the voxel-specific layer on top: chunk storage, naive
per-face hidden-face-culled meshing, a block texture atlas, noise-based terrain generation, and a
DDA voxel raycast for picking blocks.

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
- SQLite 3 development files (e.g. `libsqlite3-dev` on Debian/Ubuntu) — used for world persistence
  (see §5); found via CMake's built-in `FindSQLite3` module, no vendoring/network fetch needed

3D rendering is implemented on three of CNA's backends — pick any one at configure time:
`EASYGL` (OpenGL), `VULKAN`, or `BGFX`. `SDL_RENDERER` remains 2D-only.

## 4. Build

```bash
git submodule update --init --recursive   # only needed the first time, inside ../cna
cmake -S . -B build -DCNA_GRAPHICS_BACKEND=EASYGL   # or VULKAN, or BGFX
cmake --build build --target CnaCraft
```

## 5. Controls

| Key               | Action                              |
|-------------------|--------------------------------------|
| W / A / S / D     | Move / strafe                        |
| Mouse             | Look (yaw / pitch)                   |
| Arrow keys        | Look (yaw / pitch) — keyboard alternative to the mouse |
| Space             | Jump (game mode) / fly up (fly mode) |
| Left Ctrl         | Fly down (fly mode only)             |
| Tab               | Toggle fly mode (no gravity, free vertical movement) |
| Left click        | Break block (Bedrock is protected — cannot be broken) |
| Ctrl + Left click | Place block instead of breaking (same as Right click) |
| Right click       | Place block (currently selected type); blocked if it would overlap your own position |
| Middle click      | Eyedropper — select the hotbar slot matching the targeted block's type |
| 1-9, 0            | Select hotbar slot directly (1-9 = slots 1-9, 0 = slot 10) |
| E / R             | Cycle to next / previous hotbar slot (all 16 slots) |
| Scroll wheel      | Also cycles the hotbar slot (same as E/R)|
| Left Shift (hold) | Zoom (narrow FOV)                   |
| F (hold)          | Orthographic projection               |
| F12               | Save a screenshot to `screenshots/`   |
| Esc               | Quit                                  |

A crosshair and the currently-selected hotbar item (e.g. `#4/16 Stone`, with a
`[FLYING]` indicator while flying) are drawn on screen; both are also printed
to the console when they change.

Every block you break or place is saved automatically to `world.db` (SQLite, created next to the
executable on first launch) and restored on top of the deterministically-regenerated terrain the
next time you run the game — only your edits are stored, not the whole world, mirroring
[fogleman/Craft](https://github.com/fogleman/Craft)'s own delta-persistence approach. Delete
`world.db` to start over with a fresh, unedited world.

## 6. License

CNA Craft is licensed under the [MIT License](LICENSE).
