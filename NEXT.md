# Next People development context

Last updated: 2026-08-24

## Immediate task

`PEO-072` walk animation is the only `DOING` task. Add at least two original
procedural walk frames for every resident presentation direction, with the
frame selected from inspectable simulation movement progress and facing.
Rendering must never advance, complete, cancel, or otherwise mutate a route.

Keep the first increment narrow: reuse the existing Mara placeholder palette,
canvas size, and `(32,88)` foot anchor; add metadata/presentation tests for all
directions and deterministic phase boundaries; then make the runtime choose
idle versus walk without adding an animation graph or avatar customization.
Run the headless and displayed 15-test baselines plus the new test and perform
an actual movement visual check before marking it `DONE`.

## Current verified state

- Branch: `develop`.
- `PEO-078` is committed and pushed as `e9336cc`.
- `PEO-277` has a successful Emscripten 6.0.3/CANVAS Release build and is the
  completed task recorded by this handoff.
- Web outputs: `build-web/People.html` (19,601 bytes), `People.js` (235,688
  bytes), and `People.wasm` (5,105,329 bytes). Build trees are ignored.
- `node --check` passed for `People.js`; Binaryen `wasm-opt --all-features`
  accepted `People.wasm` with no transformation pass.
- The session browser-testing surface reported no available browser. The local
  HTTP server started successfully and was stopped, but no Canvas frame or web
  input result is claimed.
- HEADLESS: configure/build and 15/15 CTests passed.
- SDL_RENDERER/SDL3: configure/build and 15/15 CTests passed under isolated
  X11; direct SDL offscreen also passed 15/15.
- CNA: branch `next`, HEAD
  `14ff4be7c9690ead2030a02878c6be39802f6863`, with five external uncommitted
  ContentManager/SDL3 platform paths observed at final verification. The final
  incremental web build recompiled the changed platform source and relinked.
- sharp-runtime: branch `next`, clean
  `54578590b328aa9612fe38bfddca9fd8ca795144`.
- CNA consumed `../sharp-runtimenext` in desktop and web build caches.
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

## Web commands that passed

The exact verification used absolute emsdk paths rooted at
`/home/robertvokac/emsdk`; the portable equivalents are:

```bash
emcmake cmake -S . -B build-web -G Ninja \
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCNA_GRAPHICS_RENDERER=CANVAS \
  -DCNA_PLATFORM=SDL3 \
  -DCNA_AUDIO_PLATFORM=NULL \
  -DCNA_ENABLE_DRACO=OFF \
  -DZLIB_LIBRARY="$EMSDK/upstream/emscripten/cache/sysroot/lib/wasm32-emscripten/libz.a" \
  -DZLIB_INCLUDE_DIR="$EMSDK/upstream/emscripten/cache/sysroot/include" \
  -DCMAKE_CXX_FLAGS="-Wno-error=unused-function"
cmake --build build-web --target People --parallel 2
node --check build-web/People.js
wasm-opt --all-features build-web/People.wasm -o /tmp/people-cna-People-validated.wasm
```

The sandbox initially denied Emscripten's lock file in its SDK cache. Repeating
the final link with normal cache write access succeeded. Do not misreport that
environment permission as a CNA linker failure.

## Architecture and known limitations

- Movement is renderer-independent fixed-point state: 1000 units/tile,
  125 units/tick, eight ticks/tile at 20 Hz.
- A logical tile commits only on arrival; presentation derives continuous
  position and cannot mutate movement.
- Newly blocked next edges replan deterministically to the original target.
- Right-click is a temporary direct movement command. Busy residents reject a
  second click until `PEO-079`/`PEO-080` establish action semantics.
- The resident uses only idle sprites while moving; this is exactly the current
  `PEO-072` scope.
- Emscripten uses CNA's 2D `CANVAS` renderer plus SDL3 platform and NULL audio.
  People contains no conditional web backend code; only CMake emits an HTML
  shell and allows WASM memory growth.
- The runtime is still one lot, one floor, one resident, five procedural
  objects, with no motives, real interactions, autonomy, build/buy, or saves.
- All runtime art remains project-owned procedural placeholders recorded in
  `ASSET_PIPELINE.md`; this task adds no external or generated asset.
