# 13. District loading and world structure

[Back to master plan](../plan.md)

Iron City is built as several discrete districts and countryside chapters connected by loading screens, the same way the original Mafia moved between Lost Heaven, the countryside, and other areas. There is no seamless open-world streaming: within one district the player drives and walks freely without loading, and moving to a different district is a normal level transition. Keep this system small — it exists to make district transitions reliable and save-safe, not to hide a continuous always-loaded map.

- [ ] **IS-13-001 P0** — Define the district registry: id, display name, asset root, entry/exit points, and which mission chapters use it.
- [ ] **IS-13-002 P0** — Define the public API for requesting a district transition (by district id and named entry point).
- [ ] **IS-13-003 P1** — Store district registry data as versioned JSON/XML under `assets/config/`.
- [ ] **IS-13-004 P1** — Validate the district registry at startup (missing asset roots, dangling entry-point references) and fail with an actionable message.
- [ ] **IS-13-005 P2** — Document how to add a new district to the registry.
- [ ] **IS-13-006 P0** — Implement the load sequence: unload the current district's geometry/collision/navigation/entities, then load the target district's.
- [ ] **IS-13-007 P0** — Implement synchronous MC3/CNJ scene loading for one district (no cross-district streaming).
- [ ] **IS-13-008 P0** — Spawn the player and camera at the requested entry point with the correct facing direction.
- [ ] **IS-13-009 P0** — Spawn the player's current vehicle (if any) at a valid entry point when the district supports vehicle entry.
- [ ] **IS-13-010 P1** — Guarantee deterministic teardown order so no dangling references survive a district unload (physics bodies, audio emitters, entities).
- [ ] **IS-13-011 P1** — Add a hard failure path if a required entry point is missing, with a clear error rather than a silent spawn at the origin.
- [ ] **IS-13-012 P1** — Add focused unit tests for the load/unload sequence using a pair of minimal test districts.
- [ ] **IS-13-013 P0** — Show a loading screen during district transitions with a minimum display time to avoid flicker on fast loads.
- [ ] **IS-13-014 P1** — Show basic progress feedback (percentage or stage text) on the loading screen.
- [ ] **IS-13-015 P2** — Show a district-specific loading image or short lore text during the transition.
- [ ] **IS-13-016 P2** — Add a fade-out/fade-in transition around the loading screen instead of a hard cut.
- [ ] **IS-13-017 P0** — Carry player health/inventory/mission state across a district transition unchanged.
- [ ] **IS-13-018 P0** — Carry the player's currently owned/driven vehicle across a transition when the story allows it.
- [ ] **IS-13-019 P1** — Reset or re-anchor the camera cleanly at the new entry point (no leftover shake, no old target reference).
- [ ] **IS-13-020 P1** — Define which HUD/dialogue/mission state must persist vs. reset across a transition.
- [ ] **IS-13-021 P1** — Add an integration test that drives a full transition and asserts player/vehicle/mission state survived intact.
- [ ] **IS-13-022 P0** — Store per-district mutable world state (doors unlocked, pickups taken, destructible props broken, NPC defeated/alive) keyed by district id.
- [ ] **IS-13-023 P0** — Apply saved per-district state when a district is (re-)loaded, so a district reflects prior visits correctly.
- [ ] **IS-13-024 P1** — Integrate per-district state into the existing save/load system (`SaveGame`) as one section per visited district.
- [ ] **IS-13-025 P1** — Define save-compatibility rules for adding a new district to a registry that existing saves don't know about.
- [ ] **IS-13-026 P1** — Add a save/load round-trip test that saves mid-district, reloads, and verifies world state matches.
- [ ] **IS-13-027 P2** — Add a test that saves in one district, loads into a fresh process, and confirms the correct district is restored on load.
- [ ] **IS-13-028 P1** — Add frustum culling for a district's static geometry (skip draw calls for off-screen buildings/props).
- [ ] **IS-13-029 P1** — Add simple distance-based LOD tiers (near/medium/far) for buildings and props within a district.
- [ ] **IS-13-030 P2** — Split a large district into a handful of fixed always-loaded sub-areas only if profiling shows a single draw/collision pass is too slow — this is local culling, not streaming.
- [ ] **IS-13-031 P2** — Add occlusion-friendly grouping for dense city blocks (batch nearby static props).
- [ ] **IS-13-032 P2** — Profile one full district's CPU/GPU frame time and memory footprint against the docs/performance-targets.md budget.
- [ ] **IS-13-033 P3** — Add a distant skyline/proxy silhouette for the district the player is not currently in, if a mission needs visible transitions (e.g. driving toward a district border).
- [ ] **IS-13-034 P1** — Load a district's geometry/texture data on a background thread while showing the loading screen.
- [ ] **IS-13-035 P1** — Upload GPU resources (buffers, textures) from the background-loaded data on the render thread once ready.
- [ ] **IS-13-036 P2** — Add cancellation if the player quits or force-reloads mid-transition.
- [ ] **IS-13-037 P2** — Add a background-loading unit test using a fake slow asset source.
- [ ] **IS-13-038 P3** — Pre-warm the next likely district's assets in the background during a loading-screen-heavy mission sequence, if load times become a problem.
- [ ] **IS-13-039 P1** — Handle a missing or corrupt district package by returning to the main menu or last checkpoint with a clear message instead of crashing.
- [ ] **IS-13-040 P1** — Handle an interrupted/cancelled transition (e.g. process killed mid-load) by recovering to the last valid checkpoint on next launch.
- [ ] **IS-13-041 P2** — Add a fallback minimal district (a single empty room) used only in automated tests and error recovery, never shipped as real content.
- [ ] **IS-13-042 P0** — Add a deterministic core test that transitions between two minimal test districts and asserts the world ends up in the expected state.
- [ ] **IS-13-043 P1** — Add a debug/dev command to force-load any registered district at any entry point for testing.
- [ ] **IS-13-044 P2** — Add a soak test that repeats district transitions many times and checks for leaked entities, physics bodies, or audio emitters.
- [ ] **IS-13-045 P2** — Document the district-transition flow and per-district save-state contract for content authors adding new districts.
