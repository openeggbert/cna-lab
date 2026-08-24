# Black Pine — Full Game Narrative and Content Design

Status: implemented narrative specification for Black Pine 0.2.0.

The canonical 124-screen catalogue is implemented in
`src/BlackPineContent.hpp`; procedural scenes, inventory, interactions,
dialogue, hazards, hints and the complete five-act state graph are implemented
in `src/BlackPineFullWorld.cpp`. `test/ScenarioTests.cpp` performs the required
route through the evidence-broadcast ending and verifies every numbered screen.

## 1. Scope and reference target

The original *Tajná mise* contains **124 numbered gameplay screens**, IDs 1 through 124. Its publisher logo, title and menu, new-game introduction, map, death screens, narrative cards, ending, and credits are separate presentation states and are not part of that count.

The full version of *Black Pine* will therefore contain exactly **124 fixed, non-scrolling gameplay screens**. Title screens, menus, map overlays, death cards, ending cards, and credits will remain outside the 124-screen budget. The present seven-screen demonstration is treated as a vertical slice: its trailhead, cabin, relay yard, generator, ravine, tower base, and tower platform ideas survive, but are expanded and repositioned inside the larger world.

This is an original story. It borrows the structural grammar of a late-1990s keyboard adventure—compact controls, room-by-room exploration, inventory as persistent world state, code-drawn EGA art, selective animation, dialogue bubbles, hazards, puzzles, and an unlockable travel map—but no room, character, plot, dialogue, or puzzle is copied from *Tajná mise*.

## 2. Creative identity

### Working title

**Black Pine: The Long Silence**

The executable and repository may continue to use the shorter name **Black Pine**.

### Genre and tone

- A non-combat exploration and puzzle adventure.
- An analogue-technology mystery set in the autumn of 1999.
- Remote mountain weather, civil infrastructure, abandoned research machinery, and human choices rather than supernatural events.
- Serious stakes with dry, humane dialogue; tense but not grim.
- Violence is never the player's solution. Human adversaries are distracted, persuaded, exposed, locked out, or forced to surrender.
- Animals are obstacles to understand or avoid, never targets.

### Player promise

Every major observation should answer one question and raise another. The player begins by repairing a storm-damaged radio relay, gradually learns that the damage was deliberate, follows the sabotaged network through forest, quarry, logging railway, dam, mine, observatory, and bunker, and finally uses the mountain's own transmitter to defeat a regional radio-suppression experiment.

### Visual promise

All final art is built from Explore2D's fixed 640×350 logical screen, 16-colour EGA palette, bitmap text, and code-drawn primitives. Screens are detailed but readable. Only story-bearing details move: water, machinery, warning lamps, smoke, selected wildlife, patrols, weather, and scripted actions.

## 3. Premise

At 02:17, the worst electrical storm in thirty years strikes Black Pine Ridge. The mountain relay stops carrying fire, medical, and ranger traffic. A rescue helicopter, **Kestrel Six**, is forced down somewhere beyond the ridge with an injured child aboard. Roads are washed out and ordinary radios produce only a low, rhythmic pulse.

**Iris Bell**, a volunteer radio technician who once apprenticed at Black Pine, reaches the trailhead on foot. Relay caretaker **Mara Venn** expects a routine repair: replace a fuse, patch a burned cable, and restart the generator. When Iris restores local power, however, the receiver remains silent. A direction-finding trace points north—not toward the storm, but toward an abandoned government research station called **Nightjar**.

The station's former chief engineer, **Gideon Voss**, has returned with a small crew posing as geological surveyors. Voss designed the experimental **Quiet Field**, a phase-cancellation system intended to suppress hostile radio traffic while preserving a protected emergency carrier. The project was closed after engineer **Ruth Calder** proved that the field could interfere with aircraft navigation and medical telemetry. Voss has stolen three key components and chosen the storm as cover for an illegal full-power demonstration. At midnight he intends to silence every transmitter in the valley for a private buyer. Kestrel Six is already an unintended casualty.

Iris must restore the old infrastructure faster than Voss can dismantle it, rescue people caught in his wake, enter Nightjar, obtain proof of the experiment, and turn the summit transmitter against the Quiet Field. The final victory is not an explosion. It is a clear human voice returning to the air.

## 4. Story structure

### Act I — A fault that should not exist (screens 1–24)

Iris reaches Mara's cabin, collects basic repair tools, and restores the relay site's local generator, cable trench, and transformer. The repair is deliberately tactile and teaches TAKE, EXAMINE, USE, contextual actions, conditional exits, and revisiting earlier screens. At the local control room the emergency carrier remains buried under a precisely timed pulse. A maintenance trace names the dormant Nightjar circuit and gives a bearing into the north forest.

Story turn: the storm caused damage, but someone prepared the failure before the storm arrived.

### Act II — Following the stolen line (screens 25–50)

Iris follows clipped cable markers through the forest, rescues ranger Theo Gray, speaks to lookout Nell Harker, crosses the service ravine, and enters an abandoned quarry used by Voss's crew as a staging point. She recovers Nightjar's red phase coil and a survey notebook linking the false surveyors to the ridge station.

Story turn: Voss is not merely jamming the relay; he is rebuilding the Quiet Field.

### Act III — The mountain's working memory (screens 51–90)

The direct ridge road has collapsed. Iris helps mechanic Lila Mercer restart an old logging engine, reaches the dam, prevents a deliberately induced flood, drains a mine passage, restores an underground substation, and brings the freight lift back online. These regions show that Black Pine is a connected machine: timber railway, reservoir, mine power, and radio station were all reused by Nightjar.

Story turn: an archive fragment reveals that the field's midnight pulse may bring down Kestrel Six's emergency beacon permanently.

### Act IV — Nightjar wakes (screens 91–115)

Iris infiltrates the ridge observatory, decodes Ruth Calder's archive, confronts electronics specialist Sable Dunn, and enters the underground Nightjar bunker. Scientist Miriam Kline, coerced into recalibrating the device, explains how to invert the field. Iris disables its cooling and phase-lock systems, recovers a complete evidence spool, and frees Kline. Voss diverts the remaining charge into the summit tower and retreats upward.

Story turn: the tower Iris originally came to repair is both the last threat and the only transmitter powerful enough to call the rescue aircraft.

### Act V — The open channel (screens 116–124)

Iris climbs through the storm, restores the tower grounding path, installs the recovered components, aligns the antenna, and faces Voss in the summit control capsule. By broadcasting Calder's protected-carrier sequence, she collapses the Quiet Field without destroying the relay. If she also uploads the evidence spool, Voss's confession and the project archive go out with the rescue call.

Final image: the storm moves east, the tower beacon changes from red to green, and distant voices answer one by one.

## 5. Main cast

| Character | Role and personality | Arc and gameplay function |
|---|---|---|
| **Iris Bell** | Player character; a practical volunteer radio technician in her late twenties. Observant, understated, and uncomfortable with heroics. | Begins treating the outage as a repair job and ends choosing to expose Nightjar rather than quietly restoring the system. Her spoken lines are short and anchored beside her sprite. |
| **Mara Venn** | Black Pine's caretaker; blunt, warm, and deeply familiar with the relay site. | Gives the initial repair context, unlocks travel information, identifies Nightjar terminology, and provides changing radio advice after power returns. |
| **Theo Gray** | Young ranger injured while following the false survey crew. | His rescue opens the ranger cache, supplies the quarry route, and establishes that the sabotage preceded the storm. Optional later radio check-ins improve the epilogue. |
| **Nell Harker** | Fire lookout who refuses to leave her post. Wry, patient, and excellent with landmarks. | Confirms the quarry movement, lends binoculars narratively, marks remote map anchors, and reports Kestrel Six's intermittent beacon. |
| **Owen Finch** | Retired quarry foreman held in the office by Voss's crew. | Explains the hoist, gives the pulley pin, and identifies the red phase coil as equipment carried through the quarry. |
| **Lila Mercer** | Mechanic at the abandoned logging camp, stranded while salvaging the engine. | Turns the logging railway into a cooperative multi-part repair rather than a solitary lock puzzle. She later relays messages between Mara and the dam. |
| **June Mercer** | Lila's aunt and former camp cook, now a local historian. | Supplies food, the mill whistle plan, and history connecting Nightjar to the railway and mine. Her optional stories reward patient conversation. |
| **Jonah Reed** | Dam operator trapped by a jammed spillway mechanism. Methodical even under pressure. | Guides the water-control puzzle by intercom, then gives access to the east shaft and verifies that the flood command came from Nightjar. |
| **Dr. Miriam Kline** | Atmospheric physicist coerced by Voss. Calm, precise, and angry at her own earlier silence. | Explains the inversion plan, supplies the emergency override key, and changes the final solution from destructive overload to controlled broadcast. |
| **Ruth Calder** | Deceased Nightjar systems engineer, heard in archive recordings. | Her archived warnings, cipher, and protected-carrier sequence form the ethical and technical spine of the mystery. |
| **Gideon Voss** | Former Nightjar chief engineer and principal antagonist. Charismatic, proud, and convinced that a successful demonstration will vindicate him. | Speaks through radios and intercoms before appearing in person. He continually frames harm as a temporary engineering cost. The player defeats his argument as well as his machine. |
| **Sable Dunn** | Voss's electronics specialist. Competent, guarded, and increasingly disturbed by the consequences. | Maintains the jammer, patrols the observatory, and can be persuaded to stop helping Voss if Iris has Calder's recording and Theo's evidence. |
| **Brant Cole** | Voss's field enforcer. Impatient and more interested in payment than Nightjar. | A visible patrol threat at the quarry and rail trestle. He is removed from play through distraction and an empty lift-cage lock, never combat. |
| **Kade and Morrow** | Two hired survey guards represented as paired patrol sprites. | Create predictable stealth windows in the observatory courtyard and bunker corridor. They surrender after Voss abandons them. |
| **Elias Ward** | Regional emergency dispatcher, initially heard only through fragments of static. | His increasingly clear calls measure progress. He coordinates Kestrel Six and delivers the first answer after the final broadcast. |
| **Cass Holt** | Pilot of Kestrel Six. | Heard in the finale. Her weak beacon provides the human deadline; her final response confirms that the rescue route is open. |

## 6. Adversaries, hazards, and failure rules

Black Pine has no combat system. An “enemy” is a moving or stateful obstacle with readable behaviour.

