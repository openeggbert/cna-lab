# Status

Where Iron Shadows actually is, in one page. plan_38 `IG-38-014`.

Two halves: the counts below are **generated** from `plan/plan_*.md` and verified in CI, because a
number nobody notices going stale is worse than no number; the prose is hand-written, because no
script knows what "playable" means.

## What you can play right now

One district (`WarehouseBlock`) with a second (`Countryside`) reachable through a loading screen.
The prologue delivery mission runs end to end: listen to Mara, walk to the sedan, drive it into the
warehouse marker. It can now also **fail** — stay in a police chase too long and the delivery is
blown; `R` retries from the checkpoint, restoring both the mission's state and the world it was
reached in.

Around that: twelve pedestrians and four cars with lane-following and braking, a police
witness/chase/escalation cycle, a skinned player character with idle/walk blending and vehicle
entry/exit clips, baked lightmap city geometry with a dynamic sun and blob shadows, engine/horn/
footstep audio, an on-screen HUD, a district map (`Tab`), a pause menu with settings (`Esc`), quick
save/load, autosaves at checkpoints, and a damageable sedan.

## What is in progress

Everything is still at **prototype/first-pass fidelity**: pedestrians are coloured boxes, the city
is procedural geometry plus one CNJ warehouse and one CNJ sedan, and there is exactly one mission.
The framework underneath it (missions as data with a typed expression language, saves with
checksums and backups, settings, input bindings, logging, a configuration file) is further along
than the content on top of it.

## What is blocked, and by what

| Gate | Blocked on |
| --- | --- |
| M12 — performance | A controlled capture on **named physical display hardware**. Every budget passes offscreen, but an offscreen run cannot prove presentation/vblank behaviour. Not a code problem. |
| M14 — external build/release | A **clean checkout on a separate machine**, Windows packaging, and signing. The Linux archive builds, validates, and smoke-tests reproducibly here. |

Neither can be closed in a headless container; see `docs/performance-baseline.md` and
`docs/release-packaging.md` for exactly what evidence each still needs.

## Plan progress

<!-- BEGIN GENERATED PLAN PROGRESS -->

**312 of 2148 tasks done** (14%). Open by priority: P0 176 · P1 1007 · P2 596 · P3 57.

