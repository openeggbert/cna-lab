# People

People is an original, long-term household life-simulation game built in C++23
on CNA and sharp-runtime. Its working presentation is a classic sprite-based
2D isometric lot with exactly four camera orientations. The core experience is
to build a home, furnish it, guide its residents, and watch object-centric
autonomy create emergent domestic stories.

People is an independent project. It contains and requires **zero data from any
commercial game**. It does not load proprietary sprites, audio, object files,
archives, neighborhoods, user interfaces, fonts, animations, saves, or other
game resources. Shipping content must be original or redistributable under a
documented compatible license. Historical games and open reimplementations are
behavioral and architectural references only.

`People` is a working public title. A proper name and trademark clearance must
be completed before a serious public or commercial release.

## Current maturity

The first executable milestone is working on `develop`. People opens through
CNA, generates and draws a 20 x 20 isometric lot with `SpriteBatch`, supports
camera pan/zoom, cycles exactly four presentation rotations, and highlights the
tile under the mouse. Projection, inverse picking, rotation, camera focus, and
runtime startup are covered by CTest. The lot now contains an original
procedural warm-wood room with logical edge walls rendered from generated
textures in all four views plus one stateful procedural door. A headless object
model now separates immutable catalog definitions from persistent instances and
validates rotated footprints, occupancy, and access clearance. Catalog objects
carry validated four-view/state asset IDs and floor-contact anchors. The room is
furnished with an original procedural bed, chair, table, refrigerator, and
toilet; all rotate, sort, and render from 2D textures generated at runtime.
Objects can be selected through their logical footprint. One predefined
resident, Mara Vale, now exists in the active-lot simulation and renders from
an original procedural four-direction idle sprite set. Right-clicking a free
floor tile now finds a deterministic A* route and moves the resident through
fixed-tick sub-tile positions; changed static obstructions trigger a stable
replan. Walk animation, action queues, motives, and autonomy are still future
milestones.

See [plan.md](plan.md) for stable tasks, [analysis.md](analysis.md) for the
architectural rationale, and [VERIFICATION.md](VERIFICATION.md) for commands
and results that have actually run.

## Technology and invariants

- C++23 and CMake.
- CNA is the only game/framework abstraction used by game code.
- CNA consumes sharp-runtime through its supported integration.
- Runtime world rendering is SpriteBatch-based 2D; offline 3D tools may only
  manufacture final 2D sprites.
- Simulation coordinates and rendering coordinates remain separate.
- Randomness is explicitly seeded and deterministic where practical.
- Headless tests cover simulation and coordinate logic independently of the
  renderer.

## Dependency checkout layout

The supported local development layout is:

```text
parent/
|-- cnanext/
|-- sharp-runtimenext/
`-- people-cna/
```

People intentionally targets `../cnanext` and configures CNA to consume
`../sharp-runtimenext`. The older sibling directories `../cna` and
`../sharp-runtime` are not the project dependencies.

Development tracks the newest local commits in both `*next` checkouts. Recorded
SHAs are verification snapshots, not requests to roll either dependency back;
milestones are rebuilt when those local HEADs advance.

Planning-baseline inspection on 2026-08-24:

- CNA next: branch `next`, HEAD
  `33ff296f5ffe42cfa9c3a2060da55a953f2a9f4e`; its working tree had 31
  pre-existing changes, so the SHA alone does not reproduce that checkout.
- sharp-runtime next: branch `next`, HEAD
  `54578590b328aa9612fe38bfddca9fd8ca795144`, tag
  `v0.1.0-beta.1`; working tree clean.

The final executable verification used clean CNA HEAD
`14ff4be7c9690ead2030a02878c6be39802f6863` on `next`. Both headless and
SDL_RENDERER/SDL3 configurations passed the complete test suite at that
revision. See the verification record; neither dependency checkout was edited
by People work.

The first Emscripten/CANVAS build subsequently linked at the same CNA HEAD
while another agent had five uncommitted CNA ContentManager/SDL3 platform paths
in progress. That web result is therefore exact for the observed working tree
but not reproducible from the CNA SHA alone.

## Build

Configure the tested Linux displayed build explicitly with CNA's SDL3 platform
and 2D SDL renderer:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=SDL_RENDERER \
  -DCNA_PLATFORM=SDL3 \
  -DCNA_AUDIO_PLATFORM=NULL
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/People
```

A deterministic windowless test/smoke build is:

```bash
cmake -S . -B build-headless \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=HEADLESS \
  -DCNA_PLATFORM=HEADLESS \
  -DCNA_AUDIO_PLATFORM=NULL
cmake --build build-headless --parallel 2
ctest --test-dir build-headless --output-on-failure
```

The current browser build uses CNA's Emscripten-only 2D Canvas renderer. With
an activated emsdk and its zlib port available:

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
```

Serve `build-web/` over HTTP and open `People.html`; browsers normally reject
loading the adjacent WASM module correctly from a `file://` URL. The generated
HTML/JS/WASM bundle has built and passed static validation, but no real browser
was connected in the verification session, so interactive Canvas behavior is
not yet claimed.

No modeler or image-generation service is required to build or play. Generated
placeholder textures keep early builds self-contained.

## Controls

- `WASD` or arrow keys: pan the lot.
- Mouse wheel or `+`/`-`: cursor-centered zoom.
- `Q`/`E`: rotate the presentation 90 degrees counter-clockwise/clockwise.
- `F`: toggle the demo door open/closed (temporary developer control).
- Left click: select the object occupying the pointed floor tile.
- Right click: route the demo resident to a free pointed floor tile.
- `Escape`: exit.

`./build/People --smoke-test` draws four bounded frames, one in each world
orientation, then exits. `--smoke-frames N` runs a bounded interactive smoke
session without automatic rotation.

## Documentation

- [analysis.md](analysis.md): design, reference, CNA, scaling, and risk analysis.
- [plan.md](plan.md): stable `PEO-*` implementation ledger.
- [ASSET_PIPELINE.md](ASSET_PIPELINE.md): four-view visual standard and provenance.
- [THIRD_PARTY.md](THIRD_PARTY.md): dependencies and reference-only projects.
- [AGENTS.md](AGENTS.md): mandatory contributor workflow and hard boundaries.
- [VERIFICATION.md](VERIFICATION.md): build, test, runtime, and visual evidence.
- [NEXT.md](NEXT.md): exact continuation state for the next development context.
