# 39. Vertical-slice gates

[Back to master plan](../plan.md)

Define non-negotiable integrated milestones that prevent endless disconnected
engine work. Every gate below assumes the locked project scope: a Mafia-1
(2002)-style campaign built district-by-district and connected by loading
screens (not seamless streaming), Mafia-1-era traffic/police/pedestrian
fidelity (not GTA-scale simulation), `cna-extended` for ECS/scene/collision,
CNA's existing PbrEffect/shadow-mapping/instancing for rendering, no in-house
editor suite, manual MC3 content authoring, and the ~2-4GB RAM / 512MB-1GB
VRAM baked-lighting performance target from `docs/performance-targets.md`.
The full v1 target is multiple districts plus countryside across roughly
15-20 missions — M10 through M12 apply to the FIRST district, then repeat
per additional district (see the expansion gate at the end of this file)
rather than being a one-time final check.

- [ ] **IS-39-001 P0** — Gate M0: repository configures only after dependency preflight reports CNA, sharp-runtime, cna-extended, and (for EasyGL) easy-gl all present.
- [ ] **IS-39-002 P0** — Gate M1: the current procedural prototype builds, launches, completes its mission, saves, loads, and passes CTest.
- [ ] **IS-39-003 P0** — Gate M2: one MC3 building converts to GLB/CNJ, loads at runtime, collides correctly, and replaces debug geometry.
- [x] **IS-39-004 P0** — Gate M3: one CNJ vehicle renders with separate wheels and preserves the current drivable mission.
- [x] **IS-39-005 P0** — Gate M4: the selected physics library supports character, trigger, raycast, and vehicle prototypes behind Iron Shadows abstractions.
- [ ] **IS-39-006 P0** — Gate M5: two districts each load and unload cleanly through a loading-screen transition, with no leaks, crashes, or lost mission/save state.
- [ ] **IS-39-007 P0** — Gate M6: one skinned character plays blended locomotion, dialogue pose, and vehicle entry/exit animations using cna-extended's Transform3/skinned-playback.
- [ ] **IS-39-008 P0** — Gate M7: one data-driven mission controls dialogue, objective UI, vehicle entry, destination trigger, checkpoint, and completion.
- [ ] **IS-39-009 P0** — Gate M8: one in-engine cutscene can play, skip, save-safe finalize, and hand control back correctly.
- [ ] **IS-39-010 P0** — Gate M9: Mafia-1-fidelity traffic, pedestrians, and one police-response scenario survive ten minutes of walking/driving interaction in the first district.
- [ ] **IS-39-011 P0** — Gate M10: the first district's vertical slice uses production-path assets, collision, baked lighting + one dynamic sun + limited shadows, audio, and UI.
- [ ] **IS-39-012 P0** — Gate M11: the first district's complete vertical-slice mission passes happy-path, failure, retry, save/load, and cutscene-skip automation.
- [ ] **IS-39-013 P0** — Gate M12: frame-time, memory (~2-4GB RAM), VRAM (~512MB-1GB), and district-load-time budgets from docs/performance-targets.md pass on the primary target hardware/backend (Linux EasyGL).
- [ ] **IS-39-014 P0** — Gate M13: all shipping assets have approved provenance and generated third-party notices.
- [ ] **IS-39-015 P0** — Gate M14: an external clean workspace can build, install, launch, and complete the first-district demo using documented steps.
- [ ] **IS-39-016 P0** — Gate M15: each additional district repeats gates M10-M14 before its missions are considered shippable (see expansion gate below).
- [ ] **IS-39-017 P0** — M1 procedural executable: configure recursive dependencies.
- [ ] **IS-39-018 P0** — M1 procedural executable: build game and tests.
- [ ] **IS-39-019 P0** — M1 procedural executable: launch a visible window.
- [ ] **IS-39-020 P0** — M1 procedural executable: verify on-foot controls.
- [ ] **IS-39-021 P0** — M1 procedural executable: verify vehicle controls.
- [ ] **IS-39-022 P0** — M1 procedural executable: complete dialogue and mission.
- [ ] **IS-39-023 P0** — M1 procedural executable: save and load.
- [ ] **IS-39-024 P0** — M1 procedural executable: reset and quit.
- [ ] **IS-39-025 P0** — M1 procedural executable: capture logs and gameplay reference.
- [x] **IS-39-026 P0** — M2 first production asset: validate MC3.
- [x] **IS-39-027 P0** — M2 first production asset: convert MC3 to GLB.
- [ ] **IS-39-028 P0** — M2 first production asset: add a standalone GLB structural validation step (currently only implicit: `mc3togltf`/`cna_tool_gltf_to_cnj` error out on malformed input, but nothing validates the GLB independently of a successful conversion).
- [x] **IS-39-029 P0** — M2 first production asset: convert GLB to CNJ.
- [x] **IS-39-030 P0** — M2 first production asset: load CNJ.
- [ ] **IS-39-031 P0** — M2 first production asset: assign material using CNA's PbrEffect. Confirmed root cause: CNA's `cna_tool_gltf_to_cnj` only emits `PbrEffect` when a primitive has an actual normal/metallic-roughness *texture* (see `GltfImportCore.cpp`'s `usePbr` derivation) -- a flat-factor-only material like the current warehouse's is deliberately routed to `BasicEffect` instead, which is correct behavior, not a bug. Revisit once the warehouse gets a real normal/metallic-roughness texture (a content task, see plan_31), not a code task.
- [ ] **IS-39-032 P0** — M2 first production asset: derive collision from the MC3 `collision` attribute instead of the pre-existing, independently-authored procedural AABB in `PrototypeWorld` -- needs the sidecar/MCB metadata compiler from plan_10 (`IS-10-001`/`IS-10-002`), not a one-off XML parse, since the single-asset `warehouse.mc3.xml` does not carry the building's city-block world position on its own.
- [x] **IS-39-033 P0** — M2 first production asset: replace procedural mesh.
- [x] **IS-39-034 P0** — M2 first production asset: package provenance.
- [x] **IS-39-035 P0** — M2 first production asset: test missing/corrupt asset fallback (`scripts/test-missing-asset-fallback.sh`, wired into `ctest` as `iron_shadows_missing_asset_fallback`).
- [ ] **IS-39-036 P0** — M5 district load/unload: partition one district's assets for loading-screen transition.
- [ ] **IS-39-037 P0** — M5 district load/unload: load the target district asynchronously behind the loading screen.
- [ ] **IS-39-038 P0** — M5 district load/unload: upload GPU resources safely once loading completes.
- [ ] **IS-39-039 P0** — M5 district load/unload: unload the previous district's geometry, collision, and entities deterministically.
- [ ] **IS-39-040 P0** — M5 district load/unload: restore persistent world/save/mission state across the transition.
- [ ] **IS-39-041 P0** — M5 district load/unload: connect collision/navigation for the newly loaded district.
- [ ] **IS-39-042 P0** — M5 district load/unload: cancel/ignore stale loading work if the player triggers a second transition quickly.
- [ ] **IS-39-043 P0** — M5 district load/unload: soak-test repeated back-and-forth transitions between two districts.
- [ ] **IS-39-044 P0** — M9 living-district proof: spawn Mafia-1-fidelity traffic following the district's lane graph and signals.
- [ ] **IS-39-045 P0** — M9 living-district proof: spawn 10-20 pedestrians with sidewalk navigation and basic flee reactions.
- [ ] **IS-39-046 P0** — M9 living-district proof: verify a witnessed traffic offense triggers a police response.
- [ ] **IS-39-047 P0** — M9 living-district proof: verify a witnessed crime triggers a police chase with one escalation level.
- [ ] **IS-39-048 P0** — M9 living-district proof: verify losing police line-of-sight resolves the chase.
- [ ] **IS-39-049 P0** — M9 living-district proof: ten-minute soak of traffic + pedestrians + one police scenario without leaks or stalls.
- [ ] **IS-39-050 P0** — M7 data-driven mission: parse graph.
- [ ] **IS-39-051 P0** — M7 data-driven mission: validate references.
- [ ] **IS-39-052 P0** — M7 data-driven mission: start dialogue.
- [ ] **IS-39-053 P0** — M7 data-driven mission: advance objective.
- [ ] **IS-39-054 P0** — M7 data-driven mission: track vehicle interaction.
- [ ] **IS-39-055 P0** — M7 data-driven mission: track destination trigger.
- [ ] **IS-39-056 P0** — M7 data-driven mission: checkpoint.
- [ ] **IS-39-057 P0** — M7 data-driven mission: fail/retry.
- [ ] **IS-39-058 P0** — M7 data-driven mission: save/load.
- [ ] **IS-39-059 P0** — M7 data-driven mission: trace/debug via the one small mission debug view tool.
- [ ] **IS-39-060 P0** — M11 vertical slice: fresh-start playthrough.
- [ ] **IS-39-061 P0** — M11 vertical slice: save/load playthrough.
- [ ] **IS-39-062 P0** — M11 vertical slice: cutscene-skip playthrough.
- [ ] **IS-39-063 P0** — M11 vertical slice: mission-failure retry.
- [ ] **IS-39-064 P0** — M11 vertical slice: vehicle-loss recovery.
- [ ] **IS-39-065 P0** — M11 vertical slice: missing optional asset behavior.
- [ ] **IS-39-066 P0** — M11 vertical slice: district-transition mid-mission (leave and return without breaking mission state).
- [ ] **IS-39-067 P0** — M11 vertical slice: ten-minute soak.
- [ ] **IS-39-068 P0** — M11 vertical slice: performance capture against docs/performance-targets.md.
- [ ] **IS-39-069 P0** — M11 vertical slice: license audit.
- [ ] **IS-39-070 P0** — M11 vertical slice: clean package install.

## District-by-district expansion gate (M15, repeats per district)

Applies once per additional district after the first, on the way to the full
~15-20 mission, multi-district-plus-countryside v1 campaign. Do not start
authoring the next district until the current one clears this list.

- [ ] **IS-39-071 P0** — Confirm the new district's missions pass the same M7/M11 mission-flow checks as the first district.
- [ ] **IS-39-072 P0** — Confirm the new district's traffic/pedestrian/police behavior matches the same M9 Mafia-1-fidelity checks.
- [ ] **IS-39-073 P0** — Confirm the new district's load/unload transition (to and from already-shipped districts) passes the same M5 checks.
- [ ] **IS-39-074 P0** — Confirm the new district's rendering/performance stays within the M10/M12 baked-lighting and RAM/VRAM budget.
- [ ] **IS-39-075 P0** — Confirm all new district assets have approved provenance before the district is marked shippable.
- [ ] **IS-39-076 P1** — Update the release known-issues doc and save-compatibility notes for the new district before it ships.