| Threat | Screens | Behaviour and solution | Failure consequence |
|---|---:|---|---|
| Fallen live cable | 4, 21 | Blue-white arc every twelve ticks; cross only after transformer isolation and while wearing lineman gloves. | Electrocution death card. |
| Black bear | 35 | Alternates sniffing and blocking poses. Observe the wind ribbon, then use the ranger flare from upwind; the bear leaves unharmed. | A warning retreat first; repeated approach causes a non-graphic mauling death. |
| Flooded ravine | 40–44 | Water level and stepping-stone timing change after rain pulses. Anchor rope before descending. | Drowning death card. |
| Brant Cole | 47, 62 | Walks a fixed three-position patrol. Crusher horn or mill whistle draws him into an area Iris can lock behind him. | Iris is caught and expelled to the last safe anchor; no death. |
| Unstable crusher | 47 | Moving belt and descending jaw are active only after power diversion. Use the guarded catwalk, never cross the belt. | Crushing death card. |
| Rotten trestle | 62 | Boards flex in a visible three-frame cycle. The rail engine may cross only after the brake and switch are fixed. | Fall death card. |
| Electrified floodwater | 69–72 | Sparks pulse across the surface. Isolate the turbine circuit and wear insulated boots for the remaining shallow water. | Electrocution death card. |
| Mine gas | 79–83 | Lamp flame shrinks and screen tint changes over several ticks. A fitted charcoal respirator makes the route safe. | Suffocation death card. |
| Falling rock | 78, 117 | Dust motes and pebbles warn before a scripted fall. Use the marked shelter or brace point. | Fatal-fall death card. |
| Kade and Morrow | 92, 105 | Paired patrol with a pause at each turn. Lights, intercom sounds, and doors can redirect them. | Iris is detained in screen 113 and must escape; repeated failure can return to a safe anchor. |
| Capacitor discharge | 110 | Three lamps telegraph the firing order. Ground each bank before crossing. | Electrical-burn death card. |
| Coolant steam | 111 | Valve animation alternates safe and unsafe lanes until the replacement hose is fitted. | Burn death card. |
| Lightning | 116–123 | Flash, one-tick pause, then strike at ungrounded metal. Restoring the grounding chain makes the final climb safe. | Lightning death card. |
| Voss | 124 | A dialogue and console-control adversary, not a physical boss. Evidence, timing, and the protected carrier remove his control. | An incorrect console sequence resets the puzzle; exhausting the pulse timer produces the “Long Silence” failure card and restarts at screen 119. |

All lethal hazards have a warning state, a readable animation, and a recent travel anchor. No irreversible failure can arise from consuming an item in the wrong place.

## 7. World layout and screen budget

| Region | Screens | Count | Primary purpose |
|---|---:|---:|---|
| Trailhead and caretaker grounds | 1–12 | 12 | Tutorial, characters, basic tools, first clues |
| Relay yard and local power | 13–24 | 12 | First multi-stage repair and mystery reveal |
| North forest | 25–38 | 14 | Navigation, rescue, wildlife, direction finding |
| Service ravine and quarry | 39–50 | 12 | Rope traversal, first human adversaries, stolen coil |
| Logging camp and railway | 51–63 | 13 | Character-driven machinery repair and transport |
| Reservoir and pumpworks | 64–75 | 12 | Water-state puzzle and access to the mine |
| Mine and underground substation | 76–90 | 15 | Environmental survival, power routing, freight lift |
| Ridge observatory | 91–103 | 13 | Infiltration, archive mystery, Sable confrontation |
| Nightjar bunker | 104–115 | 12 | System shutdown, rescue, evidence, antagonist reveal |
| Summit and tower | 116–124 | 9 | Storm climb, final assembly, confrontation and broadcast |
| **Total** | **1–124** | **124** | Exactly matches the numbered gameplay-screen count of *Tajná mise* |

Travel-map anchors are screens 1, 7, 15, 24, 31, 38, 46, 52, 64, 67, 76, 86, 92, 101, 104, 113, and 119. An anchor appears only after Iris physically reaches it or receives a trusted route over the restored radio.

## 8. Complete 124-screen directory

The “motion/threat” column specifies intentional animation. A screen with “none” still animates Iris and dialogue bubbles, but its scenery remains still.

### Region A — Trailhead and caretaker grounds (1–12)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 001 | `storm_gate_trailhead` — Storm Gate Trailhead | Washed gravel turnout; east to 2. Later map anchor. | Opening arrival. TAKE weatherproof patch cable and folded field note from the damaged toolbox. EXAMINE the locked emergency phone and distant dark tower. | Rain drip from sign, swaying chain, one distant lightning flash. |
| 002 | `ranger_noticeboard` — Ranger Noticeboard | Roofed board; west 1, uphill 3. | Missing-aircraft notice establishes Kestrel Six. TAKE optional carved pine bird. The map names the cabin, relay, quarry, dam, and lookout but not Nightjar. | Loose notice corner lifts every sixteen ticks. |
| 003 | `lower_switchback` — Lower Switchback | Muddy zigzag; downhill 2, uphill 4, narrow deer path to 5. | Footprints from heavy survey boots predate the storm. Tire groove is too narrow for local ranger trucks. | Runoff crosses path; two-frame bootprint glint when examined. |
| 004 | `upper_switchback` — Upper Switchback | Exposed slope; down 3, east 6 after safe crossing. | A fallen live line blocks the direct path until screen 21 is isolated. The deer-path detour through 5 remains available. | Telegraph wire arcs in a clear periodic pattern; lethal if touched. |
| 005 | `pine_hollow_footbridge` — Pine Hollow Footbridge | Timber footbridge; west 3, east 6. | Safe early detour. EXAMINE a freshly cut black cable tie bearing a triangular survey logo. | Fast creek, bobbing branch, bridge drip; no timed crossing. |
| 006 | `caretaker_cabin_exterior` — Caretaker Cabin Exterior | Cabin, stacked wood, aerial mast; west 5/4, inside 7, shed path 9. | First sight of Mara through the window. The porch bell produces only a dull click until local power returns. | Chimney smoke, gutter overflow, window silhouette. |
| 007 | `caretaker_cabin_main` — Caretaker Cabin | Warm timber room; outside 6, radio nook 8, cellar 10. Travel anchor. | TALK to Mara. EXAMINE desk after her hint to reveal brass yard key. TAKE the key. Her dialogue changes after every act. | Stove flame, clock pendulum, Mara idle/head-turn and speech poses. |
| 008 | `cabin_radio_nook` — Radio Nook | Small equipment alcove; back to 7. | EXAMINE static spectrum. Before screen 24 it gives only a pulse; afterward it becomes the long-range conversation hub. TAKE Mara's annotated site map after the briefing. | Oscilloscope trace, speaker cone, status lamp states. |
| 009 | `caretaker_tool_shed` — Tool Shed | Cluttered lean-to; back 6, weather path 11. | TAKE 17 mm wrench, lineman gloves, and pruning saw. EXAMINE empty hooks to learn a climbing rope was lent to the ranger cache. | Hanging tools rock once when door opens; dust motes. |
| 010 | `cabin_root_cellar` — Root Cellar | Stone cellar; stairs to 7, low hatch to 11 after light obtained. | TAKE ceramic fuse and hand-crank torch. A faded Nightjar crate label is the first use of that name. | Torch beam cone follows Iris after pickup; dripping pipe. |
| 011 | `weather_mast_clearing` — Weather Mast Clearing | Mast behind cabin; west 9, cellar hatch 10, east 12. | After local power returns, USE the multimeter's test leads to calibrate the direction finder; collect the bearing used at Echo Grove. EXAMINE clipped sensor lead proving tool use. | Cups spin at wind-dependent speed; vane turns during calibration. |
| 012 | `old_service_road_fork` — Old Service Road Fork | South 11/6, east relay perimeter 13, north forest 25 after screen 24. | A locked forestry barrier opens with Mara's site map code only after the jammer bearing is known. Signposts foreshadow all major regions. | Barrier arm rises; puddle rings; no enemy. |

### Region B — Relay yard and local power (13–24)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 013 | `relay_perimeter` — Relay Perimeter | Chain fence; west 12, east 14. | EXAMINE cut outer alarm wire. The cut is clean and wrapped against rain, confirming preparation. | Fence shivers in gusts; loose warning plate taps. |
| 014 | `vehicle_gate` — Vehicle Gate | Locked gate; west 13, east 15 when open. | USE brass yard key. Key remains in inventory because it later opens the old workshop cabinet. | Player unlock pose; gate swings in four frames. |
| 015 | `relay_yard_west` — Relay Yard West | Antenna frames; west 14, east 16, trench 17, hall 23. Travel anchor. | Survey bootprints lead toward the cable trench. EXAMINE stripped lightning-ground braid. | Small dish twitches against a damaged stop; rain. |
| 016 | `relay_yard_east` — Relay Yard East | Transformer and fuel piping; west 15, shed 18, pump 20, pad 21. | Central routing screen. Labels teach generator → transformer → control-room dependency. | Wind sock, leaking fuel-pipe drip before repair. |
| 017 | `cable_trench` — Cable Trench | Open maintenance trench; ladder to 15. | USE weatherproof patch cable on blue terminals. Cable is consumed and remains visibly installed. Find a deliberately removed factory jumper. | Player kneel pose; two contact sparks, then steady blue link. |
| 018 | `generator_shed` — Generator Shed | Diesel set; outside 16, battery room 19, workshop 22. | USE ceramic fuse in main holder. Generator will not latch until fuel pump and transformer are ready. Main lever becomes contextual action. | Flywheel cough, exhaust puffs, belt blur after successful start. |
| 019 | `battery_room` — Battery Room | Acid-stained cells; back 18. | Reconnect a loose bus link using wrench. EXAMINE footprints that avoid the acid stain and lead to a missing diagnostic tape. | Charge bubbles and meter needle after connection. |
| 020 | `fuel_pump_alcove` — Fuel Pump Alcove | Covered pump; back 16. | USE wrench to open seized supply valve. TAKE empty fuel-siphon hose for later logging-engine repair. | Valve-turn action; fuel sight glass rises. |
| 021 | `transformer_pad` — Transformer Pad | Outdoor transformer; back 16, maintenance path rejoins 4. | Wear lineman gloves and contextually isolate the fallen feeder, making screen 4 safe. Reset transformer after generator runs. | Predictable arc stops; ceramic switches move; green lamp appears. |
| 022 | `relay_workshop` — Relay Workshop | Benches and locked cabinet; back 18. | USE brass key on old cabinet. TAKE multimeter and optional enamel relay badge. Calder's initials are scratched beneath the meter. | Cabinet-door and pickup animation; otherwise static. |
| 023 | `lower_relay_hall` — Lower Relay Hall | Cable racks; yard 15, stairs to 24. | Use multimeter to identify a signal present on the disconnected Nightjar trunk even while local power is off. | Sequential rack lamps after generator start. |
| 024 | `local_control_room` — Local Control Room | Windows over yard; down 23. Travel anchor. | Complete act-one console puzzle: generator, trench, transformer, then direction trace. Receiver remains jammed. The console prints `NIGHTJAR QUIET FIELD / BEARING 017`. Unlock north barrier at 12. | CRT scan, bearing needle, printer paper feed, first clear Voss pulse. |

