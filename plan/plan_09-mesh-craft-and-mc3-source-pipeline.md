# 09. Mesh Craft and MC3 source pipeline

[Back to master plan](../plan.md)

Make MC3 a validated, reproducible, hand-authored source format for constructional content. Content is authored by a person in Mesh Craft/MC3 and Blender; AI may assist an author (suggesting geometry, drafting a first pass) but does not run as an autonomous content generator. Procedural building/road/interior/prop generation is explicitly out of scope for v1 — see the last section.

## Schema and format foundations

- [ ] **IS-09-001 P0** — Keep the supplied MC3 schema version in the source manifest.
- [ ] **IS-09-002 P0** — Add MC3 schema validation to the asset build script.
- [ ] **IS-09-003 P0** — Make invalid MC3 fail the build with file and line diagnostics.
- [ ] **IS-09-004 P0** — Author one production-path building in MC3 and load its converted output.
- [ ] **IS-09-005 P0** — Author one production-path street prop set in MC3.
- [ ] **IS-09-006 P1** — Define Iron Shadows naming, units, axes, pivots, tags, and layer conventions for MC3.
- [ ] **IS-09-007 P1** — Define allowed MC3 collision values and required metadata.
- [ ] **IS-09-008 P1** — Define definition/instance conventions for repeated objects (lamps, benches, signs) authored once and placed many times.
- [ ] **IS-09-009 P1** — Define material and texture-reference conventions.
- [ ] **IS-09-010 P1** — Define building facade, floor, room, door, window, and portal conventions.
- [ ] **IS-09-011 P1** — Define road, sidewalk, curb, lane, crossing, and sign conventions.
- [ ] **IS-09-012 P1** — Define logical area and trigger conventions for missions.
- [ ] **IS-09-013 P1** — Define animation clip naming for doors, gates, lifts, fans, and machines.

## Authoring-support tooling

Each tool below gets one lean production lifecycle instead of a repeated 12-13-step template: define its scope/API/config, implement the smallest working version with real error reporting, test it, and document it.

- [ ] **IS-09-014 P1** — MC3 schema/lint validator: define scope (schema + house-style lint rules) and non-goals, and its CLI/API shape.
- [ ] **IS-09-015 P1** — MC3 schema/lint validator: implement the reference path with actionable failure diagnostics (file, line, rule).
- [ ] **IS-09-016 P1** — MC3 schema/lint validator: add unit tests and one integration test in the asset build script.
- [ ] **IS-09-017 P2** — MC3 schema/lint validator: document usage, invariants, and common failure modes.
- [ ] **IS-09-018 P1** — MC3 canonical formatter: define scope/API for deterministic MC3 formatting and canonicalization.
- [ ] **IS-09-019 P1** — MC3 canonical formatter: implement the reference path and wire it into pre-commit/build tooling.
- [ ] **IS-09-020 P2** — MC3 canonical formatter: add tests and document usage.
- [ ] **IS-09-021 P1** — MC3 prefab/include library: define scope for a definition/instance and include strategy without uncontrolled duplication.
- [ ] **IS-09-022 P1** — MC3 prefab/include library: implement the reference path (shared lamp/bench/window/sign prefabs) and validate reuse across districts.
- [ ] **IS-09-023 P2** — MC3 prefab/include library: add tests and document usage.
- [ ] **IS-09-024 P2** — MC3 authoring assist: define a prompt/checklist contract for using AI to help draft valid MC3, reviewed and committed by a human author.
- [ ] **IS-09-025 P2** — MC3 authoring assist: validate AI-assisted drafts through the same schema/lint/format tools as hand-authored content, with no separate acceptance path.
- [ ] **IS-09-026 P2** — MC3 schema migration tool: define scope for migrating existing scenes when the MC3 schema version changes.
- [ ] **IS-09-027 P2** — MC3 schema migration tool: implement the reference path, add a regression test using the current prototype scene, and document usage.

## MC3 -> glTF/GLB -> CNJ conversion pipeline

- [ ] **IS-09-028 P0** — Keep `mc3togltf` and the CNA glTF-to-CNJ tool wired into `scripts/build-assets.sh` as the single supported conversion path.
- [ ] **IS-09-029 P1** — Cache conversion outputs keyed by source content hash so unchanged MC3 files are not reconverted.
- [ ] **IS-09-030 P1** — Fail the asset build with a clear message when a conversion step drops MC3-only semantics (collision, areas, triggers) that the runtime still needs.
- [ ] **IS-09-031 P1** — Add a golden-output regression test for the conversion path using the prototype city block scene.
- [ ] **IS-09-032 P2** — Add golden-output regression tests for one building, one road segment, and one interior once authored.
- [ ] **IS-09-033 P2** — Document the conversion path's known semantic gaps and how gameplay metadata is preserved alongside render geometry.

## Content validation gates

Every authored asset must pass these gates before it is considered production-ready, regardless of who or what drafted it.

- [ ] **IS-09-034 P1** — Scale, unit, axis, and pivot validation against the conventions in this file.
- [ ] **IS-09-035 P1** — Triangle, vertex, material, and texture budget checks per asset category.
- [ ] **IS-09-036 P1** — Non-manifold and degenerate geometry checks.
- [ ] **IS-09-037 P1** — UV and texture-reference checks.
- [ ] **IS-09-038 P1** — Collision-proxy presence and sanity checks (matches the metadata conventions above).
- [ ] **IS-09-039 P2** — Naming and tag convention checks (catches drift from the conventions above over a long campaign).
- [ ] **IS-09-040 P1** — Runtime load test: every validated asset must load and render in a headless smoke run before it is merged.
- [ ] **IS-09-041 P2** — License/provenance record check, cross-referenced with the asset registry (group 11) for any non-original source content.
- [ ] **IS-09-042 P2** — Content hash recorded per asset so re-validation can detect silent edits.
- [ ] **IS-09-043 P2** — Roll validation results into a single build-blocking report rather than scattered warnings.

## District and campaign content planning

The campaign targets several hand-built districts plus countryside content across roughly 15-20 missions, built one district at a time.

- [ ] **IS-09-044 P0** — Maintain a per-district MC3 source manifest listing every building/street/prop/interior file and its status (drafted/validated/converted/in-game).
- [ ] **IS-09-045 P1** — Define authoring conventions specific to countryside content (terrain tiles, fences, rural roads, farm props) alongside the urban conventions above.
- [ ] **IS-09-046 P1** — Define an authoring checklist for mission-critical interiors (only the locations a mission's briefing/objective/dialogue actually needs).
- [ ] **IS-09-047 P1** — Track authored building/prop/interior counts per district against a rough content budget so no single district silently balloons.
- [ ] **IS-09-048 P2** — Require a short human content review (art direction plus a validation-gate pass) before any district's content is marked done.
- [ ] **IS-09-049 P2** — Keep a versioned content changelog per district so a regression in a later pass is traceable to a specific change.

## Explicitly out of scope for v1

- [ ] **IS-09-050 P3** — Research autonomous procedural building/road/interior/prop generation only as a someday exploration; it is not a v1 dependency, and no generator tooling should be built until manual authoring output for the full campaign proves it is actually needed.
