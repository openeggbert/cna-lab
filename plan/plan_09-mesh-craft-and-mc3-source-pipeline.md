# 09. Mesh Craft and MC3 source pipeline

[Back to master plan](../plan.md)

Make MC3 a validated, reproducible, hand-authored source format for constructional content. Content is authored by a person in Mesh Craft/MC3 and Blender; AI may assist an author (suggesting geometry, drafting a first pass) but does not run as an autonomous content generator. Procedural building/road/interior/prop generation is explicitly out of scope for v1 — see the last section.

## Schema and format foundations

- [ ] **IG-09-001 P0** — Keep the supplied MC3 schema version in the source manifest.
- [x] **IG-09-002 P0** — Add MC3 schema validation to the asset build script. *(`scripts/validate-mc3.sh` XSD-validates against Mesh Craft's own `mc3/mc3.xsd` and is called by `build-assets.sh` before anything is converted. It had existed since the pipeline was written -- what was missing is that it **could not find the schema in this workspace**: the default assumed `../mesh-craft`, and the repository has since been moved a directory deeper, so every asset build here died on "MC3 schema not found". The path is now searched across `MC3_SCHEMA`, `MESH_CRAFT_SOURCE_DIR`, `MESH_CRAFT_BUILD_DIR`, `IRON_GANG_CNA_DIR`'s sibling, and both plausible checkout depths.)*
- [x] **IG-09-003 P0** — Make invalid MC3 fail the build with file and line diagnostics. *(`xmllint --noout --schema` already produces exactly what the entry asks for -- `broken.mc3.xml:12: element sphere: Schemas validity error : ...` -- and `set -euo pipefail` makes it stop the build. **Nothing checked that**, which is what this closes: `iron_gang_validate_mc3_tests` validates every shipped MC3 file, and rejects a deliberately broken one requiring both the file name and the exact offending line number in the diagnostic. Mutation-checked: swallowing xmllint's exit code fails three assertions.)*
- [x] **IG-09-004 P0** — Author one production-path building in MC3 and load its converted output.
- [x] **IG-09-005 P0** — Author one production-path street prop set in MC3. *(`assets/source/mc3/warehouse_block_props.mc3.xml`: three definitions -- street lamp, bench, waste bin -- instanced 22 times, 984 triangles, three materials including an emissive lamp glass. Generated through the real pipeline into `cnjVersion 2` with 82 mesh parts, each a `PbrEffect` carrying the material its MC3 declares. The lamp instances reproduce the district's own `lamp_west`/`lamp_east` box positions exactly (`z = i*11`, i in [-3,3]), which a like-for-like capture confirms pixel for pixel: 1752 px of lamp glass before and after, with 1964 px of bench and bin that were not there. An earlier single-prop `street_lamp.mc3.xml` was removed when this superseded it -- two copies of the same lamp geometry is exactly the drift `IG-09-008` exists to prevent.)*
- [ ] **IG-09-006 P1** — Define Iron Gang naming, units, axes, pivots, tags, and layer conventions for MC3.
- [x] **IG-09-007 P1** — Define allowed MC3 collision values and required metadata. *(`docs/mc3-conventions.md` defines them and `scripts/check_mc3_conventions.py` enforces them, on every `ctest` run and inside `build-assets.sh`. Iron Gang's allowed values are **roles** -- `none`, `static`, `trigger` -- deliberately **not** MC3's documented shape vocabulary (`box`/`sphere`/`capsule`/`mesh`/`convex`): every collider in this game is an axis-aligned box, so shape carries no information while role carries all of it. The XSD cannot tell the difference -- it types `collision` as `xs:string` with a `"none"` default, so any spelling validates, and Iron Gang had been writing `static`/`trigger` across six files with nothing checking. Required metadata: every geometry object must **state** its collision rather than inherit the default (silence and a deliberate `none` are otherwise identical), including inside `<definition>`s and `<group>`s, and every document must declare `model`, `unit="meter"` and `coordinate_system="right_handed_y_up"`. The values survive export as `node.extras.collision`, so a future importer reads the role directly. Recorded cost: Mesh Craft's Walk Mode reads the shape vocabulary and will not understand `static`.)*
- [x] **IG-09-008 P1** — Define definition/instance conventions for repeated objects (lamps, benches, signs) authored once and placed many times. *(MC3's own `<definitions>`/`<instance>` is the convention -- no Iron Gang invention needed. A prop is authored once as a `<definition id="...">` (whose single child is a `<group>` for a multi-part prop, which the XSD requires) and placed with `<instance definition="..." position="..." rotation="..."/>`. **Placement is therefore authored content, not renderer code**: `PrototypeRenderer` draws the whole set with one `DrawModel` at the origin and neither knows nor needs to know where the benches are. It also pays: 22 instances flatten to 82 mesh parts but only ~108 triangles of *unique* geometry, because `mc3togltf` shares one glTF mesh per definition (identical bench legs dedupe further). `content_budget.py` had to learn the convention -- it refused `<instance>` outright rather than guess -- and now expands definitions, refusing a definition that instances itself.)*
- [ ] **IG-09-009 P1** — Define material and texture-reference conventions.
- [ ] **IG-09-010 P1** — Define building facade, floor, room, door, window, and portal conventions.
- [ ] **IG-09-011 P1** — Define road, sidewalk, curb, lane, crossing, and sign conventions.
- [ ] **IG-09-012 P1** — Define logical area and trigger conventions for missions.
- [ ] **IG-09-013 P1** — Define animation clip naming for doors, gates, lifts, fans, and machines.

## Authoring-support tooling

Each tool below gets one lean production lifecycle instead of a repeated 12-13-step template: define its scope/API/config, implement the smallest working version with real error reporting, test it, and document it.

- [ ] **IG-09-014 P1** — MC3 schema/lint validator: define scope (schema + house-style lint rules) and non-goals, and its CLI/API shape.
- [ ] **IG-09-015 P1** — MC3 schema/lint validator: implement the reference path with actionable failure diagnostics (file, line, rule).
- [ ] **IG-09-016 P1** — MC3 schema/lint validator: add unit tests and one integration test in the asset build script.
- [ ] **IG-09-017 P2** — MC3 schema/lint validator: document usage, invariants, and common failure modes.
- [ ] **IG-09-018 P1** — MC3 canonical formatter: define scope/API for deterministic MC3 formatting and canonicalization.
- [ ] **IG-09-019 P1** — MC3 canonical formatter: implement the reference path and wire it into pre-commit/build tooling.
- [ ] **IG-09-020 P2** — MC3 canonical formatter: add tests and document usage.
- [ ] **IG-09-021 P1** — MC3 prefab/include library: define scope for a definition/instance and include strategy without uncontrolled duplication.
- [ ] **IG-09-022 P1** — MC3 prefab/include library: implement the reference path (shared lamp/bench/window/sign prefabs) and validate reuse across districts.
- [ ] **IG-09-023 P2** — MC3 prefab/include library: add tests and document usage.
- [ ] **IG-09-024 P2** — MC3 authoring assist: define a prompt/checklist contract for using AI to help draft valid MC3, reviewed and committed by a human author.
- [ ] **IG-09-025 P2** — MC3 authoring assist: validate AI-assisted drafts through the same schema/lint/format tools as hand-authored content, with no separate acceptance path.
- [ ] **IG-09-026 P2** — MC3 schema migration tool: define scope for migrating existing scenes when the MC3 schema version changes.
- [ ] **IG-09-027 P2** — MC3 schema migration tool: implement the reference path, add a regression test using the current prototype scene, and document usage.

## MC3 -> glTF/GLB -> CNJ conversion pipeline

- [ ] **IG-09-028 P0** — Keep `mc3togltf` and the CNA glTF-to-CNJ tool wired into `scripts/build-assets.sh` as the single supported conversion path.
- [ ] **IG-09-029 P1** — Cache conversion outputs keyed by source content hash so unchanged MC3 files are not reconverted.
- [ ] **IG-09-030 P1** — Fail the asset build with a clear message when a conversion step drops MC3-only semantics (collision, areas, triggers) that the runtime still needs.
- [ ] **IG-09-031 P1** — Add a golden-output regression test for the conversion path using the prototype city block scene.
- [ ] **IG-09-032 P2** — Add golden-output regression tests for one building, one road segment, and one interior once authored.
- [ ] **IG-09-033 P2** — Document the conversion path's known semantic gaps and how gameplay metadata is preserved alongside render geometry.

## Content validation gates

Every authored asset must pass these gates before it is considered production-ready, regardless of who or what drafted it.

- [ ] **IG-09-034 P1** — Scale, unit, axis, and pivot validation against the conventions in this file.
- [ ] **IG-09-035 P1** — Triangle, vertex, material, and texture budget checks per asset category.
- [ ] **IG-09-036 P1** — Non-manifold and degenerate geometry checks.
- [ ] **IG-09-037 P1** — UV and texture-reference checks.
- [ ] **IG-09-038 P1** — Collision-proxy presence and sanity checks (matches the metadata conventions above).
- [ ] **IG-09-039 P2** — Naming and tag convention checks (catches drift from the conventions above over a long campaign).
- [ ] **IG-09-040 P1** — Runtime load test: every validated asset must load and render in a headless smoke run before it is merged.
- [ ] **IG-09-041 P2** — License/provenance record check, cross-referenced with the asset registry (group 11) for any non-original source content.
- [ ] **IG-09-042 P2** — Content hash recorded per asset so re-validation can detect silent edits.
- [ ] **IG-09-043 P2** — Roll validation results into a single build-blocking report rather than scattered warnings.

## District and campaign content planning

The campaign targets several hand-built districts plus countryside content across roughly 15-20 missions, built one district at a time.

- [ ] **IG-09-044 P0** — Maintain a per-district MC3 source manifest listing every building/street/prop/interior file and its status (drafted/validated/converted/in-game).
- [ ] **IG-09-045 P1** — Define authoring conventions specific to countryside content (terrain tiles, fences, rural roads, farm props) alongside the urban conventions above.
- [ ] **IG-09-046 P1** — Define an authoring checklist for mission-critical interiors (only the locations a mission's briefing/objective/dialogue actually needs).
- [ ] **IG-09-047 P1** — Track authored building/prop/interior counts per district against a rough content budget so no single district silently balloons.
- [ ] **IG-09-048 P2** — Require a short human content review (art direction plus a validation-gate pass) before any district's content is marked done.
- [ ] **IG-09-049 P2** — Keep a versioned content changelog per district so a regression in a later pass is traceable to a specific change.

## Explicitly out of scope for v1

- [ ] **IG-09-050 P3** — Research autonomous procedural building/road/interior/prop generation only as a someday exploration; it is not a v1 dependency, and no generator tooling should be built until manual authoring output for the full campaign proves it is actually needed.
