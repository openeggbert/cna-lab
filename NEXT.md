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
- Health, ammunition and four treasure values now use original transparent sprites instead of colored blocks.
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
- Every map `GOAL` now corresponds to a three-sided steel elevator cabin whose polygonal gate starts retracted sideways so the exit is immediately usable.
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
- Losing a non-final life now shows a short `LIFE LOST` transition, rolls score/extra-life progress back to sector entry and rebuilds the entire sector with the basic sidearm loadout; the final life still enters game over.
- Campaign metadata now defines two named chapters, five selectable sectors, a hidden Foundry elevator, a menu-hidden 64×64 reservoir and a deterministic return to Labs; all routes are tested.
- Warden Core adds an original 32-health Bunker Warden with a dedicated HUD bar, AI-generated idle/attack/pain/defeated sprites, a three-projectile fan, tested final-elevator lockdown and a separate campaign-complete screen.
- Sector completion now awards authored target-time, clear and 100% category bonuses and presents the exact percentages and award breakdown.
- Profile version 5 migrates versions 1–4 and persists a validated best-eight table; qualifying finales use a three-letter arrow-key initials editor and show the top three scores.
- `A`/`D` now strafe while arrows retain classic movement and turning; diagonal walking and run-plus-strafe are speed-normalized.
- The CNA control screen rebinds ten actions with conflict swapping, reserved system keys, classic-default restoration and five keyboard turn speeds.
- Profile version 6 migrates versions 1–5 and strictly persists unique bindings, turn speed and the existing high-score table.
- Cyan and amber cards now have matching locked doors, independent HUD icons and a version-3 run-save access mask with V1/V2 migration.
- Every sector now uses 10/25-health pickups, 4/8-round supplies and a 1,000-point peace prism; rare recovery beacons grant full health plus one life.
- Ammo and duplicate weapons respect the 99 cap, while enemy drops depend on archetype, difficulty and the strongest carried weapon; card-aware BFS tests keep every route solvable.
- Four new genuine-alpha AI-generated pickup sprites are committed with complete provenance.
- Secret `S` cells are now full polygonal push-wall blocks that move one or two cells away from the player, pause before actors and never return.
- Push-wall collision, navigation and visited-cell automap state share the same interpolated position; run-save version 4 persists it and migrates versions 1–3.
- Weapon profiles now define range, near/far damage, stationary/moving spread and cadence; explicit seeded sequences make firearm outcomes replayable.
- Held automatics emit and charge one round per projectile, dynamic doors/push walls block hitscan, and rebalanced enemy health preserves the audited clear budget.
- Four main sectors gained only the ammunition their stricter Veteran close-sidearm audits required; all six sectors now enforce per-difficulty clear budgets.
- Run-save version 5 persists the combat-shot sequence and migrates versions 1–4.
- World sounds now pan from the listener's facing and attenuate by distance through CNA; event bursts are capped at four voices while UI feedback stays centered.
- Campaign metadata selects one of two original generated ambient loops, and living/defeated hounds now have deterministic positional bark/whimper voices.
- The `I+L+M` implementation was re-audited: it grants 99 rounds and resets score plus sector score checkpoints to zero.
- Ordinary, access and elevator doors now stay at floor height and slide left or right into a real wall pocket; push walls retain their separate full-cell motion.
- The blue HUD now adds an original animated status portrait with healthy, wounded, critical, attack, hurt and defeated expressions while retaining card indicators and `LEVEL / SCORE / LIVES / HEALTH / AMMO / WEAPON` order.
- Pressing action on a fully open ordinary door now closes it deliberately when the doorway is clear; players, living enemies and defeated bodies safely block both manual and automatic closure.
- WOLF-014 is complete: guard, rapid trooper, heavy unit and Warden alert/attack events retain their archetype and select four distinct original generated positional cue families.
- Hound bark noise is gently filtered while preserving its louder two-part alert bark and distinct defeat whimper.
- WOLF-047 is complete: Emscripten 6.0.3 builds a CNA `WEBGL2` HTML/JS/WASM/data deployment with every game asset preloaded.
- The corrected web target uses the matching `cnanext`/`sharp-runtimenext` pair, JavaScript-lowered exceptions and Asyncify so CNA's blocking browser loop preserves the game object's lifetime; executable-level WebGL 2 limits prevent Firefox from silently creating WebGL 1.
- WOLF-002 is complete: CNA relative mouse mode turns the player with a fixed horizon, the left and right buttons attack and activate, and the cursor is captured only during live gameplay so every menu keeps working.
- Control setup switches the mouse on or off with five speed steps; profile version 7 persists both and migrates versions 1–6.

