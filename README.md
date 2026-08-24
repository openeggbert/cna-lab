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

The repository is at its planning foundation. The first executable milestone
will provide a CNA window, a generated 20 x 20 isometric lot, camera pan and
zoom, four rotations, and mouse tile picking. See [plan.md](plan.md) for stable
tasks and [analysis.md](analysis.md) for the architectural rationale.

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

Observed baseline on 2026-08-24:

- CNA next: branch `next`, HEAD
  `33ff296f5ffe42cfa9c3a2060da55a953f2a9f4e`; its working tree had 31
  pre-existing changes, so the SHA alone does not reproduce that checkout.
- sharp-runtime next: branch `next`, HEAD
  `54578590b328aa9612fe38bfddca9fd8ca795144`, tag
  `v0.1.0-beta.1`; working tree clean.

## Build

The executable is added in the second milestone commit. From that commit
onward, the native development build is configured and tested with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The default displayed build uses CNA's selected renderer. A deterministic
windowless smoke build can be requested with:

```bash
cmake -S . -B build-headless \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=HEADLESS \
  -DCNA_PLATFORM=HEADLESS \
  -DCNA_AUDIO_PLATFORM=NULL
cmake --build build-headless --parallel 2
ctest --test-dir build-headless --output-on-failure
```

No modeler or image-generation service is required to build or play. Generated
placeholder textures keep early builds self-contained.

## Documentation

- [analysis.md](analysis.md): design, reference, CNA, scaling, and risk analysis.
- [plan.md](plan.md): stable `PEO-*` implementation ledger.
- [ASSET_PIPELINE.md](ASSET_PIPELINE.md): four-view visual standard and provenance.
- [THIRD_PARTY.md](THIRD_PARTY.md): dependencies and reference-only projects.
- [AGENTS.md](AGENTS.md): mandatory contributor workflow and hard boundaries.

