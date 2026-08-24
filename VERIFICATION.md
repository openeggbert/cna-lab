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

## 2026-08-24: PEO-030/031/032/035 logical lot topology

The displayed tile renderer now reads the real renderer-independent `LotGrid`
rather than a hard-coded size. The model owns bounded multi-floor cells,
original terrain kinds, optional stable floor-covering IDs, visual dirty flags,
canonical wall edges, and per-floor room invalidation. Walls are stored once by
minimum vertex plus axis even when queried through either neighboring tile.

`RoomMap` deterministically flood-fills one dirty floor. Components with an
unwalled route through the lot boundary receive outside ID zero; enclosed
components receive row-major IDs. `people_lot_grid_tests` covers bounds, two
floors, state/no-op dirtiness, covering add/remove, wall add/query/remove and
neighbor normalization, boundary walls, room invalidation, open outside space,
one enclosed interior cell, a fully enclosed split room, removal-driven merge,
and floor isolation. Headless and displayed configurations passed 4/4 CTests
against the dependency snapshots recorded above.

## 2026-08-24: PEO-033 procedural room walls

The demo lot now creates a 7 x 7 warm-wood room through `LotGrid` mutations:
49 floor-covering IDs and 28 canonical perimeter edges. Runtime-generated wall
textures contain transparent sloped bounds, plaster panels, base trim, and
original neutral colors. No image or model asset is loaded.

`WallPresentation` converts each logical segment to two continuous boundary
points, its in-lot footprint, farthest view anchor, and coarse back/front layer.
Tests verify segment dimensions and exactly two camera-away/two camera-facing
perimeter sides in every rotation; the initial documented internal-wall policy
uses the front layer until segmentation/cutaway work.

Headless and displayed configurations passed 4/4 CTests. Four temporary Xvfb
captures, one per rotation, were assembled and visually inspected: wall tops,
shared corners, alpha boundaries, floor contact, warm-wood covering, selection,
and lot focus stayed aligned in all views. The captures were verification
artifacts in `/tmp`, not retained or treated as shipping art.

## 2026-08-24: PEO-037/038/046 door and architecture gate

One closed door is attached to the demo room's front canonical wall. The model
requires an existing host wall, persists open state, invalidates routing on
attach/remove/state change, exposes an open route portal, preserves room
semantics, and cascades cleanup when its host wall is removed. Duplicate and
missing mutations remain explicit no-ops or actionable errors.

Four original runtime-generated door textures cover both projected slopes and
closed/open state. The closed view has a wood panel, frame, and knob; the open
view retains frame/header pixels but leaves a genuinely transparent passage.
The temporary `F` control changes only logical door state, from which rendering
selects its texture.

Headless and displayed configurations passed 4/4 CTests. Temporary Xvfb
captures of closed and open states were visually compared; panel, opening,
wall contact, sorting, and adjacent floor remained aligned. This completes the
tested one-room architecture sub-gate, not the five-object M2 content gate.

## 2026-08-24: PEO-050/052/053/055 object placement foundation

`ObjectCatalog` now owns validated, immutable-through-its-public-interface
definitions. `ObjectWorld` owns persistent instances with explicit nonzero
64-bit IDs, definition references, logical anchors, and simulation rotations;
neither type stores a runtime texture or other renderer state.

Footprint offsets rotate through four integer transforms around an unchanged
anchor. The same definition-driven validator checks the current `LotGrid`
floor bounds, allowed orientation mask, footprint occupancy, and access
clearance. Clearance claims remain enforced in both placement orders and are
released with footprint occupancy when an object is removed. Failures return a
structured reason and, where applicable, the stable conflicting instance ID.

`people_object_model_tests` covers definition/schema rejection, heterogeneous
catalog lookup, all four rotation transforms, a four-turn round trip,
multi-cell/out-of-bounds footprints, allowed rotations, duplicate/zero stable
IDs, occupation, bidirectional clearance conflicts, and removal cleanup. Both
the HEADLESS and SDL_RENDERER/SDL3 configurations passed 5/5 CTests against
clean CNA `b6cbfcd87c08a6e0172eaf866358bf95bec277b1` and clean sharp-runtime
`54578590b328aa9612fe38bfddca9fd8ca795144`. The displayed runtime test ran
under Xvfb; no new visual acceptance is claimed for this model-only increment.