### Region C — North forest (25–38)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 025 | `north_service_road` — North Service Road | Overgrown road; south 12, north 26. | First forest marker matches bearing 017. TAKE a discarded survey ribbon as evidence, recorded but not carried. | Wet branches sway; Iris brushes foliage. |
| 026 | `burned_pine_stand` — Burned Pine Stand | Black trunks; south 25, east 27, west loop 32. | EXAMINE a cable dragged across old ash. Hidden boot cache contains ranger bandage roll. | Ash lifts in gusts; distant tree falls harmlessly. |
| 027 | `fallen_fir` — Fallen Fir | Huge trunk across road; west 26, east 28 after clearing. | USE pruning saw. The cut section becomes a permanent step, preserving visible state. | Sawing pose, chips, trunk segment rolls once. |
| 028 | `cold_creek_crossing` — Cold Creek Crossing | Stones over creek; west 27, north 29, east 33. | Water has washed away the official marker; compass bearing is needed later, not to cross. EXAMINE red fabric caught downstream from Theo. | Water and stepping splash. |
| 029 | `hunters_blind` — Hunter's Blind | Raised blind; south 28, east 30. | Find Theo's broken radio and a written call sign. TAKE signal flare from emergency box. | Shutter knocks; radio emits one weak pulse after local relay power. |
| 030 | `mossy_hollow` — Mossy Hollow | Fern hollow; west 29, north 31. | Theo is pinned by a light fallen branch. USE saw, then bandage roll. Conversation reveals false surveyors and quarry shortcut. | Theo breathing/hand signal; branch lift and bandage sequence. |
| 031 | `ranger_cache` — Ranger Cache | Small locked hut; south 30, east 36. Travel anchor after Theo rescue. | Theo gives combination. TAKE climbing rope, iron hook, mine lamp, and his compass. TAKE optional Nightjar-era ranger patch. | Door opens; cache shelves highlight one by one. |
| 032 | `charcoal_kiln_ruin` — Charcoal Kiln Ruin | Stone kiln; east 26, north 33. | TAKE clean hardwood charcoal for later respirator filter. Calder's survey mark points to Echo Grove. | Thin residual smoke; a fox crosses once, harmless. |
| 033 | `echo_grove` — Echo Grove | Similar pines form navigation puzzle; west 32, south 28, exits change by chosen bearing. | Use Theo's compass and mast bearing 017 to choose north-east, north, then east. Wrong choices loop to this screen with distinct landmarks. | Compass needle settles; echo birds indicate wrong turn. |
| 034 | `buried_cable_ridge` — Buried Cable Ridge | Ridge with concrete posts; west 33, east 35. | Use multimeter at three posts to follow live Nightjar leakage. Discover the signal grows stronger toward quarry, not tower. | Meter needle and underground pulse glow. |
| 035 | `bear_meadow` — Bear Meadow | Berry clearing; west 34, east 36 after bear leaves. | Observe wind ribbon, move to upwind edge, USE signal flare. Incorrect approach triggers retreat warning. | Bear sniff/scratch/block cycle; flare and calm departure. |
| 036 | `firebreak_junction` — Firebreak Junction | Four-way cut; west 35/31, south 37, north 38, east ravine 39 after lookout marked. | Route sign has been turned. Nell's map mark or careful moss examination reveals correct ravine path. | Sign rotates when corrected; clouds move faster. |
| 037 | `automatic_weather_station` — Automatic Weather Station | Short south spur from 36. | Connect the hand-crank torch's charging lead and crank it to read stored 02:11 wind data: sabotage occurred six minutes before the storm front. Nothing is consumed. | Chart drum advances; anemometer; data printout. |
| 038 | `north_fire_lookout` — North Fire Lookout | Tower cabin; south 36. Travel anchor. | TALK to Nell. Through binocular view she identifies Voss's crew at quarry, a working hoist, and Kestrel Six's weak beacon. Ravine route opens. | Distant quarry lights, sweeping binocular mask, Nell radio poses. |

### Region D — Service ravine and quarry (39–50)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 039 | `ravine_west_lip` — Service Ravine West Lip | Cliff edge; west 36, down 41 only after rope, bridge 40. | Attach iron hook to anchor eye, then USE climbing rope. The two objects become a visible fixed line and no longer occupy inventory. | Rope throw, hook bounce, taut-line sway. |
| 040 | `broken_service_bridge` — Broken Service Bridge | Collapsed span; west 39, east 44 only after quarry hoist later. | Early examination shows crossing is impossible. Later, activating the quarry hoist pulls a replacement cable walkway into place. | Flood surge below, loose deck boards, later cable-walkway deployment. |
| 041 | `ravine_floor_west` — Ravine Floor West | Narrow gravel shelf; up 39, east 42. | TAKE optional old relay badge from silt. Follow red survey paint. | Water-level pulse and hanging rope sway. |
| 042 | `culvert_mouth` — Culvert Mouth | Dark pipe; west 41, inside/east 43. | Use hand-crank torch. A magnetic survey case is visible behind bars but retrieved later with magnet-on-cord. | Moving torch cone, rats flee once, water reflections. |
| 043 | `waterfall_shelf` — Waterfall Shelf | Behind waterfall; west 42, east 44. | Time movement only after closing a small sluice with wrench; otherwise current pushes Iris safely back. Find quarry office key wedged in grate. | Water sheet, valve action, reduced-flow state. |
| 044 | `ravine_floor_east` — Ravine Floor East | Wider shelf; west 43, up quarry gate 45. | Footprints and a dragged crate lead upward. EXAMINE warning plaque tying quarry hoist to old mine railway. | Pebble fall warns of unstable wall; no lethal zone on main path. |
| 045 | `quarry_gate` — Quarry Gate | Rust gate; down 44, inside 46 after unlock. | USE quarry office key. Voss speaks over a field radio for the first time, calling Iris “the caretaker's apprentice.” | Gate chain drops; radio lamp; silhouetted guard crosses background. |
| 046 | `quarry_office` — Quarry Office | Glazed hut; gate 45, crusher 47. Travel anchor. | Free Owen Finch from a locked store closet. TAKE pulley pin and read hoist plan. Owen identifies the crew and provides the crusher-horn distraction. | Owen knock before rescue; map drawer; horn cord. |
| 047 | `crusher_deck` — Crusher Deck | Conveyor and catwalk; west 46, magazine 48, tunnel 49. | Brant patrols. Pull crusher horn from office window, wait until he enters inspection cage, then lock it with brass key. Crossing active belt is lethal. | Three-position patrol, belt, jaw, dust, cage-door closure. |
| 048 | `quarry_magazine` — Equipment Magazine | Dry stone store; back 47. | TAKE red phase coil, surveyor's notebook, and fuel-siphon hose if it was missed at 20. TAKE optional blue quartz sample. No explosives are usable or collectible. | Coil emits slow red pulse; notebook page flutter. |
| 049 | `quarry_tunnel` — Quarry Tunnel | Curved rail tunnel; west 47, east 50. | Use mine lamp. Repair a broken signal wire with multimeter guidance to power hoist controls. | Cart shadow illusion, lamp cone, signal changes red to green. |
| 050 | `east_hoist_landing` — East Hoist Landing | Hoist over ravine; west 49, cable walkway to 40, logging road 51. | USE pulley pin, then contextually run hoist. It deploys bridge cable and lowers a service cage, creating two permanent shortcuts. | Full hoist, wheel, cable and cage sequence; act transition cue. |

### Region E — Logging camp and railway (51–63)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 051 | `logging_road` — Abandoned Logging Road | East from 50, camp 52. | Fresh truck tracks end abruptly where the road is washed out. Find a dropped Nightjar bolt matching the red coil. | Rain weakens; water runs through ruts. |
| 052 | `sawmill_yard` — Sawmill Yard | West 51, mill 53, bunkhouse 57, mess 58, rail 60. Travel anchor. | Meet Lila Mercer. She can restart the logging engine if Iris finds oil, spark plug, drive belt, fuel, and rail key. | Lila repair poses, windmill vane, idle engine smoke later. |
| 053 | `sawmill_floor` — Sawmill Floor | Yard 52, filing room 54, boiler 55, pond 56. | Conveyor is mechanically locked. EXAMINE cut belts; TAKE usable drive belt from idle planer after releasing tension with wrench. | Overhead shaft coasts to stop; belt-removal action. |
| 054 | `saw_filing_room` — Saw Filing Room | Back 53. | TAKE oil can and hand mirror. Optional files explain Calder once inspected the rail grounding system. | Grinder wheel spins briefly when tested; harmless sparks. |
| 055 | `boiler_house` — Boiler House | Back 53. | Use fuel-siphon hose on protected reserve tank to fill engine can. Hose is retained as a general tool; fuel becomes a counted canister state, not a new inventory icon. | Fuel level, hose sag, boiler pressure needle. |
| 056 | `log_pond` — Log Pond | South door from 53. | Retrieve spark plug from a floating maintenance box by operating log pike mechanism; stepping into pond is a drowning hazard. | Logs drift in loop, box approaches, water ripple. |
| 057 | `workers_bunkhouse` — Workers' Bunkhouse | Yard 52. | Find rail switch key in foreman's boot after clue from June. TAKE optional logger token. Beds contain environmental stories, not random loot. | Curtain flutter; mouse runs once. |
| 058 | `camp_mess_hall` — Mess Hall | Yard 52, office 59. | Talk to June. Receive sealed ration for emergencies and learn the whistle once recalled workers from the trestle. Optional repeated dialogue tells Nightjar history. | Kettle steam, June stirring/turning poses. |
| 059 | `camp_office` — Camp Office | Through 58. | Read rail timetable and Voss's forged survey permit. Use hand mirror to read reversed impression on carbon paper: `RIDGE LIFT / 23:40`. | Desk fan twitches; carbon-page reveal. |
| 060 | `rail_spur_west` — Rail Spur West | Yard 52, engine 61, trestle 62. | Use rail switch key to align points. Incorrect point state prevents engine departure without consuming anything. | Switch lever and points animate. |
| 061 | `derelict_logging_engine` — Logging Engine | On spur 60. | Multi-item repair: drive belt, spark plug, oil, and siphoned fuel. Lila performs final timing adjustment. Context action starts engine. | Detailed belt, piston, smoke, headlamp and Lila sequence. |
| 062 | `trestle_approach` — Trestle Approach | West 60/61, east 63 after safe route. | Brant reappears if not secured at quarry; otherwise Kade guards the trestle. Use mill whistle from screen 52 control cable to draw guard away. Repair brake linkage with wrench before crossing. | Guard patrol, flexing trestle, whistle reaction, engine crossing cutscene. |
| 063 | `east_rail_cut` — East Rail Cut | West 62, reservoir overlook 64. | Engine stops at rockfall. This becomes a travel shortcut after first arrival. Hear first intelligible fragment from Elias Ward on repaired portable radio. | Cooling engine, falling grit, radio waveform. |

