# AGENTS.md — wolf-cna

Read `plan.md` before making substantial changes.

## Hard invariants

1. The game is C++23.
2. Rendering is real polygonal 3D.
3. Use CNA public APIs for graphics, windowing, input and audio.
4. Do not call SDL, OpenGL, Vulkan, Direct3D, Metal or native platform APIs directly from game code.
5. Do not copy Wolfenstein 3D proprietary assets, maps, sounds, names, characters or story data.
6. Prefer project-generated assets until asset provenance infrastructure is in place.
7. Preserve a runnable/playable build after each task.
8. If CNA blocks a required feature, document the blocker rather than silently bypassing CNA.

## Build

Default sibling layout:

```text
../cna
../sharp-runtime
./wolf-cna
```

Configure:

```bash
cmake -S . -B build -DCNA_GRAPHICS_RENDERER=OPENGLES3
cmake --build build -j
```

If CNA is elsewhere:

```bash
cmake -S . -B build -DCNA_ROOT=/path/to/cna
```

## Current task boundary

The starter implements WOLF-001: walkable real-3D test level.

Preferred next tasks are listed at the end of `plan.md`. Do one coherent task at a time and include build/test results in the final handoff.

## Code style

- Keep ownership explicit with RAII.
- Avoid global mutable state.
- Keep gameplay logic independent from rendering when reasonable.
- Avoid architecture-for-architecture's-sake.
- No per-frame rebuilding of static world geometry.
- Validate level input at boundaries.
- Use deterministic seeded randomness for procedural gameplay.
