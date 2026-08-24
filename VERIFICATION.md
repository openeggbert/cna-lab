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
connectivity, complete English/Czech content, more than 50 items, contextual F1
hints, item-dependent USE resolution and the entire state progression.

The palette renderer generates the title, a message state and all 124 room
previews at 640×350. Representative automated render coverage spans all ten
regions. Black Pine uses only Explore2D's 16 EGA colours and code-drawn
primitives; it has no bitmap art dependency.

The full game was built against the complete sibling CNA checkout and run for two
frames through Explore2D's CNA host with SDL's dummy video and audio drivers.
This initializes the generated title tone through CNA's `SoundEffect` path.
The F11 binding compiles against CNA's fullscreen toggle API. No CNA or
sharp-runtime sources were changed.
