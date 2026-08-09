# 30. Developer tools and editors

[Back to master plan](../plan.md)

No **second, Iron Gang-specific** editor suite is built. Spatial content (buildings, roads, interiors, props, collision, trigger/mission-marker areas) is authored visually in **Mesh Craft**, which is already a real 3D scene editor (viewport, orbit camera, gizmos, CSG, extrude-along-path, scene hierarchy/properties panels, Walk Mode, and bounded live event-binding preview) — nothing here duplicates that. Non-spatial content (mission state graphs, dialogue) is hand-written JSON/XML/text data, checked by command-line validation scripts. At most one small in-game debug/mission-state view tool is in scope, purely for runtime debugging, not authoring.

- [ ] **IG-30-001 P0** — Document that no bespoke road/lane-graph editor, mission-graph editor, or timeline editor is built on top of Mesh Craft; spatial content is authored directly in Mesh Craft's existing 3D editor, and non-spatial mission/dialogue data is hand-authored and validated by scripts.
- [ ] **IG-30-002 P0** — Create a single asset-build CLI command that validates, converts (MC3 -> GLB -> CNJ), packages, and reports.
- [ ] **IG-30-003 P0** — Create the one in-game debug/mission-state view: current mission, active objective, and forced state transitions.
- [ ] **IG-30-004 P1** — Add a collision/trigger/navigation-waypoint overlay toggle to the debug view.
- [ ] **IG-30-005 P1** — Add an entity inspector (position, state, current AI mode) to the debug view.
- [ ] **IG-30-006 P1** — Add a frame-time overlay toggle to the debug view (not a general metrics system — see plan_04).
- [ ] **IG-30-007 P1** — Create a building/interior portal validation script (CLI, not a GUI editor).
- [ ] **IG-30-008 P1** — Create a content audit CLI script for missing license, collision, LOD, texture, and ID data.
- [ ] **IG-30-009 P1** — Wire the content audit script into CI so unlicensed or invalid assets fail the build.
- [ ] **IG-30-010 P1** — Create Blender export/validation helper scripts for character and vehicle content.
- [ ] **IG-30-011 P1** — Create command-line batch tooling for CI (preflight + check-syntax + validate-mc3 + build + test in one entry point).
- [ ] **IG-30-012 P2** — Create deterministic input recording and playback for QA repro cases.
- [ ] **IG-30-013 P2** — Create screenshot/reference capture automation for regression review.
- [ ] **IG-30-014 P2** — Add hot-reload only for dialogue/mission JSON data (safe to replace at runtime); no general hot-reload framework.
- [ ] **IG-30-015 P2** — Fold world/asset dependency and memory reporting into the asset-build CLI's report output rather than a separate dashboard.
- [ ] **IG-30-016 P2** — Add a CLI save-file dump/inspector (plain text output, not a GUI).
- [ ] **IG-30-017 P2** — Document how to invoke the debug view (key binding and/or command-line flag).
- [ ] **IG-30-018 P2** — Add a unit test for the debug/mission-state view against a scripted test mission.
- [ ] **IG-30-019 P2** — Add a smoke test for the asset-build CLI against one known-good and one known-bad MC3 scene.
- [ ] **IG-30-020 P3** — Generate a simple text/DOT dependency graph from CMake as a static artifact, not an interactive browser.