### Region F — Reservoir and pumpworks (64–75)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 064 | `dam_overlook` — Black Pine Dam Overlook | Rail cut 63, abutment 65. Travel anchor. | See spillway open despite high reservoir. Jonah's emergency light flashes from gatehouse. Voss is flooding the mine access to erase his route. | Broad water, spray, rotating warning beacon. |
| 065 | `west_abutment` — West Abutment | West 64, spillway 66, turbine stairs 68. | TAKE insulated boots from rescue locker. Find operator's dropped turbine badge on safe side of railing. | Spray gusts; boots pickup; cable conduit hum. |
| 066 | `spillway_walk` — Spillway Walk | West 65, gatehouse 67. | Cross behind timed spray shields. Use wrench to secure one loose shield; sequence becomes safe permanently. | Spillway water, shield timing, rainbow palette cycle limited to EGA bands. |
| 067 | `gatehouse` — Gatehouse | West 66. Travel anchor. | Use turbine badge. Free Jonah by inserting spillway crank and manually closing the false-open command. TALK through door before rescue and in person after. | Crank, gate-position dial, water level falls across distant view. |
| 068 | `turbine_hall_upper` — Turbine Hall Upper | From 65, stairs 69, control door 70. | Badge opens control door. Inspect power diagram linking dam auxiliary feed to underground substation. | Turbine blur below, gantry lamp sweep. |
| 069 | `turbine_hall_lower` — Turbine Hall Lower | Up 68, pump gallery 70. | TAKE pump gasket from service cabinet. Isolate flooded-bay circuit using a three-breaker sequence learned from diagram. | Large shaft, meter needles, arc stops after isolation. |
| 070 | `pump_gallery` — Pump Gallery | Upper 68/lower 69, bay 71, intake 72. | Install pump gasket and dry-cell battery in emergency starter. Pump still needs intake valve at 74. | Pump cough/prime loop, battery lamp. |
| 071 | `flooded_maintenance_bay` — Flooded Maintenance Bay | From 70; exit to 75 after drain. | Initially electrified and deep. After isolation, pump, and valve, water recedes. Wear insulated boots to retrieve magnet-on-cord from shallow locker. | Four persistent water levels, floating debris, residual spark. |
| 072 | `intake_tunnel` — Intake Tunnel | From 70, shore 73. | Use hand-crank torch. TAKE detachable valve wheel from a redundant bypass. A wall chalk message from Kline reads `THE FIELD FOLLOWS THE CARRIER`. | Water reflections, bats once, valve removal. |
| 073 | `reservoir_shore` — Reservoir Shore | Tunnel 72, valve garden 74. | Find Kline's broken glasses and Voss crew bootprints. Optional calm screen with Kestrel beacon audible at certain ticks. | Waves, reeds, distant beacon flash. |
| 074 | `valve_garden` — Valve Garden | Shore 73, tunnel 72. | Install valve wheel and open pump intake. Wrong direction produces pressure warning; diagram at 68 provides answer. | Linked wheel and pipe-pressure animation. |
| 075 | `east_access_shaft` — East Access Shaft | From drained 71, ladder down 76. | Jonah opens grille after flood is controlled. He gives dry-cell replacement if earlier one was missed and warns of mine gas. | Grille lift, water drip, descending lamp transition. |

### Region G — Mine and underground substation (76–90)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 076 | `ore_cart_chamber` — Ore Cart Chamber | Shaft 75, gallery 77. Travel anchor. | TAKE respirator mask body from emergency cabinet. Mine map case is empty. Cart can later become shortcut to 83. | Dripping shaft, cart rock, lamp flicker. |
| 077 | `timber_gallery` — Timber Gallery | West 76, east 78, vent spur 79. | Read timber marks to identify safe brace. Use wrench on loose brace before passing screen 78. | Timber creak; dust pulse warns of instability. |
| 078 | `collapsed_drift` — Collapsed Drift | West 77, east 80 after brace. | Clear only small stones with hands; no implausible excavation. Correct brace prevents scripted rockfall. Find cable route behind collapse. | Pebbles, brace flex, one controlled fall. |
| 079 | `ventilation_room` — Ventilation Room | Spur from 77, connects 81 after fan works. | Pack charcoal from screen 32 into respirator filter housing and fit it to mask. Repair fan starter using multimeter; ventilation opens secondary route. | Fan accelerates, dust clears, mask assembly pose. |
| 080 | `copper_vein` — Copper Vein | West 78, east 81. | Gas zone requires fitted respirator. TAKE cut copper bus bar abandoned beside survey drill. Optional blue quartz vein records collectible if sampled with loose shard. | Lamp flame shrinks without mask; gas tint; mineral glint. |
| 081 | `mine_pump_station` — Mine Pump Station | West 80, north 79, east flooded drift 82. | TAKE magnet-on-cord if missed at 71 from pump mechanic's locker. Restore local drainage with valve-wheel knowledge, reducing 82 current. | Reciprocating pump, pipe shudder. |
| 082 | `flooded_drift` — Flooded Drift | West 81, east 83 after safe retrieval. | Use magnet-on-cord to retrieve lift fuse from submerged grate. Boots protect from remaining shallow current after substation isolation. | Magnet swing, water wake, fuse emergence. |
| 083 | `survey_chamber` — Survey Chamber | West 82, cart route 76, lift 84. | TAKE mine map, research badge, and punched code card from Voss's abandoned survey desk. Notebook from 48 explains badge belonged to Kline. | Map unfolds; ore cart shortcut rolls in and stops. |
| 084 | `freight_lift_bottom` — Freight Lift Bottom | West 83, cage to 85 after power. | Install lift fuse. Lift remains dead until underground substation screens 86–88 are repaired via maintenance crawl. | Relay click, cage lamp, later lift travel. |
| 085 | `freight_lift_top` — Freight Lift Top | Cage to 84, substation 86, final lift 90 after repair. | Intermediate landing shows Nightjar cable entering older mine power system. Kade's dropped radio carries Voss order to flood the shaft. | Cage doors, radio lamp, cable pulse. |
| 086 | `underground_substation` — Underground Substation | West 85, aisles 87/88. Travel anchor. | Main routing board. Install copper bus bar only after grounding switch at 87. Power must feed lift, not Quiet Field trunk. | Large knife switches, transformer hum, labelled lamp states. |
| 087 | `switchgear_aisle` — Switchgear Aisle | From 86. | Solve safe isolation order using multimeter and Calder's scratched arrows. Wrong order trips breakers without death. | Breaker handles, controlled spark, inspection light. |
| 088 | `cable_vault` — Cable Vault | From 86, sealed door route 89. | Use wrench to disconnect black Quiet Field feed, then install copper bus bar in lift circuit. This weakens Voss's system and powers 84/90. | Thick cable pulse fades; lift lamp turns green. |
| 089 | `sealed_research_door` — Sealed Research Door | Between 88 and 90. | Present research badge and punched card. Card alone gives `CALDER ACCESS REVOKED`; notebook clue tells Iris to turn it upside down, exposing Kline's emergency code. | Card reader text, lock bolts retract. |
| 090 | `ridge_freight_lift` — Ridge Freight Lift | From 89/85 to observatory 91. | Context action starts a long but skippable ascent. Voss addresses Iris over intercom and offers safe passage if she leaves the phase coil. Refusal is automatic; player retains it. | Counterweight, passing strata, intercom portrait-free bubble. |

### Region H — Ridge observatory (91–103)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 091 | `freight_lift_lobby` — Ridge Lift Lobby | Lift 90, courtyard 92, service corridor 96. | First Nightjar interior. Research badge opens staff route; public door is chained. A camera tracks Iris but can be blinded with hand mirror. | Camera sweep, mirror flash, lift indicator. |
| 092 | `ridge_courtyard` — Ridge Courtyard | Lobby 91, dormitory 93, kitchen 94, infirmary 95, lab 98. Travel anchor. | Kade and Morrow patrol. Fog-horn control from screen 100 or kitchen timer from 94 creates crossing window. | Paired patrol, searchlights, windswept snow/rain mix. |
| 093 | `observatory_dormitory` — Observatory Dormitory | From 92, archive hall 96. | Search Voss's temporary bunk to find notes showing midnight buyer call. TAKE optional Nightjar cloth patch. | Locker door, hanging coat sway. |
| 094 | `observatory_kitchen` — Observatory Kitchen | From 92. | Set mechanical kitchen timer, place sealed ration by back door, and draw one guard away without harming him. Find staff memo about instrument-dome key. | Timer hand, kettle, guard silhouette at door. |
| 095 | `observatory_infirmary` — Observatory Infirmary | From 92, hidden observation grille to 113 discovered later. | Empty but recently used. Find Kline's medical chart and recording addressed to “whoever restores the carrier.” TAKE first-aid kit for optional rescue state. | Tape reels, heartbeat lamp only during recording. |
| 096 | `archive_hall` — Archive Hall | Dorm 93/lobby 91, records 97, security 102. | Portraits and project dates establish Calder/Voss conflict. A display case holds magnetic archive reel behind a puzzle lock. | Fluorescent flicker, display carousel. |
| 097 | `records_room` — Records Room | From 96. | TAKE cipher lens and magnetic archive reel after aligning project dates on four drawers. Use lens over punched card to reveal `RUTH / OPEN CHANNEL`. | Drawer slides, lens overlay, reel rotation. |
| 098 | `weather_lab` — Weather Lab | From 92, dome 99, comm lab 101. | TAKE phase prism from Voss calibration rig. Weather data proves storm was natural but deliberately exploited. Kline speaks weakly through ventilation duct. | Barographs, prism colour stepping, rain radar sweep. |
| 099 | `instrument_dome` — Instrument Dome | Lab 98, telescope platform 100. | Dome locked until key from 102. Once open, TAKE calibration fork and align dome slit to north to power archive reader. | Dome rotation, slit light, fork vibration. |
| 100 | `telescope_platform` — Telescope Platform | From 99. | Aim old sight at three landmarks using Nell's directions. This confirms tower azimuth and activates fog horn that redirects courtyard patrol. Record antenna alignment chart automatically in notebook. | Telescope pan, fog-horn shutters, distant tower lightning. |
| 101 | `communications_lab` — Communications Lab | From 98, antechamber 103. Travel anchor after Sable encounter. | Confront Sable. With Calder recording plus survey notebook, expose Voss's lie about Kestrel Six. Sable disables her jammer rack and gives location of Kline; otherwise she flees and leaves clues. | Rack waterfall lights, Sable console poses, jammer pulse stopping. |
| 102 | `security_office` — Security Office | From 96. | Use hand mirror through pass-through to read keypad, or obtain code from persuaded Sable. TAKE weather-dome key and inspect patrol controls. | CCTV screens cycle, lock drawer opens. |
| 103 | `nightjar_antechamber` — Nightjar Antechamber | Comm lab 101, bunker decon 104. | Assemble access proof: research badge, Calder phrase revealed by cipher lens, and calibration fork tone. Heavy bunker door opens. | Three locks respond in sequence; door irises apart. |

