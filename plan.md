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
- Move genuinely reusable framework work to CNA/cna-extended only after its general contract is clear; do not design a new "CNA EXT" engine layer from scratch (see group 06).
- Preserve persistent `cmake-build-*` directories, ccache, and at most four build jobs.
- Never create build trees under `/tmp`, `/var/tmp`, or `/dev/shm`.
- Every external asset requires provenance and license approval before it can ship.

## Locked scope decisions

This plan was cut down from an earlier 6,380-task revision that had drifted toward AAA/open-world scale. The project owner locked the following decisions; every group below must stay consistent with them rather than re-introducing the cut ambition. Full reasoning is in `analysis.md`.

1. **Engine layer:** depend on `cna-extended` (sibling repo, already wired into `CMakeLists.txt`) for ECS, `Transform3` scene hierarchy, 3D collision/octree, and skinned-model playback. Materials, shadows, instancing, and post-processing are integrated from CNA's existing `PbrEffect`/`SkinnedPbrEffect`/shadow-mapping/instancing examples, not designed as a new "CNA EXT" layer.
2. **World structure:** Mafia-1 style — several discrete districts/chapters connected by loading screens, not a seamless open-world streaming map.
3. **Traffic AI:** Mafia-1 fidelity — lane-following, signals, obstacle braking. No intersection-reservation deadlock avoidance, overtaking, or public transit.
4. **Police/wanted:** Mafia-1 fidelity — a witnessed offense triggers a chase with one escalation level. No multi-tier wanted stars, search-area decay, roadblocks, or detective persistence.
5. **Pedestrians/ambient AI:** Mafia-1 fidelity — sidewalk waypoint navigation, ~10-20 nearby simulated pedestrians, basic reactions. No statistical unloaded-population simulation or density-by-time/weather budgets.
6. **Developer tools:** no in-house editor suite. At most one small debug/mission-state view tool; content is authored in Mesh Craft/MC3, Blender, and hand-written data files.
7. **Content scope for v1:** a full Mafia-1-scale campaign — multiple districts plus countryside, roughly 15-20 missions — built one district/chapter at a time, each passing its own vertical-slice gate before the next starts (see group 39).
8. **MC3 content pipeline:** manual hand-authoring in Mesh Craft/MC3 (AI as an authoring assistant, not an autonomous generator). No procedural building/road/interior generator tooling.
9. **Rendering target:** baked lighting, one dynamic sun, a handful of important dynamic lights, limited/simple shadows — the ~2-4GB RAM / 512MB-1GB VRAM target in `docs/performance-targets.md`. No SSAO/SSR/volumetric fog/cascaded shadows for v1.
10. **Localization:** ship one language first, but dialogue/UI text uses stable string IDs from day one so localization can be added later without rewriting the dialogue system.

Guiding philosophy: full Mafia-1 **content** scope, but Mafia-1-era **system** fidelity — not AAA/GTA-scale simulation, and not a from-scratch second engine.

## Milestone order

