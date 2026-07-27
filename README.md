# Iron Shadows

**Iron Shadows** is a provisional name for an original historical open-city action-adventure built in C++ on **CNA** and **sharp-runtime**, with **Mesh Craft / MC3** planned as the main constructional 3D authoring pipeline.

This repository is not a clone of any existing game. It uses the broad genre idea of a cinematic city story with walking, driving, interiors, dialogue, missions, and cutscenes while keeping its own fictional city, characters, plot, map, assets, and identity.

## What already works in this foundation

The current C++ prototype is intentionally simple but not empty. It provides:

- A CNA `Game` application and 60 Hz update loop.
- A procedurally rendered 3D city block made from vertex/index buffers.
- A third-person camera.
- On-foot movement with simple building collision.
- A driveable kinematic sedan with acceleration, reverse, steering, drag, handbrake, and collision response.
- Entering and exiting the vehicle with **E**.
- A three-line prologue dialogue advanced with **Enter**.
- A mission flow: hear the briefing → reach the car → enter it → drive into the warehouse marker.
- Save and load with **F5/F9**, implemented through sharp-runtime `System::IO`.
- A reset key (**R**).
- Headless core tests for collision, vehicle movement, mission progression, dialogue, and persistence.
- An editable MC3 source scene and an MC3 → GLB → CNJ helper script.
- Two real production assets: the warehouse building and the sedan (as four composed parts — body/cabin/windshield/wheel) are authored in MC3, converted through the full Mesh Craft → CNA pipeline, and loaded in-engine as CNJ models (with a procedural fallback if a generated asset is missing), replacing their procedural counterparts while driving and the delivery mission still work unchanged.
- A prototyped Jolt Physics integration (`IronShadows::Physics::PhysicsWorld`) with tested character, trigger, raycast, and 4-wheel vehicle behavior, not yet wired into gameplay movement.

The renderer otherwise uses colored boxes. It is a debug scaffold designed to be replaced incrementally by MC3/glTF/CNJ content, one building at a time, following the warehouse's example.

## Controls

| Context | Control | Action |
|---|---|---|
| Dialogue | Enter | Advance line |
| On foot | W/S or Up/Down | Move forward/back |
| On foot | A/D | Strafe |
| On foot | Left/Right | Turn |
| On foot | Shift | Sprint |
| Near car | E | Enter |
| Driving | W/S or Up/Down | Accelerate/reverse |
| Driving | A/D or Left/Right | Steer |
| Driving | Space | Handbrake |
| Driving | E | Exit |
| Any | F5 / F9 | Save / load |
| Any | R | Reset prototype |
| Any | Escape | Quit |

Current dialogue and objectives are displayed in the window title and console. A proper HUD is planned.


## Validation status

The generated source was checked against the supplied repository snapshots rather than only drafted from memory:

- all Iron Shadows translation units passed C++23 syntax-only compilation against the real CNA and sharp-runtime headers;
- the sample MC3 scene validates against the supplied `mc3.xsd`;
- sharp-runtime built successfully, its `System::IO` file path was executed in a linked smoke program, and the Iron Shadows core tests passed when linked with the real CNA math/color implementation;
- a full CNA-linked game build was **not** completed in the validation workspace because the supplied CNA ZIP had empty SDL/SDL_image/SDL_mixer submodule directories and no EasyGL sibling repository.

Use a recursive CNA checkout with populated submodules. See [`docs/validation.md`](docs/validation.md) and run `./scripts/preflight.sh` before configuration.

## Required workspace layout

CNA currently expects `sharp-runtime` and, for the recommended EasyGL backend, `easy-gl` as sibling repositories. Iron Shadows also depends on `cna-extended` for its ECS, 3D transform hierarchy, 3D collision/octree, and skinned-model playback, instead of reimplementing that boilerplate. Use this layout:

