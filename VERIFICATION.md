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

## 2026-08-24: PEO-051 four-view sprite metadata

Object definitions can now carry renderer-independent visual metadata: an
authored default state, any number of exact named state sets, four asset IDs per
set, and a floor-contact anchor per frame. Catalog validation rejects a missing
default state and any incomplete directional set. The metadata still contains
no CNA texture or renderer handle.

`ObjectPresentation` converts simulation orientation to presented sprite
direction using the same clockwise convention as `IsometricProjection`; the
logical object does not rotate when the camera does. It then selects the exact
state/direction reference without a silent fallback. The dedicated tests cover
all 16 object-orientation/view-rotation combinations, alternate state lookup,
anchor preservation, missing states, and invalid authored definitions.

HEADLESS and SDL_RENDERER/SDL3 builds each passed 6/6 CTests against clean CNA
`b6cbfcd87c08a6e0172eaf866358bf95bec277b1` and clean sharp-runtime
`54578590b328aa9612fe38bfddca9fd8ca795144`. No visual result is claimed yet;
this increment establishes the deterministic metadata contract used by the
next procedural-furniture renderer.

## 2026-08-24: PEO-066/067/069 furnished-room M2 gate

`DemoFurniture` registers five original native definitions and places one
instance of each inside the existing room: Cedar Nest Bed, Sunny Dining Chair,
Roundleaf Table, Mintbox Refrigerator, and Cloudline Toilet. Together they
exercise all four simulation orientations, twenty unique directional asset
IDs, a two-cell bed footprint, occupancy, clearance, categories, prices, and
explicit stable instance IDs. This remains intentionally native content until
several interactions prove the external schema.

At content load, People deterministically rasterizes each referenced frame to a
transparent 128 x 128 CNA `Texture2D`. The five independently drawn silhouettes
use project-owned pixel geometry and colors; there is no source bitmap, model,
download, or generative service. Runtime rendering resolves state/direction
metadata, anchors each sprite at its declared floor contact, and inserts object
IDs into the same global floor/wall painter queue using rotated physical
footprints and the `WorldEntity` layer.

`people_demo_furniture_tests` proves five definitions/instances, twenty unique
four-view IDs, common v1 anchors, all four placed orientations, in-lot physical
footprints, occupancy-based selection, and the bed's multi-tile footprint.
HEADLESS and SDL_RENDERER/SDL3 configurations both passed 7/7 CTests against
clean CNA `b6cbfcd87c08a6e0172eaf866358bf95bec277b1` and clean sharp-runtime
`54578590b328aa9612fe38bfddca9fd8ca795144`.

Four temporary 1040 x 720 Xvfb captures were visually inspected at North,
East, South, and West. All five objects remained distinct and aligned with the
warm-wood floor, rotated their authored presentation, and sorted plausibly
against the door and camera-facing walls. A held XTest click over logical tile
`9,9` produced `selected-object=1003` for the table and a visible footprint
overlay. Selection is currently occupancy/tile based; precise opaque sprite
bounds and front-to-back overlap picking remain `PEO-024`. Screenshots stayed
in `/tmp` and are not shipping assets. Procedural provenance is recorded in
`ASSET_PIPELINE.md`. This closes the small-house M2 gate, not the resident or
interaction milestones.

## 2026-08-24: PEO-070 resident simulation identity

`ResidentRegistry` adds a renderer-independent active-lot resident model.
Residents have explicit nonzero 64-bit resident and household IDs, an original
display name, one logical `TileCoordinate`, and optional stable references to a
movement request and active action. The registry exposes only const resident
views; validated mutations cannot move a resident outside the current lot or
introduce zero/unknown handles.

Removal returns resident/household identity plus any live movement/action
handles before erasing the state, establishing an explicit cleanup handoff for
the later movement and action owners. It does not pretend to implement action
cancellation or reservations yet; those remain `PEO-079`, `PEO-084`, and the
reservation tasks. The demo active lot now contains predefined adult resident
Mara Vale at a free logical room tile, but deliberately has no resident sprite.

