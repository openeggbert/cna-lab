# Verification

Verified on 2026-08-24.

The automated scenario now performs the full five-act gameplay chain:

```text
relay repair and Nightjar trace (screens 001-024)
-> Theo rescue, forest bearing, ravine and quarry hoist (025-050)
-> logging engine, dam drainage, mine power and ridge lift (051-090)
-> observatory archive, Sable, bunker inversion and Kline rescue (091-115)
-> summit grounding, phase assembly, evidence upload and open channel (116-124)
-> evidence-broadcast victory
```

The test verifies all 124 canonical IDs, all 17 map anchors, bidirectional room
connectivity, complete English/Czech content, exactly 64 inventory records,
contextual F1 hints, item-dependent USE resolution and the entire state
progression. Its main route walks Iris through room boundaries and into hotspot
range using the public session controls; it no longer teleports by restoring
synthetic snapshots. All 124 locations must be physically visited before the
final broadcast. Separate terminal-state checks cover Carrier Restored, Open
Channel and Keeper of Black Pine, and a public-input route into the live feeder
verifies death followed by restart.

The palette renderer generates the title, a message state and all 124 room
previews at 640×350. Representative automated render coverage spans all ten
regions. Black Pine uses only Explore2D's 16 EGA colours and code-drawn
primitives; it has no bitmap art dependency. Location artwork and F1 guidance
use place names rather than visible catalogue numbers.

The full game was built against the complete sibling CNA checkout and run for two
frames through Explore2D's CNA host with SDL's dummy video and audio drivers.
This initializes the generated title tone through CNA's `SoundEffect` path.
The F11 binding compiles against CNA's fullscreen toggle API. No CNA or
sharp-runtime sources were changed.

## Regional playability status

The authored, physical-route audit currently covers screens 1-103: the storm
gate, relay, forest, quarry, logging railway, reservoir, dam, mine, underground
power, observatory and Nightjar entrance. These routes use visible doors,
ladders and paths, persistent before/after art, contextual hints and public
session controls. Screens 104-124 remain fully scripted and reachable, but the
Nightjar bunker and summit still require the same human-playability and visual
authoring pass. `plan.md` is the canonical continuation handoff.

## Browser build

The `web-emscripten` CMake preset and `scripts/build-web.sh` provide a
reproducible Emscripten build against the sibling Explore2D and CNA checkouts.
The wrapper sets the additional `EMSCRIPTEN` variable required by CNA's Draco
dependency, disables an unusable system `ccache`, and checks for non-empty
`black-pine.html`, `black-pine.js` and `black-pine.wasm` outputs.
Black Pine's CMake adds a web-only forced `<algorithm>` include to `draco_io`
because CNA's bundled `ply_reader.cc` uses `std::all_of` without including that
header. This keeps the compatibility workaround local and leaves CNA and
sharp-runtime unchanged. The executable link also selects CNA's
`-fwasm-exceptions` mode so Emscripten uses the matching C++ runtime.
The checked-in `web/shell.html` presents the same 1280×700 integer-scaled game
surface as the native CNA window without Emscripten's generic logo, console or
resize controls.

For presentation-parity diagnosis, `black_pine_render_preview` was compiled
both natively and as WebAssembly. Their complete PPM output trees—title,
trailhead, message, all 124 rooms and persistent completed states—were
byte-for-byte identical. Any remaining platform difference is therefore outside
the Explore2D software framebuffer and belongs to the CNA/SDL presentation or
browser display layer.

```bash
./scripts/build-web.sh
python3 -m http.server 8080 --directory build-web-emscripten
```

The web verification in this document covers configure, compile, link and
artifact checks. Browser input, audio unlock, fullscreen and persistence remain
explicit release-hardening tasks until they have been exercised interactively.

The release profile was compiled and linked successfully with Emscripten 6.0.3
on 2026-08-24. The artifact checks produced non-empty HTML (19,605 bytes),
JavaScript (186,951 bytes) and WebAssembly (4,469,968 bytes) files. The native
CNA build and `black-pine.scenario` test were then rerun successfully after the
web compatibility changes.
