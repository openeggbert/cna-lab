# Validation record

## Completed for this scaffold

- Every Iron Shadows `.cpp` file passed a C++23 syntax-only compile against the actual supplied CNA and sharp-runtime headers with software-backend definitions.
- The prototype MC3 scene passed the supplied Mesh Craft `mc3.xsd`.
- `./scripts/preflight.sh` confirms CNA, sharp-runtime, EasyGL, and cna-extended siblings, plus populated CNA-vendored SDL/SDL_image/SDL_mixer.
- The full `compile-software` preset configured and built (780 targets, `-j4`, ccache), including `cna-extended` linking against the parent-provided `CNA` target as designed.
- `iron_shadows_core_tests` linked against the real project sources, CNA, sharp-runtime, and `CNA_EXTENDED`, and all tests (collision, vehicle, mission, dialogue, save round-trip) passed via `ctest --preset compile-software`.
- CMake target and backend names used by Iron Shadows were checked against CNA's and cna-extended's current CMake files.
- The full MC3 -> GLB -> CNJ pipeline ran end to end for a real production asset: `assets/source/mc3/warehouse.mc3.xml` validated against `mc3.xsd`, converted via Mesh Craft's `mc3togltf` and CNA's `cna_tool_gltf_to_cnj` (both already built in this workspace), producing `warehouse.cnj` + binary vertex/index sidecars. `./cmake-build-compile-software/iron_shadows --smoke 30` loaded it through `Content.Load<Model>()` (confirmed by the `[IronShadows] Loaded generated warehouse.cnj` log line), drew it in place of the procedural warehouse box, and exited cleanly; `ctest --preset compile-software` still passes, confirming the mission/collision logic is unaffected.
- The same pipeline was repeated for the sedan as four single-object MC3 files (`vehicle_body`/`vehicle_cabin`/`vehicle_windshield`/`vehicle_wheel`, one MC3 file per part because `cna_tool_gltf_to_cnj` does not bake per-object node transforms -- see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md` `IS-10-004b`), composed by `PrototypeRenderer` with Iron Shadows' own per-part transforms; the smoke run logs `[IronShadows] Loaded generated vehicle_{body,cabin,windshield,wheel}.cnj` and `ctest` still passes.
- Jolt Physics v5.6.0 (shared checkout at `~/deps/jolt`) was added as a dependency and built successfully as part of the `compile-software` preset (GPU-compute shader options disabled; not needed for CPU rigid-body/character/vehicle physics). `IronShadows::Physics::PhysicsWorld` (`src/Physics/PhysicsWorld.cpp`) hides every Jolt type behind a PIMPL boundary. `tests/PhysicsTests.cpp` (`iron_shadows_physics_tests`, run via `ctest`) exercises and passes 5 scenarios: a raycast hitting a known static floor, a dynamic box settling under gravity, a trigger volume firing enter/exit events, a capsule character controller stopped by a wall while remaining grounded, and a 4-wheel `VehicleConstraint` vehicle settling onto its suspension and then driving forward under throttle input.
- `PlayerController` and `VehicleController` were migrated to be driven by `PhysicsWorld` instead of the old fixed-height/kinematic math, with world geometry collision coming from real static bodies (`PrototypeWorld::BuildPhysicsStaticBodies`, including a dedicated ground-plane body -- a real bug: the render-only ground box is deliberately `collidable=false` for the unrelated XZ-only `CanOccupy` check, so it was silently never getting a physics body until this was added). `tests/CoreTests.cpp` gained `TestPlayerMotion` (walks forward from spawn, and confirms walking into the hotel's static collider for 5 simulated seconds does not tunnel through it) and an updated `TestVehicleMotion` (confirms the physics-driven vehicle accelerates on the real district's ground plane); both pass via `ctest --preset compile-software`. A standalone diagnostic (not committed) confirmed the sedan settles with all four wheels in ground contact and accelerates smoothly. **Not verified**: actual driving/handling *feel* (acceleration curve, top speed, steering sharpness) and visual alignment of the rendered mesh with the physics capsule/chassis, since this environment has no display or interactive input -- see `NEXT.md`.
- Gate M5 (second district): `IronShadows::DistrictManager` (`src/World/DistrictManager.cpp`) was added to own the currently loaded `PrototypeWorld` and its static physics bodies, and a second, genuinely different `Countryside` district was added alongside `WarehouseBlock`, each with its own exit trigger back to the other. `PhysicsWorld::GetBodyCount()` was added (test/diagnostic only, wraps `JPH::PhysicsSystem::GetNumBodies()`) so `tests/CoreTests.cpp`'s new `TestDistrictTransition` could assert that two full round trips (warehouse -> countryside -> warehouse -> countryside) leave the physics body count exactly where it started each time it revisits the same district -- this caught a test-design mistake (asserting equal body counts after a *one-way* swap, which is wrong since the two districts have a different number of static bodies) before it was corrected to only assert equality on round trips. `SaveSnapshot`/`SaveGame` gained a `districtId` field (additive, no save-format version bump) and `TestSaveRoundTrip` now covers it. A full `./scripts/check-syntax.sh` pass and a `./cmake-build-compile-software/iron_shadows --smoke 120` run (exit 0, correct warehouse/vehicle CNJ loading logged) both passed after this change. **Not verified**: actually walking/driving through an exit trigger interactively (no display access), and the loading screen's visual appearance beyond a dark clear + "Loading..." window title.

## Full CNA-linked build status

A full Iron Shadows executable (`iron_shadows`) now links successfully in this workspace using the `compile-software` preset. The CNA-vendored SDL/SDL_image/SDL_mixer submodules are populated here, and both `easy-gl` and `cna-extended` are present as siblings, so the earlier missing-submodule/missing-sibling limitation no longer applies in this environment. The `dev-easygl`/`dev-vulkan` presets (real rendering backends) have not yet been build-verified here; only `compile-software` has been exercised end to end.

## Reproduction

```bash
./scripts/preflight.sh compile-software
./scripts/check-syntax.sh
MESH_CRAFT_SOURCE_DIR=../mesh-craft ./scripts/validate-mc3.sh
```

After dependencies are complete:

```bash
./scripts/configure.sh dev-easygl
./scripts/build.sh dev-easygl
./scripts/test.sh dev-easygl
./scripts/run.sh dev-easygl --smoke 120
```
