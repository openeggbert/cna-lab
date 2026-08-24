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
- Pressing and releasing `M` opens a paused map of visited cells; a latched cheat chord keeps gradual `I+L+M` input independent.
- The map always marks the sector exit as red `GOAL`, changing it to cyan after its objective is activated, without revealing nearby rooms.
- Firearm attack frames remain visible for a few extra frames so their generated muzzle flashes read clearly.
- Every sector now requires activating a separate violet power relay and amber terminal before its exit comes online.
- Every sector now places three freestanding plant landmarks with its own original transparent storage, foundry or laboratory sprite.
- Every sector now has two solid polygonal tables with four-leg silhouettes, static geometry and matching player/enemy collision.
- The automap now shows independent color-coded `POWER` and `TERMINAL` progress without revealing either objective's location.
- The automap now includes a compact side legend for player, door, lock, discovered-secret and goal colors.
- A fourth progressively unlocked 64×64 archive sector adds a violet/bronze material palette, altered combat roster and original archive-palm landmarks.
- Relay and terminal interactions now show `POWER ONLINE`, `TERMINAL ONLINE` or `EXIT ONLINE` for two seconds.
- The illustrated splash is separate from the main menu.
- Every campaign sector now has an exact authored 64×64 footprint; tests require substantial use of the area and no disconnected rooms.

## Next tasks

1. Playtest the authored route and rebalance health placement and difficulty-specific incoming damage.
2. Playtest sprite motion amplitudes together with the authored combat route.
3. Playtest the full four-sector objective route and completion pacing.

## Verification

```bash
cmake --build build-cnanext --target wolf-cna level-definition-tests -j8
ctest --test-dir build-cnanext --output-on-failure
./build-cnanext/wolf-cna
```
