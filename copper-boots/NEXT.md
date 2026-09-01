# Copper Boots continuation handoff

Updated: 2026-08-24

## Repository state

- Work on branch `develop`; `main` intentionally remains the documentation-only
  baseline.
- The current feature is a two-stage campaign: Green Ruins Relay followed by
  Brassworks Shift (`--stage 2` starts Factory directly).
- `MAR-036A`, M12, web milestone M13 and `MAR-074` are complete. `MAR-036`
  remains open for four more original stages.
- Immediate task: implement `MAR-036B` (Underground), centered on conduit
  transitions, tight navigation and aimed projectiles.
- Current web artifacts are generated under ignored `build-web/`; rebuild them
  rather than committing binaries.

Always run `git status --short`, `git log -1 --oneline`, and inspect `plan.md`
before changing files.

## Pinned local dependencies

- CNA: sibling `../cna`, branch `develop`, commit
  `1bb2145d99ed572dd4eb15009c34e2e5f410fcf0`.
- sharp-runtime: sibling `../sharp-runtime`, branch `develop`, commit
  `54578590b328aa9612fe38bfddca9fd8ca795144`.
- CNA already contains the embedded-consumer fixes recorded as closed
  `MAR-CNA-001`/`002`; do not restore the removed include workarounds.
- Web-only `MAR-CNA-004` through `006` remain narrowly worked around in the
  root CMake file. Do not broaden them or edit dependency trees casually.
- Preserve the unrelated untracked CNA discovery JSON noted in `analysis.md`.

## Verified native commands

Use no more than two build jobs:

```bash
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -j2

cmake --build build-software --parallel 2
ctest --test-dir build-software --output-on-failure -j2

DISPLAY=:0 ./build/copper-boots --stage 2 --no-settings
```

Both native renderer lanes should report six tests: renderer-free gameplay,
normal/silent/Factory CNA smoke, isolated storage, and display lifecycle.

## Verified web commands

With emsdk activated and `EMSCRIPTEN` pointing at its Emscripten directory:

```bash
emcmake cmake -S . -B build-web -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCNA_ROOT_DIR=../cna \
  -DCNA_GRAPHICS_RENDERER=CANVAS \
  -DCOPPER_BOOTS_BUILD_TESTS=OFF
cmake --build build-web --parallel 2
python3 -m http.server 8000 --directory build-web
```

Emscripten 6.0.3 produced HTML/JS/Wasm/data artifacts and Chrome 151 rendered
Green Ruins for a six-second automated check with no runtime exception. The
Emscripten `Game` must remain heap/static allocated; a stack-local instance is
invalidated when CNA installs its browser main loop.

## Implementation landmarks

- `gameplay/include/CopperBoots/Campaign.hpp`: pure ordered campaign data.
- `Content/Levels/factory.cbl`: original 96x12 Factory stage.
- `game/src/CopperBootsGame.cpp`: CNA presentation, stage loading/progression,
  Factory palette and parallax.
- `gameplay/src/LevelDefinition.cpp`: strict external level parser including
  optional theme metadata.
- `test/GameplayTests.cpp`: campaign and shipping Factory structure checks.
- `plan.md`: authoritative stable task/status ledger.
- `analysis.md`: provenance, historical findings and technical evidence.

## Boundaries and risks

- Reference archives/executables stay ignored and must not be committed. The
  historical archive's restrictive text does not grant redistribution rights.
- No Nintendo or historical game assets/data are shipped; current visuals and
  sound are generated and both levels are original.
- Game code must continue using CNA public APIs only. A web backend may use
  platform APIs internally through CNA, never directly from game code.
- Factory has automated structural and runtime-smoke coverage; retain a manual
  playability check when changing platform spacing or player physics.
- The web result verifies startup/rendering and asset preload, not a complete
  interactive campaign playthrough or automated keyboard/gamepad input.
- Do not modify CNA/sharp-runtime without a concrete minimal reproduction and a
  `MAR-CNA-*`/`MAR-SR-*` entry.
