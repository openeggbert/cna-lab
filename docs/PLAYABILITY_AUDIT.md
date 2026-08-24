# Human Playability Audit

Black Pine's automated scenario proves that all required items, flags, rules,
hazards and endings can be reached. It does **not** by itself prove that a new
player can read the artwork, discover the intended route and finish the game
without knowing internal target IDs. This audit tracks that second standard.

## Definition of done for a region

A region is human-playable only when all of the following are true:

- its routes match `GAME_DESIGN.md`, including branches and return paths;
- every door, path, ladder, hatch and dangerous route has a visible affordance;
- collectible items are visibly placed where the story says they are;
- important mechanisms have recognisable code-drawn artwork, not only a generic
  hotspot mark;
- F1 describes the next physical action in terms visible on screen;
- death resumes from a safe checkpoint without discarding story progress;
- the physical scenario test traverses the same route a player uses;
- representative before/after states have been rendered and inspected.

## Current regional status

| Region | Screens | Status | Remaining work |
|---|---:|---|---|
| Storm gate and caretaker hub | 1–12 | Audited | Safe opening route, visible phone and pickups, enterable cabin, distinct cabin/radio/shed/cellar interiors, authored hub portals and a gated three-way service-road fork. |
| Relay yard and local power | 13–24 | Audited | The yard, trench, generator, battery, pump, transformer, workshop, hall and control-room branches use visible labelled portals. Repair targets have persistent before/after artwork and the physical scenario traverses the complete route. |
| North forest | 25–38 | Audited | The creek, Theo, cache, kiln, Echo Grove, weather station and lookout are connected by visible routes. Hidden supplies require exploration, the bear solution is harmless, and the scenario follows the physical loop. |
| Quarry and ravine | 39–50 | Audited | The rope descent, lit culvert, safe waterfall route, gate/office/crusher branches, magazine, tunnel and two-way hoist shortcut are physically traversed and visibly labelled. |
| Logging railway | 51–63 | Audited | Sawmill, bunkhouse, mess, office, spur and engine branches use visible two-way routes. June gates the switch-key clue; the engine and trestle have persistent repair states, and the scenario follows the complete physical route. |
| Reservoir and dam | 64–75 | Audited | Gatehouse, two-level turbine hall, pump gallery, intake route and drained-bay path use visible two-way connections. Water, power, valve, pump and grille states persist, and the scenario follows the complete physical route. |
| Mine and underground power | 76–90 | Scripted, not audited | Build mine branches, readable gas/electrical warnings, lift and switchgear topology. |
| Observatory | 91–102 | Scripted, not audited | Build courtyard diversion routes, archive/dome branches and room-specific interiors. |
| Nightjar bunker | 103–115 | Scripted, not audited | Build access-lock sequence, patrol containment, laboratory branches and Kline rescue route. |
| Summit and transmitter | 116–124 | Scripted, not audited | Build the branching climb, readable lightning shelters, persistent grounding repairs and three ending presentations. |

## Completed systemic fixes

- Directly collectible objects use visible 16-colour code-drawn object classes.
- The emergency phone remains visible before and after the opening call.
- The live-feeder route cannot trap a new player in a death/new-game loop.
- Death resumes at the latest Explore2D travel-anchor checkpoint.
- The caretaker cabin is entered through visible doors with ENTER.
- Screens 6–11 now form the authored caretaker hub instead of a false linear
  sequence: porch, cabin, radio nook, cellar, tool shed and weather mast.
- Screen 12 is a real relay/forest fork. Its visible FOREST barrier stays
  closed until the direction trace is complete, then returns through screen 25.
- Screens 13–24 form the authored relay-yard hub. Every branch has a labelled
  entrance and return route, and F1 names those labels rather than internal
  screen numbers.
- Cable, fuse, battery, fuel, feeder, cabinet, Nightjar trunk, generator lever
  and trace-console states have dedicated 16-colour before/after drawings.
- The render-preview utility emits repaired-state frames for the relay region,
  and the scenario test now walks through the same portals as a player.
- Screens 25–38 now form the documented forest loops instead of a linear road:
  the creek branches to Theo and Echo Grove, while Firebreak Junction branches
  to the cache, weather station, lookout and Nell's marked ravine route.
- The ranger boot must be examined before its dry bandage appears, the survey
  ribbon is a visible evidence clue, and all cache supplies are visibly gated
  by Theo's combination.
- The bear is no longer a lethal proximity trap. It blocks only the east path,
  makes Iris retreat safely, leaves after the upwind flare solution, and has a
  persistent cleared-meadow drawing.
- Fallen-fir, compass-bearing, buried-cable and weather-recorder actions have
  recognisable 16-colour artwork and persistent completion states.
- Screens 39–50 now use the authored vertical geography: a visible rope reaches
  the west ravine floor, stairs reach the quarry, and the crusher deck branches
  separately to the office, equipment magazine and tunnel.
- The dark culvert and tunnel require their documented portable lights. The
  waterfall pushes Iris back safely until its visible sluice is closed.
- Brant still makes the active crusher belt dangerous, but labelled routes keep
  the player on the horn-and-cage solution. The magazine remains inaccessible
  until he is contained without injury.
- Running the east hoist visibly deploys the missing-span walkway in both
  directions, creating tested shortcuts from landing to bridge and east floor.
- Screens 51–63 now form the documented logging-camp hub instead of a linear
  catalogue route. The yard branches to the mill, bunkhouse, mess hall and rail
  spur; the mill branches to filing, boiler and pond work areas; every branch
  has a visible return route.
- June's conversation now reveals the foreman's switch key, so it cannot be
  collected before its story clue. The log-pond pike is safely reachable while
  the moving-log water remains a persistent hazard.
- Belt tension, reserve fuel, floating service box, rail points, four engine
  repair stations, starter, whistle, brake linkage and portable radio all have
  distinct before/after 16-colour drawings. The running engine gains selective
  smoke animation, while other camp machinery remains deliberately sparse.
- F1 now guides the logging sequence one visible action at a time, and the
  automated scenario traverses the same labelled doors and paths as a player.
- Screens 64–75 now match the dam's vertical design: the west abutment branches
  to the spillway and turbine hall, both turbine levels return to the pump
  gallery, and the gallery branches to the intake tunnel and maintenance bay.
- The hand-crank torch is now required in the dark intake tunnel before
  Kline's warning and the removable valve wheel become visible. The scenario
  can no longer bypass this documented step.
- Spillway shields, badge reader, crank socket, auxiliary diagram, three bay
  breakers, pump flange, starter, intake valve and shaft grille have distinct
  before/after drawings. Animated state is limited to water, warning lamps,
  turbines, pump rotation, pressure and drips.
- The maintenance bay visibly changes from deep live water to isolated water
  and finally a shallow drained state. The shaft route cannot be used until the
  pump is running and the magnet has been recovered safely.
- F1 guides every physical dam action by the labels drawn in the scene, and the
  full scenario uses every stair, door, tunnel, shoreline and drained passage.

The next implementation pass starts with the Mine and Underground Power
(screens 76–90), where the ventilation, drainage, lift and substation routes
are still presented as a linear sequence.
