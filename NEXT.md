# Next People development context

Last updated: 2026-08-24

## Immediate task

`PEO-277` is the only `DOING` task. The project owner explicitly requested an
early web build after the `PEO-078` desktop movement commit is pushed. Inspect
the current `../cnanext` Emscripten documentation and CMake implementation,
then use CNA's supported public consumer configuration rather than inventing
platform or renderer flags. Build the current prototype for Web and exercise
the generated output in a real browser if the local toolchain permits it.

Do not add web-only backend calls to People game code. If current CNA or the
local Emscripten environment prevents a real build, record an exact
`PEO-CNA-*` or environment blocker with the failed command and evidence; do not
claim a web result from a desktop or headless build.

After `PEO-277`, restore the normal milestone order by making `PEO-072` walk
animation the sole `DOING` task. Its smallest complete outcome is two or more
procedural walk frames for every presented direction, selected entirely from
simulation movement progress and facing. Animation must never advance or
otherwise mutate the route.

## Current verified state

- Branch before the handoff commit: `develop`.
- Last committed task before this work: `PEO-076` at `32a2ee7`.
- `PEO-078` adds `MovementExecutor`, its dedicated tests, a 20 Hz runtime
  movement loop, continuous isometric presentation, and right-click routing.
- HEADLESS: configure/build and 15/15 CTests passed.
- SDL_RENDERER/SDL3: configure/build and 15/15 CTests passed under isolated
  X11; direct SDL offscreen also passed 15/15.
- CNA: branch `next`, clean
  `14ff4be7c9690ead2030a02878c6be39802f6863`.
- sharp-runtime: branch `next`, clean
  `54578590b328aa9612fe38bfddca9fd8ca795144`.
- CNA consumed `../sharp-runtimenext` in both existing build caches.
- No People/CNA/sharp-runtime blocker is open.

Recheck both dependency HEADs and worktrees before any build or commit because
another agent advances CNA concurrently. Never roll either checkout backward.

## Desktop commands that passed

```bash
cmake -S . -B build-headless -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=HEADLESS \
  -DCNA_PLATFORM=HEADLESS \
  -DCNA_AUDIO_PLATFORM=NULL
cmake --build build-headless --parallel 2
ctest --test-dir build-headless --output-on-failure

cmake -S . -B build -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=SDL_RENDERER \
  -DCNA_PLATFORM=SDL3 \
  -DCNA_AUDIO_PLATFORM=NULL
cmake --build build --parallel 2
xvfb-run -a -s '-screen 0 1280x720x24' \
  env SDL_VIDEODRIVER=x11 ctest --test-dir build --output-on-failure
```

The first sandboxed Xvfb attempt could not expose X11 to SDL. The same command
outside that restricted sandbox passed; `SDL_VIDEODRIVER=offscreen` also passed
without Xvfb. This is an execution-environment detail, not a recorded CNA
blocker.

## Architecture and known limitations

- Movement is renderer-independent fixed-point state: 1000 units/tile,
  125 units/tick, eight ticks/tile at 20 Hz.
- A logical tile commits only on arrival; presentation derives continuous
  position and cannot mutate movement.
- Newly blocked next edges replan deterministically to the original target.
- Right-click is a temporary direct movement command. Busy residents reject a
  second click until `PEO-079`/`PEO-080` establish action semantics.
- The resident uses only idle sprites while moving; `PEO-072` is next after the
  requested web feasibility task.
- The runtime is still one lot, one floor, one resident, five procedural
  objects, with no motives, real interactions, autonomy, build/buy, or saves.
- All runtime art remains project-owned procedural placeholders recorded in
  `ASSET_PIPELINE.md`; this task adds no external or generated asset.
