# Next work

## Last completed work

- Doors defer automatic closing while the player or a defeated enemy occupies the doorway.
- Guards and hounds require multiple hits; rapid troopers and heavy units add distinct ranged combat roles.
- Primitive weapon shapes are replaced by original transparent high-resolution knife, sidearm, repeater and heavy-automatic sprites.
- Ceiling lamps now cast a warm, softly fading pool of light onto the floor below.
- Only one visible ranged enemy attacks at a time, with lower damage and slower archetype-specific cadences.
- Health, ammunition and three treasure values now use original transparent sprites instead of colored blocks.
- The illustrated splash is separate from the main menu, and campaign sector size now targets an authored 64×64 footprint.

## Next tasks

1. Playtest the authored route and rebalance the four enemy archetypes, ammunition and health placement.
2. Add attack/death animation frames for enemies and weapon attack animation frames.
3. Expand each campaign sector to an authored 64×64 room-and-corridor footprint with dimension tests.

## Verification

```bash
cmake --build build-cnanext --target wolf-cna level-definition-tests -j8
ctest --test-dir build-cnanext --output-on-failure
./build-cnanext/wolf-cna
```
