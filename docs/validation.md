# Validation record

## Completed for this scaffold

- Every Iron Shadows `.cpp` file passed a C++23 syntax-only compile against the actual supplied CNA and sharp-runtime headers with software-backend definitions.
- The prototype MC3 scene passed the supplied Mesh Craft `mc3.xsd`.
- `./scripts/preflight.sh` confirms CNA, sharp-runtime, EasyGL, and cna-extended siblings, plus populated CNA-vendored SDL/SDL_image/SDL_mixer.
- The full `compile-software` preset configured and built (780 targets, `-j4`, ccache), including `cna-extended` linking against the parent-provided `CNA` target as designed.
- `iron_shadows_core_tests` linked against the real project sources, CNA, sharp-runtime, and `CNA_EXTENDED`, and all tests (collision, vehicle, mission, dialogue, save round-trip) passed via `ctest --preset compile-software`.
- CMake target and backend names used by Iron Shadows were checked against CNA's and cna-extended's current CMake files.
- The full MC3 -> GLB -> CNJ pipeline ran end to end for a real production asset: `assets/source/mc3/warehouse.mc3.xml` validated against `mc3.xsd`, converted via Mesh Craft's `mc3togltf` and CNA's `cna_tool_gltf_to_cnj` (both already built in this workspace), producing `warehouse.cnj` + binary vertex/index sidecars. `./cmake-build-compile-software/iron_shadows --smoke 30` loaded it through `Content.Load<Model>()` (confirmed by the `[IronShadows] Loaded generated warehouse.cnj` log line), drew it in place of the procedural warehouse box, and exited cleanly; `ctest --preset compile-software` still passes, confirming the mission/collision logic is unaffected.

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
