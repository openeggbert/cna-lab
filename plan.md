# Iron Shadows Master Plan

This is the long-horizon implementation backlog for the provisional **Iron Shadows** project. It intentionally contains far more work than the first game needs so important dependencies are not forgotten. It is not permission to implement everything immediately.

## How to use this plan

- **P0** — blocks the next integrated milestone or protects legal/build correctness.
- **P1** — required for the intended vertical slice or production foundation.
- **P2** — valuable after the relevant P0/P1 path works and is measured.
- **P3** — research or distant optional work; must not delay a playable game.
- `[x]` means implemented or validated in the initial scaffold.
- `[ ]` means open. A task may still need decomposition into code-review-sized work.
- Prefer completing a vertical-slice gate over starting unrelated subsystems.
- Move genuinely reusable framework work to CNA/CNA EXT only after its general contract is clear.
- Preserve persistent `cmake-build-*` directories, ccache, and at most four build jobs.
- Never create build trees under `/tmp`, `/var/tmp`, or `/dev/shm`.
- Every external asset requires provenance and license approval before it can ship.

## Milestone order

1. **M0 — Reproducible workspace:** dependencies and recursive submodules are present; preflight is actionable.
2. **M1 — Running scaffold:** current procedural prototype builds, runs, saves, loads, and passes tests.
3. **M2 — First MC3/CNJ asset:** one building replaces procedural geometry.
4. **M3 — First production vehicle asset:** one CNJ vehicle preserves the mission loop.
5. **M4 — Production physics decision:** character, trigger, query, and vehicle prototypes are proven.
6. **M5 — Streaming proof:** two sectors work under high-speed travel.
7. **M6 — Character proof:** skinned character, blended locomotion, dialogue pose, and vehicle entry.
8. **M7 — Data-driven mission:** mission, dialogue, UI, checkpoint, and save integration.
9. **M8 — Cinematic proof:** in-engine sequence with safe skip and handoff.
10. **M9 — Living-block proof:** pedestrians and traffic survive sustained interaction.
11. **M10 — Content-complete vertical slice:** production assets, lighting, audio, UI, and interior.
12. **M11 — QA-complete vertical slice:** automated flows, failures, saves, skip, recovery, and soak.
13. **M12 — Performance gate:** target budgets pass.
14. **M13 — Legal/content gate:** all shipping assets are approved and credited.
15. **M14 — External build/release gate:** clean machine/workspace can build, install, and complete the demo.

## Task index