### Region I — Nightjar bunker (104–115)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 104 | `decontamination_hall` — Decontamination Hall | Antechamber 103, corridor 105. Travel anchor. | Decon controls are repurposed as security. Use badge and wait through harmless air cycle; forcing door triggers guards. | Fans, warning lamps, air jets, two-door interlock. |
| 105 | `main_bunker_corridor` — Main Bunker Corridor | Hub to 106, 109, 110, 112, 113, 114. | Kade/Morrow patrol if not redirected. Use intercom playback from archive reel to call them to decon and seal them there. | Patrol loop, ceiling lamps, intercom response. |
| 106 | `phase_lab` — Phase Laboratory | From 105, calibration 107, test cell 108. | Install recovered red phase coil and phase prism in diagnostic rig—not the live machine—to calculate inversion values. Both remain recoverable for summit use. | Concentric phase rings, coil pulse, plotted waveform. |
| 107 | `calibration_chamber` — Calibration Chamber | From 106. | Strike calibration fork in three marked positions. Use cipher-lens colours to choose order. Produces protected-carrier sequence `4-1-3`. | Fork arcs, oscillograph, standing-wave line animation. |
| 108 | `quiet_field_test_cell` — Quiet Field Test Cell | From 106, observation link to 113. | Safe history puzzle. Play Calder reel; her recording explains why she shut Nightjar down. Observe Kline through window and exchange signs before rescue. | Sound-wave bands collapse around silent bell; Kline silhouette. |
| 109 | `bunker_machine_shop` — Machine Shop | From 105. | TAKE coolant hose and grounding clamp. Use wrench to free both from seized rack. TAKE optional Ruth Calder photograph. | Drill press coasts, clamp pickup, photo reveal. |
| 110 | `capacitor_hall` — Capacitor Hall | From 105, cooling 111. | Three charged banks block passage. Use grounding clamp according to 4-1-3 sequence. Clamp remains installed until retrieved on return. | Lamps telegraph discharge; large safe grounding arcs. |
| 111 | `cooling_gallery` — Cooling Gallery | Through 110, maintenance loop to 112. | Replace split coolant hose, then divert cooling away from live Quiet Field into emergency dump. This forces Voss to move remaining charge to summit. | Steam lanes, hose fitting, coolant gauges fall. |
| 112 | `command_archive` — Command Archive | Corridor 105/cooling 111. | Use magnetic archive reel and cipher lens to copy complete evidence spool. Optional upload route becomes available in finale. Voss appears on monitor and admits the demonstration. | Reel-to-reel transfer, printer, Voss monochrome monitor portrait. |
| 113 | `holding_room` — Holding Room | Corridor 105, observation route 108, emergency stair 114. Travel anchor after rescue. | Open with punched emergency code. Free Miriam Kline, use first-aid kit optionally, and receive emergency override key. Full inversion plan is delivered here. | Door release, Kline stand/support poses, emergency map illumination. |
| 114 | `emergency_stair` — Emergency Stair | Holding room 113/corridor 105, summit lock 115. | Voss cuts bunker lights and retreats. Use hand-crank torch; Sable, if persuaded, holds lower door against guards. Retrieve grounding clamp if left in 110 through shortcut. | Red emergency lamps, moving torch cone, closing blast door. |
| 115 | `summit_access_lock` — Summit Access Lock | Stair 114, summit stair 116. | Use Kline's override key and protected sequence. System reports 18 minutes to field pulse; this is narrative pressure, not a real-world countdown until final console. | Mechanical tumblers, countdown display, outer door opens into storm. |

### Region J — Summit and tower (116–124)

| # | Screen ID and title | Environment and exits | Story, interaction, and puzzle content | Motion / threat |
|---:|---|---|---|---|
| 116 | `storm_stair_lower` — Storm Stair Lower | Lock 115, ledge 117. | Inspect broken grounding cable. Carry grounding clamp forward; metal stairs are unsafe during telegraphed strikes until repaired at 118. | Heavy rain, lightning warning/strike cycle, cable whip. |
| 117 | `windbreak_ledge` — Windbreak Ledge | Stair 116, gallery 118. | Cross between stone windbreaks. Use rope handline already fixed by maintenance crew; falling-rock warning requires shelter pause. | Strong wind push pose, dust/pebble warning, distant beacon. |
| 118 | `lightning_gallery` — Lightning Gallery | Ledge 117, tower base 119. | Use grounding clamp to bridge broken copper strap, then tighten with wrench. This permanently makes screens 119–123 safe. | Clamp installation, major strike routed visibly into ground, lamps recover. |
| 119 | `tower_base` — Black Pine Tower Base | Gallery 118, ladder 120. Travel anchor. | Meet Sable here if persuaded; she gives Voss's dropped transmitter key. Otherwise retrieve it from his abandoned field case. Insert phase coil in tower feed cabinet. | Tower sway illusion, service lift unavailable, green ground lamp. |
| 120 | `mid_tower_platform` — Mid-Tower Platform | Ladder 119/121. | Use compass to choose sheltered ladder side. Hear Mara and Elias in broken fragments as jammer weakens. | Clouds scroll behind lattice, ladder-climb sequence, cable sway. |
| 121 | `microwave_deck` — Microwave Deck | Ladder 120/122. | Install phase prism in waveguide and tune it with calibration fork. Wrong tuning gives reflected-power warning but no item loss. | Dish increments through angles, fork rings, meter settles. |
| 122 | `beacon_ring` — Beacon Ring | Ladder 121/123. | Recover beacon crystal from cracked lamp housing, clean it with cloth from first-aid kit or shirt sleeve, and reinstall it as protected-carrier reference. | Red beacon rotation becomes steady green pulse. |
| 123 | `antenna_service_platform` — Antenna Service Platform | Ladder 122, capsule 124. | Use antenna alignment chart and wrench to rotate azimuth mount onto north mark. Voss tries to override motor; emergency key locks local control. | Multi-frame antenna swing, player wrench pose, storm begins to clear. |
| 124 | `summit_control_capsule` — Summit Control Capsule | Final enclosed room; back to 123 until pulse starts. | Dialogue confrontation with Voss. Insert transmitter key, enter 4-1-3, optionally load evidence spool, then key protected carrier. Correct timing collapses Quiet Field and restores emergency network. | Voss/console poses, waveform contest, tower-wide green pulse, Kestrel response and victory transition. |

## 9. Collectible item specification

The full design has **64 inventory records**: 56 functional tools, components, documents, or clue records and 8 optional keepsakes. Some remain in the inventory, some are visibly installed in the world, and some are converted into persistent clue flags. No required item can be permanently wasted on an incorrect target.

### Functional tools, components, and clues (56)

| # | Item | Found / received | Purpose and final state |
|---:|---|---:|---|
| 01 | Weatherproof patch cable | 1 | Bridges blue cable-trench terminals at 17; consumed and drawn in place. |
| 02 | Folded field note | 1 | Early repair order and handwriting clue; retained as a readable document. |
| 03 | Brass yard key | 7 | Opens gate 14, cabinet 22, and crusher cage 47; retained. |
| 04 | 17 mm wrench | 9 | Repeated mechanical tool for valves, braces, grounding, and antenna; retained. |
| 05 | Lineman gloves | 9 | Allows safe electrical isolation at 21 and examination of live equipment; retained. |
| 06 | Pruning saw | 9 | Clears tree 27 and frees Theo at 30; retained. |
| 07 | Ceramic fuse | 10 | Installed in generator at 18; consumed and visibly present. |
| 08 | Hand-crank torch | 10 | Lights cellar, culvert, mine, and blackout stair; retained. |
| 09 | Mara's annotated site map | 8 | Unlocks map labels and forestry barrier context; retained. |
| 10 | Multimeter | 22 | Traces Nightjar leakage and solves safe electrical routing; retained. |
| 11 | Fuel-siphon hose | 20, fallback 48 | Transfers reserve fuel at 55; retained after use. |
| 12 | Ranger bandage roll | 26 | Treats Theo at 30; consumed. |
| 13 | Signal flare | 29 | Moves bear safely from screen 35; consumed. |
| 14 | Theo's compass | 31 | Solves Echo Grove and helps on tower; retained. |
| 15 | Climbing rope | 31 | Fixed to ravine anchor at 39; consumed into world state. |
| 16 | Iron hook | 31 | Fixed before rope at 39; consumed into world state. |
| 17 | Mine lamp | 31 | Lights quarry tunnel and signals mine gas; retained. |
| 18 | Clean hardwood charcoal | 32 | Packed into respirator at 79; consumed. |
| 19 | Quarry office key | 43 | Opens quarry gate 45; retained. |
| 20 | Hoist pulley pin | 46 | Repairs hoist at 50; consumed and drawn in machinery. |
| 21 | Red phase coil | 48 | Diagnosed at 106, recovered, and installed in tower feed at 119. |
| 22 | Surveyor's notebook | 48 | Names crew, forged routes, and Voss orders; retained as evidence. |
| 23 | Drive belt | 53 | Installed on logging engine 61; consumed. |
| 24 | Oil can | 54 | Lubricates logging engine and seized mechanisms; empty can remains usable as a prop. |
| 25 | Hand mirror | 54 | Reads reversed carbon clue and blinds tracking camera; retained. |
| 26 | Filled fuel can | 55 | Fuel siphoned from reserve tank; consumed at engine 61. |
| 27 | Spark plug | 56 | Installed in engine 61; consumed. |
| 28 | Rail switch key | 57 | Operates points at 60; retained. |
| 29 | Sealed ration | 58 | Optional clean distraction for courtyard guard at 94; consumed if used. |
| 30 | Insulated boots | 65 | Protect against residual shallow current at 71/82; retained. |
| 31 | Turbine access badge | 65 | Opens gatehouse/turbine controls; retained. |
| 32 | Spillway hand crank | 67 | Closes false-open spillway command; left installed. |
| 33 | Emergency dry-cell battery | 67 | Starts repaired pump at 70; consumed into starter. |
| 34 | Pump gasket | 69 | Repairs pump at 70; consumed. |
| 35 | Detachable valve wheel | 72 | Installed at valve garden 74; consumed. |
| 36 | Magnet on cord | 71, fallback 81 | Retrieves lift fuse and culvert survey case; retained. |
| 37 | Respirator mask body | 76 | Receives filter at 79; converted into fitted respirator record. |
| 38 | Empty filter housing | 79 | Filled with charcoal and attached to mask; consumed into respirator. |
| 39 | Copper bus bar | 80 | Reroutes substation feed at 88; left installed. |
| 40 | Freight-lift fuse | 82 | Installed at lift 84; consumed. |
| 41 | Mine route map | 83 | Prevents loops in old drifts and documents Nightjar cable route; retained. |
| 42 | Research access badge | 83 | Opens research doors and observatory staff routes; retained. |
| 43 | Punched code card | 83 | Opens screen 89 and contributes to bunker access; retained. |
| 44 | First-aid kit | 95 | Optional care for Kline at 113; remaining cloth can clean beacon 122. |
| 45 | Cipher lens | 97 | Reveals Calder phrase, calibration colours, and archive layer; retained. |
| 46 | Magnetic archive reel | 97 | Plays Calder recording and accepts copied evidence; later transformed into evidence spool. |
| 47 | Phase prism | 98 | Used diagnostically at 106, recovered, installed in waveguide at 121. |
| 48 | Weather-dome key | 102 | Opens instrument dome 99; retained. |
| 49 | Calibration fork | 99 | Generates carrier sequence and tunes summit waveguide; retained. |
| 50 | Antenna alignment chart | 100 | Generated after telescope survey; retained as a readable record. |
| 51 | Coolant hose | 109 | Repairs and redirects cooling at 111; consumed into machinery. |
| 52 | Grounding clamp | 109 | Grounds capacitors 110, then repairs summit strap 118; retained until final installation. |
| 53 | Evidence spool | 112 | Complete Nightjar archive and Voss admission; optional final upload at 124. |
| 54 | Emergency override key | 113 | Opens summit lock and prevents remote antenna override; retained. |
| 55 | Beacon crystal | 122 | Removed, cleaned, and reinstalled as protected carrier reference. |
| 56 | Voss's transmitter key | 119 | Enables final console; inserted at 124. |

