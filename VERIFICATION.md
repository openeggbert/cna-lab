# Verification

Verified on 2026-08-24 with GCC 14.2 and CMake 3.31-compatible project syntax.

## Passed

- Clean Explore2D core configure/build.
- `explore2d.core` headless test.
- Explore2D CNA adapter compile against the complete sibling CNA checkout.
- Full Black Pine build against that CNA checkout.
- Full Black Pine scenario test through the winning interaction.
- Independent Black Pine hazard/death/restart test.
- Two-frame CNA-host smoke run of the demo with SDL's dummy video and audio
  drivers, including title-tone construction and playback through CNA.
- Canvas palette, flood-fill, polygon, palette capture/blit and XOR tests.
- QBasic timer-tick tone synthesis test, including an explicit rest.
- Looping-frame selection and one-shot animation state tests.
- Player TAKE pose and per-line player/target speech-anchor tests.
- F11 fullscreen binding compiled against CNA's
  `GraphicsDeviceManager::ToggleFullScreen()` API.
- Configurable title renderer at the fixed 640×350 resolution.
- CPU renderer previews generated for the title, HUD, messages, speaker-anchored
  bubbles, TAKE pose, generator action and all seven Black Pine rooms; every
  game-facing draw call is limited to the EGA palette.
- The dependency-free English website, developer guide and 24-lesson tutorial
  are served successfully over local HTTP. The bundled validator confirms all
  three pages, lesson/checkpoint numbering, copy targets, internal anchors and
  local assets; HTML, CSS, JavaScript and social-preview responses all succeed.

## CNA dependency

The graphical build was verified with a complete CNA checkout, including its
vendored third-party content. No CNA or sharp-runtime sources were changed.
