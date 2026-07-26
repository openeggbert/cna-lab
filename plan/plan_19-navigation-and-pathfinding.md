# 19. Navigation and pathfinding

[Back to master plan](../plan.md)

Pedestrian navigation uses a simple waypoint graph authored alongside the sidewalk data in plan 14 — not a baked navmesh, crowd-density system, or danger-cost pathfinding. Vehicles path over the road graph defined in plan 14/21, not this system. This stays intentionally small: Mafia 1's pedestrians did not need more than "walk the sidewalk, use the crossing, avoid the car in front of you."

## Pedestrian waypoint graph

- [ ] **IS-19-001 P0** — Choose a simple waypoint/graph approach for pedestrian navigation (reuses the sidewalk-graph schema from plan 14).
- [ ] **IS-19-002 P0** — Connect exterior and interior navigation graphs through portals (ties to plan 14's interior portals).
- [ ] **IS-19-003 P1** — Define agent size, slope, and step-clearance classes for pedestrians (a single "adult pedestrian" class is enough for v1).
- [ ] **IS-19-004 P1** — Validate a district's navigation graph for disconnected authored destinations at load time.
- [ ] **IS-19-005 P1** — Add debug rendering for waypoint nodes, edges, portals, and active paths.
- [ ] **IS-19-006 P1** — Add focused unit tests for waypoint-graph loading, connectivity, and portal linking.
- [ ] **IS-19-007 P2** — Document the waypoint-graph authoring convention alongside the sidewalk-graph docs in plan 14.

## Path requests

- [ ] **IS-19-008 P0** — Implement asynchronous path requests with cancellation for pedestrians.
- [ ] **IS-19-009 P1** — Add path-result staleness checks (discard a stale path if the world changed since the request).
- [ ] **IS-19-010 P1** — Add basic path smoothing/corner-cutting so pedestrians don't hug every waypoint exactly.
- [ ] **IS-19-011 P2** — Add focused unit tests for async path requests and cancellation.

## Off-mesh links

- [ ] **IS-19-012 P1** — Define off-mesh links for doors, stairs, and vehicle-entry points.
- [ ] **IS-19-013 P2** — Add an integration test where a pedestrian path correctly uses a door off-mesh link into an interior.
- [ ] **IS-19-014 P2** — Document the off-mesh-link authoring convention for content authors.

## Local avoidance and dynamic obstacles

- [ ] **IS-19-015 P1** — Implement basic local avoidance so pedestrians don't walk through each other or a stopped vehicle.
- [ ] **IS-19-016 P1** — Handle a temporary dynamic obstacle (a player-parked car, a dropped object) blocking part of the sidewalk graph.
- [ ] **IS-19-017 P2** — Add an integration test with ~10-20 pedestrians navigating around each other and a temporary obstacle.
- [ ] **IS-19-018 P2** — Profile local avoidance/dynamic-obstacle handling against the ~10-20 concurrent pedestrian budget in `docs/performance-targets.md`.

## Pedestrian/road graph handshake

- [ ] **IS-19-019 P1** — Ensure pedestrian crossings in the sidewalk graph align with the road graph's crossing/stop-line data (traffic-side pathfinding itself lives in plan 21, not here).
- [ ] **IS-19-020 P2** — Add a validation check that every road-graph crossing has a matching sidewalk-graph crossing.

## Save integration and tests

- [ ] **IS-19-021 P1** — Define save/checkpoint restoration rules for in-progress pedestrian paths (simplest correct behavior: re-request a fresh path on load).
- [ ] **IS-19-022 P0** — Add an integration test that walks a scripted pedestrian across one hand-authored district end to end using the waypoint graph.
- [ ] **IS-19-023 P2** — Document this system's scope and explicit non-goals (no navmesh, no crowd heatmaps, no danger-cost routing) for contributors.

## Deferred / only if actually needed later

- [ ] **IS-19-024 P3** — Evaluate Recast/Detour or another navmesh library only if the waypoint graph proves insufficient at real production content scale.
- [ ] **IS-19-025 P3** — Add crowd-flow heatmaps only if a later analytics/VFX need justifies them.
- [ ] **IS-19-026 P3** — Add danger/police/traffic route-cost overlays only if a future, more advanced wanted-response feature needs smarter pedestrian fleeing than basic local avoidance already provides.