### Optional keepsakes (8)

| # | Item | Screen | Narrative reward |
|---:|---|---:|---|
| 57 | Carved pine bird | 2 | Made by a child from Kestrel Six; Cass identifies it in the epilogue. |
| 58 | Enamel relay badge | 22 | Mara explains it belonged to the first civilian relay crew. |
| 59 | Ranger service patch | 31 | Theo offers it only after respectful dialogue and rescue. |
| 60 | Silted old relay badge | 41 | A 1964 predecessor to item 58; unlocks June's longest history story. |
| 61 | Blue quartz sample | 48 | A harmless local mineral, not part of Nightjar technology. |
| 62 | Logger payroll token | 57 | June trades a story, not a required item, when shown it. |
| 63 | Nightjar cloth patch | 93 | Evidence that Voss kept project insignia after closure. |
| 64 | Ruth Calder photograph | 109 | Changes Kline's epilogue and adds Calder's name to the final dedication. |

## 10. Major puzzle chains and dependencies

### Chain A — Restore local relay power

1. Talk to Mara and examine the cabin desk.
2. Open the relay gate with the brass key.
3. Install the patch cable at 17 and ceramic fuse at 18.
4. Open fuel valve at 20, reconnect battery link at 19, and isolate/reset transformer at 21.
5. Start generator at 18.
6. Retrieve multimeter at 22, inspect Nightjar trunk at 23, and calibrate the mast at 11.
7. Run direction trace at 24 and unlock the north forest route.

### Chain B — Reach and clear the quarry

1. Clear fallen fir and rescue Theo.
2. Collect compass, rope, hook, lamp, and flare from ranger cache.
3. Solve Echo Grove using bearing 017.
4. Move the bear without harm.
5. Talk to Nell, rig the ravine descent, slow waterfall, and recover quarry key.
6. Free Owen, distract/contain Brant, recover phase coil and notebook.
7. Repair hoist to create permanent ravine and logging-road shortcuts.

### Chain C — Restart the logging railway

1. Talk to Lila for the engine diagnosis.
2. Recover drive belt, oil, fuel, spark plug, and switch key.
3. Repair engine in any order; Lila completes timing only when all states are true.
4. Set rail points, distract the trestle guard, repair brake linkage, and cross.

### Chain D — Prevent the flood and open the mine

1. Obtain boots and turbine badge at west abutment.
2. Reach Jonah, install spillway crank, and close false command.
3. Isolate electrical circuit, install pump gasket and dry cell.
4. Remove valve wheel from intake and install it in valve garden.
5. Drain maintenance bay, recover magnet, and descend east shaft.

### Chain E — Power the ridge lift

1. Build respirator from mask, filter housing, and charcoal.
2. Brace collapsed drift and traverse gas zone.
3. Recover bus bar, lift fuse, mine map, badge, and punched card.
4. Ground substation, disconnect Quiet Field trunk, and install bus bar in lift feed.
5. Install lift fuse and solve upside-down code-card door.

### Chain F — Decode Nightjar

1. Evade courtyard patrol through timer/fog-horn route.
2. Solve archive drawers for cipher lens and reel.
3. Recover dome key; align dome and telescope to obtain chart.
4. Take phase prism and calibration fork.
5. Present Calder recording and survey notebook to Sable for the cooperative branch.
6. Open bunker using badge, phrase, and fork tone.

### Chain G — Invert the Quiet Field

1. Diagnose coil/prism at phase lab and recover both.
2. Derive 4-1-3 sequence with fork and cipher lens.
3. Redirect guards with archive playback.
4. Ground capacitor banks, replace coolant hose, and divert cooling.
5. Copy evidence spool and rescue Kline for override key.

### Chain H — Restore the open channel

1. Repair summit ground with clamp and wrench.
2. Install phase coil at tower base.
3. Install prism and tune with fork at microwave deck.
4. Recover/reinstall beacon crystal.
5. Align antenna using chart and override key.
6. Insert Voss key, enter 4-1-3, optionally upload evidence, and transmit on protected carrier.

The main route is gated by knowledge and world state rather than arbitrary invisible locks. Optional map travel reduces backtracking after the player has proven each route once.

## 11. Dialogue script

Dialogue uses compact blue bubbles anchored next to the current speaker. Each line is one bubble unless marked as a continuation. The player advances with Enter/Space. Important conversations may change flags only after their final line so save/load cannot create half-applied story state.

### D01 — Opening radio fragment (screen 1)

**Elias, broken radio:** “...Black Pine relay, answer... Kestrel Six overdue... any station...”

**Iris:** “I hear you. You cannot hear me yet.”

**Iris:** “First the relay. Then the mountain.”

### D02 — Mara's first briefing (screen 7)

**Mara:** “You took your time.”

**Iris:** “The lower road is now part of the river.”

**Mara:** “Then we have two broken things. The storm killed the relay at 02:17. The generator will not hold, the yard is locked, and every receiver is singing the same ugly note.”

**Iris:** “Where is the gate key?”

**Mara:** “Under the desk logbook, unless the mice have begun issuing work orders.”

**Mara:** “Fuse first. Patch the blue trench terminals. Fuel, battery, transformer, then the main lever. Electricity rewards the correct order and punishes enthusiasm.”

### D03 — Mara recognizes Nightjar (screen 8, after screen 24)

**Iris:** “The local plant is sound. The interference is coming from bearing zero-one-seven. The console calls it Nightjar.”

**Mara:** “That name was painted over before you were born.”

**Iris:** “Paint did not disconnect the trunk.”

**Mara:** “Nightjar sits beyond the old mine, under the ridge observatory. They said it listened to storms. They lied about most things up there.”

**Mara:** “Take my map. Find Nell at the lookout. She sees every fool who crosses that mountain.”

### D04 — Theo in Mossy Hollow (screen 30)

**Theo:** “Stop. The branch is light. The angle is not.”

**Iris:** “I have a saw and a bandage. Which lie would you like first?”

**Theo:** “Tell me this is a routine service call.”

**Iris:** “It stopped being routine at Nightjar.”

**Theo:** “The survey crew cut markers before the storm. One of them carried a red coil into the quarry. Their boss called it the heart.”

**Theo:** “My cache is north. Combination seven-one-four. Take the rope, and tell Nell I finally followed one bad idea far enough.”

### D05 — Nell at the lookout (screen 38)

**Nell:** “You are the moving red coat below Echo Grove.”

**Iris:** “And you are the reason I did not spend all night walking in circles.”

**Nell:** “Quarry has four people, one hoist, and lights they did not ask permission to use. Their truck came before the first thunder.”

**Iris:** “Can you see Kestrel Six?”

**Nell:** “No aircraft. One weak beacon beyond the east ridge, every forty-three seconds. It is getting weaker.”

**Nell:** “Take the ravine route. The bridge is gone, but Owen kept an old hoist line.”

### D06 — Voss's first contact (screen 45)

**Voss, field radio:** “Iris Bell. The caretaker's apprentice who repaired everything twice.”

**Iris:** “You prepared the relay failure.”

**Voss:** “I prepared a controlled demonstration. The weather contributed theatre.”

**Iris:** “A rescue aircraft is down.”

**Voss:** “Then restore your little local transmitter and let serious work continue.”

**Iris:** “Your serious work is sitting on my frequency.”

### D07 — Owen in the quarry office (screen 46)

**Owen:** “If you are with the survey, your customer service has improved.”

**Iris:** “Mara sent me. Can you stand?”

**Owen:** “I can complain standing, sitting, or under warranty.”

**Owen:** “They took a red electrical coil through the magazine. Brant watches the crusher because noise helps him think.”

**Iris:** “How do I reach the east landing?”

**Owen:** “Pin the hoist pulley, repair the tunnel signal, and pull the lever marked DO NOT PULL. That last label is mine.”

### D08 — Brant contained (screen 47)

**Brant:** “Open this cage.”

**Iris:** “You locked Owen in a cupboard.”

**Brant:** “That was business.”

**Iris:** “Then this is accounting.”

**Brant:** “Voss will leave you on this mountain.”

**Iris:** “He already left you.”

### D09 — Lila's engine diagnosis (screen 52)

**Lila:** “That engine can reach the dam. At the moment it can also be a shed.”

**Iris:** “What does the shed need?”

**Lila:** “Drive belt from the planer. Plug from the pond box. Oil from filing. Fuel from the protected boiler tank. And the rail key June hid where no sensible thief would look.”

**Iris:** “A short list.”

**Lila:** “The long list begins after it starts.”

### D10 — June and the old project (screen 58)

**June:** “Nightjar men ate here in sixty-eight. Never removed their hats, never said what they heard.”

**Iris:** “Did Ruth Calder come through?”

**June:** “Red scarf, muddy boots, asked better questions. She said a radio should carry a voice, not own it.”

**June:** “Take this ration. Guards and bears both make worse decisions when hungry, but only one can read the label.”

### D11 — Elias returns faintly (screen 63)

**Elias, radio:** “...station Bell... if receiving, Kestrel beacon is failing... medical oxygen...”

