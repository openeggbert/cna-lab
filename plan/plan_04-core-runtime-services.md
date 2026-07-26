# 04. Core runtime services

[Back to master plan](../plan.md)

Provide only the deterministic time, configuration, logging, IDs, and error-handling services a small linear-campaign game actually needs. No full job-scheduling framework, no interactive developer console, and no metrics/telemetry registry — those are AAA/engine-team scope, not this game's scope.

- [ ] **IS-04-001 P0** — Create a central game configuration loader (JSON via sharp-runtime) with defaults and validation.
- [ ] **IS-04-002 P0** — Create structured logging categories and severity levels.
- [ ] **IS-04-003 P0** — Create a monotonic simulation clock independent of wall-clock time.
- [ ] **IS-04-004 P0** — Clamp or subdivide extreme frame deltas.
- [ ] **IS-04-005 P0** — Define fixed-step versus variable-step update responsibilities.
- [ ] **IS-04-006 P1** — Add unit tests for the configuration loader (defaults, validation failures, round-trip).
- [ ] **IS-04-007 P1** — Add unit tests for the simulation clock (delta clamping, fixed-step accumulation).
- [ ] **IS-04-008 P1** — Create command-line parsing for backend-independent game options (already partially present in `main.cpp`; extend it).
- [ ] **IS-04-009 P1** — Create a result/error object with contextual chaining for fallible operations (save/load, asset loading, dialogue parsing).
- [ ] **IS-04-010 P1** — Create assertion and verify macros with defined release-build semantics.
- [ ] **IS-04-011 P1** — Create a single seeded deterministic RNG wrapper for gameplay randomness (no multi-stream framework).
- [ ] **IS-04-012 P1** — Add unit tests for the RNG wrapper's determinism given a fixed seed.
- [ ] **IS-04-013 P1** — Create localization-safe string formatting helpers (placeholders, not concatenation) ahead of the single-language-first localization work in plan_25.
- [ ] **IS-04-014 P1** — Add a single background loader thread for district-transition asset loading (see plan_13); no general-purpose job scheduler.
- [ ] **IS-04-015 P2** — Create crash-context collection (log tail + build version) without storing sensitive user data.
- [ ] **IS-04-016 P2** — Create a build/version information service exposed in the window title and logs.
- [ ] **IS-04-017 P2** — Document logging categories, severity levels, and how to add a new one.
- [ ] **IS-04-018 P2** — Document the configuration schema and how to add a new tunable value.
- [ ] **IS-04-019 P3** — Add a frame-time counter printed only in debug builds (not a general metrics registry).
- [ ] **IS-04-020 P3** — Add a data-driven tuning-variable override (env var or config override) only if manual tuning during development proves too slow without it.
