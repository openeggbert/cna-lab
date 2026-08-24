# Iron Gang

**Iron Gang** is a provisional name for an original historical open-city action-adventure built in C++ on **CNA** and **sharp-runtime**, with **Mesh Craft / MC3** planned as the main constructional 3D authoring pipeline.

This repository is not a clone of any existing game. It uses the broad genre idea of a cinematic city story with walking, driving, interiors, dialogue, missions, and cutscenes while keeping its own fictional city, characters, plot, map, assets, and identity.

## What already works in this foundation

The current C++ prototype is intentionally simple but not empty. It provides:

- A CNA `Game` application and 60 Hz update loop.
- A procedurally rendered 3D city block made from vertex/index buffers, plus a second, genuinely different countryside district.
- A third-person camera.
- On-foot movement driven by a real Jolt Physics character controller (`JPH::CharacterVirtual` capsule), colliding against static world geometry.
- A driveable sedan driven by a real Jolt Physics 4-wheel raycast vehicle (`JPH::VehicleConstraint`/`WheeledVehicleController`), with acceleration, reverse, steering, drag, and handbrake.
- Entering and exiting the vehicle with **E**.
- A three-line prologue dialogue advanced with **Enter**, alongside a short in-engine intro cutscene (a camera pan to the warehouse delivery target and back) that can be skipped with **Enter** too, once dialogue has finished.
- A data-driven mission flow (`assets/missions/prologue.mission.json`): hear the briefing → reach the car → enter it → drive into the warehouse marker, with a hardcoded fallback if the file is missing/invalid.
- Two discrete districts (a warehouse block and a countryside area) connected by exit triggers and a loading screen, Mafia-1-style — not seamless open-world streaming.
- Save and load with **F5/F9**, implemented through sharp-runtime `System::IO`; the current district persists across a save/load round trip.
- A reset key (**R**).
- Headless core tests for collision, on-foot/vehicle physics motion, district transitions, mission progression, dialogue, and persistence.
- An editable MC3 source scene and an MC3 → GLB → CNJ helper script.
- Two real production assets: the warehouse building and the sedan (as four composed parts — body/cabin/windshield/wheel) are authored in MC3, converted through the full Mesh Craft → CNA pipeline, and loaded in-engine as CNJ models (with a procedural fallback if a generated asset is missing), replacing their procedural counterparts while driving and the delivery mission still work unchanged.
- A skinned test character (hand-authored glTF, since Mesh Craft/MC3 has no rigging/skinning support) plays "Idle"/"Walk" clips, crossfading between them, from live on-foot input, a "Dialogue" pose while dialogue is active, and "EnterVehicle"/"ExitVehicle" clips around getting in/out of the sedan, directly through modular CNA `GraphicsCore`'s `AnimationPlayer`, replacing the procedural on-foot player box when the asset is available.

The renderer otherwise uses colored boxes. It is a debug scaffold designed to be replaced incrementally by MC3/glTF/CNJ content, one building at a time, following the warehouse's example.

## Controls

| Context | Control | Action |
|---|---|---|
| Dialogue | Enter | Advance line |
| On foot | W/S or Up/Down | Move forward/back |
| On foot | A/D | Strafe |
| On foot | Left/Right | Turn |
| On foot | Shift | Sprint |
| Any | Tab | Toggle district map |
| Near car | E | Enter |
| Driving | W/S or Up/Down | Accelerate/reverse |
| Driving | A/D or Left/Right | Steer |
| Driving | Space | Handbrake |
| Driving | E | Exit |
| Any | F5 / F9 | Save / load |
| Any | R | Reset prototype |
| Any | Escape | Quit |

Dialogue, objectives, status, and the toggleable district map are displayed by the in-game HUD;
the window title and console retain a compact diagnostic copy.


## Validation status

The generated source was checked against the supplied repository snapshots rather than only drafted from memory:

