# Copper Boots

Copper Boots is a free/libre C++23 side-scrolling platform game built on
[CNA](https://github.com/openeggbert/cna). It is an independent, loose technical
reinterpretation of Mike Wiering's compact 1994 DOS platform game commonly
known as *Mario & Luigi*. The repository name `mario-cna` records that historical
study; the public game identity is original.

The project has three goals:

- become a responsive, original retro platform game;
- exercise CNA's 2D graphics, input, audio, timing, content, and platform APIs;
- preserve enough analysis and deterministic tests for later contributors and
  AI agents to continue the work safely.

## Legal and creative boundary

Copper Boots is not a Nintendo product and is not endorsed by or affiliated
with Nintendo. Nintendo characters, trademarks, graphics, music, sound effects,
level artwork, logos, and other proprietary material are not included.

Mike Wiering's source is used as historical and behavioral reference. Its
archive contains restrictive usage language and is not licensed under this
project's MIT license. No original archive, executable, sprite, sound, embedded
sprite data, or level layout is shipped here. See [analysis.md](analysis.md) for
the provenance and licensing findings.

All initial visuals are generated from programmatically created color textures
and geometric shapes. Externally sourced assets, if added later, must be
recorded in [THIRD_PARTY.md](THIRD_PARTY.md).

## Game direction

You play a courier-mechanic exploring overgrown machine ruins. The current
working vocabulary replaces the historical presentation with an original one:

| Gameplay role | Copper Boots concept |
|---|---|
| player | courier-mechanic |
| collectible | copper cog |
| growth/health state | plated jacket |
| projectile ability | arc capacitor |
| pipe/subarea route | maintenance conduit |
| basic enemy | clockwork crawler |

The intended first milestone is a compact original level with walk, run, jump,
tile collision, smooth bounded camera movement, and visible parallax.

## Technology

- C++23
- CMake 3.23 or newer
- CNA as the only game/framework abstraction
- sharp-runtime through CNA's public dependency graph
- a deterministic 60 Hz gameplay simulation
- a 320x180 logical render target scaled with point filtering when supported

Game code does not call SDL, OpenGL, Vulkan, Direct3D, Metal, or native window,
input, rendering, audio, timing, content, filesystem, or platform APIs. Those
are CNA implementation concerns.

## Repository layout

```text
Content/             original external level data and future assets
game/include/        game declarations
game/src/            CNA presentation and game implementation
gameplay/            renderer-free simulation code
test/                deterministic logic tests
reference/original/  ignored local historical download area
analysis.md           historical research and architecture decisions
plan.md               stable MAR-* task ledger
```

The application avoids a root-level `src/` directory because the pinned CNA
revision currently mistakes a consumer's `src/` for a reintroduced legacy CNA
source tree. This is tracked as `MAR-CNA-001` in [plan.md](plan.md).

## Dependencies

Place sibling checkouts next to this repository:

```text
openeggbert/
├── cna/
├── sharp-runtime/
└── mario-cna/
```

CNA itself locates sharp-runtime. Both roots remain configurable:

```bash
cmake -S . -B build \
  -DCNA_ROOT_DIR=../cna \
  -DCNA_GRAPHICS_RENDERER=SDL_RENDERER
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -j2
```

`SDL_RENDERER` is the initial conservative 2D validation lane. It does not mean
game code depends on SDL: the executable links CNA's public `CNA` target and
uses only CNA/XNA-style public APIs. Other CNA renderers will be added to the
compatibility matrix as milestones stabilize.

## Controls

Current intended defaults are:

| Action | Keyboard | Gamepad |
|---|---|---|
| move | A/D or Left/Right | left stick or D-pad |
| run | Left/Right Shift | X, right shoulder, or right trigger |
| jump | Space | A |
| interact/down | S or Down | Y or D-pad Down |
| attack | Ctrl | B |
| aim projectile | W/S or Up/Down | D-pad/left-stick vertical |
| pause/resume | Escape | Start |
| debug overlay | F1 | — |

Stomping a crawler always bounces the courier. Keep Jump held during contact for
the higher bounce needed by some elevated Green Ruins routes.

While paused, R/Y restarts at the current spawn and Q/Back quits safely.

Controls and bindings will become configurable. Historical controls and
their verified meanings are documented in [analysis.md](analysis.md).

## Branches and contributions

`main` holds the initial documentation foundation. Active development occurs on
`develop`. Consult [plan.md](plan.md), select the highest-priority unblocked
`MAR-*` task, satisfy its acceptance criteria, run relevant tests, and update
the ledger in the same coherent commit.

Keep gameplay logic independent of a graphics device. Preserve unrelated local
changes in sibling dependencies and record framework defects as `MAR-CNA-*` or
`MAR-SR-*` tasks instead of hiding them in game code.

## License

New code and original project documentation in this repository are licensed
under the MIT License; see [LICENSE](LICENSE).

That license does not apply to Mike Wiering's historical source/reference
material, Nintendo-owned material, CNA, sharp-runtime, or any other third-party
work. Their respective ownership and license terms remain unchanged.
