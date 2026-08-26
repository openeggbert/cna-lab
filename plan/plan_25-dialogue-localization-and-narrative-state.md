# 25. Dialogue, localization, and narrative state

[Back to master plan](../plan.md)

Support voiced, interruptible conversations tied to mission state, shipping in one
language first. Every line of dialogue and UI text uses a stable string ID from day
one so a second language can be added later without touching the dialogue system or
rewriting content; the full multi-locale runtime (language switching, a translation
pipeline, per-locale asset variants) is explicitly deferred until a second language is
actually planned.

## Core dialogue data and flow

- [x] **IG-25-001 P0** — Replace the prototype delimiter text with versioned JSON or XML dialogue data. *(`assets/dialogues/prologue.dialogue.json`, schema version 1, replacing the `speaker|text` file. Every line carries a **stable id** (`prologue.mara.no_heroics`) -- named for what the line *is* rather than what it says, so editing the English text never invalidates a reference or a translation. That is plan.md's locked decision 10 and this group's own header, which the prototype format had quietly made untrue for as long as it shipped. Validation refuses an unsupported version, a missing or duplicate line id, an empty speaker or text, an unknown field, a non-string field, and a conversation with no lines; a rejected file leaves the previously loaded conversation intact, because half a conversation is worse than the built-in fallback. `DialogueSystem::FindLine` resolves an id to a line and returns null for one that no longer exists. The fallback carries the same ids as the file it stands in for. Covered by `TestDialogueLinesCarryStableIds`.)*
- [ ] **IG-25-002 P0** — Assign stable dialogue, conversation, line, and speaker IDs; the same ID doubles as the future localization key.
- [ ] **IG-25-003 P0** — Display dialogue through a proper subtitle UI.
- [ ] **IG-25-004 P0** — Bind dialogue completion to the data-driven mission.
- [ ] **IG-25-005 P1** — Add voice asset references and duration metadata.
- [ ] **IG-25-006 P1** — Add automatic timing from decoded voice duration.
- [ ] **IG-25-007 P1** — Add branching choices and conditions.
- [ ] **IG-25-008 P1** — Add interruption, cancellation, resumption, and priority rules.
- [ ] **IG-25-009 P1** — Add speaker entity binding and fallback display names.
- [ ] **IG-25-010 P1** — Add conversation start positions and facing/look-at behavior.
- [ ] **IG-25-011 P1** — Add dialogue camera/cinematic hooks.
- [ ] **IG-25-012 P1** — Add mission actions from completed or selected lines.
- [ ] **IG-25-013 P1** — Add subtitle speaker labels, background, size, and timing accessibility options.
- [ ] **IG-25-014 P1** — Write all dialogue/UI text in one shipped language, keyed by the stable IDs from IG-25-002, so a translation pass later only touches data, never code.
- [ ] **IG-25-015 P1** — Add dialogue history/replay screen if desired.
- [ ] **IG-25-016 P1** — Add save/checkpoint restoration of active conversations.
- [ ] **IG-25-017 P1** — Add a dialogue graph validation/preview script (not a GUI editor) that finds missing references and unreachable lines.
- [ ] **IG-25-018 P2** — Add simple jaw movement and facial events.
- [ ] **IG-25-019 P2** — Add ambient barks with cooldown and priority.
- [ ] **IG-25-020 P2** — Add radio/telephone conversation presentation modes.
- [ ] **IG-25-021 P2** — Add lip-sync data import only when voice production is stable.
- [ ] **IG-25-022 P3** — Design the full multi-locale runtime (language-switch UI, translation-management pipeline, per-locale voice/asset variants) only once a second shipped language is actually planned.

## Dialogue database

- [ ] **IG-25-023 P1** — Define the scope and public API of the dialogue database (dialogue/conversation/line/speaker records from IG-25-002/IG-25-009).
- [ ] **IG-25-024 P1** — Implement the smallest deterministic reference path: load one conversation, resolve its lines and speakers.
- [ ] **IG-25-025 P1** — Add input validation and actionable failure reporting for malformed dialogue data.
- [ ] **IG-25-026 P1** — Add unit tests and one integration scenario exercising the database in a running game flow.
- [ ] **IG-25-027 P1** — Define save/checkpoint serialization and restoration where dialogue state affects mission variables.
- [ ] **IG-25-028 P2** — Add debug logging/inspection and document usage.

## Conversation player

- [ ] **IG-25-029 P1** — Define the scope and public API of the conversation player (advance/interrupt/resume from IG-25-007/IG-25-008).
- [ ] **IG-25-030 P1** — Implement the smallest deterministic reference path: play one conversation start to finish.
- [ ] **IG-25-031 P1** — Add input validation and actionable failure reporting for malformed conversation graphs.
- [ ] **IG-25-032 P1** — Add unit tests and one integration scenario covering interruption/resumption.
- [ ] **IG-25-033 P1** — Define save/checkpoint serialization and restoration for an in-progress conversation.
- [ ] **IG-25-034 P2** — Add debug logging/inspection and document usage.

## Subtitle presenter

- [ ] **IG-25-035 P1** — Define the scope and public API of the subtitle presenter, including timing (IG-25-006) and accessibility options (IG-25-013).
- [ ] **IG-25-036 P1** — Implement the smallest deterministic reference path: show and time one subtitle line against voice duration.
- [ ] **IG-25-037 P1** — Add input validation and actionable failure reporting for missing voice/timing data.
- [ ] **IG-25-038 P1** — Add unit tests and one integration scenario covering accessibility display options.
- [ ] **IG-25-039 P2** — Add debug logging/inspection and document usage.

## Dialogue choice system

- [ ] **IG-25-040 P1** — Define the scope and public API of the dialogue choice system (branching choices/conditions from IG-25-007).
- [ ] **IG-25-041 P1** — Implement the smallest deterministic reference path: present one branching choice and follow the selected line.
- [ ] **IG-25-042 P1** — Add input validation and actionable failure reporting for malformed choice data.
- [ ] **IG-25-043 P1** — Add unit tests and one integration scenario covering condition-gated choices.
- [ ] **IG-25-044 P1** — Define save/checkpoint serialization and restoration for pending/selected choices.
- [ ] **IG-25-045 P2** — Add debug logging/inspection and document usage.

## Conversation staging and camera bridge

- [ ] **IG-25-046 P1** — Define the scope and public API for conversation staging (start positions/facing from IG-25-010) and the camera/cinematic hooks (IG-25-011).
- [ ] **IG-25-047 P1** — Implement the smallest deterministic reference path: stage two speakers and cut to a conversation camera.
- [ ] **IG-25-048 P1** — Add input validation and actionable failure reporting for missing staging/camera data.
- [ ] **IG-25-049 P1** — Add unit tests and one integration scenario covering staging + camera together in a running conversation.
- [ ] **IG-25-050 P2** — Add debug logging/inspection and document usage.