```text
workspace/
├── cna/
├── sharp-runtime/
├── easy-gl/          # required for CNA_GRAPHICS_BACKEND=EASYGL
├── cna-extended/      # ECS, Transform3, 3D collision/octree, skinned-model playback
├── mesh-craft/       # authoring/conversion tool; optional for compiling the game
└── iron-shadows/
```

Jolt Physics (rigid bodies, character controller, vehicle constraint) is not a workspace sibling; it is a shared checkout at `~/deps/jolt`, pinned to `v5.6.0`, per `CLAUDE.md`'s dependency-reuse convention:

```bash
git clone --branch v5.6.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git ~/deps/jolt
```

Override either path with `-DIRON_SHADOWS_CNA_DIR=...` / `-DIRON_SHADOWS_JOLT_DIR=...` if your layout differs.

## Configure, build, test, run

```bash
cd workspace/iron-shadows
cmake --preset dev-easygl
cmake --build --preset dev-easygl --parallel 4
ctest --preset dev-easygl
./scripts/run.sh dev-easygl
```

For Vulkan:

```bash
cmake --preset dev-vulkan
cmake --build --preset dev-vulkan --parallel 4
./scripts/run.sh dev-vulkan
```

A software-backend preset is included primarily as a compile and core-test check. The prototype's real 3D rendering path should use EasyGL or Vulkan.

The build policy intentionally uses persistent `cmake-build-*` directories, ccache, and no more than four jobs to avoid unnecessary SSD writes.

## MC3 asset conversion

Build Mesh Craft and CNA's conversion tools first, then set:

```bash
export MESH_CRAFT_BUILD_DIR=/path/to/mesh-craft/cmake-build-release
export CNA_BUILD_DIR=/path/to/cna/cmake-build-tools
./scripts/build-assets.sh
./scripts/build-assets.sh assets/source/mc3/warehouse.mc3.xml assets/generated/models
./scripts/build-assets.sh assets/source/mc3/vehicle_body.mc3.xml assets/generated/models
./scripts/build-assets.sh assets/source/mc3/vehicle_cabin.mc3.xml assets/generated/models
./scripts/build-assets.sh assets/source/mc3/vehicle_windshield.mc3.xml assets/generated/models
./scripts/build-assets.sh assets/source/mc3/vehicle_wheel.mc3.xml assets/generated/models
```

This runs, for every input listed above:

```text
<name>.mc3.xml → mc3togltf → <name>.glb → cna_tool_gltf_to_cnj → CNJ + binary sidecars
```

All generated CNJ output shares one content root, `assets/generated/models/cnj/`, which the executable points its `ContentManager` at during startup.