**Iris:** “Ward, this is Bell. I am north of the relay. The outage is deliberate.”

**Elias:** “...one word received... deliberate...”

**Iris:** “Good. Keep listening. I intend to become less subtle.”

### D12 — Jonah behind the gatehouse door (screen 67)

**Jonah, intercom:** “Identify yourself before you touch anything.”

**Iris:** “Iris Bell. Relay technician. Spillway command is false-open.”

**Jonah:** “Correct answer. Somebody drove it from the ridge and broke the return crank.”

**Iris:** “I have the crank.”

**Jonah:** “Insert it, three turns counter-clockwise, pause at amber, then two more. Rush it and the gate will bite the threads.”

### D13 — Jonah after the flood recedes (screen 67)

**Jonah:** “That command carried a Nightjar service signature. I have not seen one in twenty years.”

**Iris:** “Voss is powering the old field.”

**Jonah:** “Then the flooded shaft was not vandalism. He wanted the mine route erased behind him.”

**Jonah:** “Drain the maintenance bay. The east shaft reaches his freight lift. I will hold the spillway even if the ridge asks politely.”

### D14 — Voss in the freight lift (screen 90)

**Voss, intercom:** “Leave the phase coil in the lift, Iris. Go down. I will restore ordinary service after midnight.”

**Iris:** “Kestrel Six may not have after midnight.”

**Voss:** “One aircraft does not define a system.”

**Iris:** “A system is exactly what it refuses to sacrifice.”

**Voss:** “Ruth used to speak in slogans too.”

**Iris:** “Ruth shut you down.”

### D15 — Calder archive recording (screens 97/108)

**Ruth, recording:** “Project Nightjar log, Ruth Calder, systems engineering.”

**Ruth:** “The Quiet Field cannot distinguish an enemy command from a distress call outside its protected carrier.”

**Ruth:** “Gideon calls that a calibration problem. It is a moral problem expressed in volts.”

**Ruth:** “I have placed the protected sequence in the instrument archive: open channel, four-one-three.”

**Ruth:** “If this recording is needed, do not destroy the transmitter. Give it back its proper purpose.”

### D16 — Kline through the weather-lab duct (screen 98)

**Kline, faintly:** “Who is in the weather lab?”

**Iris:** “Iris Bell. I am trying to stop Voss.”

**Kline:** “Then do not pull the phase prism from a live rack. Put the rig in diagnostic first.”

**Iris:** “Where are you?”

**Kline:** “Below. Holding room beside the test cell. He needs me until calibration, which is the closest thing I have to a lock pick.”

### D17 — Sable confrontation (screen 101, evidence branch)

**Sable:** “Step away from the jammer.”

**Iris:** “Listen to Calder's reel.”

**Sable:** “Old project politics will not fix the valley.”

**Iris:** “Theo saw your crew cut the relay before the storm. This notebook schedules the cut. Kestrel Six fell after your field came up.”

**Sable:** “Voss said the protected carrier covered emergency traffic.”

**Iris:** “He has not enabled it.”

**Sable:** “...Then he did not need a technician. He needed a witness who would blame herself.”

**Sable:** “I will drop the jammer rack. Kline is below the test cell. Do not make me regret choosing late.”

### D18 — Sable confrontation (screen 101, no-evidence branch)

**Sable:** “You repaired more than Voss expected.”

**Iris:** “Help me repair the rest.”

**Sable:** “You have accusations and a wrench.”

**Iris:** “The wrench has been more reliable.”

**Sable:** “Find the archive. If Calder says what you claim, bring it to the tower.”

Sable flees but leaves the jammer disabled; she can still change sides at screen 119 if the evidence is later recovered.

### D19 — Voss in the command archive (screen 112)

**Voss, monitor:** “You are copying property that no government admits owning.”

**Iris:** “You sabotaged a public relay with it.”

**Voss:** “For eighteen minutes the valley will be perfectly quiet. Buyers understand proof.”

**Iris:** “So will a court.”

**Voss:** “Courts require a chain of custody.”

**Iris:** “This machine appears to have a printer.”

### D20 — Kline rescued (screen 113)

**Kline:** “Did you recover the red coil and the phase prism?”

**Iris:** “Both. Calder's sequence is four-one-three.”

**Kline:** “Good. Ground the capacitor banks in that order, dump the cooling, and Voss must route the field through the summit waveguide.”

**Iris:** “That sounds like helping him.”

**Kline:** “Only until you install the prism and beacon crystal. Then the tower can broadcast the protected carrier inside his own cancellation field.”

**Kline:** “Take my override key. And Iris—save the transmitter. Ruth was right about its purpose.”

### D21 — Sable at tower base (screen 119, cooperative branch)

**Sable:** “Voss dropped this transmitter key when he ran for the ladder.”

**Iris:** “You could have kept running.”

**Sable:** “I have been running since the first distress call.”

**Iris:** “Mara has a radio at the lower relay. Tell her where Kline is.”

**Sable:** “And you?”

**Iris:** “I am going to make the loudest repair of my career.”

### D22 — Final confrontation (screen 124)

**Voss:** “Look at the instruments. Stable phase, full reach. Nightjar works.”

**Iris:** “The silent radios work. The missing aircraft works. Is that your demonstration?”

**Voss:** “Every technology has a cost before people understand its value.”

**Iris:** “Then say that on an open channel.”

**Voss:** “There is no open channel.”

**Iris:** “Four-one-three.”

**Voss:** “Ruth.”

**Iris:** “Yes.”

At this point control returns for the final console sequence.

### D23 — Standard rescue broadcast (screen 124 victory)

**Iris, transmitter:** “Black Pine calling all stations. Emergency carrier restored. Kestrel Six, respond.”

**Static:** “...”

**Cass, faintly:** “Black Pine, Kestrel Six. We read you.”

**Elias:** “Kestrel Six, hold beacon. Rescue is tracking. Black Pine, keep that carrier open.”

**Iris:** “Carrier is open. It is staying that way.”

### D24 — Evidence broadcast addition (screen 124, if spool loaded)

**Voss, recorded:** “For eighteen minutes the valley will be perfectly quiet. Buyers understand proof.”

**Iris, transmitter:** “All stations, the preceding recording and Nightjar archive identify the cause of the outage. Preserve this transmission.”

**Mara:** “Copied at the relay. Every word.”

**Sable, if turned:** “Copied at the ridge. I will testify to the rest.”

### D25 — Cabin epilogue (ending card outside screen count)

**Mara:** “You repaired the relay.”

**Iris:** “Eventually.”

**Mara:** “You also repaired a dam, a railway, a mine lift, an observatory, and one engineer's conscience.”

**Iris:** “The invoice will be difficult.”

**Elias, radio:** “Black Pine, Kestrel Six crew are safe.”

**Mara:** “Put the kettle on, technician.”

**Iris:** “That, I know how to fix.”

### Contextual and repeat dialogue rules

- Mara has short update conversations after acts I, II, III, and IV; each summarizes known facts without giving the next puzzle solution outright.
- Theo, Nell, Owen, Lila, June, Jonah, Kline, and Sable each have at least three repeat states: before request, while task incomplete, and after task complete.
- Repeating a conversation never replays its whole first encounter. The first line acknowledges prior contact and provides only currently relevant information.
- Voss's remote dialogue cannot be skipped before its first completion, but later attempts may be dismissed immediately.
- Enemy barks are short and positional: “Check the east door,” “The pulse drifted again,” “Voss said midnight,” and “Did you hear the hoist?” They convey patrol timing without a stealth HUD.
- EXAMINE narration uses Iris's concise internal voice. It never impersonates an omniscient narrator and never states information she could not perceive.

## 12. Environment and visual direction

### Colour and region identity

The same fixed EGA palette is used throughout, but each region has a dominant contrast pair so the player can recognize location at a glance.

| Region | Dominant colours | Shape language and landmarks |
|---|---|---|
| Trailhead/cabin | brown, green, bright yellow | Rounded pine crowns, timber rectangles, warm windows, diagonal rain |
| Relay yard | cyan, dark gray, bright yellow | Straight cable lines, insulators, dish arcs, labelled panels |
| North forest | green, black, cyan | Layered ellipses, broken vertical trunks, water highlights, compass markers |
| Ravine/quarry | brown, dark gray, bright red | Jagged polygons, heavy hoist circles, catwalk grids, red survey marks |
| Logging camp | brown, red, yellow | Repeating plank lines, toothed saw circles, rail perspective, steam puffs |
| Reservoir | blue, cyan, light gray | Large water planes, white spray pixels, concrete blocks, gauge arcs |
| Mine/substation | black, brown, yellow/cyan accents | Tight timber frames, lamp cones, ore pixels, thick power buses |
| Observatory | blue, white, light gray | Snow/rain, clean institutional rectangles, glass domes, fine instrument traces |
| Bunker | dark gray, black, bright red/green | Repeated bulkhead shapes, warning chevrons, concentric phase geometry |
| Summit/tower | blue, black, white, bright green finale | Steel lattice diagonals, fast cloud bands, lightning lines, beacon circles |

### Screen composition rules

- Every screen has one unmistakable silhouette or machine, one near-field detail, and one navigational edge cue.
- Required hotspots have a visible physical presence before interaction; hidden objects are revealed by a justified EXAMINE action, never pixel hunting.
- Foreground shapes may partially overlap Iris for depth, but never hide her feet or collision edges.
- Walkable floor and hazards use stable palette-index contrast. Decorative pixels never reuse a collision-index convention accidentally.
- Text labels are sparse and diegetic: signs, panel legends, file tabs, and instrument readouts.
- The right-side inventory and bottom action panel remain visually consistent across all regions. Dialogue bubbles occupy only the local world viewport and point to their speaker.
- State changes persist visually: installed parts remain installed, drained rooms remain drained, opened doors remain open, and disabled equipment loses its warning animation.

## 13. Animation specification

Animation uses QBasic-timer-inspired integer ticks and discrete code-drawn frames. Static art stays static. A normal room should have no more than two ambient loops plus a one-shot action, preventing visual noise and keeping authored animation meaningful.

### Ambient loops

| Loop family | Typical duration | Screens | Frames and intent |
|---|---:|---|---|
| Rain and gutter water | 6–12 ticks | 1–16, 91–98 | Two or three diagonal/ripple states; weather continuity. |
| Wind in pine/cloth/cable | 10–18 ticks | Exterior regions | Small endpoint shifts, never whole-tree deformation. |
| Running water | 4–8 ticks | 5, 28, 40–44, 56, 64–74 | Alternating highlight lines and foam pixels. |
| Electrical arc | 12-tick cycle | 4, 21, 69–72, 110 | Warning glow, pause, strike, recovery; hazard timing is readable. |
| Instrument trace | 4–10 ticks | 8, 11, 23, 24, 98, 101, 106–108, 124 | Pixel waveform or needle; colour/state shows progress. |
| Machinery idle | 6–16 ticks | 18, 50, 61, 68–70, 84–90, 111 | Wheel/belt/piston motion only while powered. |
| Patrol | authored route | 47, 62, 92, 105 | Walk, pause, turn, inspect. Facing and sound disclose the safe window. |
| Beacon | 8 or 16 ticks | 38, 64, 119–124 | Red before inversion, green after protected carrier. |

