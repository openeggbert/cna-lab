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

The renderer uses colored boxes only. It is a debug scaffold designed to be replaced incrementally by MC3/glTF/CNJ content.

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
```

This runs:

```text
prototype_city_block.mc3.xml
  → mc3togltf → prototype_city_block.glb
  → cna_tool_gltf_to_cnj → CNJ model and binary sidecars
```

The executable does not yet load the generated CNJ scene; connecting that runtime path is an early task in `plan.md`.

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

Replace one procedural building and the sedan with generated CNJ content while preserving the existing playable mission. That proves the complete Mesh Craft → CNA runtime loop before expanding the map.
