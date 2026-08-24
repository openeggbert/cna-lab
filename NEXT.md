# Explore2D next steps

The 0.1 prototype establishes the fixed-screen/rule/inventory architecture and a
minimal CNA host. Good follow-up work, in roughly this order:

1. Add a formal custom-mode/minigame interface that can temporarily replace the
   ordinary room controls while still reading/writing `SessionSnapshot` state.
2. Add external world serialization (JSON/YAML or a purpose-built format) and
   content-version metadata for saves.
3. Add richer scripted sequences: waits, presentation cues, conditional
   branches and explicit choice dialogue.
4. Optionally add external localization catalogs and translation-completeness
   tooling on top of the implemented `LocalizedText` runtime model.
5. Add a room/hotspot debug overlay and eventually an authoring/editor tool.
6. Add gamepad input mapping and configurable controls.
7. Add install/export CMake packaging once the API stabilizes.
8. Expand deterministic tests around transitions, overlapping hotspots, save
   migration and rule priority conflicts.

Selective palette animation, QBasic-like GET/PUT drawing, speaker-anchored
bubbles, action poses and monophonic tone effects are implemented in the current
prototype rather than remaining future work.