| # | Group | Done | |
| --- | --- | --- | --- |
| 00 | [Repository foundation and completed scaffold](../plan/plan_00-repository-foundation-and-completed-scaffold.md) | 29/33 | 87% |
| 01 | [Product identity, scope, and legal separation](../plan/plan_01-product-identity-scope-and-legal-separation.md) | 0/25 | 0% |
| 02 | [Workspace, dependencies, and CMake](../plan/plan_02-workspace-dependencies-and-cmake.md) | 9/31 | 29% |
| 03 | [Architecture and module boundaries](../plan/plan_03-architecture-and-module-boundaries.md) | 2/25 | 8% |
| 04 | [Core runtime services](../plan/plan_04-core-runtime-services.md) | 11/20 | 55% |
| 05 | [CNA integration and backend policy](../plan/plan_05-cna-integration-and-backend-policy.md) | 2/17 | 11% |
| 06 | [CNA and cna-extended integration roadmap](../plan/plan_06-cna-ext-collaboration-roadmap.md) | 1/27 | 3% |
| 07 | [Rendering foundation](../plan/plan_07-rendering-foundation.md) | 0/61 | 0% |
| 08 | [Materials, textures, lighting, shadows, and post-processing](../plan/plan_08-materials-textures-lighting-shadows-and-post-processing.md) | 0/78 | 0% |
| 09 | [Mesh Craft and MC3 source pipeline](../plan/plan_09-mesh-craft-and-mc3-source-pipeline.md) | 6/50 | 12% |
| 10 | [glTF, CNJ, MCB, and runtime packages](../plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md) | 1/68 | 1% |
| 11 | [Asset registry, provenance, and build cache](../plan/plan_11-asset-registry-provenance-and-build-cache.md) | 25/67 | 37% |
| 12 | [Entity, scene, and world-object model](../plan/plan_12-entity-scene-and-world-object-model.md) | 0/58 | 0% |
| 13 | [District loading and world structure](../plan/plan_13-world-partitioning-and-streaming.md) | 16/45 | 35% |
| 14 | [Terrain, roads, sidewalks, buildings, and interiors](../plan/plan_14-terrain-roads-sidewalks-buildings-and-interiors.md) | 6/42 | 14% |
| 15 | [Physics integration](../plan/plan_15-physics-integration.md) | 11/40 | 27% |
| 16 | [Player controller and cameras](../plan/plan_16-player-controller-and-cameras.md) | 3/80 | 3% |
| 17 | [Vehicles and driving](../plan/plan_17-vehicles-and-driving.md) | 2/97 | 2% |
| 18 | [Characters, skeletons, and animation](../plan/plan_18-characters-skeletons-and-animation.md) | 7/98 | 7% |
| 19 | [Navigation and pathfinding](../plan/plan_19-navigation-and-pathfinding.md) | 3/26 | 11% |
| 20 | [Pedestrians and ambient AI](../plan/plan_20-pedestrians-and-ambient-ai.md) | 6/60 | 10% |
| 21 | [Traffic simulation](../plan/plan_21-traffic-simulation.md) | 5/60 | 8% |
| 22 | [Police, witnesses, crime, and wanted response](../plan/plan_22-police-witnesses-crime-and-wanted-response.md) | 5/60 | 8% |
| 23 | [Combat, damage, and interaction](../plan/plan_23-combat-damage-and-interaction.md) | 0/44 | 0% |
| 24 | [Mission framework and scripting](../plan/plan_24-mission-framework-and-scripting.md) | 28/55 | 50% |
| 25 | [Dialogue, localization, and narrative state](../plan/plan_25-dialogue-localization-and-narrative-state.md) | 2/50 | 4% |
| 26 | [Cutscenes and cinematic sequencing](../plan/plan_26-cutscenes-and-cinematic-sequencing.md) | 8/35 | 22% |
| 27 | [Audio, music, ambience, and radio](../plan/plan_27-audio-music-ambience-and-radio.md) | 6/57 | 10% |
| 28 | [UI, HUD, menus, accessibility, and input rebinding](../plan/plan_28-ui-hud-menus-accessibility-and-input-rebinding.md) | 3/58 | 5% |
| 29 | [Save games, checkpoints, profiles, and migration](../plan/plan_29-save-games-checkpoints-profiles-and-migration.md) | 16/41 | 39% |
| 30 | [Developer tools and editors](../plan/plan_30-developer-tools-and-editors.md) | 2/20 | 10% |
| 31 | [Environment content production](../plan/plan_31-environment-content-production.md) | 0/58 | 0% |
| 32 | [Character and vehicle content production](../plan/plan_32-character-and-vehicle-content-production.md) | 0/98 | 0% |
| 33 | [Story, campaign, and mission content](../plan/plan_33-story-campaign-and-mission-content.md) | 0/143 | 0% |
| 34 | [Automated tests, CI, and regression control](../plan/plan_34-automated-tests-ci-and-regression-control.md) | 7/38 | 18% |
| 35 | [Performance, memory, and scalability](../plan/plan_35-performance-memory-and-scalability.md) | 42/52 | 80% |
| 36 | [Robustness, security, and untrusted content](../plan/plan_36-robustness-security-and-untrusted-content.md) | 3/37 | 8% |
| 37 | [Platforms, packaging, release, and operations](../plan/plan_37-platforms-packaging-release-and-operations.md) | 8/76 | 10% |
| 38 | [Documentation, contribution, and governance](../plan/plan_38-documentation-contribution-and-governance.md) | 1/22 | 4% |
| 39 | [Vertical-slice gates](../plan/plan_39-vertical-slice-gates.md) | 36/76 | 47% |
| 40 | [Post-slice expansion and optional research](../plan/plan_40-post-slice-expansion-and-optional-research.md) | 0/20 | 0% |

Regenerate with `python3 scripts/status_report.py --write docs/status.md`; `--check` fails when it is stale.

<!-- END GENERATED PLAN PROGRESS -->

## Where the detail lives

| Question | Document |
| --- | --- |
| What changed recently, and what a resumed session should pick up | `NEXT.md` |
| What was verified, how, and with what evidence | `docs/validation.md` |
| How a subsystem works | `docs/mission-scripting.md`, `docs/save-format.md`, `docs/configuration.md`, `docs/logging.md`, `docs/vehicles.md`, `docs/architecture.md` |
| The full task backlog | `plan.md` and `plan/plan_*.md` |
