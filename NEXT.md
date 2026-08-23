# Next work

## Last completed work

- The exit now presents a centered `LEVEL COMPLETE` card while keeping the final total in the status bar.
- Score is uncapped: gold, defeated enemies and the exit add points.
- Doors close after four seconds, but remain open when a dead guard or hound blocks the doorway.
- Generated CNA audio now covers shots, defeated enemies, pickups, doors, locks and player damage.

## Next tasks

1. Playtest the authored route and rebalance enemy, ammo and health placement.
2. Add a second objective loop before the exit, such as a terminal or power switch that opens a route.
3. Extend the authored content with another route/encounter section before considering advanced vertical features such as elevators.

## Verification

```bash
cmake --build build-cnanext --target wolf-cna level-definition-tests -j8
ctest --test-dir build-cnanext --output-on-failure
./build-cnanext/wolf-cna
```