1. **M0 — Reproducible workspace:** dependencies and recursive submodules are present; preflight is actionable.
2. **M1 — Running scaffold:** current procedural prototype builds, runs, saves, loads, and passes tests.
3. **M2 — First MC3/CNJ asset:** one building replaces procedural geometry.
4. **M3 — First production vehicle asset:** one CNJ vehicle preserves the mission loop.
5. **M4 — Production physics decision:** character, trigger, query, and vehicle prototypes are proven.
6. **M5 — District transition proof:** loading between two hand-authored districts preserves player/world/save state.
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
- [02. Workspace, dependencies, and CMake](plan/plan_02-workspace-dependencies-and-cmake.md) — 31 tasks
- [03. Architecture and module boundaries](plan/plan_03-architecture-and-module-boundaries.md) — 25 tasks
- [04. Core runtime services](plan/plan_04-core-runtime-services.md) — 20 tasks
- [05. CNA integration and backend policy](plan/plan_05-cna-integration-and-backend-policy.md) — 17 tasks
- [06. CNA and cna-extended integration roadmap](plan/plan_06-cna-ext-collaboration-roadmap.md) — 27 tasks
- [07. Rendering foundation](plan/plan_07-rendering-foundation.md) — 61 tasks
- [08. Materials, textures, lighting, shadows, and post-processing](plan/plan_08-materials-textures-lighting-shadows-and-post-processing.md) — 78 tasks
- [09. Mesh Craft and MC3 source pipeline](plan/plan_09-mesh-craft-and-mc3-source-pipeline.md) — 50 tasks
- [10. glTF, CNJ, MCB, and runtime packages](plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md) — 68 tasks
- [11. Asset registry, provenance, and build cache](plan/plan_11-asset-registry-provenance-and-build-cache.md) — 67 tasks
- [12. Entity, scene, and world-object model](plan/plan_12-entity-scene-and-world-object-model.md) — 58 tasks
- [13. District loading and world structure](plan/plan_13-world-partitioning-and-streaming.md) — 45 tasks
- [14. Terrain, roads, sidewalks, buildings, and interiors](plan/plan_14-terrain-roads-sidewalks-buildings-and-interiors.md) — 42 tasks
- [15. Physics integration](plan/plan_15-physics-integration.md) — 40 tasks
- [16. Player controller and cameras](plan/plan_16-player-controller-and-cameras.md) — 80 tasks
- [17. Vehicles and driving](plan/plan_17-vehicles-and-driving.md) — 97 tasks
- [18. Characters, skeletons, and animation](plan/plan_18-characters-skeletons-and-animation.md) — 98 tasks
- [19. Navigation and pathfinding](plan/plan_19-navigation-and-pathfinding.md) — 26 tasks
- [20. Pedestrians and ambient AI](plan/plan_20-pedestrians-and-ambient-ai.md) — 60 tasks
- [21. Traffic simulation](plan/plan_21-traffic-simulation.md) — 60 tasks
- [22. Police, witnesses, crime, and wanted response](plan/plan_22-police-witnesses-crime-and-wanted-response.md) — 60 tasks
- [23. Combat, damage, and interaction](plan/plan_23-combat-damage-and-interaction.md) — 44 tasks
- [24. Mission framework and scripting](plan/plan_24-mission-framework-and-scripting.md) — 55 tasks
- [25. Dialogue, localization, and narrative state](plan/plan_25-dialogue-localization-and-narrative-state.md) — 50 tasks
- [26. Cutscenes and cinematic sequencing](plan/plan_26-cutscenes-and-cinematic-sequencing.md) — 35 tasks
- [27. Audio, music, ambience, and radio](plan/plan_27-audio-music-ambience-and-radio.md) — 57 tasks
- [28. UI, HUD, menus, accessibility, and input rebinding](plan/plan_28-ui-hud-menus-accessibility-and-input-rebinding.md) — 58 tasks
- [29. Save games, checkpoints, profiles, and migration](plan/plan_29-save-games-checkpoints-profiles-and-migration.md) — 41 tasks
- [30. Developer tools and editors](plan/plan_30-developer-tools-and-editors.md) — 20 tasks
- [31. Environment content production](plan/plan_31-environment-content-production.md) — 58 tasks
- [32. Character and vehicle content production](plan/plan_32-character-and-vehicle-content-production.md) — 98 tasks
- [33. Story, campaign, and mission content](plan/plan_33-story-campaign-and-mission-content.md) — 143 tasks
- [34. Automated tests, CI, and regression control](plan/plan_34-automated-tests-ci-and-regression-control.md) — 38 tasks
- [35. Performance, memory, and scalability](plan/plan_35-performance-memory-and-scalability.md) — 52 tasks
- [36. Robustness, security, and untrusted content](plan/plan_36-robustness-security-and-untrusted-content.md) — 37 tasks
- [37. Platforms, packaging, release, and operations](plan/plan_37-platforms-packaging-release-and-operations.md) — 76 tasks
- [38. Documentation, contribution, and governance](plan/plan_38-documentation-contribution-and-governance.md) — 22 tasks
- [39. Vertical-slice gates](plan/plan_39-vertical-slice-gates.md) — 76 tasks
- [40. Post-slice expansion and optional research](plan/plan_40-post-slice-expansion-and-optional-research.md) — 20 tasks

## Backlog maintenance rules

- At the start of each milestone, select a small committed subset and move it to a short milestone board.
- Do not mark tasks complete without code/data, validation evidence, and documentation appropriate to their scope.
- Split tasks when implementation exceeds a reviewable change, but keep the parent outcome visible.
- Close or rewrite obsolete tasks instead of preserving them as misleading historical requirements.
- When a task uncovers a CNA, sharp-runtime, or Mesh Craft defect, create a minimal upstream reproduction and link it here.
- After every milestone, update analysis.md assumptions, measured budgets, supported backend tiers, and known build requirements.
- Keep P3 research disabled unless it has a named owner, bounded experiment, and no impact on the vertical-slice critical path.

**Total addressable tasks in this revision: 2148** (down from 6,380 in the pre-scoping revision; see "Locked scope decisions" above).
