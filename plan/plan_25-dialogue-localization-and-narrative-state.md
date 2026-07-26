# 25. Dialogue, localization, and narrative state

[Back to master plan](../plan.md)

Support voiced, interruptible conversations tied to mission state, shipping in one
language first. Every line of dialogue and UI text uses a stable string ID from day
one so a second language can be added later without touching the dialogue system or
rewriting content; the full multi-locale runtime (language switching, a translation
pipeline, per-locale asset variants) is explicitly deferred until a second language is
actually planned.

## Core dialogue data and flow

- [ ] **IS-25-001 P0** — Replace the prototype delimiter text with versioned JSON or XML dialogue data.
- [ ] **IS-25-002 P0** — Assign stable dialogue, conversation, line, and speaker IDs; the same ID doubles as the future localization key.
- [ ] **IS-25-003 P0** — Display dialogue through a proper subtitle UI.
- [ ] **IS-25-004 P0** — Bind dialogue completion to the data-driven mission.
- [ ] **IS-25-005 P1** — Add voice asset references and duration metadata.
- [ ] **IS-25-006 P1** — Add automatic timing from decoded voice duration.
- [ ] **IS-25-007 P1** — Add branching choices and conditions.
- [ ] **IS-25-008 P1** — Add interruption, cancellation, resumption, and priority rules.
- [ ] **IS-25-009 P1** — Add speaker entity binding and fallback display names.
- [ ] **IS-25-010 P1** — Add conversation start positions and facing/look-at behavior.
- [ ] **IS-25-011 P1** — Add dialogue camera/cinematic hooks.
- [ ] **IS-25-012 P1** — Add mission actions from completed or selected lines.
- [ ] **IS-25-013 P1** — Add subtitle speaker labels, background, size, and timing accessibility options.
- [ ] **IS-25-014 P1** — Write all dialogue/UI text in one shipped language, keyed by the stable IDs from IS-25-002, so a translation pass later only touches data, never code.
- [ ] **IS-25-015 P1** — Add dialogue history/replay screen if desired.
- [ ] **IS-25-016 P1** — Add save/checkpoint restoration of active conversations.
- [ ] **IS-25-017 P1** — Add a dialogue graph validation/preview script (not a GUI editor) that finds missing references and unreachable lines.
- [ ] **IS-25-018 P2** — Add simple jaw movement and facial events.
- [ ] **IS-25-019 P2** — Add ambient barks with cooldown and priority.
- [ ] **IS-25-020 P2** — Add radio/telephone conversation presentation modes.
- [ ] **IS-25-021 P2** — Add lip-sync data import only when voice production is stable.
- [ ] **IS-25-022 P3** — Design the full multi-locale runtime (language-switch UI, translation-management pipeline, per-locale voice/asset variants) only once a second shipped language is actually planned.

## Dialogue database

- [ ] **IS-25-023 P1** — Define the scope and public API of the dialogue database (dialogue/conversation/line/speaker records from IS-25-002/IS-25-009).
- [ ] **IS-25-024 P1** — Implement the smallest deterministic reference path: load one conversation, resolve its lines and speakers.
- [ ] **IS-25-025 P1** — Add input validation and actionable failure reporting for malformed dialogue data.
- [ ] **IS-25-026 P1** — Add unit tests and one integration scenario exercising the database in a running game flow.
- [ ] **IS-25-027 P1** — Define save/checkpoint serialization and restoration where dialogue state affects mission variables.
- [ ] **IS-25-028 P2** — Add debug logging/inspection and document usage.

## Conversation player

- [ ] **IS-25-029 P1** — Define the scope and public API of the conversation player (advance/interrupt/resume from IS-25-007/IS-25-008).
- [ ] **IS-25-030 P1** — Implement the smallest deterministic reference path: play one conversation start to finish.
- [ ] **IS-25-031 P1** — Add input validation and actionable failure reporting for malformed conversation graphs.
- [ ] **IS-25-032 P1** — Add unit tests and one integration scenario covering interruption/resumption.
- [ ] **IS-25-033 P1** — Define save/checkpoint serialization and restoration for an in-progress conversation.
- [ ] **IS-25-034 P2** — Add debug logging/inspection and document usage.

## Subtitle presenter

- [ ] **IS-25-035 P1** — Define the scope and public API of the subtitle presenter, including timing (IS-25-006) and accessibility options (IS-25-013).
- [ ] **IS-25-036 P1** — Implement the smallest deterministic reference path: show and time one subtitle line against voice duration.
- [ ] **IS-25-037 P1** — Add input validation and actionable failure reporting for missing voice/timing data.
- [ ] **IS-25-038 P1** — Add unit tests and one integration scenario covering accessibility display options.
- [ ] **IS-25-039 P2** — Add debug logging/inspection and document usage.

## Dialogue choice system

- [ ] **IS-25-040 P1** — Define the scope and public API of the dialogue choice system (branching choices/conditions from IS-25-007).
- [ ] **IS-25-041 P1** — Implement the smallest deterministic reference path: present one branching choice and follow the selected line.
- [ ] **IS-25-042 P1** — Add input validation and actionable failure reporting for malformed choice data.
- [ ] **IS-25-043 P1** — Add unit tests and one integration scenario covering condition-gated choices.
- [ ] **IS-25-044 P1** — Define save/checkpoint serialization and restoration for pending/selected choices.
- [ ] **IS-25-045 P2** — Add debug logging/inspection and document usage.

## Conversation staging and camera bridge

- [ ] **IS-25-046 P1** — Define the scope and public API for conversation staging (start positions/facing from IS-25-010) and the camera/cinematic hooks (IS-25-011).
- [ ] **IS-25-047 P1** — Implement the smallest deterministic reference path: stage two speakers and cut to a conversation camera.
- [ ] **IS-25-048 P1** — Add input validation and actionable failure reporting for missing staging/camera data.
- [ ] **IS-25-049 P1** — Add unit tests and one integration scenario covering staging + camera together in a running conversation.
- [ ] **IS-25-050 P2** — Add debug logging/inspection and document usage.
