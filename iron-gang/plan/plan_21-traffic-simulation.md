# 21. Traffic simulation

[Back to master plan](../plan.md)

Build lane-based traffic at Mafia-1 (2002) fidelity: cars follow lanes, obey signals, brake for the player and obstacles, and despawn out of attention. No intersection-reservation deadlock avoidance, no AI overtaking/passing maneuvers, and no public transit — those belong to a much larger open-world simulation, not this game.

### Gate M9 status (first pass, vertical slice)

`TrafficVehicle` (`include/IronGang/Gameplay/TrafficVehicle.hpp`) is a kinematic (non-Jolt) mover following a fixed `WaypointPath` loop (plan 19) at its own cruise speed (four vehicles, speeds varied from a district-seeded `RandomSource`), braking smoothly when the shared `DistanceAheadInLane()` helper (`Gameplay/LaneClearance.hpp`, also used by pedestrian congestion avoidance) reports something (another traffic vehicle, or the player's own vehicle) ahead in roughly the same lane. Since 2026-08-26 the crossing has a fixed-time signal and two stop lines, obeyed by folding a red into the same obstacle-distance test used for the car ahead (IG-21-003/007). No no turning/lane graph, no yield rules, no spawn/despawn-by-attention, no accident/stuck handling, and no debug visualization — those all remain unstarted; this system only needs to prove "follow a loop, brake for what's ahead."

- [x] **IG-21-001 P0** — Spawn three to five AI vehicles on a closed lane loop. *(Four, one per corner of the traffic oval, each with its own cruise speed between 5 and 7 from the district-seeded `RandomSource` (plan_04 IG-04-011). The varied speeds matter beyond looks: identical speeds hold their spacing forever, so the following-distance braking in IG-21-002 would never actually fire. Measured in a `mixed --smoke 400` run: at most 16 obstacle checks per update, `ai_cpu` p95 0.002 ms.)*
- [x] **IG-21-002 P0** — Implement lane following, speed target, and following distance. *(`TrafficVehicle::Update()`: accelerates toward a cruise speed, brakes smoothly starting 10 units from an obstacle ahead, matching the minimum-gap/braking-distance constants in `src/Gameplay/TrafficVehicle.cpp`.)*
- [x] **IG-21-003 P0** — Implement stop line and traffic signal compliance. *(The warehouse block has a signalled crossing: two `TrafficStopLine`s, one per direction, each carrying the heading of the traffic it stops. **A red light is modelled as an obstacle at the stop line**, folded into the same `DistanceAheadInLane` minimum traffic already uses for the car in front -- so the existing following-distance braking does the stopping, `TrafficVehicle` needed no new state, and a car queued behind one waiting at a red brakes for the car rather than the light. A line only stops the lane it governs, tested by direction rather than by angle so there is no wrapping to get wrong. Verified by `TestTrafficSignalCyclesAndOpposesItself`, which includes a vehicle braking to a halt for a close obstacle and pulling away when it clears.)*
- [x] **IG-21-004 P0** — Implement obstacle braking for the player and blocked traffic. *(`DistanceAheadIfInLane()` in `IronGangGame.cpp` considers both other `TrafficVehicle`s and the player's own vehicle when driving.)*
- [ ] **IG-21-005 P1** — Create directed lane graphs with turn connections.
- [ ] **IG-21-006 P1** — Create simple route selection across intersections using yield/priority rules, not reservation-based deadlock avoidance.
- [x] **IG-21-007 P1** — Create traffic-light phases and timing data. *(`TrafficSignal` (`include/IronGang/Gameplay/TrafficSignal.hpp`): green/amber/red on a fixed cycle (9/2/11 s), with an offset so a light can start partway through. **One signal drives both directions**: `GetOpposingPhase()` derives the crossing direction's colour from the same timer, which is what makes it impossible for the two to show green together -- two independent lights at one crossing eventually drift into exactly that. The opposing direction gets its own amber before this one turns green. Amber stops traffic, deliberately: deciding whether a car can "make it" is a rule nobody watching would notice and one that leaves cars in the crossing when the phase flips. A zero-length phase and a negative/NaN delta are refused. Tested over 2000 non-cycle-dividing steps for the never-both-moving invariant, **and** that both directions do get a green -- an intersection nobody may cross satisfies the first property and is still broken.)*
- [ ] **IG-21-008 P1** — Create stop-sign and priority-junction yield behavior.
- [ ] **IG-21-009 P1** — Create lane-change rules only where roads support them.
- [ ] **IG-21-010 P1** — Create parked vehicle slots and departure/arrival behavior.
- [ ] **IG-21-011 P1** — Create traffic spawn/despawn outside player attention.
- [ ] **IG-21-012 P1** — Create blocked-road rerouting onto an alternate lane segment.
- [ ] **IG-21-013 P1** — Create stuck detection and bounded recovery.
- [ ] **IG-21-014 P1** — Create simple accident/disabled-vehicle handling after a player collision.
- [ ] **IG-21-015 P1** — Create emergency-vehicle priority hooks for later police use.
- [ ] **IG-21-016 P1** — Create debug lane, route, and target-speed visualization.
- [ ] **IG-21-017 P1** — Create deterministic per-intersection scenario tests (signal compliance, yield, braking).
- [ ] **IG-21-018 P2** — Create honking and simple social driving reactions.
- [ ] **IG-21-019 P2** — Create overtaking only on selected multi-lane roads, added after core flow is stable.
- [ ] **IG-21-020 P2** — Create parking maneuvers after core flow is robust.
- [ ] **IG-21-021 P1** — Define the scope, public API, and explicit non-goals of the lane follower.
- [ ] **IG-21-022 P1** — Define versioned configuration/data for the lane follower.
- [ ] **IG-21-023 P1** — Implement the smallest deterministic reference path for the lane follower.
- [ ] **IG-21-024 P1** — Add focused unit tests and one integration scenario for the lane follower.
- [ ] **IG-21-025 P1** — Define save/checkpoint serialization and restoration for the lane follower.
- [ ] **IG-21-026 P2** — Add logging, counters, and a debug view for the lane follower.
- [ ] **IG-21-027 P2** — Define CPU/memory/latency budgets for the lane follower and profile a worst-case scene.
- [ ] **IG-21-028 P2** — Document usage examples and common failure modes for the lane follower.
- [ ] **IG-21-029 P1** — Define the scope, public API, and explicit non-goals of the traffic routing and signal system.
- [ ] **IG-21-030 P1** — Define versioned configuration/data for the traffic routing and signal system.
- [ ] **IG-21-031 P1** — Implement the smallest deterministic reference path for the traffic routing and signal system.
- [ ] **IG-21-032 P1** — Add focused unit tests and one integration scenario for the traffic routing and signal system.
- [ ] **IG-21-033 P1** — Define save/checkpoint serialization and restoration for the traffic routing and signal system.
- [ ] **IG-21-034 P2** — Add logging, counters, and a debug view for the traffic routing and signal system.
- [ ] **IG-21-035 P2** — Define CPU/memory/latency budgets for the traffic routing and signal system and profile a worst-case scene.
- [ ] **IG-21-036 P2** — Document usage examples and common failure modes for the traffic routing and signal system.
- [ ] **IG-21-037 P1** — Define the scope, public API, and explicit non-goals of the vehicle spacing controller.
- [ ] **IG-21-038 P1** — Define versioned configuration/data for the vehicle spacing controller.
- [ ] **IG-21-039 P1** — Implement the smallest deterministic reference path for the vehicle spacing controller.
- [ ] **IG-21-040 P1** — Add focused unit tests and one integration scenario for the vehicle spacing controller.
- [ ] **IG-21-041 P1** — Define save/checkpoint serialization and restoration for the vehicle spacing controller.
- [ ] **IG-21-042 P2** — Add logging, counters, and a debug view for the vehicle spacing controller.
- [ ] **IG-21-043 P2** — Define CPU/memory/latency budgets for the vehicle spacing controller and profile a worst-case scene.
- [ ] **IG-21-044 P2** — Document usage examples and common failure modes for the vehicle spacing controller.
- [ ] **IG-21-045 P1** — Define the scope, public API, and explicit non-goals of the traffic population and recovery manager.
- [ ] **IG-21-046 P1** — Define versioned configuration/data for the traffic population and recovery manager.
- [ ] **IG-21-047 P1** — Implement the smallest deterministic reference path for the traffic population and recovery manager.
- [ ] **IG-21-048 P1** — Add focused unit tests and one integration scenario for the traffic population and recovery manager.
- [ ] **IG-21-049 P1** — Define save/checkpoint serialization and restoration for the traffic population and recovery manager.
- [ ] **IG-21-050 P2** — Add logging, counters, and a debug view for the traffic population and recovery manager.
- [ ] **IG-21-051 P2** — Define CPU/memory/latency budgets for the traffic population and recovery manager and profile a worst-case scene.
- [ ] **IG-21-052 P2** — Document usage examples and common failure modes for the traffic population and recovery manager.
- [ ] **IG-21-053 P2** — Define the scope, public API, and explicit non-goals of the parking and arrival system.
- [ ] **IG-21-054 P2** — Define versioned configuration/data for the parking and arrival system.
- [ ] **IG-21-055 P2** — Implement the smallest deterministic reference path for the parking and arrival system.
- [ ] **IG-21-056 P2** — Add focused unit tests and one integration scenario for the parking and arrival system.
- [ ] **IG-21-057 P2** — Define save/checkpoint serialization and restoration for the parking and arrival system.
- [ ] **IG-21-058 P2** — Add logging, counters, and a debug view for the parking and arrival system.
- [ ] **IG-21-059 P2** — Define CPU/memory/latency budgets for the parking and arrival system and profile a worst-case scene.
- [ ] **IG-21-060 P2** — Document usage examples and common failure modes for the parking and arrival system.
