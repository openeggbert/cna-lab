# Black Pine completion plan

Last updated: 2026-08-24

## Objective

Finish Black Pine as a complete, human-playable 124-screen Explore2D adventure
while preserving its fixed-screen, 16-colour, code-drawn presentation and its
non-combat exploration design. Automated reachability is already complete; the
remaining work is regional visual authoring, human usability validation and
release hardening.

## Current baseline

- Branch: `develop`.
- All 124 canonical locations and the complete five-act victory route exist.
- English and Czech localization, F1 guidance, save/load, sound, fullscreen,
  travel, hazards and three epilogues are implemented.
- Screens 1-103 have completed human-playability passes with authored routes,
  visible portals, region-specific artwork and a physical scenario traversal.
- Screens 104-124 remain scripted and reachable but still use broad catalogue
  routing and several generic visual motifs.
- Native CNA and AddressSanitizer scenario builds pass.
- A reproducible Emscripten build profile is available through
  `scripts/build-web.sh` and the `web-emscripten` CMake preset.

## Completed regional passes

| Region | Screens | State |
|---|---:|---|
| Storm gate and caretaker hub | 1-12 | Audited |
| Relay yard and local power | 13-24 | Audited |
| North forest | 25-38 | Audited |
| Quarry and ravine | 39-50 | Audited |
| Logging railway | 51-63 | Audited |
| Reservoir and dam | 64-75 | Audited |
| Mine and underground power | 76-90 | Audited |
| Observatory and Nightjar entrance | 91-103 | Audited |
| Nightjar bunker | 104-115 | Scripted; next implementation target |
| Summit and transmitter | 116-124 | Scripted |

## Next implementation milestone: Nightjar bunker

Author screens 104-115 as a physical facility rather than a linear sequence.
The required topology and puzzle order are defined in `docs/GAME_DESIGN.md`.

1. Build the decontamination entrance and the main-corridor hub.
2. Branch the hub to the phase laboratory, machine shop, capacitor hall,
   command archive, holding room and emergency stair.
3. Add two-way links for calibration, test cell, cooling and rescue routes.
4. Give decontamination, patrol containment, diagnostic equipment, grounding,
   coolant diversion, archive copying and Kline's rescue persistent before/after
   artwork.
5. Rewrite F1 guidance so every step names a label visible in the current room.
6. Rewrite the scenario to traverse every required door and return path through
   public session controls.
7. Render default and completed states, inspect them, run native and sanitizer
   tests, update the audit, and commit the region independently.

## Following milestone: summit and endings

After the bunker pass, author screens 116-124 with a branching climb, readable
lightning shelter timing, persistent grounding and antenna repairs, and distinct
presentations for all three successful epilogues.

## Release-hardening backlog

1. Exercise every lethal warning and checkpoint restart through public inputs.
2. Perform a complete human keyboard playthrough in both languages and record
   every navigation, collision, dialogue and hint issue.
3. Add a content/save-schema version before changing any shipped puzzle flag.
4. Validate browser controls, audio unlock, fullscreen and persistence in an
   interactive browser session.
5. Produce release archives only after native and web release builds pass from
   clean build directories.

## Invariants for future work

- Do not replace physical traversal with snapshot restoration or test-only
  teleportation.
- Do not expose catalogue screen numbers in gameplay or F1 text.
- Keep all runtime art procedural and inside the 16-colour EGA palette.
- Animate only story-relevant signals, hazards, people or machinery.
- Preserve both English and Czech text for every player-visible addition.
- Keep `docs/GAME_DESIGN.md` English-only.
- Update `docs/PLAYABILITY_AUDIT.md`, `NEXT.md` and `VERIFICATION.md` after each
  regional pass.

## Verification commands

```bash
cmake -S . -B build-core \
  -DEXPLORE2D_BUILD_CNA=OFF \
  -DBLACK_PINE_BUILD_TESTS=ON
cmake --build build-core -j
ctest --test-dir build-core --output-on-failure
```

For the browser build, activate or install Emscripten and run:

```bash
./scripts/build-web.sh
python3 -m http.server 8080 --directory build-web-emscripten
```

Then open `http://localhost:8080/black-pine.html`.
