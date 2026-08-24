# Next work

## Last completed work

- Doors defer automatic closing while the player or a defeated enemy occupies the doorway.
- Guards and hounds require multiple hits; rapid troopers and heavy units add distinct ranged combat roles.
- Primitive weapon shapes are replaced by original transparent high-resolution knife, sidearm, repeater and heavy-automatic sprites.
- Every enemy archetype now has its own original non-gory defeated sprite above the existing floor decal.
- Guard, hound, rapid trooper and heavy unit attacks now trigger dedicated firing/lunge sprites for a short synchronized interval.
- Every surviving enemy now shows its own brief non-gory hit-reaction sprite before resuming pursuit.
- Idle enemies now breathe subtly and each chasing archetype has deterministic step bob and sway instead of sliding as a static image.
- Defeated armed enemies now drop 3/5/8 rounds by archetype; hounds correctly drop none.
- All four player weapons now use dedicated original attack frames instead of a rectangular muzzle-flash overlay.
- Ceiling lamps now cast a warm, softly fading pool of light onto the floor below.
- Only one visible ranged enemy attacks at a time, with lower damage and slower archetype-specific cadences.
- Health, ammunition and three treasure values now use original transparent sprites instead of colored blocks.
- Holding Ctrl repeats repeater and heavy-automatic bursts at distinct bounded cadences; knife and sidearm remain one attack per press.
- Health kits are preserved when the player is already at 100% and can be collected after later damage.
- Weapon sprites now lunge or recoil during attacks instead of remaining completely static.
- Holding `Tab` shows the paused map of visited cells and releasing it resumes play; `I+L+M` remains an independent loadout cheat.
- The map always marks the sector exit as cyan `GOAL` without revealing nearby rooms.
- Firearm attack frames remain visible for a few extra frames so their generated muzzle flashes read clearly.
- Firearm attack frames use CNA straight-alpha blending and partial opacity over the solid weapon view, preventing transparent RGB from becoming a white rectangle.
- Every sector contains a separate violet power relay and amber terminal as optional bunker systems; the elevator remains available independently.
- Every sector now places three freestanding plant landmarks with its own original transparent storage, foundry or laboratory sprite.
- Every sector now has two solid polygonal tables with four-leg silhouettes, static geometry and matching player/enemy collision.
- The automap now shows independent color-coded `POWER` and `TERMINAL` progress without revealing either objective's location.
- The automap now includes a compact side legend for player, door, lock, discovered-secret and goal colors.
- A fourth progressively unlocked 64×64 archive sector adds a violet/bronze material palette, altered combat roster and original archive-palm landmarks.
- Relay and terminal interactions now show `POWER ONLINE`, `TERMINAL ONLINE` or `SYSTEMS COMPLETE` for two seconds.
- Health placement now scales from two kits in early sectors to three in labs/archive; tests require enough guaranteed ammunition for a full clear.
- The illustrated splash is separate from the main menu.
- Every campaign sector now has an exact authored 64×64 footprint; tests require substantial use of the area and no disconnected rooms.
- Sector unlocks, five-step master volume, four view angles and the last selected difficulty now persist in one validated, legacy-compatible profile.
- WOLF-013 is complete: all enemy and weapon states use original provenance-recorded sprites with deterministic animation and translucent firing feedback.
- Holding left or right `Shift` now runs at 1.65× walking speed while retaining the clamped frame step and existing collision path.
- Four original generated wall families now alternate in coherent room-scale regions, and every polygonal table uses a dedicated dark-oak texture.
- Access cards, both weapon pickups, terminals, power relays, exits and enemy projectiles now use original transparent sprites; the last untextured colored cuboid path is gone.
- Every map `GOAL` now corresponds to a three-sided steel elevator cabin whose polygonal gate starts raised so the exit is immediately usable.
- Holding `G+O+A+L` once teleports the player to the safe approach cell outside the current elevator and turns them toward its doors without changing objective progress.
- `Space` now activates an elevator from its approach regardless of optional relay and terminal progress; entering the cabin follows the same completion path.
- The `G+O+A+L` cheat remains purely positional and gives the same usable elevator as reaching the goal on foot.
- Enemy projectiles now create a short expanding project-generated spark on impact; player hits also add a translucent amber flash and a generated impact sound.
- Sector completion now plays a deterministic original four-note fanfare through CNA audio in addition to the elevator confirmation.
- Campaign pacing tests now require early and late health recovery plus a 90–130-cell relay/terminal/exit route; sector 1's second kit moved into its late combat room and Veteran damage is now 130%.
- The title menu now cycles CNA master volume through 0/25/50/75/100%; profile format 3 persists it and migrates both older profile versions.
- The title menu now cycles a CNA-rendered 60/72/84/96-degree view angle; profile format 4 persists it and migrates formats 1–3.
- The held automap prompt now correctly says `RELEASE TAB` instead of the obsolete `M CLOSE` binding.
- `P` or `Escape` now opens an in-run pause menu with resume, sound, view-angle and quit-to-title actions; gameplay no longer exits immediately on Escape.
- Scout, Operative and Veteran now deterministically scale active encounter tiers, enemy health/speed/firing cadence, incoming damage and starting/fixed/dropped ammunition; all four sectors have monotonic balance audits.
- Enemies now patrol authored arrow routes, see directionally, react after archetype-specific delays, hear firearm noise through connected ordinary doors, search last-known positions and open ordinary doors without bypassing locks; every sector includes a patrol and ambush encounter.
- Three versioned run-save slots now preserve full player/world/AI/objective/automap state, use recoverable temporary-file replacement and are available from title/pause menus plus F8/F9.

## Next tasks

1. Implement WOLF-036 classic life-loss sector restart semantics.
2. Continue through WOLF-034 and WOLF-037–WOLF-044 in the dependency order recorded in `plan.md`.
3. Subjectively playtest all three deterministic difficulty profiles, save slots and the full four-sector route.

Longer-term M7 work keeps true vertical spaces and moving elevators separate from
the current campaign-transition cabins.

## Verification

```bash
cmake --build build-cnanext --target wolf-cna level-definition-tests -j8
ctest --test-dir build-cnanext --output-on-failure
./build-cnanext/wolf-cna
```