- all Iron Gang translation units pass C++23 syntax-only compilation against the modular `../cnanext` and `../sharp-runtime` headers;
- the sample MC3 scene validates against the supplied `mc3.xsd`;
- sharp-runtime built successfully, its `System::IO` file path was executed in a linked smoke program, and the Iron Gang core tests passed when linked with the real CNA math/color implementation;
- the `compile-software` preset configures, builds, links, and runs the test suite against those sibling checkouts.

Use a recursive CNA checkout with populated submodules. See [`docs/validation.md`](docs/validation.md) and run `./scripts/preflight.sh` before configuration.

## Required workspace layout

The modular CNA checkout expects `sharp-runtime` and, for the recommended `OPENGLES3` renderer (implemented through EasyGL), `easy-gl` as sibling repositories. Use this layout:

```text
workspace/
├── cnanext/
├── sharp-runtime/
├── easy-gl/          # required for CNA_GRAPHICS_RENDERER=OPENGLES3
├── mesh-craft/       # authoring/conversion tool; optional for compiling the game
└── iron-gang/
```

Jolt Physics (rigid bodies, character controller, vehicle constraint) is not a workspace sibling; it is a shared checkout at `~/deps/jolt`, pinned to `v5.6.0`, per `CLAUDE.md`'s dependency-reuse convention:

```bash
git clone --branch v5.6.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git ~/deps/jolt
```

Override either path with `-DIRON_GANG_CNA_DIR=...` / `-DIRON_GANG_JOLT_DIR=...` if your layout differs.

## Configure, build, test, run

```bash
cd workspace/iron-gang
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
export CNA_BUILD_DIR=/path/to/cnanext/cmake-build-tools
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

The executable loads `warehouse.cnj` and draws it in place of the procedural warehouse box, and loads `vehicle_body.cnj`/`vehicle_cabin.cnj`/`vehicle_windshield.cnj`/`vehicle_wheel.cnj` (the wheel model reused for all four wheel positions) and composes them with Iron Gang's own per-part transforms in place of the procedural sedan, proving the Mesh Craft → CNA runtime loop end to end while every other building stays procedural for now. The sedan is authored as four single-object MC3 files rather than one multi-part scene because the current `cna_tool_gltf_to_cnj` does not bake per-object glTF node transforms into vertex data — a multi-object MC3 scene loaded as one CNJ model would lose each part's relative position (confirmed empirically; see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md` `IG-10-004b`). If a generated asset is missing (a fresh checkout that has not run `build-assets.sh` yet), the game logs a warning and falls back to procedural geometry instead of failing to start (`scripts/test-missing-asset-fallback.sh` covers both the warehouse and the sedan in `ctest`). Deriving collision from the MC3 `collision` attribute instead of the separate procedural AABB and a standalone GLB validation step are still open (`plan/plan_39-vertical-slice-gates.md`).

## Repository map

```text
assets/                 Source dialogue, mission data, MC3, license registry
cmake/                  Dependency and linker integration
include/IronGang/    Public project headers
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
docs/performance-baseline.md Current EasyGL M12 measurements and open failure
```

## License and assets

Original repository code and original sample assets are MIT-licensed. Dependencies retain their own licenses. Downloaded assets are not accepted without a registry entry and a verified license permitting the intended use and redistribution. See `LICENSE`, `THIRD_PARTY.md`, and `assets/licenses/asset-registry.csv`. The title is provisional; `docs/renaming.md` lists every identifier that must change together.

## Immediate next milestone

