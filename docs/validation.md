# Validation record

## Completed for this scaffold

- Every Iron Shadows `.cpp` file passed a C++23 syntax-only compile against the actual supplied CNA and sharp-runtime headers with software-backend definitions.
- The prototype MC3 scene passed the supplied Mesh Craft `mc3.xsd`.
- sharp-runtime configured and built as `libSHARP_RUNTIME.a` in a persistent validation build directory.
- The Iron Shadows core tests were linked from the real project sources, CNA math/color sources, and the built sharp-runtime library; all collision, vehicle, mission, dialogue, and save round-trip tests passed.
- A small executable linked to that library and successfully exercised `System::IO::Directory` and `System::IO::File` by creating, writing, reading, deleting, and removing temporary test data.
- CMake target and backend names used by Iron Shadows were checked against CNA's current CMake files.

## Full CNA-linked build status

A full Iron Shadows executable was not linked in the supplied validation workspace. CNA configuration stopped while resolving SDL because the ZIP contained empty vendored `third_party/SDL`, `third_party/SDL_image`, and `third_party/SDL_mixer` directories, and a compatible system SDL3 package was not available there. The EasyGL sibling repository was also not supplied.

This is a dependency-checkout limitation, not a C++ syntax error found in Iron Shadows. Use a recursive CNA checkout with populated submodules, place sharp-runtime next to CNA, and place EasyGL next to CNA for the EasyGL preset.

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