`people_resident_model_tests` covers identity/household/name/location
validation, duplicate and unknown IDs, in/out-of-bounds movement, assign/clear
of movement and action references, zero-handle rejection, and deletion cleanup
results. HEADLESS and SDL_RENDERER/SDL3 configurations both passed 8/8 CTests
against clean CNA `b6cbfcd87c08a6e0172eaf866358bf95bec277b1` and clean
sharp-runtime `54578590b328aa9612fe38bfddca9fd8ca795144`. Startup smoke reports
`residents=1`; no new visual result is claimed for this model-only increment.

## 2026-08-24: PEO-071 four-view idle resident

Simulation-facing direction is now explicit on `ResidentState` and remains
independent from presentation. `ResidentPresentation` applies the same
clockwise view transform as lot/object projection and selects one of four
metadata-only idle references. `DemoResident` supplies Mara Vale's original
identity, initial south facing, four unique asset IDs, and common `(32,88)` foot
anchor without adding a customization system or animation graph.

At content load, People rasterizes four transparent 64 x 96 idle textures using
new project-owned pixel geometry. The frames share scale and ground contact but
have distinct front, side, and back details. Mara's one-tile footprint enters
the global deterministic `WorldEntity` painter queue; camera rotation changes
only the selected frame, never her simulation tile/facing.

`people_resident_presentation_tests` covers the predefined content, unique IDs,
common anchors, all sixteen facing/view combinations, exact metadata selection,
and invalid direction rejection. The expanded resident-model test also covers
validated simulation-facing mutation. HEADLESS and SDL_RENDERER/SDL3 builds
both passed 9/9 CTests against CNA HEAD
`b6cbfcd87c08a6e0172eaf866358bf95bec277b1` and clean sharp-runtime
`54578590b328aa9612fe38bfddca9fd8ca795144`. During this verification the CNA
checkout contained another agent's five pre-existing uncommitted graphics
changes (`CHECKLIST.md`, `DirectionalLight.hpp/.cpp`, `BasicEffectTests.cpp`,
and new `DirectionalLightTests.cpp`), so this CNA state is not reproducible from
the SHA alone; People did not edit those files.

Four temporary Xvfb captures were visually inspected. Mara remained visibly
grounded and correctly ordered with furniture/walls in North, East, South, and
West while showing the expected directional variation. The captures remain in
`/tmp`; procedural provenance is recorded in `ASSET_PIPELINE.md`. Idle is the
only clip—walk frames and simulation-driven animation remain `PEO-072`.

## 2026-08-24: PEO-073 static navigation snapshot

`StaticNavigationGrid` builds an immutable, renderer-independent snapshot of
one logical lot floor. It marks physical object footprint cells unavailable but
leaves placement-clearance cells traversable for later interaction approaches.
Cardinal movement crosses a tile edge only when both cells are walkable and the
edge has no blocking wall; an open attached door makes its canonical wall edge
passable. Neighbor queries use the documented North, East, South, West order.

The snapshot deliberately does not observe later lot/object mutations. Tests
prove that object removal and door opening affect a newly rebuilt snapshot but
cannot mutate an in-flight route query. They also cover floor isolation, lot
bounds, diagonal/non-adjacent rejection, stable corner/center neighbor order,
multi-cell footprints, traversable clearance, walls in both directions, and
closed/open door portals.

HEADLESS and SDL_RENDERER/SDL3 configurations both built and passed 10/10
CTests, including the CNA runtime smoke. Dependency state was CNA HEAD
`b6cbfcd87c08a6e0172eaf866358bf95bec277b1` and clean sharp-runtime HEAD
`54578590b328aa9612fe38bfddca9fd8ca795144`. CNA still contained the same five
uncommitted external graphics changes recorded under PEO-071; People did not
edit or depend on them. No new visual acceptance or framework blocker is
claimed for this simulation-only increment.