Gates M2 and M3 are done: the warehouse building and the sedan both load as generated CNJ models in place of their procedural counterparts, with driving and the delivery mission still working unchanged, asset provenance recorded, and an automated `ctest` regression (`iron_gang_missing_asset_fallback`) covering the missing-asset fallback path for both (`plan/plan_39-vertical-slice-gates.md` gate M2 tasks `IG-39-026/027/029/030/033/034/035`, gate M3 task `IG-39-004`). Still open from M2: a standalone GLB validation step, and deriving collision from the MC3 `collision` attribute instead of the separate procedural AABB (needs the sidecar/MCB metadata compiler from `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md`, not a one-off parse). `PbrEffect` materials turned out not to be a gap: CNA's own conversion tool only emits `PbrEffect` when a material has a real normal/metallic-roughness texture, and the current materials are deliberately flat-color, so `BasicEffect` is already the correct choice — revisit once they get real textures (a content task, not a code task). M3 also surfaced a real pipeline gap worth fixing upstream: `cna_tool_gltf_to_cnj` does not bake per-object node transforms, so a multi-part prop currently needs one MC3 file per part plus manual composition code (`IG-10-004b`) instead of one authored scene.

Gate M4 is done, including the follow-up gameplay migration: **Jolt Physics** (v5.6.0, MIT, pinned in `THIRD_PARTY.md`, shared checkout at `~/deps/jolt`) sits behind `IronGang::Physics::PhysicsWorld` (`include/`/`src/Physics/`), a PIMPL boundary that keeps every Jolt type out of gameplay code, and now actually drives gameplay: `PlayerController`'s on-foot movement is a `JPH::CharacterVirtual` capsule, and `VehicleController`'s driving is a `JPH::VehicleConstraint`/`WheeledVehicleController` 4-wheel raycast vehicle, both colliding against real static bodies built from `PrototypeWorld`'s geometry (`PrototypeWorld::BuildPhysicsStaticBodies`). `tests/PhysicsTests.cpp` proves the standalone character/trigger/raycast/vehicle prototypes; `tests/CoreTests.cpp`'s `TestPlayerMotion`/`TestVehicleMotion` prove the wired-up gameplay versions don't tunnel through world geometry and do accelerate (`plan/plan_39-vertical-slice-gates.md` gate M4, task `IG-39-005`; `plan/plan_15-physics-integration.md` `IG-15-021`/`IG-15-025`/`IG-15-027`). Vehicle engine/transmission/suspension tuning still uses Jolt's own generic defaults rather than a deliberately-tuned "feel" for this sedan, and has not been visually verified (no display/interactive access in the environment that did this migration) — see `NEXT.md` for what to check first with a real display.

Gate M5 is done: `IronGang::DistrictManager` (`include/`/`src/World/`) owns the currently loaded `PrototypeWorld` and its static physics bodies, and a second, genuinely different `Countryside` district was added alongside the original `WarehouseBlock`, each with its own exit trigger back to the other. Requesting a transition destroys the old district's static bodies, constructs the new `PrototypeWorld`, rebuilds its bodies and renderer geometry (`PrototypeRenderer::RebuildStaticGeometry`), and shows a loading screen with a 0.6s minimum display time; mission state and the player's driving/vehicle state carry across unchanged, and `SaveGame` now persists the current district id so a save/load round trip restores the right one. `tests/CoreTests.cpp`'s `TestDistrictTransition` proves two full round trips leak no static physics bodies and that arrival is signaled exactly once per transition (`plan/plan_39-vertical-slice-gates.md` gate M5, task `IG-39-006`; `plan/plan_13-world-partitioning-and-streaming.md` for the itemized breakdown). Still open: background/async loading (loading is currently synchronous, the loading screen only enforces a minimum display time), a longer soak test beyond two round trips, and per-district mutable world state (unlocked doors, taken pickups, etc.) beyond spawn/exit points — none of these existed to test yet in this prototype.