- [00. Repository foundation and completed scaffold](plan/plan_00-repository-foundation-and-completed-scaffold.md) — 33 tasks
- [01. Product identity, scope, and legal separation](plan/plan_01-product-identity-scope-and-legal-separation.md) — 25 tasks
- [02. Workspace, dependencies, and CMake](plan/plan_02-workspace-dependencies-and-cmake.md) — 74 tasks
- [03. Architecture and module boundaries](plan/plan_03-architecture-and-module-boundaries.md) — 80 tasks
- [04. Core runtime services](plan/plan_04-core-runtime-services.md) — 121 tasks
- [05. CNA integration and backend policy](plan/plan_05-cna-integration-and-backend-policy.md) — 81 tasks
- [06. CNA EXT collaboration roadmap](plan/plan_06-cna-ext-collaboration-roadmap.md) — 111 tasks
- [07. Rendering foundation](plan/plan_07-rendering-foundation.md) — 207 tasks
- [08. Materials, textures, lighting, shadows, and post-processing](plan/plan_08-materials-textures-lighting-shadows-and-post-processing.md) — 220 tasks
- [09. Mesh Craft and MC3 source pipeline](plan/plan_09-mesh-craft-and-mc3-source-pipeline.md) — 157 tasks
- [10. glTF, CNJ, MCB, and runtime packages](plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md) — 156 tasks
- [11. Asset registry, provenance, and build cache](plan/plan_11-asset-registry-provenance-and-build-cache.md) — 95 tasks
- [12. Entity, scene, and world-object model](plan/plan_12-entity-scene-and-world-object-model.md) — 165 tasks
- [13. World partitioning and streaming](plan/plan_13-world-partitioning-and-streaming.md) — 181 tasks
- [14. Terrain, roads, sidewalks, buildings, and interiors](plan/plan_14-terrain-roads-sidewalks-buildings-and-interiors.md) — 181 tasks
- [15. Physics integration](plan/plan_15-physics-integration.md) — 181 tasks
- [16. Player controller and cameras](plan/plan_16-player-controller-and-cameras.md) — 166 tasks
- [17. Vehicles and driving](plan/plan_17-vehicles-and-driving.md) — 212 tasks
- [18. Characters, skeletons, and animation](plan/plan_18-characters-skeletons-and-animation.md) — 230 tasks
- [19. Navigation and pathfinding](plan/plan_19-navigation-and-pathfinding.md) — 174 tasks
- [20. Pedestrians and ambient AI](plan/plan_20-pedestrians-and-ambient-ai.md) — 181 tasks
- [21. Traffic simulation](plan/plan_21-traffic-simulation.md) — 181 tasks
- [22. Police, witnesses, crime, and wanted response](plan/plan_22-police-witnesses-crime-and-wanted-response.md) — 178 tasks
- [23. Combat, damage, and interaction](plan/plan_23-combat-damage-and-interaction.md) — 164 tasks
- [24. Mission framework and scripting](plan/plan_24-mission-framework-and-scripting.md) — 194 tasks
- [25. Dialogue, localization, and narrative state](plan/plan_25-dialogue-localization-and-narrative-state.md) — 179 tasks
- [26. Cutscenes and cinematic sequencing](plan/plan_26-cutscenes-and-cinematic-sequencing.md) — 193 tasks
- [27. Audio, music, ambience, and radio](plan/plan_27-audio-music-ambience-and-radio.md) — 186 tasks
- [28. UI, HUD, menus, accessibility, and input rebinding](plan/plan_28-ui-hud-menus-accessibility-and-input-rebinding.md) — 206 tasks
- [29. Save games, checkpoints, profiles, and migration](plan/plan_29-save-games-checkpoints-profiles-and-migration.md) — 166 tasks
- [30. Developer tools and editors](plan/plan_30-developer-tools-and-editors.md) — 188 tasks
- [31. Environment content production](plan/plan_31-environment-content-production.md) — 294 tasks
- [32. Character and vehicle content production](plan/plan_32-character-and-vehicle-content-production.md) — 230 tasks
- [33. Story, campaign, and mission content](plan/plan_33-story-campaign-and-mission-content.md) — 143 tasks
- [34. Automated tests, CI, and regression control](plan/plan_34-automated-tests-ci-and-regression-control.md) — 181 tasks
- [35. Performance, memory, and scalability](plan/plan_35-performance-memory-and-scalability.md) — 193 tasks
- [36. Robustness, security, and untrusted content](plan/plan_36-robustness-security-and-untrusted-content.md) — 149 tasks
- [37. Platforms, packaging, release, and operations](plan/plan_37-platforms-packaging-release-and-operations.md) — 163 tasks
- [38. Documentation, contribution, and governance](plan/plan_38-documentation-contribution-and-governance.md) — 77 tasks
- [39. Vertical-slice gates](plan/plan_39-vertical-slice-gates.md) — 64 tasks
- [40. Post-slice expansion and optional research](plan/plan_40-post-slice-expansion-and-optional-research.md) — 20 tasks

## Backlog maintenance rules

- At the start of each milestone, select a small committed subset and move it to a short milestone board.
- Do not mark tasks complete without code/data, validation evidence, and documentation appropriate to their scope.
- Split tasks when implementation exceeds a reviewable change, but keep the parent outcome visible.
- Close or rewrite obsolete tasks instead of preserving them as misleading historical requirements.
- When a task uncovers a CNA, sharp-runtime, or Mesh Craft defect, create a minimal upstream reproduction and link it here.
- After every milestone, update analysis.md assumptions, measured budgets, supported backend tiers, and known build requirements.
- Keep P3 research disabled unless it has a named owner, bounded experiment, and no impact on the vertical-slice critical path.

**Total addressable tasks in this revision: 6380.**