## Current handoff

- Work is on `develop`; do not modify `main`.
- Native `build-cnanext` compiles `wolf-cna` and `level-definition-tests`; the focused test suite passes.
- Web output is in ignored `build-web-cnanext/` as `wolf-cna.html`, `.js`, `.wasm` and `.data`; all four files were rebuilt from a clean configure against explicit `cnanext`/`sharp-runtimenext` paths and served successfully over local HTTP.
- Firefox 140.10.1 ESR completed a clean-profile headless smoke test without a page exception and rendered the 800×480 Wolf CNA title screen; manual audio/fullscreen testing still needs an interactive browser.
- Mouse control has not been played yet. It is covered by unit tests for the yaw
  conversion, the per-frame clamp and profile migration, and the game starts and
  runs, but capture, cursor release and pointer feel need a real window. Browser
  pointer lock is entirely unverified.
- Control setup now holds fifteen rows plus its prompt in the 260px panel, so the
  row step dropped to 13px. The arithmetic leaves the last line clear of the
  bottom border, but the screen has not been looked at; it is worth one glance.
- The web target now also builds and passes `level-definition-tests.js` under
  node. Attempts to screenshot the game under Xvfb produced blank captures, so
  automated visual checks of menus are not currently possible that way.
- The stale `build-web-cna-old-20260824/` directory was deleted.

## Known gaps in difficulty scaling

An audit traced every difficulty field to a runtime effect. The system is real, not
a stub: enemy counts (`World.cpp` spawn-tier `continue`), enemy health, move speed,
firing cadence, incoming damage and ammunition all scale, and difficulty is
correctly re-derived on every level load, life loss and save load. Three gaps
remain, in priority order:

1. **Reaction time does not scale at all.** `reactionDuration`, `hearingRange`,
   `viewDotThreshold` and `EnemyWakeRange` are identical on every difficulty, so a
   Veteran guard notices the player exactly as slowly as a Scout guard. This is the
   part of "higher difficulty means more aggressive enemies" that is simply absent.
2. **Only one ranged enemy may fire at a time**, on every difficulty
   (`designatedRangedAttacker`). Veteran's extra spawns therefore add health to
   grind through but no additional incoming fire, which is why the higher
   difficulties do not feel proportionally harder.
3. **Player health never scales**: 100 HP, 3 lives and 25/10 HP pickups are
   identical on all three.

Also worth knowing: the incoming-damage multiplier is only asserted at the constant
table level in the tests, never end to end to `health_`.

## Next tasks

1. Play the native build and confirm the mouse: turning feel at each speed step, that `Escape` frees the cursor into the pause menu, that the splash and menus stay clickable, and that switching `MOUSE` off leaves keyboard turning intact.
2. Rebuild the web target and check whether CNA's relative mouse mode reaches browser pointer lock; if it does not, that is a genuine CNA finding to record under `plan.md` §17 rather than a game bug.
3. Interactively smoke-test menu/game keyboard input, audio unlock and fullscreen behavior in `build-web-cnanext/wolf-cna.html`; basic loading and title-screen rendering already pass in Firefox.
4. Subjectively playtest the animated HUD, positional audio, lateral doors and all three deterministic difficulty profiles.
5. Playtest save slots, life loss, push walls and the full six-sector route including the hidden branch.

The classic 1992 gaps still open are listed at the end of `plan.md` §16: a fourth
difficulty, attract-mode demo playback, adjustable viewport size, separate music
and effect volume, an inter-sector loading screen and a wider set of blocking
props. Prefer those over new M8-style extras.

Longer-term M7 work keeps true vertical spaces and moving elevators separate from
the current campaign-transition cabins.

## Verification

```bash
cmake -S . -B build -DCNA_GRAPHICS_RENDERER=OPENGLES3
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/wolf-cna
```

The web target reuses `build-web-cnanext` and needs the emsdk on `PATH`:

```bash
source ~/emsdk/emsdk_env.sh
cmake --build build-web-cnanext -j
node build-web-cnanext/level-definition-tests.js
```
