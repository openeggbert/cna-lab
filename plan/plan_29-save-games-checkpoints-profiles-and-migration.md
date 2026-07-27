# 29. Save games, checkpoints, profiles, and migration

[Back to master plan](../plan.md)

Persist logical state safely across district transitions, version changes, and interrupted writes.

- [ ] **IS-29-001 P0** — Version the current save format and add explicit migration infrastructure.
- [ ] **IS-29-002 P0** — Implement atomic write to a temporary file followed by replacement.
- [ ] **IS-29-003 P0** — Keep at least one rolling backup.
- [ ] **IS-29-004 P0** — Add checksum/corruption detection.
- [ ] **IS-29-005 P0** — Separate user settings from campaign save data.
- [ ] **IS-29-006 P1** — Create profile and save-slot metadata.
- [ ] **IS-29-007 P1** — Create stable serialization for mission, player, vehicle, world, inventory, and wanted state.
- [ ] **IS-29-008 P1** — Create district persistence records for entities and world state that must survive a district transition.
- [ ] **IS-29-009 P1** — Create checkpoint snapshot semantics.
- [ ] **IS-29-010 P1** — Create autosave scheduling that avoids unsafe moments.
- [ ] **IS-29-011 P1** — Create save-blocking reasons and clear UI feedback.
- [ ] **IS-29-012 P1** — Create asynchronous save snapshot preparation where safe.
- [ ] **IS-29-013 P1** — Create thumbnail capture after render pipeline support exists.
- [ ] **IS-29-014 P1** — Create migration fixtures for every released format version.
- [ ] **IS-29-015 P1** — Create tests for truncated, duplicated, invalid, and future-version saves.
- [ ] **IS-29-016 P1** — Create behavior when optional content referenced by a save is absent.
- [ ] **IS-29-017 P1** — Create cross-platform path and filename policy.
- [ ] **IS-29-018 P1** — Create a privacy policy for telemetry/crash data, kept separate from saves.
- [ ] **IS-29-019 P1** — Create a small CLI save-inspection/diff tool for debugging (not a full editor).
- [ ] **IS-29-020 P2** — Create import/export support for debugging.
- [ ] **IS-29-021 P2** — Create campaign chapter replay snapshots only if game design uses them.

- [ ] **IS-29-022 P0** — Implement the save-snapshot data model and atomic write/replace mechanics.
- [ ] **IS-29-023 P0** — Add unit tests for snapshot serialization and atomic-write failure injection (crash mid-write, disk full).
- [ ] **IS-29-024 P1** — Add an integration test for a full save/load round trip across mission, player, vehicle, world, and inventory state.
- [ ] **IS-29-025 P2** — Document the save-snapshot schema and atomic-write guarantees.

- [ ] **IS-29-026 P1** — Implement the save migration registry and per-version migration functions.
- [ ] **IS-29-027 P1** — Add unit tests covering every released format version's migration path.
- [ ] **IS-29-028 P2** — Document how to add a new migration when the save format changes.

- [ ] **IS-29-029 P1** — Implement checkpoint snapshot creation and restoration tied to mission/district state.
- [ ] **IS-29-030 P1** — Add an integration test for checkpoint restore after a scripted mission failure.
- [ ] **IS-29-031 P2** — Document checkpoint placement conventions for mission authors.

- [ ] **IS-29-032 P1** — Implement profile and save-slot management (create/select/delete/rename).
- [ ] **IS-29-033 P1** — Add unit tests for profile-slot edge cases (full slots, corrupted profile).

- [ ] **IS-29-034 P1** — Implement per-district persistence for entities and world state that must survive a district transition.
- [ ] **IS-29-035 P1** — Add an integration test for district-transition save/restore.

- [ ] **IS-29-036 P1** — Implement autosave scheduling and rolling backup rotation.
- [ ] **IS-29-037 P1** — Add unit tests for autosave-timing and backup-rotation edge cases.

- [ ] **IS-29-038 P0** — Implement corruption detection and recovery/fallback-to-backup behavior.
- [ ] **IS-29-039 P0** — Add unit tests for corrupted, truncated, and future-version save handling.

- [ ] **IS-29-040 P2** — Implement a save-compatibility reporter that explains why an old save cannot load.
- [ ] **IS-29-041 P3** — Defer cloud-save adapter design until a specific platform target requires it, matching the locked one-platform-first decision.
