# 04. Core runtime services

[Back to master plan](../plan.md)

Provide only the deterministic time, configuration, logging, IDs, and error-handling services a small linear-campaign game actually needs. No full job-scheduling framework, no interactive developer console, and no metrics/telemetry registry — those are AAA/engine-team scope, not this game's scope.

- [x] **IG-04-001 P0** — Create a central game configuration loader (JSON via sharp-runtime) with defaults and validation. *(`GameConfig`/`LoadGameConfig` (`include/IronGang/Core/GameConfig.hpp`, `src/Core/GameConfig.cpp`) read `assets/config/game.json` through sharp-runtime's `System::Text::Json` -- the file existed since the scaffold but nothing had ever read it. Every member's initializer **is** the default, so a broken or partial file costs the tuning and never the run: a missing file, an unknown key, a wrong-typed value, an out-of-range number, or an empty string are all warnings that keep the default, and only malformed JSON or a non-object root fails -- leaving the caller's configuration untouched. Unknown keys are named in the warning, which is what catches the common `projectNmae` mistake. `notes` is accepted and ignored so the file can carry a comment. Currently drives the window title (`projectName`), the district map header (`cityName`/`prototypeYear`), and the autosave scheduler's interval and spacing, whose defaults come from `AutosaveScheduler`'s own constants rather than being repeated. Documented in `docs/configuration.md`.)*
- [ ] **IG-04-002 P0** — Create structured logging categories and severity levels.
- [ ] **IG-04-003 P0** — Create a monotonic simulation clock independent of wall-clock time.
- [ ] **IG-04-004 P0** — Clamp or subdivide extreme frame deltas.
- [ ] **IG-04-005 P0** — Define fixed-step versus variable-step update responsibilities.
- [x] **IG-04-006 P1** — Add unit tests for the configuration loader (defaults, validation failures, round-trip). *(`TestGameConfigLoadsValidatesAndFallsBack`: a missing file leaving every default (one warning, not an error); a full round trip of all five values; an unknown key named in the warning while the correct keys still apply; three wrong-typed values each reported once and each keeping its default; an absurd year, negative seconds clamped to 0, and an empty string; a spacing longer than the interval warned about; malformed JSON and a non-object root failing while leaving the caller's own configuration untouched; and -- the one that keeps the shipped file honest -- the **committed** `assets/config/game.json` loading with zero warnings and the expected identity.)*
- [ ] **IG-04-007 P1** — Add unit tests for the simulation clock (delta clamping, fixed-step accumulation).
- [ ] **IG-04-008 P1** — Create command-line parsing for backend-independent game options (already partially present in `main.cpp`; extend it).
- [ ] **IG-04-009 P1** — Create a result/error object with contextual chaining for fallible operations (save/load, asset loading, dialogue parsing).
- [ ] **IG-04-010 P1** — Create assertion and verify macros with defined release-build semantics.
- [ ] **IG-04-011 P1** — Create a single seeded deterministic RNG wrapper for gameplay randomness (no multi-stream framework).
- [ ] **IG-04-012 P1** — Add unit tests for the RNG wrapper's determinism given a fixed seed.
- [ ] **IG-04-013 P1** — Create localization-safe string formatting helpers (placeholders, not concatenation) ahead of the single-language-first localization work in plan_25.
- [ ] **IG-04-014 P1** — Add a single background loader thread for district-transition asset loading (see plan_13); no general-purpose job scheduler.
- [ ] **IG-04-015 P2** — Create crash-context collection (log tail + build version) without storing sensitive user data.
- [ ] **IG-04-016 P2** — Create a build/version information service exposed in the window title and logs.
- [ ] **IG-04-017 P2** — Document logging categories, severity levels, and how to add a new one.
- [x] **IG-04-018 P2** — Document the configuration schema and how to add a new tunable value. *(`docs/configuration.md`: the schema table (type, default, what each value drives), a table of what happens for every kind of bad input, the six-step procedure for adding a tunable -- including adding it to `IsKnownKey` and to the committed file plus its registry hash -- and an explicit list of what is not implemented (overrides, hot reload, per-platform files). Linked from `README.md`'s file map.)*
- [ ] **IG-04-019 P3** — Add a frame-time counter printed only in debug builds (not a general metrics registry).
- [ ] **IG-04-020 P3** — Add a data-driven tuning-variable override (env var or config override) only if manual tuning during development proves too slow without it.
