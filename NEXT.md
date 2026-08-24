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
- The map always marks the sector exit as red `GOAL`, changing it to cyan after its objective is activated, without revealing nearby rooms.
- Firearm attack frames remain visible for a few extra frames so their generated muzzle flashes read clearly.
- Firearm attack frames use CNA straight-alpha blending and partial opacity over the solid weapon view, preventing transparent RGB from becoming a white rectangle.
- Every sector now requires activating a separate violet power relay and amber terminal before its exit comes online.
- Every sector now places three freestanding plant landmarks with its own original transparent storage, foundry or laboratory sprite.
- Every sector now has two solid polygonal tables with four-leg silhouettes, static geometry and matching player/enemy collision.
- The automap now shows independent color-coded `POWER` and `TERMINAL` progress without revealing either objective's location.
- The automap now includes a compact side legend for player, door, lock, discovered-secret and goal colors.
- A fourth progressively unlocked 64×64 archive sector adds a violet/bronze material palette, altered combat roster and original archive-palm landmarks.
- Relay and terminal interactions now show `POWER ONLINE`, `TERMINAL ONLINE` or `EXIT ONLINE` for two seconds.
- Health placement now scales from two kits in early sectors to three in labs/archive; tests require enough guaranteed ammunition for a full clear.
- The illustrated splash is separate from the main menu.
- Every campaign sector now has an exact authored 64×64 footprint; tests require substantial use of the area and no disconnected rooms.
- Sector unlocks, master sound and the last selected difficulty now persist in one validated, legacy-compatible profile.
- WOLF-013 is complete: all enemy and weapon states use original provenance-recorded sprites with deterministic animation and translucent firing feedback.
- Holding left or right `Shift` now runs at 1.65× walking speed while retaining the clamped frame step and existing collision path.
- Four original generated wall families now alternate in coherent room-scale regions, and every polygonal table uses a dedicated dark-oak texture.
- Access cards, both weapon pickups, terminals, power relays, exits and enemy projectiles now use original transparent sprites; the last untextured colored cuboid path is gone.
- Every map `GOAL` now corresponds to a three-sided steel elevator cabin whose red polygonal gate blocks entry and rises only after the sector objective is complete.
- Holding `G+O+A+L` once teleports the player to the safe approach cell outside the current elevator and turns them toward its doors without changing objective progress.
- `Space` now activates an online elevator from its approach; an offline elevator responds with visible `EXIT OFFLINE` feedback and a lock sound.
- Enemy projectiles now create a short expanding project-generated spark on impact; player hits also add a translucent amber flash and a generated impact sound.
- Sector completion now plays a deterministic original four-note fanfare through CNA audio in addition to the elevator confirmation.

## Next tasks

1. Playtest the authored route and rebalance health placement and difficulty-specific incoming damage.
2. Playtest sprite motion amplitudes together with the authored combat route.
3. Playtest the full four-sector objective route and completion pacing.

Longer-term M7 work keeps true vertical spaces and moving elevators separate from
the current campaign-transition cabins.

## Verification

```bash
cmake --build build-cnanext --target wolf-cna level-definition-tests -j8
ctest --test-dir build-cnanext --output-on-failure
./build-cnanext/wolf-cna
```
