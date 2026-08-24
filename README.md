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
- crosshair-free play view with a large generated knife, sidearm, repeater or heavy automatic in the lower center
- discoverable repeater and heavy automatic weapon with distinct first-person views and HUD icons
- clearly audible generated CNA effects for firearm shots, knife attacks, ammunition, enemy alerts and attacks, defeated enemies, doors, locks and player damage
- a generated looping bunker ambience, with a master sound on/off control in the title menu
- uncapped score for gold, defeated enemies and completing the level; every 40,000 points awards another life
- a centered completion card appears at the level exit
- grid collision with wall sliding
- level loaded from a validated text file
- a three-sector authored bunker campaign with guards, hounds, pickups, sliding doors, security doors, terminals and exits
- original transparent pixel-art guard and hound sprites rendered as camera-facing polygons in the 3D world
- defeated guards and hounds leave a stylized procedurally textured blood-pool decal on the floor
- authored rooms include procedural framed paintings, peace-symbol banners and ceiling lamps
- distinct generated material palettes for each sector: warm bunker, green industrial, and cold technical
- guards fire visible ranged projectiles; hounds remain close-range attackers
- no external copyrighted game assets
- original title menu with difficulty selection before a run begins
- illustrated splash with a generated original bunker background and a large sharp `WOLF CNA` heading before the separate main menu
- persistent sector selection: a fresh profile starts with sector 1 and completing a sector unlocks the next one

## Controls

- up/down arrow keys: forward / backward
- left/right arrow keys: turn left / right
- title/sector/difficulty menus: arrows select, `Enter` or `Space` confirms, `Escape` backs out; the title menu also has a master sound toggle
- three difficulty modes: Scout (70% enemy damage), Operative (normal), Veteran (140% enemy damage)
- `Space`: open the door in front of you (doors close after four seconds unless the player or a body blocks them)
- left or right `Ctrl`: attack with the selected weapon; empty firearms automatically fall back to the knife
- `1` / `2` / `3` / `4`: knife / sidearm / three-round repeater / five-round heavy automatic; weapons 3 and 4 must be found first
- collecting ammunition after reaching zero restores the last firearm automatically
- `F11`: toggle fullscreen
- `P`: pause / resume
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
- `S`: secret moving wall; use it to expose a hidden reward
- `G`: guard spawn
- `K`: hound spawn
- `H`: health pickup
- `A`: ammunition pickup
- `T`: gold pickup
- `E`: level exit
- `R`: wall-mounted framed landscape painting; must be next to a wall
- `B`: wall-mounted banner with an original peace symbol; must be next to a wall
- `L`: ceiling lamp
- `W`: repeater weapon pickup with six rounds
- `V`: heavy automatic weapon pickup with ten rounds

The loader rejects malformed rows, unknown symbols, and levels without exactly one player spawn.

An exit is red while its terminal objective is incomplete and turns cyan when it is online.
The current authored sectors are compact prototypes; the planned final floor size
is the classic 64×64-cell grid.

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

`wolf-cna` is not a distribution of Wolfenstein 3D and contains no Wolfenstein game data. World and weapon textures are generated by project code; the provenance of committed generated art is recorded in [`ASSET_PROVENANCE.md`](ASSET_PROVENANCE.md).

Read `plan.md` before extending the game, especially the CNA-only boundary and asset policy.
