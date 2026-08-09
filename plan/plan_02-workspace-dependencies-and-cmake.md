# 02. Workspace, dependencies, and CMake

[Back to master plan](../plan.md)

Make recursive dependency checkout, configuration, build, test, install, and packaging reproducible.

- [x] **IG-02-001 P0** — Add a dependency preflight script that detects missing CNA/sharp-runtime/EasyGL/cna-extended sibling repositories and empty CNA submodules.
- [x] **IG-02-002 P0** — Add cna-extended as a sibling dependency, wired through `IRON_GANG_CNA_EXTENDED_DIR`, auto-linking the parent-provided `CNA` target.
- [ ] **IG-02-003 P0** — Make the preflight script print exact recursive checkout commands.
- [ ] **IG-02-004 P0** — Document minimum CMake, C++ compiler, Ninja, SDL, Vulkan, EasyGL, and cna-extended requirements.
- [ ] **IG-02-005 P0** — Pin tested CNA, sharp-runtime, cna-extended, Mesh Craft, and EasyGL revisions in a dependency manifest.
- [x] **IG-02-006 P0** — Use documented sibling checkouts (not submodules or a superproject) for CNA, sharp-runtime, EasyGL, and cna-extended.
- [x] **IG-02-007 P0** — Ensure no build script creates build trees under /tmp, /var/tmp, or /dev/shm.
- [x] **IG-02-008 P0** — Keep ccache enabled and expose a diagnostic when it is unavailable.
- [x] **IG-02-009 P0** — Ensure all helper scripts and CMake presets use at most four build jobs by default.
- [ ] **IG-02-010 P1** — Add a `dev-headless` configure/build/test preset.
- [ ] **IG-02-011 P1** — Add an ASan/UBSan preset for supported Linux backends.
- [ ] **IG-02-012 P1** — Add a release-with-debug-info preset.
- [x] **IG-02-013 P1** — Export `compile_commands.json` consistently for CLion and tooling.
- [ ] **IG-02-014 P1** — Add an install-tree smoke test that runs the installed binary with `--smoke`.
- [ ] **IG-02-015 P1** — Verify GNU and Clang builds.
- [ ] **IG-02-016 P2** — Verify MSVC and MinGW builds when Windows support begins.
- [x] **IG-02-017 P1** — Record every persistent build directory used in validation reports (see docs/validation.md).
- [x] **IG-02-018 P1** — Add a preflight report that prints the selected CNA backend and every resolved dependency path.
- [ ] **IG-02-019 P1** — Detect incompatible cached backend selections and explain how to reconfigure without deleting the build tree.
- [ ] **IG-02-020 P1** — Verify a from-scratch clone following the README workspace layout configures cleanly end to end.
- [ ] **IG-02-021 P1** — Add a CI job that runs preflight, check-syntax, and a `compile-software` build on every push.
- [ ] **IG-02-022 P1** — Add unit tests for the preflight script's dependency-detection logic.
- [ ] **IG-02-023 P2** — Add a build option for excluding the prototype renderer once production content loads.
- [ ] **IG-02-024 P2** — Add a build option for a dedicated asset-validation executable without the game executable.
- [ ] **IG-02-025 P2** — Add a dependency license collection step to packaging.
- [ ] **IG-02-026 P2** — Verify `cmake --install` produces a runnable install tree with bundled runtime assets.
- [ ] **IG-02-027 P2** — Add reproducible source archive generation for release packaging.
- [ ] **IG-02-028 P2** — Document common preflight failure modes and fixes.
- [ ] **IG-02-029 P3** — Add a warnings-as-errors CI preset without forcing it on local developers.
- [ ] **IG-02-030 P3** — Add link-time optimization only after debuggability and linker compatibility are verified.
- [ ] **IG-02-031 P3** — Evaluate vcpkg/conan only if sibling-source builds become unmanageable.
