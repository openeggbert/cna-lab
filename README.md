# Black Pine

Black Pine is an original C++23 demonstration game for the **Explore2D**
2D exploration/adventure engine. Its purpose is to exercise the engine's room,
inventory, exploration and persistent-state model with a small game that can be
played from start to finish.

No Tajná mise rooms, story, dialogue or art are reused.

## Premise

A storm has knocked the Black Pine mountain radio relay off the air. Starting at
the trailhead, you must reach the caretaker, find a way through the relay yard,
repair the generator, climb the tower, realign the antenna, and bring the
emergency radio net back online.

There is also optional material to discover and one deliberately dangerous side
route.

## What the demo exercises

- Seven fixed connected screens.
- Six inventory items, including optional/non-usable discoveries.
- USE / EXAMINE / TAKE.
- A character with state-dependent dialogue.
- Compact blue dialogue bubbles that alternate between Mara and the player.
- Examination that reveals a hidden key.
- Overlapping hotspots (the key sits on the desk) to exercise action-aware
  target resolution.
- A locked conditional exit.
- Using the key on the gate.
- Installing/consuming a fuse and patch cable.
- Contextually operating a generator lever.
- A contextual ladder transition to a separate tower-top screen.
- Using a tool to repair/alignment state.
- A final condition-gated win interaction.
- Visited-location fast travel.
- A ravine hazard, death and restart.
- Save/load through the Explore2D host.
- A configurable code-drawn title/menu screen before play begins.
- Seven original code-drawn scenes using only the fixed 16-colour EGA palette.
- Visible state changes for the gate, installed generator components, power
  lamps, tower beacon and antenna alignment.
- Selective animation: generator startup and antenna alignment are one-shot
  actions; powered lamps, console scan and tower beacon loop while appropriate.
- Monophonic QBasic/PC-speaker-style cues for menu, movement actions, pickups,
  repairs, warnings, death, save/load and victory, played through CNA.
- Entirely procedural graphics; there are no external art assets.
- Complete English and Czech localization for the current game, including
  menus, settings, room and hotspot names, inventory, descriptions, dialogue,
  system feedback, hazards and ending text.
- Context-sensitive F1 help that follows the actual puzzle state and suggests
  the next required action without changing progress.

## Build

The project expects Explore2D next to it by default:

```text
parent/
  explore-2d/
  black-pine/
```

Then configure against a **complete CNA checkout** (including its required
third-party/submodule content):

```bash
cmake -S black-pine -B black-pine/build \
  -DCNA_SOURCE_DIR=/path/to/cna
cmake --build black-pine/build -j
./black-pine/build/black-pine
```

If Explore2D is elsewhere:

```bash
-DEXPLORE2D_SOURCE_DIR=/path/to/explore-2d
```

For a headless rules-only build/test, CNA is unnecessary:

```bash
cmake -S . -B build-core \
  -DEXPLORE2D_BUILD_CNA=OFF \
  -DBLACK_PINE_BUILD_TESTS=ON
cmake --build build-core -j
ctest --test-dir build-core --output-on-failure
```

The scenario test scripts the complete required puzzle chain through victory and
also verifies the hazard/death/restart path.

## Controls

| Key | Action |
|---|---|
| Left / Right | turn or walk |
| Up | cycle USE / EXAMINE / TAKE |
| Down | perform selected verb |
| 1 | USE |
| 2 | EXAMINE |
| 3 | TAKE |
| Enter / Space | contextual action, otherwise jump |
| M | travel to a discovered anchor |
| S / L | save / load |
| F11 | toggle window / fullscreen |
| F1 | show or close the context-sensitive next-step hint |
| Q | quit |
| Up / Down + Enter | choice/map navigation |
| Escape | pause from the world; cancel/back in choices and menus |
| Left / Right in Settings | change language |

Black Pine inherits Explore2D's fixed 640×350 display, room/inventory/action
layout and palette. Its mountain, forest, cabin, machinery and title artwork are
original combinations of rectangles, lines, circles, ellipses and bitmap text
declared in `BlackPineWorld.cpp`. Only story-relevant actions and machinery are
animated; static scenery deliberately remains static.

## Language settings

Choose **Settings** on the title screen to switch between English and Czech.
During play, press **Escape**, choose **Settings**, and change the language
without restarting or altering the current game state. Explore2D stores the
preference in `black-pine.e2dsettings` and restores it on the next launch.

English is the canonical fallback. Czech is supplied as UTF-8 text and rendered
by Explore2D's code-drawn bitmap font, including Czech diacritics. The
pre-production `docs/GAME_DESIGN.md` intentionally remains English-only.

## Suggested route if you are testing mechanics

Explore the trailhead, talk to Mara in the cabin, inspect the desk, open the yard
gate, repair both missing parts of the generator circuit, operate the main lever,
climb the tower, align the antenna mount, and use the relay console.

The game intentionally does not print a full item-by-item solution in its HUD;
EXAMINE is expected to matter.

If you become stuck, press **F1**. Explore2D pauses the action and Black Pine
selects the highest-priority unfinished objective whose conditions match the
current session. The hint is localized and closes with F1, Enter, or Escape.

## License

Black Pine is available under the [MIT License](LICENSE).
