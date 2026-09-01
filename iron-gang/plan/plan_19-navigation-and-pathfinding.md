# 19. Navigation and pathfinding

[Back to master plan](../plan.md)

Pedestrian navigation uses a simple waypoint graph authored alongside the sidewalk data in plan 14 — not a baked navmesh, crowd-density system, or danger-cost pathfinding. Vehicles path over the road graph defined in plan 14/21, not this system. This stays intentionally small: Mafia 1's pedestrians did not need more than "walk the sidewalk, use the crossing, avoid the car in front of you."

### Gate M9 status (first pass, vertical slice)

Delivered the smallest possible version of IG-19-001 only: `WaypointPath` (`include/IronGang/World/WaypointPath.hpp`) is a hand-authored, ordered list of points with a `loop` flag — no graph, no branching, no portals, no async requests, no local avoidance. A shared `AdvanceAlongPath()` free function walks a mover toward its current target and advances/wraps on arrival; it is reused as-is by both `TrafficVehicle` (plan 21) and `Pedestrian` (plan 20). `PrototypeWorld::BuildWarehouseBlock()` authors one traffic loop and two sidewalk back-and-forth paths by hand; `BuildCountryside()` has none (no ambient life there yet). Everything else below — the actual graph/portal/off-mesh-link/local-avoidance/async-path machinery this plan describes — remains not started; the traffic/pedestrian systems built for M9 do not need it yet because they each follow exactly one fixed path.

## Pedestrian waypoint graph

- [x] **IG-19-001 P0** — Choose a simple waypoint/graph approach for pedestrian navigation (reuses the sidewalk-graph schema from plan 14). *(Done in the smallest possible form: a single ordered polyline per mover, not a graph — see the Gate M9 status note above.)*
- [ ] **IG-19-002 P0** — Connect exterior and interior navigation graphs through portals (ties to plan 14's interior portals).
- [ ] **IG-19-003 P1** — Define agent size, slope, and step-clearance classes for pedestrians (a single "adult pedestrian" class is enough for v1).
- [x] **IG-19-004 P1** — Validate a district's navigation graph for disconnected authored destinations at load time. *(`SidewalkGraph::FindUnreachableNodes()`, run at load: a graph with any node unreachable from the first is **rejected**, naming one. It immediately caught a real content bug shipped the previous iteration -- the warehouse block's crossing and door nodes were never joined to the pavements, leaving six of eight nodes stranded and nothing noticing. `assets/districts/warehouse_block.sidewalks.json` was rewritten as a connected graph: each pavement split at its crossing and its doorway, with spurs to the two doors.)*
- [ ] **IG-19-005 P1** — Add debug rendering for waypoint nodes, edges, portals, and active paths.
- [ ] **IG-19-006 P1** — Add focused unit tests for waypoint-graph loading, connectivity, and portal linking.
- [ ] **IG-19-007 P2** — Document the waypoint-graph authoring convention alongside the sidewalk-graph docs in plan 14.

## Path requests

- [ ] **IG-19-008 P0** — Implement asynchronous path requests with cancellation for pedestrians.
- [ ] **IG-19-009 P1** — Add path-result staleness checks (discard a stale path if the world changed since the request).
- [ ] **IG-19-010 P1** — Add basic path smoothing/corner-cutting so pedestrians don't hug every waypoint exactly.
- [ ] **IG-19-011 P2** — Add focused unit tests for async path requests and cancellation.

## Off-mesh links

- [ ] **IG-19-012 P1** — Define off-mesh links for doors, stairs, and vehicle-entry points.
- [ ] **IG-19-013 P2** — Add an integration test where a pedestrian path correctly uses a door off-mesh link into an interior.
- [ ] **IG-19-014 P2** — Document the off-mesh-link authoring convention for content authors.

## Local avoidance and dynamic obstacles

- [ ] **IG-19-015 P1** — Implement basic local avoidance so pedestrians don't walk through each other or a stopped vehicle.
- [ ] **IG-19-016 P1** — Handle a temporary dynamic obstacle (a player-parked car, a dropped object) blocking part of the sidewalk graph.
- [ ] **IG-19-017 P2** — Add an integration test with ~10-20 pedestrians navigating around each other and a temporary obstacle.
- [ ] **IG-19-018 P2** — Profile local avoidance/dynamic-obstacle handling against the ~10-20 concurrent pedestrian budget in `docs/performance-targets.md`.

## Pedestrian/road graph handshake

- [ ] **IG-19-019 P1** — Ensure pedestrian crossings in the sidewalk graph align with the road graph's crossing/stop-line data (traffic-side pathfinding itself lives in plan 21, not here).
- [ ] **IG-19-020 P2** — Add a validation check that every road-graph crossing has a matching sidewalk-graph crossing.

## Save integration and tests

- [ ] **IG-19-021 P1** — Define save/checkpoint restoration rules for in-progress pedestrian paths (simplest correct behavior: re-request a fresh path on load).
- [x] **IG-19-022 P0** — Add an integration test that walks a scripted pedestrian across one hand-authored district end to end using the waypoint graph. *(`TestSidewalkRoutingAcrossTheDistrict` routes from the hotel door to the apartments door -- opposite sides of the road, so the route can only exist through the crossing -- then walks a `Pedestrian` along the built path at a fixed 60 Hz step and requires it to arrive within a metre of the far door. Also checks symmetry, a route to oneself, a route to a node that does not exist, that adjacent nodes route directly rather than the long way round, and that nothing in the shipped district is stranded.)*
- [ ] **IG-19-023 P2** — Document this system's scope and explicit non-goals (no navmesh, no crowd heatmaps, no danger-cost routing) for contributors.

## Deferred / only if actually needed later

- [ ] **IG-19-024 P3** — Evaluate Recast/Detour or another navmesh library only if the waypoint graph proves insufficient at real production content scale.
- [ ] **IG-19-025 P3** — Add crowd-flow heatmaps only if a later analytics/VFX need justifies them.
- [ ] **IG-19-026 P3** — Add danger/police/traffic route-cost overlays only if a future, more advanced wanted-response feature needs smarter pedestrian fleeing than basic local avoidance already provides.