Gate M6 is done, at a prototype fidelity level. The first implementation used a `cna-extended` ECS component/system around CNA's general-purpose `Model` + `AnimationPlayer` path. After CNA's modularization, Iron Gang moved the same small playback state and 0.25-second per-bone `Matrix::Lerp` crossfade into `PrototypeRenderer` and consumes `AnimationPlayer` directly from `CNA::GraphicsCore`; `cna-extended` is no longer a build dependency. A hand-authored 3-bone test character (`assets/source/gltf/test_character.gltf`; Mesh Craft/MC3 has no rigging/skinning authoring support, so MC3 is deliberately bypassed here, unlike every other asset in this repo) was converted through `cna_tool_gltf_to_cnj` to `assets/generated/models/cnj/test_character.cnj` (5 clips) and plays "Idle"/"Walk" selected from live on-foot input (`IronGangGame::Update`), crossfading between them; a "Dialogue" clip while `DialogueSystem::IsActive()`; and "EnterVehicle"/"ExitVehicle" one-shot clips driving a small `VehicleTransitionState` state machine. The modular software build and smoke path exercise the real CNJ model and animation player without pulling in a second ECS/rendering framework (`plan/plan_39-vertical-slice-gates.md` gate M6, task `IG-39-007`; `plan/plan_18-characters-skeletons-and-animation.md` for the itemized sub-task breakdown). Not done or verified: sit/drive/steer poses while actually driving, and any visual/interactive check of these animations in an environment with a display.

Gate M7 is done, at prototype fidelity. `assets/missions/prologue.mission.json` (a pre-existing stub from the original scaffold, previously just documentation of an "intended future form") is now the real, active mission definition: 5 states, each with objective text, a named transition condition, and a next-state id, loaded via a new `MissionDefinition`/`LoadMissionDefinition` (`include/`/`src/Missions/`, parsed with sharp-runtime's own `System::Text::Json` -- already a linked dependency, no new library needed) with inline validation (duplicate ids, dangling references, unknown condition names all rejected with an actionable error). `PrototypeMission` looks up state definitions instead of a hardcoded switch, keeping its enum-based public API and `SaveGame` compatibility unchanged, and falls back to an identical hardcoded default if the file fails to load. Not a general condition/action expression language -- conditions are a small, fixed, named set (`plan/plan_24-mission-framework-and-scripting.md`'s own explicit non-goal). Verified: the existing mission-flow test (hardcoded default, unchanged) plus two new tests (the real committed file drives the identical flow; 6 malformed/missing-file cases correctly rejected) and a `--smoke` run with no crash (`plan/plan_39-vertical-slice-gates.md` gate M7, task `IG-39-008`; `plan/plan_24-mission-framework-and-scripting.md` for the itemized breakdown).

Gate M8 is done, at prototype fidelity (camera-track only). New `CutscenePlayer`/`CutsceneSequence` (`include/`/`src/Cutscenes/`) play a hand-written, versioned JSON camera keyframe sequence (`assets/cutscenes/prologue_intro.cutscene.json`) -- a 2.5s pan from an establishing shot of the warehouse delivery target to the exact framing of the normal gameplay follow-camera at the player's spawn point, authored by hand so the cut back to gameplay has no visible pop. It starts alongside the opening dialogue in `IronGangGame::Initialize()`; pressing Enter once dialogue has finished but the cutscene is still playing skips straight to its terminal camera state, identical to a natural finish; `LoadPrototype()`/`ResetPrototype()` both force it to that terminal state defensively for save-safety; player/vehicle input and physics stay frozen while it plays (the same mechanism dialogue already uses), and `Draw()` only overrides the camera while it's active, so control (and the camera) return to normal gameplay automatically once it finishes. Verified: 3 new unit tests (advance/interpolate/finish with fixed deltaSeconds, skip applies the exact terminal state, and 5 malformed-data cases + a missing file all correctly rejected), a standalone diagnostic confirming the real committed file parses to the intended keyframes, and `--smoke` runs covering both mid-playback and the cutscene's full natural completion with no crash (`plan/plan_39-vertical-slice-gates.md` gate M8, task `IG-39-009`; `plan/plan_26-cutscenes-and-cinematic-sequencing.md` for the itemized breakdown). Not done: animation/dialogue/audio/event/fade tracks, a timeline debug overlay, or any visual/interactive check of how the pan actually looks, since this environment has no display.

