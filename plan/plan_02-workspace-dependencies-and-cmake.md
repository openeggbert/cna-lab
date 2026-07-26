# 02. Workspace, dependencies, and CMake

[Back to master plan](../plan.md)

Make recursive dependency checkout, configuration, build, test, install, and packaging reproducible.

- [x] **IS-02-001 P0** — Add a dependency preflight script that detects missing CNA/sharp-runtime/EasyGL/cna-extended sibling repositories and empty CNA submodules.
- [x] **IS-02-002 P0** — Add cna-extended as a sibling dependency, wired through `IRON_SHADOWS_CNA_EXTENDED_DIR`, auto-linking the parent-provided `CNA` target.
- [ ] **IS-02-003 P0** — Make the preflight script print exact recursive checkout commands.
- [ ] **IS-02-004 P0** — Document minimum CMake, C++ compiler, Ninja, SDL, Vulkan, EasyGL, and cna-extended requirements.
- [ ] **IS-02-005 P0** — Pin tested CNA, sharp-runtime, cna-extended, Mesh Craft, and EasyGL revisions in a dependency manifest.
- [x] **IS-02-006 P0** — Use documented sibling checkouts (not submodules or a superproject) for CNA, sharp-runtime, EasyGL, and cna-extended.
- [x] **IS-02-007 P0** — Ensure no build script creates build trees under /tmp, /var/tmp, or /dev/shm.
- [x] **IS-02-008 P0** — Keep ccache enabled and expose a diagnostic when it is unavailable.
- [x] **IS-02-009 P0** — Ensure all helper scripts and CMake presets use at most four build jobs by default.
- [ ] **IS-02-010 P1** — Add a `dev-headless` configure/build/test preset.
- [ ] **IS-02-011 P1** — Add an ASan/UBSan preset for supported Linux backends.
- [ ] **IS-02-012 P1** — Add a release-with-debug-info preset.
- [x] **IS-02-013 P1** — Export `compile_commands.json` consistently for CLion and tooling.
- [ ] **IS-02-014 P1** — Add an install-tree smoke test that runs the installed binary with `--smoke`.
- [ ] **IS-02-015 P1** — Verify GNU and Clang builds.
- [ ] **IS-02-016 P2** — Verify MSVC and MinGW builds when Windows support begins.
- [x] **IS-02-017 P1** — Record every persistent build directory used in validation reports (see docs/validation.md).
- [x] **IS-02-018 P1** — Add a preflight report that prints the selected CNA backend and every resolved dependency path.
- [ ] **IS-02-019 P1** — Detect incompatible cached backend selections and explain how to reconfigure without deleting the build tree.
- [ ] **IS-02-020 P1** — Verify a from-scratch clone following the README workspace layout configures cleanly end to end.
- [ ] **IS-02-021 P1** — Add a CI job that runs preflight, check-syntax, and a `compile-software` build on every push.
- [ ] **IS-02-022 P1** — Add unit tests for the preflight script's dependency-detection logic.
- [ ] **IS-02-023 P2** — Add a build option for excluding the prototype renderer once production content loads.
- [ ] **IS-02-024 P2** — Add a build option for a dedicated asset-validation executable without the game executable.
- [ ] **IS-02-025 P2** — Add a dependency license collection step to packaging.
- [ ] **IS-02-026 P2** — Verify `cmake --install` produces a runnable install tree with bundled runtime assets.
- [ ] **IS-02-027 P2** — Add reproducible source archive generation for release packaging.
- [ ] **IS-02-028 P2** — Document common preflight failure modes and fixes.
- [ ] **IS-02-029 P3** — Add a warnings-as-errors CI preset without forcing it on local developers.
- [ ] **IS-02-030 P3** — Add link-time optimization only after debuggability and linker compatibility are verified.
- [ ] **IS-02-031 P3** — Evaluate vcpkg/conan only if sibling-source builds become unmanageable.
