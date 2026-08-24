# People verification record

This file records evidence that actually ran. It is not a promise of coverage
on platforms or renderers not listed here. Revisions below are historical test
snapshots: active development always consumes the newest local `*next` HEAD and
re-verifies after it advances.

## 2026-08-24: M1 isometric foundation

Validated dependency state at final build time:

- CNA: `../cnanext`, branch `next`, HEAD
  `b6cbfcd87c08a6e0172eaf866358bf95bec277b1`, clean.
- sharp-runtime: `../sharp-runtimenext`, branch `next`, HEAD
  `54578590b328aa9612fe38bfddca9fd8ca795144`, clean.

The dependency directories were read and built but not edited by People work.

Headless configuration and validation:

```bash
cmake -S . -B build-headless \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=HEADLESS \
  -DCNA_PLATFORM=HEADLESS \
  -DCNA_AUDIO_PLATFORM=NULL
cmake --build build-headless --target People people_core_tests --parallel 2
ctest --test-dir build-headless --output-on-failure
```

Result: configure and build succeeded. `people_core_tests` and
`people_runtime_smoke` both passed (2/2). The four-frame smoke logged
`rotation=3; rotation-cycle=yes`, proving it drew North, East, South, and West
before clean exit.

Displayed Linux configuration:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=SDL_RENDERER \
  -DCNA_PLATFORM=SDL3 \
  -DCNA_AUDIO_PLATFORM=NULL
cmake --build build --target People people_core_tests --parallel 2
xvfb-run -a env SDL_VIDEODRIVER=x11 ./build/People --smoke-test
```

Result: configure/build succeeded and the real 2D SDL renderer ran all four
rotations and exited with code 0. A temporary 1280 x 1024 X-root capture was
visually inspected: the 1280 x 720 People window contained the complete 20 x 20
green diamond lot, deterministic alternating tiles, dark background, and a
yellow translucent hover highlight. The capture was a verification artifact in
`/tmp`, not a shipping asset or committed screenshot.

XTest-driven input checks delivered input directly to the CNA window:

- holding `W` changed camera origin Y from `72` to `184`;
- one mouse-wheel notch changed zoom from `0.62` to `0.6944` and adjusted the
  origin around the cursor;
- holding `E` across update ticks changed rotation from North (`0`) to East
  (`1`).

Pure tests additionally exhaust every tile for all rotations on square and
rectangular lots, verify inverse transforms, deterministic shared-edge picking
in all four views, projection/elevation constants, outside rejection,
cursor-focused zoom, zoom clamping, and lot-center preservation during
rotation.

Known limitations of this evidence:

- the displayed run used Linux/Xvfb and CNA `SDL_RENDERER`; no other displayed
  renderer or OS is claimed;
- the screenshot is intentionally not final art and was not retained;
- CNA/sharp-runtime headers emit an external `__int128` pedantic warning, and
  vendored Draco emits policy/deprecation warnings; People-owned core sources
  compiled without their own diagnostics;
- the lot has no logical floors/walls/objects/residents yet.

No `PEO-CNA-*` or `PEO-SR-*` blocker was confirmed. The initial inability to
create an X socket inside the filesystem sandbox disappeared when the approved
display smoke ran outside that sandbox, so it is an environment constraint, not
a CNA defect.

## 2026-08-24: PEO-020 deterministic render keys

The renderer-independent `RenderKey` orders lexicographically by logical floor,
farthest rotated footprint depth, draw layer, rotated anchor Y/X, local segment
offset, and stable entity ID. The 20 x 20 tile gather now constructs and sorts
these keys instead of using an application-local comparison.

`people_render_order_tests` verifies every field's priority, farthest-footprint
depth and authored anchors in four rotations, invalid empty/mixed-floor/outside
footprints, and identical sorted output after 20 seeded input shuffles. Headless
and displayed configurations both passed 3/3 CTests against clean CNA
`b6cbfcd87c08a6e0172eaf866358bf95bec277b1` and clean sharp-runtime
`54578590b328aa9612fe38bfddca9fd8ca795144`.
