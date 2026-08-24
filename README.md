# wolf-cna

An original retro first-person shooter prototype written in **C++23** and rendered as **real polygonal 3D through CNA**.

This starter is deliberately small. It proves the basic direction before local AI agents continue the project.

## What already works

- CNA `Game` startup
- true 3D triangle world
- `VertexBuffer` / `IndexBuffer`
- `BasicEffect`
- depth testing
- generated wall/floor/ceiling texture atlas
- first-person camera
- classic arrow-key movement
- keyboard turning with a fixed horizon
- crosshair-free play view with a large original AI-generated knife, sidearm, repeater or heavy automatic sprite in the lower center
- discoverable repeater and heavy automatic weapon with distinct first-person sprites and matching HUD icons
- every weapon has an original dedicated slash/firing frame, combined with visible knife lunge or firearm recoil for immediate attack feedback
- clearly audible generated CNA effects for firearm shots, knife attacks, ammunition, enemy alerts and attacks, defeated enemies, doors, locks and player damage
- a generated looping bunker ambience, with a master sound on/off control in the title menu
- uncapped score for gold, defeated enemies and completing the level; every 40,000 points awards another life
- a centered completion card appears at the level exit
- grid collision with wall sliding
- level loaded from a validated text file
- a three-sector authored bunker campaign with guards, hounds, pickups, sliding doors, security doors, terminals and exits
- original transparent pixel-art guard, hound, rapid-trooper and heavy-unit sprites rendered as camera-facing polygons in the 3D world
- idle enemies breathe subtly, while chasing enemies use faster archetype-specific step bob and sway
- every enemy uses a brief dedicated firing/lunge sprite synchronized with its actual attack event
- every surviving enemy briefly switches to its own non-gory recoil pose when hit
- every defeated enemy switches to its own original collapsed/resting sprite above a stylized procedurally textured blood-pool decal
- authored rooms include procedural framed paintings, peace-symbol banners, ceiling lamps with warm floor-light pools and three sector-specific freestanding plant landmarks
- distinct generated material palettes for each sector: warm bunker, green industrial, and cold technical
- only the nearest eligible ranged enemy fires at one time; guards, rapid troopers and heavy units use slower distinct cadences while hounds remain close-range attackers
- defeated guards, rapid troopers and heavy units drop 3, 5 and 8 collectible rounds respectively; hounds do not drop ammunition
- health, ammunition and three differently valued treasures use original transparent pixel-art pickup sprites instead of colored blocks
- health kits remain in the level at 100% health and can be collected after the player takes damage
- no external copyrighted game assets
- original title menu with difficulty selection before a run begins
- illustrated splash with a generated original bunker background and a large sharp `WOLF CNA` heading before the separate main menu
- persistent sector selection: a fresh profile starts with sector 1 and completing a sector unlocks the next one
- `M` toggles a paused floor map that reveals only visited cells while always marking the sector exit as `GOAL`

## Controls

- up/down arrow keys: forward / backward
- left/right arrow keys: turn left / right
- title/sector/difficulty menus: arrows select, `Enter` or `Space` confirms, `Escape` backs out; the title menu also has a master sound toggle
- three difficulty modes: Scout (70% enemy damage), Operative (normal), Veteran (140% enemy damage)
- `Space`: open the door in front of you (doors close after four seconds unless the player or a body blocks them)
- left or right `Ctrl`: attack with the selected weapon; hold for repeater/heavy automatic fire, while the knife and sidearm fire once per press; empty firearms automatically fall back to the knife
- `1` / `2` / `3` / `4`: knife / sidearm / three-round repeater / five-round heavy automatic; weapons 3 and 4 must be found first
- collecting ammunition after reaching zero restores the last firearm automatically
- `F11`: toggle fullscreen
- `P`: pause / resume
- `M`: press and release to open / close the explored-area map; red `GOAL` marks a locked exit and cyan `GOAL` an active exit; the `I` + `L` + `M` cheat takes priority even when its keys are pressed gradually
- `Escape`: quit
- `I` + `L` + `M` together: retro loadout cheat — full health, all weapons,
  access card, heavy automatic selected, ammunition set to 99, and score reset to zero

After all lives are lost, press `Space` to return to the title menu and start a new run.
At a sector exit, `Space` takes the run to the next sector; score, lives, health,
ammunition and the selected weapon carry forward, while sector access cards do not.
Before the transition, the game shows the sector time and the collected/total
counts for kills, gold and secrets.
Unlocked sectors are stored in `wolf-cna-progress.dat` in the launch working
directory. Invalid progress data safely falls back to sector 1 only.

## Level files

The campaign uses [`assets/levels/starter.level`](assets/levels/starter.level),
[`assets/levels/sector-02.level`](assets/levels/sector-02.level) and
[`assets/levels/sector-03.level`](assets/levels/sector-03.level). They use larger
rooms connected by corridors rather than a single continuous maze. Each row must
have the same width and use only these symbols:

- `#`: solid wall
- `.`: empty floor
- `P`: the single player spawn
- `D`: closed sliding door
- `Q`: closed red security door
- `C`: cyan security-card pickup, required to open `Q`
- `M`: amber terminal; use it to bring the exit online
- `O`: violet power relay; activate it as the other half of the exit objective
- `S`: secret moving wall; use it to expose a hidden reward
- `G`: guard spawn
- `K`: hound spawn
- `F`: rapid-fire trooper spawn
- `U`: heavy-unit spawn
- `H`: health pickup
- `A`: ammunition pickup
- `T`: gold-bars pickup worth 100 score
- `J`: golden-goblet pickup worth 250 score
- `N`: peace-medallion pickup worth 500 score
- `E`: level exit
- `R`: wall-mounted framed landscape painting; must be next to a wall
- `B`: wall-mounted banner with an original peace symbol; must be next to a wall
- `I`: freestanding decorative plant using the current sector's original sprite
- `L`: ceiling lamp
- `W`: repeater weapon pickup with six rounds
- `V`: heavy automatic weapon pickup with ten rounds

The loader rejects malformed rows, unknown symbols, and levels without exactly one player spawn.

An exit is red until every power relay and terminal is active, then turns cyan.
All three campaign sectors use an exact 64×64-cell footprint with large authored
rooms, connecting corridors, loops and optional secret spaces. Focused test maps
may remain smaller.

## Expected checkout layout

The default CMake configuration expects CNA to be a sibling checkout:

```text
development/
  cna/
  sharp-runtime/
  wolf-cna/
```

CNA itself expects `sharp-runtime` next to the CNA checkout.

If your CNA checkout has another name/location, pass it explicitly:

```bash
cmake -S . -B build \
  -DCNA_ROOT=/absolute/path/to/cna \
  -DCNA_GRAPHICS_RENDERER=OPENGLES3

cmake --build build -j
./build/wolf-cna
```

You can select another CNA renderer with `CNA_GRAPHICS_RENDERER` as supported by your current CNA checkout.

## Important

`wolf-cna` is not a distribution of Wolfenstein 3D and contains no Wolfenstein game data. World textures are generated by project code; the provenance of committed AI-generated original art is recorded in [`ASSET_PROVENANCE.md`](ASSET_PROVENANCE.md).

Read `plan.md` before extending the game, especially the CNA-only boundary and asset policy.
