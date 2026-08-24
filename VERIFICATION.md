# Verification

Verified on 2026-08-24.

The automated scenario performs the required gameplay chain:

```text
collect cable
-> talk to Mara
-> examine desk / reveal key
-> take key and wrench
-> unlock yard
-> take/install fuse
-> patch blue terminals
-> start generator
-> climb tower
-> align antenna with wrench
-> activate relay console
-> victory
```

It also runs a separate ravine hazard -> death -> restart path.

The palette renderer additionally generates title, message and seven-room
640×350 previews for visual inspection. Black Pine uses only Explore2D's 16 EGA
colours and code-drawn primitives; it has no bitmap art dependency.

The demo was built against the complete sibling CNA checkout and run for two
frames through Explore2D's CNA host with SDL's dummy video driver. No CNA or
sharp-runtime sources were changed.
