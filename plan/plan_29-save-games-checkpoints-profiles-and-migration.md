# 29. Save games, checkpoints, profiles, and migration

[Back to master plan](../plan.md)

Persist logical state safely across district transitions, version changes, and interrupted writes.

- [ ] **IG-29-001 P0** — Version the current save format and add explicit migration infrastructure.
- [ ] **IG-29-002 P0** — Implement atomic write to a temporary file followed by replacement.
- [ ] **IG-29-003 P0** — Keep at least one rolling backup.
- [ ] **IG-29-004 P0** — Add checksum/corruption detection.
- [ ] **IG-29-005 P0** — Separate user settings from campaign save data.
- [ ] **IG-29-006 P1** — Create profile and save-slot metadata.
- [ ] **IG-29-007 P1** — Create stable serialization for mission, player, vehicle, world, inventory, and wanted state.
- [ ] **IG-29-008 P1** — Create district persistence records for entities and world state that must survive a district transition.
- [ ] **IG-29-009 P1** — Create checkpoint snapshot semantics.
- [ ] **IG-29-010 P1** — Create autosave scheduling that avoids unsafe moments.
- [ ] **IG-29-011 P1** — Create save-blocking reasons and clear UI feedback.
- [ ] **IG-29-012 P1** — Create asynchronous save snapshot preparation where safe.
- [ ] **IG-29-013 P1** — Create thumbnail capture after render pipeline support exists.
- [ ] **IG-29-014 P1** — Create migration fixtures for every released format version.
- [ ] **IG-29-015 P1** — Create tests for truncated, duplicated, invalid, and future-version saves.
- [ ] **IG-29-016 P1** — Create behavior when optional content referenced by a save is absent.
- [ ] **IG-29-017 P1** — Create cross-platform path and filename policy.
- [ ] **IG-29-018 P1** — Create a privacy policy for telemetry/crash data, kept separate from saves.
- [ ] **IG-29-019 P1** — Create a small CLI save-inspection/diff tool for debugging (not a full editor).
- [ ] **IG-29-020 P2** — Create import/export support for debugging.
- [ ] **IG-29-021 P2** — Create campaign chapter replay snapshots only if game design uses them.

- [ ] **IG-29-022 P0** — Implement the save-snapshot data model and atomic write/replace mechanics.
- [ ] **IG-29-023 P0** — Add unit tests for snapshot serialization and atomic-write failure injection (crash mid-write, disk full).
- [ ] **IG-29-024 P1** — Add an integration test for a full save/load round trip across mission, player, vehicle, world, and inventory state.
- [ ] **IG-29-025 P2** — Document the save-snapshot schema and atomic-write guarantees.

- [ ] **IG-29-026 P1** — Implement the save migration registry and per-version migration functions.
- [ ] **IG-29-027 P1** — Add unit tests covering every released format version's migration path.
- [ ] **IG-29-028 P2** — Document how to add a new migration when the save format changes.

- [ ] **IG-29-029 P1** — Implement checkpoint snapshot creation and restoration tied to mission/district state.
- [ ] **IG-29-030 P1** — Add an integration test for checkpoint restore after a scripted mission failure.
- [ ] **IG-29-031 P2** — Document checkpoint placement conventions for mission authors.

- [ ] **IG-29-032 P1** — Implement profile and save-slot management (create/select/delete/rename).
- [ ] **IG-29-033 P1** — Add unit tests for profile-slot edge cases (full slots, corrupted profile).

- [ ] **IG-29-034 P1** — Implement per-district persistence for entities and world state that must survive a district transition.
- [ ] **IG-29-035 P1** — Add an integration test for district-transition save/restore.

- [ ] **IG-29-036 P1** — Implement autosave scheduling and rolling backup rotation.
- [ ] **IG-29-037 P1** — Add unit tests for autosave-timing and backup-rotation edge cases.

- [ ] **IG-29-038 P0** — Implement corruption detection and recovery/fallback-to-backup behavior.
- [ ] **IG-29-039 P0** — Add unit tests for corrupted, truncated, and future-version save handling.

- [ ] **IG-29-040 P2** — Implement a save-compatibility reporter that explains why an old save cannot load.
- [ ] **IG-29-041 P3** — Defer cloud-save adapter design until a specific platform target requires it, matching the locked one-platform-first decision.