The executable loads `warehouse.cnj` and draws it in place of the procedural warehouse box, and loads `vehicle_body.cnj`/`vehicle_cabin.cnj`/`vehicle_windshield.cnj`/`vehicle_wheel.cnj` (the wheel model reused for all four wheel positions) and composes them with Iron Shadows' own per-part transforms in place of the procedural sedan, proving the Mesh Craft → CNA runtime loop end to end while every other building stays procedural for now. The sedan is authored as four single-object MC3 files rather than one multi-part scene because the current `cna_tool_gltf_to_cnj` does not bake per-object glTF node transforms into vertex data — a multi-object MC3 scene loaded as one CNJ model would lose each part's relative position (confirmed empirically; see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md` `IS-10-004b`). If a generated asset is missing (a fresh checkout that has not run `build-assets.sh` yet), the game logs a warning and falls back to procedural geometry instead of failing to start (`scripts/test-missing-asset-fallback.sh` covers both the warehouse and the sedan in `ctest`). Deriving collision from the MC3 `collision` attribute instead of the separate procedural AABB and a standalone GLB validation step are still open (`plan/plan_39-vertical-slice-gates.md`).

## Repository map

```text
assets/                 Source dialogue, mission data, MC3, license registry
cmake/                  Dependency and linker integration
include/IronShadows/    Public project headers
src/Application/        CNA game loop
src/Graphics/           Temporary procedural renderer
src/Gameplay/           Player and vehicle controllers
src/World/              Prototype world and collision
src/Missions/           Mission state machine
src/Dialogue/           Dialogue loading/progression
src/Persistence/        sharp-runtime-based save/load
scripts/                Build, test, run, and asset helpers
tests/                  Window-free core tests
analysis.md             Technical feasibility and architecture analysis
plan.md                 Master plan index; links to the 41 group files below
plan/                   2,148-task backlog split into one file per group
docs/renaming.md        Checklist for replacing the provisional title safely
docs/performance-targets.md  Realistic hardware/RAM/VRAM targets for this game
```

## License and assets

Original repository code and original sample assets are MIT-licensed. Dependencies retain their own licenses. Downloaded assets are not accepted without a registry entry and a verified license permitting the intended use and redistribution. See `LICENSE`, `THIRD_PARTY.md`, and `assets/licenses/asset-registry.csv`. The title is provisional; `docs/renaming.md` lists every identifier that must change together.

## Immediate next milestone

Gates M2 and M3 are done: the warehouse building and the sedan both load as generated CNJ models in place of their procedural counterparts, with driving and the delivery mission still working unchanged, asset provenance recorded, and an automated `ctest` regression (`iron_shadows_missing_asset_fallback`) covering the missing-asset fallback path for both (`plan/plan_39-vertical-slice-gates.md` gate M2 tasks `IS-39-026/027/029/030/033/034/035`, gate M3 task `IS-39-004`). Still open from M2: a standalone GLB validation step, and deriving collision from the MC3 `collision` attribute instead of the separate procedural AABB (needs the sidecar/MCB metadata compiler from `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md`, not a one-off parse). `PbrEffect` materials turned out not to be a gap: CNA's own conversion tool only emits `PbrEffect` when a material has a real normal/metallic-roughness texture, and the current materials are deliberately flat-color, so `BasicEffect` is already the correct choice — revisit once they get real textures (a content task, not a code task). M3 also surfaced a real pipeline gap worth fixing upstream: `cna_tool_gltf_to_cnj` does not bake per-object node transforms, so a multi-part prop currently needs one MC3 file per part plus manual composition code (`IS-10-004b`) instead of one authored scene.

Gate M4 is done, including the follow-up gameplay migration: **Jolt Physics** (v5.6.0, MIT, pinned in `THIRD_PARTY.md`, shared checkout at `~/deps/jolt`) sits behind `IronShadows::Physics::PhysicsWorld` (`include/`/`src/Physics/`), a PIMPL boundary that keeps every Jolt type out of gameplay code, and now actually drives gameplay: `PlayerController`'s on-foot movement is a `JPH::CharacterVirtual` capsule, and `VehicleController`'s driving is a `JPH::VehicleConstraint`/`WheeledVehicleController` 4-wheel raycast vehicle, both colliding against real static bodies built from `PrototypeWorld`'s geometry (`PrototypeWorld::BuildPhysicsStaticBodies`). `tests/PhysicsTests.cpp` proves the standalone character/trigger/raycast/vehicle prototypes; `tests/CoreTests.cpp`'s `TestPlayerMotion`/`TestVehicleMotion` prove the wired-up gameplay versions don't tunnel through world geometry and do accelerate (`plan/plan_39-vertical-slice-gates.md` gate M4, task `IS-39-005`; `plan/plan_15-physics-integration.md` `IS-15-021`/`IS-15-025`/`IS-15-027`). Vehicle engine/transmission/suspension tuning still uses Jolt's own generic defaults rather than a deliberately-tuned "feel" for this sedan, and has not been visually verified (no display/interactive access in the environment that did this migration) — see `NEXT.md` for what to check first with a real display.

Next: **M5 — a second district**, forcing the discrete district-loading design (loading screen, world/save state preserved across the transition) that gate M2/M3's single always-loaded block never had to prove.
