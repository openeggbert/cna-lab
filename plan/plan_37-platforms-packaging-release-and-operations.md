# 37. Platforms, packaging, release, and operations

[Back to master plan](../plan.md)

Create repeatable distributable builds for one primary platform and a small,
explicit set of secondary/validation targets. This project ships one
single-player campaign as a normal desktop release, not a live-updated
multi-platform product — patch/DLC infrastructure, storefront SDK matrices,
and console/mobile/web ports are explicit non-goals for v1 (see group 40).

## Release policy and packaging basics

- [ ] **IS-37-001 P0** — Define Linux desktop + EasyGL as the officially supported primary OS/backend combination.
- [ ] **IS-37-002 P0** — Define Windows desktop + EasyGL as the officially supported secondary OS/backend combination.
- [ ] **IS-37-003 P0** — Create one reproducible development package and one release package per supported OS.
- [ ] **IS-37-004 P0** — Include license and third-party notices in every package.
- [ ] **IS-37-005 P1** — Create platform-specific settings/save/cache directories for Linux and Windows.
- [ ] **IS-37-006 P1** — Create application icon, metadata, and version resources after branding is cleared.
- [ ] **IS-37-007 P1** — Create clean install/uninstall tests for both supported platforms.
- [ ] **IS-37-008 P1** — Create runtime dependency bundling and verification (see runtime dependency checker below).
- [ ] **IS-37-009 P1** — Create crash-safe first-run configuration.
- [ ] **IS-37-010 P1** — Create graphics fallback and safe-mode startup (see safe-mode launcher below).
- [ ] **IS-37-011 P1** — Create command-line options for backend, resolution, window mode, logs, and smoke tests.
- [ ] **IS-37-012 P1** — Create a release-candidate checklist covering build, tests, license audit, and known-issues review.
- [ ] **IS-37-013 P1** — Create signed checksums for downloadable archives.
- [ ] **IS-37-014 P1** — Define a simple version-numbering scheme (semantic version + build identifier).
- [ ] **IS-37-015 P1** — Create a changelog policy updated once per release, not per commit.
- [ ] **IS-37-016 P1** — Create save-compatibility notes in release notes for every release that changes save data.
- [ ] **IS-37-017 P1** — Create a rollback strategy: keep the previous release archive available if a new one is broken.
- [ ] **IS-37-018 P1** — Create a bug-report template capturing logs and non-sensitive diagnostics.
- [ ] **IS-37-019 P1** — Create a known-issues document maintained per release, organized by platform/backend.
- [ ] **IS-37-020 P1** — Define a hotfix process (new point release, not a live patch/update system) for release-blocking bugs found after ship.
- [ ] **IS-37-021 P2** — Write a simple store/distribution page description and screenshots policy (e.g. itch.io or direct download), deferring any storefront SDK integration.
- [ ] **IS-37-022 P2** — Define a privacy-by-default policy: no telemetry collection for v1; document this explicitly in release notes.
- [ ] **IS-37-023 P2** — Create a credits screen sourced from the asset-registry attribution requirements.

## Linux EasyGL (primary shipping target)

- [ ] **IS-37-024 P0** — Define the support tier (fully supported) and any explicit non-goals for Linux EasyGL.
- [ ] **IS-37-025 P0** — Confirm the `dev-easygl`/`release-easygl` configure/build presets stay green in CI.
- [ ] **IS-37-026 P0** — Verify startup, window creation, and input on Linux EasyGL.
- [ ] **IS-37-027 P0** — Verify rendering and resource lifetime across a full district load/unload cycle on Linux EasyGL.
- [ ] **IS-37-028 P1** — Verify audio and media playback on Linux EasyGL.
- [ ] **IS-37-029 P1** — Verify save/load paths and packaging on Linux EasyGL.
- [ ] **IS-37-030 P1** — Run smoke and representative mission-scenario tests on Linux EasyGL before every release.
- [ ] **IS-37-031 P1** — Record known limitations and a performance baseline (frame time, RAM/VRAM) against the docs/performance-targets.md budget for Linux EasyGL.

## Linux Vulkan (secondary validation backend)

- [ ] **IS-37-032 P1** — Define the support tier (validation only, not a primary ship target) for Linux Vulkan.
- [ ] **IS-37-033 P1** — Confirm the `dev-vulkan` configure/build preset stays green in CI.
- [ ] **IS-37-034 P1** — Verify startup and input on Linux Vulkan.
- [ ] **IS-37-035 P2** — Verify rendering parity against EasyGL for one full district on Linux Vulkan.
- [ ] **IS-37-036 P2** — Verify audio and media on Linux Vulkan.
- [ ] **IS-37-037 P2** — Verify save paths and packaging on Linux Vulkan.
- [ ] **IS-37-038 P2** — Run smoke tests on Linux Vulkan before every release.
- [ ] **IS-37-039 P2** — Record known rendering-parity differences between Vulkan and EasyGL.

