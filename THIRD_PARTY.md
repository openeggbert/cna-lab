# Third-party dependencies

Iron Gang's original source code is MIT-licensed. Dependencies keep their own licenses and are not relicensed by this repository.

| Dependency | Role | Expected repository relationship | License handling |
|---|---|---|---|
| CNA (`cnanext`) | Modular game loop, graphics, input, audio, and content; Iron Gang directly links only `CNA::GraphicsCore` and `CNA::Runtime` | Sibling checkout | Keep CNA copyright and license notices |
| sharp-runtime | Modular .NET-like C++ runtime services; Iron Gang directly links only `SharpRuntime::IO` and `SharpRuntime::Text.Json`, plus CNA's transitive component closure | Sibling checkout required by CNA | Keep sharp-runtime copyright and license notices |
| easy-gl | Recommended CNA OpenGL backend | Sibling checkout when using the `OPENGLES3` renderer | Keep easy-gl notices |
| Jolt Physics | Rigid bodies, character controller, vehicle constraint, raycast/trigger queries (`plan/plan_15-physics-integration.md`) | Shared checkout at `~/deps/jolt`, pinned to tag `v5.6.0` (commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`) | MIT license (Copyright 2021 Jorrit Rouwe); keep Jolt copyright and license notices |
| Mesh Craft | Authoring MC3 scenes and conversion to glTF/GLB | External tool/sibling checkout; not linked into the prototype | Keep Mesh Craft notices and record generated-source provenance |
| SDL and CNA transitive dependencies | Platform, windowing, media, compression | Provided by CNA's build | Distribute required notices with releases |

Two pieces of external content are now bundled (gate M10), both tracked in `assets/licenses/asset-registry.csv` (license audit, `plan/plan_39-vertical-slice-gates.md` `IG-39-069`):

- **font8x8** (Public Domain, no attribution required) — a public-domain 8x8 bitmap font (`https://github.com/dhepper/font8x8`) embedded directly as a C++ array in `src/UI/BitmapFont.cpp` to build the on-screen HUD's `SpriteFont`.
- **Nox Sound Design's "Essentials Series"** (CC0, no attribution required) — three individual sound effects (`assets/audio/engine_loop.wav`, `horn.wav`, `footstep.wav`) extracted from the pack on itch.io.

The per-file content notice, exact SHA-256 values, primary-source links, and local review-evidence
hashes are generated from the approved registry in `THIRD_PARTY_ASSETS.md`. Both notice files are
installed with every CMake package; CTest refuses a stale generated asset notice.

No other external textures, character models, vehicle models, music, or other downloaded content is bundled — everything else (MC3 scenes, the hand-authored test-character glTF, mission/cutscene/dialogue data) is an original Iron Gang asset. Future additions must be entered in `assets/licenses/asset-registry.csv` and reviewed for commercial use, modification, attribution, and redistribution rights.
