# Next work

## Last completed work

- Doors defer automatic closing while the player or a defeated enemy occupies the doorway.
- Guards and hounds require multiple hits; rapid troopers and heavy units add distinct ranged combat roles.
- Primitive weapon shapes are replaced by original transparent high-resolution knife, sidearm, repeater and heavy-automatic sprites.
- Ceiling lamps now cast a warm, softly fading pool of light onto the floor below.
- Only one visible ranged enemy attacks at a time, with lower damage and slower archetype-specific cadences.
- Health, ammunition and three treasure values now use original transparent sprites instead of colored blocks.
- Holding Ctrl repeats repeater and heavy-automatic bursts at distinct bounded cadences; knife and sidearm remain one attack per press.
- Health kits are preserved when the player is already at 100% and can be collected after later damage.
- Weapon sprites now lunge or recoil during attacks instead of remaining completely static.
- The illustrated splash is separate from the main menu.
- Every campaign sector now has an exact authored 64×64 footprint; tests require substantial use of the area and no disconnected rooms.

## Next tasks

1. Playtest the authored route and rebalance the four enemy archetypes, ammunition and health placement.
2. Add attack/death animation frames for enemies and weapon attack animation frames.
3. Implement `WOLF-018`: an `M`-key floor map that reveals only explored cells and does not conflict with `I+L+M`.
4. Add another objective interaction and sector-specific freestanding room decorations.

## Verification

```bash
cmake --build build-cnanext --target wolf-cna level-definition-tests -j8
ctest --test-dir build-cnanext --output-on-failure
./build-cnanext/wolf-cna
```
