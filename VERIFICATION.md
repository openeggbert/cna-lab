# Verification

Verified on 2026-08-24 with GCC 14.2 and CMake 3.31-compatible project syntax.

## Passed

- Clean Explore2D core configure/build.
- `explore2d.core` headless test.
- Explore2D CNA adapter compile against the complete sibling CNA checkout.
- Full Black Pine build against that CNA checkout.
- Full Black Pine scenario test through the winning interaction.
- Independent Black Pine hazard/death/restart test.
- Two-frame CNA-host smoke run of the demo with SDL's dummy video driver.
- CPU renderer preview generated successfully at 640x360.

## CNA dependency

The graphical build was verified with a complete CNA checkout, including its
vendored third-party content. No CNA or sharp-runtime sources were changed.
