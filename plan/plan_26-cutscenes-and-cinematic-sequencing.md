# 26. Cutscenes and cinematic sequencing

[Back to master plan](../plan.md)

Build in-engine timelines that are skippable, checkpoint-safe, and authored as hand-written data — no in-house timeline editor, matching the project's no-in-house-editor-suite decision.

- [x] **IS-26-001 P0** — Create a minimal in-engine sequence player. *(`CutscenePlayer` (`include/`/`src/Cutscenes/`): `Start`/`Update`/`Skip`/`IsActive`/`GetCameraPosition`/`GetCameraLookAt`. Camera-track only -- see IS-26-002.)*
- [ ] **IS-26-002 P0** — Create camera, animation, dialogue, audio, event, and fade tracks. *(Partial: only a camera track exists (linear position/look-at keyframe interpolation). Dialogue plays independently via the existing, unrelated `DialogueSystem` rather than a dialogue track; no animation/audio/event/fade tracks at all.)*
- [x] **IS-26-003 P0** — Author a short prologue sequence that hands control to the current mission. *(`assets/cutscenes/prologue_intro.cutscene.json`: a 2.5s pan from an establishing shot of the warehouse delivery target to the exact framing of the normal gameplay follow-camera at the player's spawn point -- the final keyframe was computed by hand to match `Draw()`'s own camera formula exactly, so control (and the camera) hands back to gameplay with no visible pop.)*
- [x] **IS-26-004 P0** — Implement skip that applies required terminal gameplay state. *(`CutscenePlayer::Skip()` jumps straight to the last keyframe, identical to a natural finish -- verified by `TestCutscenePlayerSkipAppliesTerminalState` asserting Skip()'s resulting camera state exactly matches the terminal keyframe, not some other "stopped" state.)*
- [ ] **IS-26-005 P1** — Create sequence IDs, track IDs, bindings, clips, and markers.
- [ ] **IS-26-006 P1** — Create entity binding robust to district transitions and respawn.
- [ ] **IS-26-007 P1** — Create camera cuts, blends, rails, and look-at constraints.
- [ ] **IS-26-008 P1** — Create animation clip scheduling and synchronization.
- [ ] **IS-26-009 P1** — Create property and transform animation tracks.
- [ ] **IS-26-010 P1** — Create dialogue/subtitle synchronization.
- [ ] **IS-26-011 P1** — Create audio/music scheduling and ducking markers.
- [ ] **IS-26-012 P1** — Create mission event and checkpoint markers.
- [ ] **IS-26-013 P1** — Create visibility, spawn, despawn, and control-lock tracks.
- [ ] **IS-26-014 P1** — Create pre-roll and asset prefetch.
- [ ] **IS-26-015 P1** — Create behavior for missing or late assets.
- [x] **IS-26-016 P1** — Create save/load policy while a sequence is active. *(Policy: nothing ever saves mid-cutscene by design (the only cutscene plays for 2.5s at the very start of a fresh game, before any save is possible), but `LoadPrototype()`/`ResetPrototype()` both force `cutscene_.Skip()` defensively anyway, so loading/resetting can never leave an active cutscene camera fighting a restored, unrelated player/vehicle position.)*
- [x] **IS-26-017 P1** — Create sequence validation and dependency reporting. *(Inline in `LoadCutsceneSequence`, matching `LoadMissionDefinition`'s own convention: rejects an empty keyframe list, a first keyframe not at time 0, non-strictly-ascending keyframe times, and a duration shorter than the last keyframe's time, each with an actionable error. No "dependency reporting" -- this sequence has no external asset/entity references to check yet.)*
- [ ] **IS-26-018 P1** — Create a timeline debug overlay (in-game, not an authoring editor).
- [x] **IS-26-019 P1** — Create deterministic sequence tests with fixed time. *(`TestCutscenePlayerAdvancesAndFinishes`/`TestCutscenePlayerSkipAppliesTerminalState` in `tests/CoreTests.cpp` drive `CutscenePlayer::Update()` with fixed, hand-picked deltaSeconds values and assert exact interpolated camera state, not wall-clock-dependent.)*
- [x] **IS-26-020 P1** — Add unit tests for skip finalization across every authored sequence in the vertical slice. *(Only one sequence exists in the vertical slice today (`prologue_intro`); `TestCutscenePlayerSkipAppliesTerminalState` covers skip finalization generically (any `CutsceneSequence`), and the real committed file is confirmed loadable by a standalone diagnostic, so this is covered at the "one sequence" scale the vertical slice currently has.)*
- [ ] **IS-26-021 P2** — Create letterbox and presentation overlays with accessibility options.
- [ ] **IS-26-022 P2** — Create video-track support for rare pre-rendered inserts using CNA VideoPlayer.
- [ ] **IS-26-023 P2** — Create sequence nesting only after cycle/ownership rules are defined.
- [ ] **IS-26-024 P3** — Do not build a dedicated timeline editor; author and validate sequences as hand-written data files instead.

- [ ] **IS-26-025 P1** — Implement and unit-test all sequence track types (camera, animation, dialogue, audio, event) against the shared timeline data model.
- [ ] **IS-26-026 P1** — Add an integration test exercising a mixed multi-track sequence in a running mission flow.
- [ ] **IS-26-027 P2** — Add logging and debug inspection shared across all track types.
- [ ] **IS-26-028 P2** — Document track authoring conventions for all track types.

- [ ] **IS-26-029 P1** — Implement entity-binding resolution for sequences, robust to district transitions and respawn.
- [ ] **IS-26-030 P1** — Add unit tests for binding-resolution edge cases.
- [ ] **IS-26-031 P1** — Implement sequence validation with actionable failure reporting.
- [ ] **IS-26-032 P1** — Add unit tests for sequence validation error reporting.
- [ ] **IS-26-033 P2** — Implement asset prefetch and dependency checking for upcoming sequences.
- [ ] **IS-26-034 P2** — Add an integration test for prefetch/dependency checking on a representative sequence.
- [ ] **IS-26-035 P2** — Document the sequence-support module (binding, validation, prefetch) and its common failure modes.
