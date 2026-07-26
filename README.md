# CNA Tamagotchi

An original C++ virtual-pet game built on [CNA](https://github.com/openeggbert/cna).
The repository currently contains a buildable visual prototype: a CNA window,
a gentle warm-background transition, an original pastel egg-shaped device,
eight care icons, three controls, and a 32 × 24 monochrome LCD with a demo
pixel creature. The playable care loop, assets, and saves are intentionally
future work.

Read [analysis.md](analysis.md) for the design and technical proposal, and
[plan.md](plan.md) for the staged roadmap.

## Prerequisites

- CMake 3.21 or newer
- A C++23 compiler
- A sibling CNA checkout at `../cna` (or a supplied `CNA_ROOT_DIR`)
- CNA's own sibling dependencies, as described by the CNA project

## Build and run

```bash
cmake --preset sdl-renderer
cmake --build --preset sdl-renderer
./cmake-build-sdl-renderer/CnaTamagotchi
```

The skeleton opens a 540 × 760 window. Press Escape to close it. Pass
`--smoke-test` to have the application exit after three rendered frames.

## Prototype controls

- `A` or Right arrow: select the next care icon; Left arrow: select the previous one.
- `B`, Enter, or Space: perform the selected action.
- `C` or Backspace: return selection to the meal icon; Escape: close the game.

The active icons currently implement meal, play, sleep, discipline, cleaning,
and medicine. The LCD bars show hunger, happiness, and health; one real minute
currently advances one simulated minute while the application is open.

To use a CNA checkout outside the default sibling location:

```bash
cmake -S . -B build -DCNA_ROOT_DIR=/path/to/cna -DCNA_GRAPHICS_BACKEND=SDL_RENDERER
cmake --build build --target CnaTamagotchi
```

## Tests

The deterministic pet rules have no CNA dependency and are tested separately:

```bash
cmake --build --preset sdl-renderer --target CnaTamagotchiDomainTests
ctest --test-dir cmake-build-sdl-renderer --output-on-failure
```

## Save foundation

The framework-free persistence layer now writes and reads version-1 JSON save
files, validates every field, writes through a `.tmp` file, and creates a
`.bak` copy before replacing an existing slot. Automatic save-slot selection,
UTC time catch-up, and in-game loading will be connected in a later milestone.

## Layout

```text
include/CnaTamagotchi/  public application, display, and domain seams
src/                    executable entry point and implementations
analysis.md             product analysis and technical proposal
plan.md                 ordered development roadmap
```

No licence file is included yet; it will be added separately.