## Windows EasyGL (secondary release target)

- [ ] **IS-37-040 P1** — Define the support tier (secondary release target) and explicit non-goals for Windows EasyGL.
- [ ] **IS-37-041 P1** — Create and validate a Windows configure/build preset mirroring `release-easygl`.
- [ ] **IS-37-042 P1** — Verify startup and input on Windows EasyGL.
- [ ] **IS-37-043 P1** — Verify rendering and resource lifetime for one full district on Windows EasyGL.
- [ ] **IS-37-044 P2** — Verify audio and media on Windows EasyGL.
- [ ] **IS-37-045 P2** — Verify save paths and packaging (including SDL runtime bundling via `cna_copy_sdl_runtime`) on Windows EasyGL.
- [ ] **IS-37-046 P2** — Run smoke and representative mission-scenario tests on Windows EasyGL before every release.
- [ ] **IS-37-047 P2** — Record known limitations and a performance baseline for Windows EasyGL.

## CI-only compile-check backend

- [ ] **IS-37-048 P1** — Keep the `compile-software` preset green in CI as a fast, headless compile/link/test check; it is not a release target.
- [ ] **IS-37-049 P1** — Run `iron_shadows_core_tests` under the software backend on every CI run.
- [ ] **IS-37-050 P2** — Track compile-software build time as a CI health signal.

## Safe-mode launcher

- [ ] **IS-37-051 P1** — Define scope: detect a failed graphics-backend init and relaunch with the software/headless backend or a minimal-settings profile.
- [ ] **IS-37-052 P1** — Implement the smallest deterministic fallback path (crash/init-failure on preferred backend -> retry once with safe defaults).
- [ ] **IS-37-053 P1** — Add a focused unit test for the fallback decision logic.
- [ ] **IS-37-054 P2** — Add an integration test that forces a backend-init failure and confirms safe-mode recovery.
- [ ] **IS-37-055 P2** — Log the safe-mode trigger reason for bug reports.
- [ ] **IS-37-056 P2** — Document safe-mode behavior for players in the known-issues doc.

## Runtime dependency checker

- [ ] **IS-37-057 P1** — Define scope: verify required shared libraries/runtime assets are present before the game window opens.
- [ ] **IS-37-058 P1** — Implement the smallest deterministic check-and-report path (missing dependency -> actionable error message, not a crash).
- [ ] **IS-37-059 P1** — Add a focused unit test for the dependency-check logic.
- [ ] **IS-37-060 P2** — Add an integration test with a deliberately missing dependency.
- [ ] **IS-37-061 P2** — Reuse this checker inside `scripts/preflight.sh` so developers and players get the same diagnostics.
- [ ] **IS-37-062 P2** — Document common missing-dependency failure modes per platform.

## Release archive builder

- [ ] **IS-37-063 P1** — Define scope: package the built executable, assets, license notices, and readme into one distributable archive per platform.
- [ ] **IS-37-064 P1** — Implement the smallest deterministic build-to-archive script reusing existing `scripts/build.sh`/`install(...)` targets.
- [ ] **IS-37-065 P1** — Add a test that a produced archive extracts and runs a smoke test cleanly on a clean directory.
- [ ] **IS-37-066 P2** — Add checksum generation to the archive-builder script.
- [ ] **IS-37-067 P2** — Add version-stamped archive naming matching the versioning scheme.
- [ ] **IS-37-068 P2** — Document the archive-builder script's usage and failure modes.

## Explicit non-goals for v1

- [ ] **IS-37-069 P2** — Record explicit non-goal: no patch/DLC/live-update manifest system for v1; releases are whole new archives.
- [ ] **IS-37-070 P2** — Record explicit non-goal: no multi-storefront SDK integration for v1; a direct-download/itch.io-style release is enough.
- [ ] **IS-37-071 P2** — Record explicit non-goal: no macOS, Steam Deck-specific, D3D11, WebGPU-web, or historical/diagnostic-backend release builds for v1; treat any of these as post-slice research (see group 40) if ever revisited.
- [ ] **IS-37-072 P3** — Create an optional symbol package for crash diagnosis, only after the primary platforms are stable and a real crash-triage need appears.

## Operations after release

- [ ] **IS-37-073 P1** — Define a bug-triage cadence (e.g. weekly) for incoming reports against the known-issues doc.
- [ ] **IS-37-074 P1** — Define what counts as a release-blocking bug vs. a known-issue-list item.
- [ ] **IS-37-075 P2** — Keep a simple release history (version, date, notable changes, save-compatibility notes).
- [ ] **IS-37-076 P2** — Define an end-of-support policy for old builds once save-format migrations are no longer maintained.
