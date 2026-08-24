# Explore2D next steps

The 0.1 prototype establishes the fixed-screen/rule/inventory architecture and a
minimal CNA host. Good follow-up work, in roughly this order:

1. Add a formal custom-mode/minigame interface that can temporarily replace the
   ordinary room controls while still reading/writing `SessionSnapshot` state.
2. Add palette-indexed procedural animation primitives while preserving the
   fixed 640×350 display, 16-colour palette and shared interface.
3. Add an audio service at the CNA host boundary (music, one-shot effects,
   dialogue cues), keeping the gameplay core headless.
4. Add external world serialization (JSON/YAML or a purpose-built format) and
   content-version metadata for saves.
5. Add richer scripted sequences: waits, presentation cues, conditional
   branches and explicit choice dialogue.
6. Add localization keys rather than embedding user-facing strings directly in
   world definitions.
7. Add a room/hotspot debug overlay and eventually an authoring/editor tool.
8. Add gamepad input mapping and configurable controls.
9. Add install/export CMake packaging once the API stabilizes.
10. Expand deterministic tests around transitions, overlapping hotspots, save
    migration and rule priority conflicts.
