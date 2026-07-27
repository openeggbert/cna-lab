# 20. Pedestrians and ambient AI

[Back to master plan](../plan.md)

Populate the city at Mafia-1 (2002) fidelity: a modest number of pedestrians near the player, sidewalk navigation, and simple reactive behavior. No statistical/demographic unloaded-population simulation and no density-by-time/weather budgeting — that belongs to a much larger open-world simulation, not this game.

### Gate M9 status (first pass, vertical slice)

`Pedestrian` (`include/IronShadows/Gameplay/Pedestrian.hpp`) walks a fixed sidewalk `WaypointPath` (plan 19) back and forth, and overrides that with a 4-second "flee directly away from the threat" state once the player's vehicle comes within 6 units (`IronShadowsGame::Update()`'s `kPedestrianThreatRadius`) — a simplified stand-in for IS-20-013's "flee before being hit" and IS-20-004's "avoid the player" rather than a real predictive/trajectory check. `IronShadowsGame` spawns one pedestrian per sidewalk path in `WarehouseBlock` only (`RespawnTrafficAndPedestrians()`); `Countryside` has none. Pedestrians are drawn as a plain colored box (`PrototypeRenderer::DrawTraffic`) — no skinned model, no walk/idle animation state (IS-20-003 not done). Their positions feed `PoliceSystem` as witnesses (IS-20-016's query need), satisfied inline rather than via a separate query API. No perception-by-radius-only-while-driving nuance, no personality variation, no group walking, no crowd/congestion avoidance between pedestrians themselves, no persistence, and no population cap/despawn logic — everything below this note beyond what's called out as done stays unstarted.

- [ ] **IS-20-001 P0** — Spawn 10-20 pedestrians near the player in the vertical-slice district. *(Only 2 spawned — one per sidewalk path — not 10-20; see status note above.)*
- [x] **IS-20-002 P0** — Give pedestrians destinations and idle points along sidewalk waypoint data. *(Back-and-forth walking only, no idle points, via the shared `WaypointPath`/`AdvanceAlongPath()` from plan 19.)*
- [ ] **IS-20-003 P0** — Connect pedestrian locomotion to animation states (walk/idle/turn).
- [ ] **IS-20-004 P0** — Make pedestrians avoid the player and moving vehicles at a basic level. *(Only flees the player's vehicle within a fixed radius; does not avoid other traffic or other pedestrians — see status note above.)*
- [ ] **IS-20-005 P1** — Create idle, walk, talk, wait, browse, flee, and recover behaviors.
- [ ] **IS-20-006 P1** — Create perception of vehicles, horns, collisions, weapons, and police within a limited radius.
- [ ] **IS-20-007 P1** — Create reaction selection with cooldowns and simple personality variation (calm/nervous/aggressive).
- [ ] **IS-20-008 P1** — Create safe pedestrian spawn/despawn outside player attention using a simple distance-based population cap.
- [ ] **IS-20-009 P1** — Create persistence for important named NPCs across a district visit.
- [ ] **IS-20-010 P1** — Create sidewalk congestion avoidance so pedestrians do not stack or clip through each other.
- [ ] **IS-20-011 P1** — Create blocked-path recovery when a pedestrian's route is obstructed.
- [ ] **IS-20-012 P1** — Create pedestrian crossing behavior at marked crosswalks, respecting traffic signals.
- [ ] **IS-20-013 P1** — Create vehicle-impact avoidance so pedestrians flee an incoming vehicle before being hit.
- [ ] **IS-20-014 P1** — Create a bounded panic reaction (nearby pedestrians flee) to gunfire or violence within a limited radius.
- [ ] **IS-20-015 P1** — Create an AI debug view showing a selected pedestrian's current goal, path, and perception state.
- [x] **IS-20-016 P1** — Expose a query API so traffic and police systems can find nearby pedestrian witnesses. *(Satisfied inline, not as a separate API: `IronShadowsGame::Update()` collects every `Pedestrian::GetPosition()` into the `witnessPositions` vector passed to `PoliceSystem::Update()` each frame.)*
- [ ] **IS-20-017 P2** — Create group walking for a few pedestrian pairs (couples, friends).
- [ ] **IS-20-018 P2** — Create seated and simple indoor ambient idles for interior extras.
- [ ] **IS-20-019 P2** — Create short ambient dialogue barks between passing pedestrian pairs.
- [ ] **IS-20-020 P2** — Create an accessibility option to reduce simulated pedestrian density.
- [ ] **IS-20-021 P1** — Define the scope, public API, and explicit non-goals of the pedestrian agent.
- [ ] **IS-20-022 P1** — Define versioned configuration/data for the pedestrian agent.
- [ ] **IS-20-023 P1** — Implement the smallest deterministic reference path for the pedestrian agent.
- [ ] **IS-20-024 P1** — Add focused unit tests and one integration scenario for the pedestrian agent.
- [ ] **IS-20-025 P1** — Define save/checkpoint serialization and restoration for the pedestrian agent.
- [ ] **IS-20-026 P2** — Add logging, counters, and a debug view for the pedestrian agent.
- [ ] **IS-20-027 P2** — Define CPU/memory/latency budgets for the pedestrian agent and profile a worst-case scene.
- [ ] **IS-20-028 P2** — Document usage examples and common failure modes for the pedestrian agent.
- [ ] **IS-20-029 P1** — Define the scope, public API, and explicit non-goals of pedestrian perception.
- [ ] **IS-20-030 P1** — Define versioned configuration/data for pedestrian perception.
- [ ] **IS-20-031 P1** — Implement the smallest deterministic reference path for pedestrian perception.
- [ ] **IS-20-032 P1** — Add focused unit tests and one integration scenario for pedestrian perception.
- [ ] **IS-20-033 P1** — Define save/checkpoint serialization and restoration for pedestrian perception.
- [ ] **IS-20-034 P2** — Add logging, counters, and a debug view for pedestrian perception.
- [ ] **IS-20-035 P2** — Define CPU/memory/latency budgets for pedestrian perception and profile a worst-case scene.
- [ ] **IS-20-036 P2** — Document usage examples and common failure modes for pedestrian perception.
- [ ] **IS-20-037 P1** — Define the scope, public API, and explicit non-goals of the reaction selector.
- [ ] **IS-20-038 P1** — Define versioned configuration/data for the reaction selector.
- [ ] **IS-20-039 P1** — Implement the smallest deterministic reference path for the reaction selector.
- [ ] **IS-20-040 P1** — Add focused unit tests and one integration scenario for the reaction selector.
- [ ] **IS-20-041 P1** — Define save/checkpoint serialization and restoration for the reaction selector.
- [ ] **IS-20-042 P2** — Add logging, counters, and a debug view for the reaction selector.
- [ ] **IS-20-043 P2** — Define CPU/memory/latency budgets for the reaction selector and profile a worst-case scene.
- [ ] **IS-20-044 P2** — Document usage examples and common failure modes for the reaction selector.
- [ ] **IS-20-045 P1** — Define the scope, public API, and explicit non-goals of the pedestrian population manager.
- [ ] **IS-20-046 P1** — Define versioned configuration/data for the pedestrian population manager.
- [ ] **IS-20-047 P1** — Implement the smallest deterministic reference path for the pedestrian population manager.
- [ ] **IS-20-048 P1** — Add focused unit tests and one integration scenario for the pedestrian population manager.
- [ ] **IS-20-049 P1** — Define save/checkpoint serialization and restoration for the pedestrian population manager.
- [ ] **IS-20-050 P2** — Add logging, counters, and a debug view for the pedestrian population manager.
- [ ] **IS-20-051 P2** — Define CPU/memory/latency budgets for the pedestrian population manager and profile a worst-case scene.
- [ ] **IS-20-052 P2** — Document usage examples and common failure modes for the pedestrian population manager.
- [ ] **IS-20-053 P2** — Define the scope, public API, and explicit non-goals of the ambient conversation system.
- [ ] **IS-20-054 P2** — Define versioned configuration/data for the ambient conversation system.
- [ ] **IS-20-055 P2** — Implement the smallest deterministic reference path for the ambient conversation system.
- [ ] **IS-20-056 P2** — Add focused unit tests and one integration scenario for the ambient conversation system.
- [ ] **IS-20-057 P2** — Define save/checkpoint serialization and restoration for the ambient conversation system.
- [ ] **IS-20-058 P2** — Add logging, counters, and a debug view for the ambient conversation system.
- [ ] **IS-20-059 P2** — Define CPU/memory/latency budgets for the ambient conversation system and profile a worst-case scene.
- [ ] **IS-20-060 P2** — Document usage examples and common failure modes for the ambient conversation system.