Gate M9 is done at first-pass/prototype fidelity, including the gate's own literal "ten-minute soak" wording (verified via a ~16.3-minute background `--smoke 3000` run with no crash). A new shared `WaypointPath`/`AdvanceAlongPath()` (`include/`/`src/World/`) -- a hand-authored ordered polyline, not a graph -- underlies both new `TrafficVehicle` (accelerates toward a cruise speed, brakes smoothly for an obstacle ahead) and new `Pedestrian` (walks a fixed sidewalk path, overridden by a timed flee-directly-away state near the player's vehicle), `include/`/`src/Gameplay/`. New `PoliceSystem` runs a `Clear -> Dispatched -> Chasing` state machine off a simplified fixed-radius "witness" check (not real vision-cone/line-of-sight): a witnessed speeding/close-proximity offense dispatches a patrol car after a fixed delay; it then drives straight at the player (ignoring roads); a second patrol car joins after a fixed escalation time (the one locked escalation tier); the chase resolves back to Clear once sustained beyond a fixed distance for a few seconds. `PrototypeWorld::BuildWarehouseBlock()` hand-authors one traffic loop and two sidewalk paths (`BuildCountryside()` has none, by design); `IronGangGame` spawns/ticks/renders 2 traffic vehicles and 2 pedestrians every frame regardless of dialogue/cutscenes (gated only on district transitions), with none of this ambient state persisted to `SaveGame`. Verified: 4 new deterministic unit tests including a full `Clear -> Dispatched -> Chasing -> escalate -> resolve` cycle against hand-computed, standalone-diagnostic-confirmed values, the full `ctest` suite, `./scripts/check-syntax.sh`, two short `--smoke` runs, and a ~16.3-minute (980-second) `--smoke 3000` background soak run (`date +%s` timed before/after) that exited cleanly with no crash, error, or asset-fallback message while all three systems ticked every frame (`plan/plan_39-vertical-slice-gates.md` gate M9, task `IG-39-010`/`049`; `plan/plan_19-navigation-and-pathfinding.md`, `plan/plan_20-pedestrians-and-ambient-ai.md`, `plan/plan_21-traffic-simulation.md`, `plan/plan_22-police-witnesses-crime-and-wanted-response.md` for the itemized breakdown). Not done: traffic signals, real vision-cone perception, lane-graph route-following for patrol cars, local avoidance, 10-20 pedestrians/3-5 vehicles (only 2 each), and any debug overlay.

Gate M10 is done at first-pass/prototype fidelity across all five pieces: **UI** — a real on-screen `SpriteBatch`/`SpriteFont` HUD (`include/`/`src/UI/BitmapFont.hpp`/`.cpp`, a hand-built bitmap font from the public-domain font8x8 glyphs, since CNA has no XNB font pipeline) replacing the window-title-only display. **Dynamic sun** — a single shared sun direction (`include/IronGang/Graphics/SunLight.hpp`) applied as a CPU-computed per-actor `DiffuseColor` brightness multiplier (confirmed `BasicEffect`'s built-in lighting is a no-op on the SOFTWARE backend; `DiffuseColor` itself is NOT). **Limited shadows** — simple alpha-blended ground-decal "blob shadows" under the player and their own vehicle (real shadow-mapping isn't achievable without modifying CNA). **Baked lighting** — a real lightmap texture atlas (`include/`/`src/Graphics/LightmapMesh.hpp`/`.cpp`) for the procedural static city mesh, one flat-shaded tile per box face, sampled via `DualTextureEffect` (confirmed fully implemented on the SOFTWARE backend); MC3-sourced models (warehouse/vehicle) stay out of scope, no lightmap UV channel in that pipeline yet. **Audio** — real CC0 sound (Nox Sound Design's "Essentials Series" pack, itch.io; the user downloaded it manually since the free-download flow needs browser JS this environment can't run): a looped engine idle tied to `playerDriving_` with speed-scaled volume/pitch, a one-shot horn (H key), and one-shot footsteps on foot, via `SoundEffect`/`SoundEffectInstance`. No ambience/siren (not in the chosen pack). Verified: new unit tests (`TestSunBrightnessMatchesHandComputedValue`, `TestLightmapMeshBuilderBakesPerFaceBrightness`, `TestBitmapFontGlyphAtlas`), the full `ctest` suite, `./scripts/check-syntax.sh`, `--smoke` runs, and a standalone diagnostic that exercised real audio playback end to end through SDL3_mixer (`plan/plan_39-vertical-slice-gates.md` gate M10, task `IG-39-011`; `plan/plan_08`, `plan_27`, `plan_28` for the itemized per-area breakdown). Measured a real ~4-5x per-frame slowdown from the lightmap's two-texture-sample draw path on this environment's CPU software rasterizer, not yet profiled against `docs/performance-targets.md` (gate M12 scope). Not verified: how any of this actually looks or sounds, since this environment has no display and cannot play audio for a human to hear.

