# CNA Craft

A C++ port of [**fogleman/Craft**](https://github.com/fogleman/Craft) (MIT license) onto
[CNA](https://github.com/openeggbert/cna), a C++ reimplementation of the XNA 4.0 programming
model.

## 1. Overview

**CNA Craft is a derivative work of [Craft](https://github.com/fogleman/Craft) by Michael
Fogleman** — not merely "inspired by" it. The goal is a faithful reimplementation of Craft's
behavior on a different engine: gameplay, world generation, ambient occlusion, the world-editing
`/`-commands, the SQLite save schema and the multiplayer protocol are all ported from Craft, and
tracked feature-by-feature against its source in [CRAFT_PARITY.md](CRAFT_PARITY.md). No Craft
source code is copied verbatim (this is C++23 against a different API, and no Craft files ship
here), but the substance is Craft's — see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full attribution and Craft's license
text, which this project reproduces as MIT requires.

What that gets you today: an unbounded, chunk-streamed voxel world with procedural terrain, baked
per-vertex ambient occlusion, a textured day/night sky, block breaking/placing with a 54-block
roster, signs, world-editing commands, SQLite delta persistence, and multiplayer against this
project's own server — all built directly on CNA's `Microsoft::Xna::Framework` API
(`Vector3`/`Matrix`, `VertexBuffer`/`IndexBuffer`, `BasicEffect`, `Texture2D`,
`Keyboard`/`Mouse`), and also playable in a browser (WebAssembly/WebGL 2).

The first-person camera, movement, and AABB-collision approach comes from CNA's own
`examples/house3d_demo.cpp`; the voxel layer on top (chunk storage, hidden-face-culled meshing, a
procedural texture atlas, noise terrain, DDA raycast picking) follows Craft's design.

See [plan.md](plan.md) for the detailed architecture and implementation history, and
[analysis.md](analysis.md) for the feasibility analysis this project started from.

## 2. Status

Early prototype / active development. Not a finished game.

## 3. Dependencies

CNA Craft builds against sibling checkouts, following the same convention as
[galaxy-eggbert](https://github.com/openeggbert/galaxy-eggbert):

```text
openeggbert/
├── cna-craft/        (this repo)
├── cna/              (engine — https://github.com/openeggbert/cna)
└── sharp-runtime/     (modular utility/runtime layer, pulled in automatically by CNA)
```

- CMake 3.21+
- A C++23-capable compiler (GCC 12+ or Clang 15+)
- `../cna` and `../sharp-runtime` present next to this repo (or pass `-DCNA_HOME=<path>`)
- SQLite 3 development files (e.g. `libsqlite3-dev` on Debian/Ubuntu) — used for world persistence
  (see §5); found via CMake's built-in `FindSQLite3` module, no vendoring/network fetch needed

The build consumes `CNA::Runtime` rather than CNA's historical full-framework umbrella. It disables
CNA's unused ENet multiplayer module and selects the closure required by CNA's chosen runtime
modules plus sharp-runtime's `Net.Sockets` component for CnaCraft's TCP transport; sharp-runtime's
`All` bundle and unrelated HTTP, XML, JSON, compression, and process facilities are not selected.

3D rendering is implemented on three of CNA's renderers — pick any one at configure time:
the OpenGL family (`OPENGLES3` default on Linux, or `OPENGLES2`/`OPENGL33`), `VULKAN`, or `BGFX`.
`SDL_RENDERER` remains 2D-only.

## 4. Build

```bash
git submodule update --init --recursive   # only needed the first time, inside ../cna
cmake -S . -B build -DCNA_GRAPHICS_RENDERER=OPENGLES3   # or VULKAN, or BGFX
cmake --build build --target CnaCraft
```

### 4.1 Web build (Emscripten / WebAssembly)

The game also builds to WebAssembly and runs in the browser on WebGL 2 (via CNA's shared EasyGL
implementation, publicly selected as `WEBGL2`), with terrain streaming, ambient occlusion, the
day/night sky and the full HUD intact:

```bash
source /path/to/emsdk/emsdk_env.sh
embuilder build zlib                          # one-off: sharp-runtime needs zlib

# Portable single-thread build (the default): works in Firefox and Chromium.
emcmake cmake -S . -B build-web -DCNA_GRAPHICS_RENDERER=WEBGL2
cmake --build build-web --target CnaCraft

python3 web/serve.py 8000 build-web           # then open http://localhost:8000/CnaCraft.html
```

The default web build is **single-threaded**, which is the reliable choice for Firefox and needs
no special response headers. Generation and meshing happen on the main thread, so frames can
stutter while new terrain streams in, but the page stays responsive. For a tested Chromium-only
deployment, enable Emscripten pthreads with
`CFLAGS=-pthread CXXFLAGS=-pthread emcmake cmake -S . -B build-web -DCNA_GRAPHICS_RENDERER=WEBGL2 -DCNA_CRAFT_WASM_THREADS=ON`.
That mode needs a `SharedArrayBuffer`, so the server must send
`Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp` —
`web/serve.py` does exactly that (plain `python3 -m http.server` does not).

Other web-specific differences, all deliberate: **single-player only** (a browser page cannot host
or open raw TCP sockets, so `--server` and `CnaCraftServer` are native-only) and **edits are not
persisted** (no SQLite in the browser — the world resets on reload). Click the page to lock the
pointer; Esc releases it, exactly like the desktop build.

## 5. Controls

| Key               | Action                              |
|-------------------|--------------------------------------|
| W / A / S / D     | Move / strafe                        |
| Mouse             | Look (yaw / pitch), while the cursor is captured |
| Space             | Jump (game mode) / ascend (fly mode) |
| Left Shift (hold) | Descend (fly mode only) / zoom, narrow FOV (ground mode) |
| Tab               | Toggle fly mode (no gravity; Space/Shift move straight up/down regardless of where you're looking, same as Minecraft's creative flight — holding both cancels out) |
| Left click        | Break block (Bedrock is protected — cannot be broken); if the cursor is released, re-captures it instead |
| Ctrl + Left click | Place block instead of breaking (same as Right click) |
| Right click       | Place block (currently selected type); blocked if it would overlap your own position |
| Middle click      | Eyedropper — select the hotbar slot matching the targeted block's type |
| 1-9, 0            | Select hotbar slot directly (1-9 = slots 1-9, 0 = slot 10) |
| E / R             | Cycle to next / previous hotbar slot (all 54 slots) |
| Scroll wheel      | Also cycles the hotbar slot (same as E/R)|
| F (hold)          | Orthographic projection               |
| F11               | Toggle fullscreen                     |
| F12               | Save a screenshot to `screenshots/`   |
| ` (backtick)      | Start typing a sign on the targeted block face |
| /                 | Start typing a world-editing command (see below) |
| Enter             | Submit the sign/command/chat being typed; when NOT typing and connected to a server: open the chat box |
| P                 | (Multiplayer) Cycle picture-in-picture observation of other players' views; one step past the last player switches it off |
| Backspace         | Delete the last character while typing  |
| Esc               | While typing: cancel. Otherwise: release the mouse cursor (matches Craft — there is no in-game quit key; close the window to quit) |

A crosshair and the currently-selected hotbar item (e.g. `#4/54 Stone`, with a
`[FLYING]` indicator while flying) are drawn on screen; both are also printed
to the console when they change. While typing a sign or command, WASD/mouse-look/click
input is suspended (gravity still applies) and the in-progress text is shown
on screen above the hotbar. Command feedback ("Unknown command", etc.) appears in a
small 4-line scrolling message log above that, which stays visible for a few
messages even after you close the typing box.

### 5.1 World-editing commands

Press `/` to start typing a command, then Enter to run it — ports Craft's real
world-editing macros (`src/main.c`'s `parse_command`) almost verbatim. Every command
except `/view` reads its position(s) from the last one or two blocks you broke or
placed (Craft's own "marked block" model — there's no separate marking action,
breaking/placing *is* marking):

| Command                                  | Effect |
|-------------------------------------------|--------|
| `/view N`                                 | Set the streamed view/create/delete radius (1-24) |
| `/cube`, `/fcube`                         | Hollow / filled box between your last two marks (they must be the same block type) |
| `/sphere N`, `/fsphere N`                 | Hollow / filled sphere of radius N centered on your last mark |
| `/circlex N`/`y`/`z`, `/fcirclex N`/`y`/`z` | Same as `/sphere`, flattened to a disc along one axis |
| `/cylinder N`, `/fcylinder N`             | Hollow / filled cylinder of radius N along the axis between your last two marks |
| `/array N` or `/array X Y Z`              | Repeats your last mark's block type N times (or X/Y/Z times per axis) stepping by the offset between your last two marks |
| `/tree`                                   | Grows a Wood-trunk-and-Leaves-canopy tree at your last mark |
| `/copy` then `/paste`                     | Copies the live block contents of the region between your last two marks (at `/copy` time) to a new region between your last two marks (at `/paste` time) |

Any block you paint through a command is saved just like a normal break/place. Unlike
Craft, a mismatched-type `/cube`/`/array`/`/cylinder` (your last two marks aren't the
same block type) shows a short message instead of silently doing nothing.

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

### 5.2 Multiplayer

cna-craft plays multiplayer against its own server (built alongside the game as
`CnaCraftServer` — a headless console binary, no GPU needed). It is **not** compatible with
original Craft servers (different terrain generator, chunk size, and block ids — a deliberate,
documented scope decision; see `MULTIPLAYER_PLAN.md`), but the wire protocol is a faithful
dialect of Craft's own ASCII line protocol.

```bash
# Host (any machine, no display needed):
./build/CnaCraftServer 4080 --seed 1337

# Players:
./build/CnaCraft --server HOST 4080
```

In game: **Enter** opens the chat box (online only) — plain text is chat, `@nick text` sends a
private message; `/list`, `/nick NAME` and `/help` run on the server; `/online HOST [PORT]` and
`/offline` switch modes at runtime; **P** cycles a picture-in-picture inset showing another
player's view. Other players appear as skin-toned cubes (Craft's own look) with their name shown
near the crosshair when you aim at them. All world edits are validated by the server (its word
wins — a rejected edit snaps back with a message) and synced live to everyone; each server you
join gets its own local cache file (`cache.<host>.<port>.db`), so your single-player `world.db`
is never touched by online play. The day/night clock and terrain seed come from the server, so
everyone shares one world and one sky. If the connection drops, the game says so and keeps
running offline.

## 6. License

CNA Craft is licensed under the [MIT License](LICENSE).

It is a derivative work of [Craft](https://github.com/fogleman/Craft), Copyright © 2013 Michael
Fogleman, also MIT-licensed — Craft's copyright notice and full license text are reproduced in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), as its license requires, together with a
detailed account of exactly what this project takes from it. It also builds on
[CNA](https://github.com/openeggbert/cna) (Ms-PL) and its sharp-runtime layer; see the same file.