### One-shot authored sequences

1. Brass gate unlock and swing.
2. Cable installation with Iris kneeling left/right according to hotspot side.
3. Generator false starts followed by steady run.
4. Fallen fir sawing and branch roll.
5. Theo rescue and bandage.
6. Bear flare, startled step, and calm exit.
7. Hook throw and rope tension.
8. Waterfall sluice closing.
9. Crusher-horn guard response and cage closure.
10. Quarry hoist repair, cable bridge deployment, and cage descent.
11. Logging engine repair summary and first start.
12. Engine crossing the trestle.
13. Spillway crank and distant gate closure.
14. Four-stage maintenance-bay drainage.
15. Respirator assembly and ventilation fan start.
16. Ore-cart shortcut ride.
17. Freight-lift ascent past rock strata.
18. Observatory dome opening and rotating.
19. Sable shutting down jammer racks.
20. Bunker iris door opening.
21. Capacitor grounding sequence.
22. Cooling diversion and Voss's forced reroute.
23. Kline rescue.
24. Summit grounding strike.
25. Phase-prism waveguide tuning.
26. Beacon changing from red rotation to green pulse.
27. Antenna alignment through three mechanical positions.
28. Final competing waveforms, field collapse, and valley lights returning.

### Character animation set

Each speaking character needs only a small, reusable set:

- stand facing left/right;
- one talking gesture and one listening gesture;
- walk cycle where movement occurs;
- character-specific action pose, such as Mara at radio, Lila at engine, Jonah at crank, Kline at console, or Voss operating the transmitter;
- sit/injured pose where required;
- reaction pose for alarm, relief, or surrender.

Iris retains the Explore2D base poses—standing, turning before walking, walking, jumping, taking while bent toward the item—and adds sawing, wrenching, pulling, climbing, reading, radio, and console poses. There is no universal idle animation merely to keep the screen moving.

## 14. Audio direction

Audio remains inside Explore2D's QBasic-like sound model: monophonic square-wave tone steps, frequencies in hertz, durations in legacy timer ticks, and silence steps. There are no sampled voices, environmental recordings, streamed music, stereo effects, or modern DSP.

| Motif | Shape | Use |
|---|---|---|
| Black Pine title | rising 392–523–659–784 Hz phrase | Title reveal and complete ending reprise |
| Menu move / confirm | one high tick / two-note rise | Menus, choices, map |
| Pickup | three quick ascending tones | Ordinary item collection |
| Discovery | low-high-low question figure | Significant EXAMINE clue |
| Repair accepted | repeated 880 Hz taps | Component installed correctly |
| Power restored | slow 110–165–220–330 rise | Generator, substation, pump |
| Radio fragment | alternating 147/155 Hz pulse | Quiet Field and broken messages |
| Clear carrier | stable 440 Hz followed by 660 Hz | Successful local or final radio link |
| Patrol warning | two low separated notes | Enemy enters adjacent patrol position |
| Environmental danger | descending 196–147–110 Hz | Electrical, water, gas, or fall warning |
| Death | restrained descending four-note phrase | Failure card; never comical |
| Calder archive | four-note 4-1-3 mnemonic | Archive and calibration clue |
| Voss field | dissonant pulse plus short high tone | Remote antagonist contact |
| Victory | title motif extended to 1047 Hz | Kestrel response and ending |

Machinery rhythm is represented by sparse tone events at state changes, not a continuous synthesized hum. Silence is important: the Quiet Field test cell intentionally drops nearly all cues, making the restored carrier perceptually distinct.

## 15. Title, map, death, ending, and credits outside the 124 screens

### Title sequence

The title begins on black. Rain outlines the mountain one line at a time. A red beacon appears, the words **BLACK PINE** are drawn in four EGA colours, and a narrow waveform passes beneath **THE LONG SILENCE**. The main menu then appears over a procedural view of the ridge. New Game, Continue, Load, Speed, How to Play, and Quit are arranged in a compact radio-dial motif.

### New-game introduction

The introduction intercuts three code-drawn tableaux:

1. Kestrel Six flies under a storm front while Elias calls Black Pine.
2. The relay beacon goes dark at 02:17 and a low square pulse replaces the carrier.
3. At dawn Iris reaches the washed-out trailhead, shoulders her tool bag, and hears the broken opening call.

Control begins at screen 1. The introduction is not counted as a gameplay screen.

### Travel map

The map is a simplified relief diagram drawn in EGA colours. Only visited or radio-confirmed anchors appear. Up to five anchors are shown per page, grouped by region. Selecting an anchor represents believable travel over a proven safe route. Map travel is disabled during an active enemy encounter, inside the mine/bunker, and after the final pulse begins.

### Failure presentation

Each death uses a short contextual animation, a blue-black card naming the cause without gore, and a choice to resume from the most recent safe anchor or return to title. Being caught by a guard usually causes relocation or the holding-room escape branch, not death.

Failure texts include:

- “THE LINE WAS STILL LIVE.”
- “THE RAVINE TOOK THE SHORTER ROUTE.”
- “THE MINE HAD NO AIR TO SPARE.”
- “THE CAPACITOR FOUND GROUND FIRST.”
- “THE MOUNTAIN ANSWERED IN LIGHTNING.”
- “THE LONG SILENCE REACHED MIDNIGHT.”

### Endings

There is one successful mechanical outcome—Quiet Field disabled, carrier restored—and three epilogue grades based on optional state:

1. **Carrier Restored:** standard victory. Kestrel Six is located and its occupants survive.
2. **Open Channel:** evidence spool was broadcast. Voss's buyer is exposed, Sable testifies if persuaded, and Nightjar is placed under public investigation.
3. **Keeper of Black Pine:** Open Channel plus all people helped, all optional conversations completed, and all eight keepsakes recovered. The epilogue names Ruth Calder, the community converts the ridge station into a public emergency-radio museum, and Iris becomes its first technical keeper.

The better endings add consequences and acknowledgement but never imply that a player who missed collectibles failed to save the aircraft.

### Credits

Credits scroll over ten revisited screen silhouettes in reverse journey order, each now calm and powered. Character epilogue bubbles appear without pausing the scroll. The final frame is screen 1's trailhead sign with a tiny green tower beacon visible above it.

## 16. Persistent story state

The implementation may use more granular flags, but the authored design depends on the following state groups.

| State group | Key milestones |
|---|---|
| Local relay | gate open; trench patched; fuse installed; fuel ready; battery linked; transformer isolated/reset; generator on; Nightjar bearing known |
| Forest | Theo found/freed/treated; cache opened; Echo Grove solved; bear moved; Nell informed |
| Quarry | ravine rope fixed; sluice closed; Owen freed; Brant contained; coil/notebook recovered; hoist repaired; shortcuts open |
| Railway | Lila briefed; belt/plug/oil/fuel installed; switch aligned; guard redirected; brake fixed; engine east |
| Reservoir | badge acquired; spillway closed; Jonah freed; turbine isolated; pump repaired/powered; intake open; bay drained |
| Mine power | respirator complete; brace safe; fuse retrieved/installed; bus bar installed; Quiet Field trunk cut; ridge lift powered |
| Observatory | patrol route changed; archive decoded; dome aligned; antenna chart known; Sable hostile/uncertain/cooperative; jammer disabled |
| Nightjar | bunker open; inversion 4-1-3 known; guards redirected; capacitor banks grounded; cooling dumped; evidence copied; Kline rescued/treated |
| Summit | grounding repaired; coil installed; prism tuned; beacon crystal restored; antenna aligned; key inserted; evidence uploaded; carrier transmitted |
| Optional/community | eight keepsakes; June stories; Theo follow-up; Nell beacon reports; Owen safe; Lila safe; Jonah safe; Sable testimony; Kline treated |

All permanent scene mutations must be saveable. Animation playback position need not be persisted; after load, the world renders the stable post-animation state.

## 17. Writing and interaction rules

- TAKE describes a physical action and changes Iris to the directional taking pose.
- EXAMINE always provides either usable information, characterization, foreshadowing, or a concise confirmation that state changed. It should not drown important clues in identical jokes.
- USE failures are target-specific when plausible: “The wrench fits, but the circuit is still live” is better than “That does not work.”
- Required puzzle clues appear in at least two channels: visual/environmental plus dialogue/document/instrument. Optional ending content may rely on a single careful clue.
- Codes are short and meaningful. The only central numeric sequence, 4-1-3, is tied to Calder's carrier and is retained in Iris's notebook after discovery.
- Characters do not hand Iris arbitrary fetch quests. Their requests arise from their work and immediate circumstances.
- Backtracking after a long sequence creates a shortcut, travel anchor, or radio alternative.
- A consumed item always becomes visible world state or a clearly recorded flag.
- The player may discover items before learning their uses. Dialogue adapts rather than blocking pickup artificially.
- Humour comes from character perspective, labels, and understatement, never from mocking injury or failure.

## 18. Production status

The complete 124-screen route is implemented. Content was delivered region by
region, with each screen serving a navigational, narrative, visual, or puzzle
purpose from this design. The original seven-screen prototype was replaced by
the canonical world rather than duplicated as filler.

The implementation locks the screen IDs and region topology, supports all 17
travel anchors, and carries the five-act state graph through the bunker and
summit finales. Optional evidence changes the ending without blocking the
critical path. Automated scenario and rendering tests cover the full catalogue.

### Acceptance criteria

- Exactly 124 numbered, playable, fixed screens.
- All screens reachable in at least one valid state; no decorative duplicate rooms used only to inflate the count.
- One complete no-hint route from new game to Carrier Restored.
- One complete route to Open Channel and one to Keeper of Black Pine.
- Every required item has a verified source, use, and stable post-use state.
- Every character conversation has first, pending, and completed variants where appropriate.
- Every hazard has visible warning, safe solution, and tested failure/restart route.
- Every authored animation terminates in the correct persistent state.
- All visuals remain code-drawn with the fixed Explore2D palette and layout.
- All audio remains inside the monophonic QBasic-like tone model.
- No *Tajná mise* story, text, characters, rooms, or art are reused.

## 19. Canonical count statement

For planning, testing, website copy, and README updates, the authoritative statement is:

> *Tajná mise* has 124 numbered gameplay screens. *Black Pine: The Long Silence* also has exactly 124 numbered gameplay screens, plus separate title, menu, map, failure, ending, and credits presentation states.