Gate M11 is done: five new integration tests prove the mission's happy-path/save-load/cutscene-skip/retry/vehicle-separation/district-transition scenarios end to end (`tests/CoreTests.cpp`: `TestSaveLoadMidMissionPlaythrough`, `TestCutsceneSkipDoesNotBlockMissionProgression`, `TestMissionResetActsAsRetry`, `TestVehicleStatePersistsIndependentlyOfPlayer`, `TestDistrictTransitionPreservesMissionState`) -- note this prototype's one mission has no real failure/branching state or vehicle-destruction mechanic yet, so "failure retry" and "vehicle-loss recovery" are proven at the level that actually exists (`Reset()`, and independent save/load of player/vehicle position) rather than invented from scratch. A `--smoke 3000` soak ran for 65 minutes (stopped deliberately, not crashed -- the M10 lightmap draw path made it far slower per frame than the M9 baseline) with no error in the log. The older CPU-software performance capture is superseded for M12 by the EasyGL measurements below. A license audit found and fixed two real gaps: two original data files missing from `assets/licenses/asset-registry.csv`, and `THIRD_PARTY.md` still claiming no external content was bundled (false as of gate M10's font/audio additions).

All of gates M0-M11 are now fully done at prototype/first-pass fidelity. M12 is instrumented but open: `--profile <json>` plus `--profile-scenario intro|idle|walk|drive|mixed` records deterministic EasyGL workloads, `--vsync on|off` controls the requested presentation interval, and the schema-7 report separates request from platform swap acknowledgement, CPU Draw submission, asynchronous real GPU Draw-range timing, EndDraw/Present CPU, scoped 3D workload counts, exact Jolt-seam physics workload, exact ambient-AI state/loop work, exact game-owned audio state/control work, and per-transition district world/physics vs renderer-upload phases with target counts and memory deltas. Audio backend voice lifetime, decoder/mixer, channel, and bus costs remain explicitly unavailable through CNA rather than being reported as zero. Historic hardware-backed mixed captures fail at 51.628-57.705 ms despite small subsystem times, while identical intro runs varied between 51.381 ms and 16.897 ms; those captures predate GPU timing and need a controlled rerun. Isolated Xvfb/llvmpipe full-mixed validation passes 30 FPS but cannot qualify real hardware or v-sync; it explicitly rejects both tested 0/1 swap intervals. RAM, physics/AI/audio CPU, and real district-transition time pass. VRAM reporting includes a category breakdown for game-owned resources plus deduplicated imported CNJ buffers and effect-bound textures, but full backend residency is still not exposed by CNA. See `docs/performance-baseline.md`. Next: require an acknowledged interval and use the Draw/GPU/Present split on controlled named hardware, then obtain backend/external VRAM residency before marking `IG-39-013` done.
