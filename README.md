# CNA Craft

A Minecraft-like voxel world prototype built on [CNA](https://github.com/openeggbert/cna), a C++
reimplementation of the XNA 4.0 programming model.

## 1. Overview

CNA Craft is a small first-person voxel-world prototype: an unbounded, chunk-streamed block
terrain, procedural terrain generation, textured/lit cube meshing, and block breaking/placing,
all built directly on
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
| Mouse             | Look (yaw / pitch), while the cursor is captured |
| Arrow keys        | Look (yaw / pitch) — keyboard alternative to the mouse, works even while the cursor is released |
| Space             | Jump (game mode) / force full ascend (fly mode) |
| Tab               | Toggle fly mode (no gravity; while flying, movement is pitch-coupled — look up/down while moving forward/back to climb/descend, matching real Craft; there is no dedicated descend key) |
| Left click        | Break block (Bedrock is protected — cannot be broken); if the cursor is released, re-captures it instead |
| Ctrl + Left click | Place block instead of breaking (same as Right click) |
| Right click       | Place block (currently selected type); blocked if it would overlap your own position |
| Middle click      | Eyedropper — select the hotbar slot matching the targeted block's type |
| 1-9, 0            | Select hotbar slot directly (1-9 = slots 1-9, 0 = slot 10) |
| E / R             | Cycle to next / previous hotbar slot (all 54 slots) |
| Scroll wheel      | Also cycles the hotbar slot (same as E/R)|
| Left Shift (hold) | Zoom (narrow FOV)                   |
| F (hold)          | Orthographic projection               |
| F12               | Save a screenshot to `screenshots/`   |
| ` (backtick)      | Start typing a sign on the targeted block face |
| Enter             | Submit the sign being typed           |
| Backspace         | Delete the last character while typing a sign |
| Esc               | While typing a sign: cancel. Otherwise: release the mouse cursor (matches Craft — there is no in-game quit key; close the window to quit) |

A crosshair and the currently-selected hotbar item (e.g. `#4/54 Stone`, with a
`[FLYING]` indicator while flying) are drawn on screen; both are also printed
to the console when they change. While typing a sign, WASD/mouse-look/click
input is suspended (gravity still applies) and the in-progress text is shown
on screen above the hotbar.

The mouse cursor is released (not quit) by pressing Esc, matching real Craft — mouse-look and
mouse-button actions (break/place/eyedropper) stop working until you left-click to re-capture it.
There is no in-game quit key; close the window (or Alt+F4) to exit.

Placed signs render as a small text billboard on the block face they were
attached to and are saved/restored the same way as block edits (below).

The world has no fixed boundary — new terrain generates on the fly as you move away from spawn,
and chunks far behind you unload to free memory (both happen in the background; you shouldn't
notice a hitch). Every block you break or place is saved automatically to `world.db` (SQLite,
created next to the executable on first launch) and restored per-chunk on top of the
deterministically-regenerated terrain as each chunk streams back in — only your edits are stored,
not the whole world, mirroring [fogleman/Craft](https://github.com/fogleman/Craft)'s own
delta-persistence approach. Delete `world.db` to start over with a fresh, unedited world.

**If you have a `world.db` from before 2026-07-10**, delete it before running this version — the
save schema changed (added Craft's own `p,q` chunk-address columns to support per-chunk loading)
and there is no migration path from the old schema. An incompatible `world.db` is detected and
ignored (with a console warning) rather than causing a crash, but your old edits won't be
readable; a fresh `world.db` will be created in its place.

## 6. License

CNA Craft is licensed under the [MIT License](LICENSE).
