# 34. Automated tests, CI, and regression control

[Back to master plan](../plan.md)

Protect deterministic systems, converters, builds, and representative gameplay flows at Mafia-1 fidelity (district loading, simplified traffic/police/pedestrian AI, EasyGL as the shipped backend) rather than a modern open-world test matrix.

- [ ] **IS-34-001 P0** — Keep core tests window-free and fast.
- [ ] **IS-34-002 P0** — Add integrated startup/smoke tests once CNA dependencies build.
- [ ] **IS-34-003 P0** — Run MC3 schema validation in CI.
- [ ] **IS-34-004 P0** — Run asset registry/license validation in CI.
- [ ] **IS-34-005 P0** — Run the EasyGL production-backend build in CI.
- [ ] **IS-34-006 P1** — Add unit tests for every parser, state machine, serializer, and deterministic controller.
- [ ] **IS-34-007 P1** — Add property tests for IDs, package bounds, and save round trips.
- [ ] **IS-34-008 P1** — Add malformed asset/save/mission-data input tests.
- [ ] **IS-34-009 P1** — Add fixed-input gameplay scenario tests.
- [ ] **IS-34-010 P1** — Add mission happy-path, skip, fail, retry, save, and load tests.
- [ ] **IS-34-011 P1** — Add district load/unload and cancellation tests.
- [ ] **IS-34-012 P1** — Add simplified-traffic lane-following and signal-compliance scenarios (no deadlock-avoidance/intersection-reservation testing — that system doesn't exist by design).
- [ ] **IS-34-013 P1** — Add vehicle stability and collision scenarios.
- [ ] **IS-34-014 P1** — Add animation transition and missing-clip tests.
- [ ] **IS-34-015 P1** — Add dialogue stable-ID missing-reference tests.
- [ ] **IS-34-016 P1** — Add cutscene skip terminal-state tests.
- [ ] **IS-34-017 P1** — Add install/package launch tests.
- [ ] **IS-34-018 P1** — Add sanitizer jobs with controlled scope.
- [ ] **IS-34-019 P1** — Add test artifact upload for logs, traces, screenshots, and package reports.
- [ ] **IS-34-020 P1** — Quarantine flaky tests only with owner and expiration date.
- [ ] **IS-34-021 P2** — Add rendering reference images on the EasyGL backend.
- [ ] **IS-34-022 P2** — Add district-load and simplified traffic/pedestrian/police soak jobs (not seamless-streaming soak — that system doesn't exist by design).
- [ ] **IS-34-023 P2** — Add fuzzing for package, save, and mission-data boundaries.
- [ ] **IS-34-024 P2** — Add performance regression thresholds after stable baselines exist (see plan_35).
- [ ] **IS-34-025 P1** — Define scope and implement the smallest working asset validation CI job (MC3/glTF/CNJ schema, budgets, license registry).
- [ ] **IS-34-026 P1** — Add tests and CI wiring for the asset validation job; document its failure modes.
- [ ] **IS-34-027 P1** — Define scope and implement the smallest working sanitizer CI job (ASan/UBSan on core tests, controlled scope per docs/build rules).
- [ ] **IS-34-028 P1** — Add tests and CI wiring for the sanitizer job; document its failure modes.
- [ ] **IS-34-029 P2** — Define scope and implement the smallest working soak-test runner (district load/unload, mission replay, save/load repeated cycles).
- [ ] **IS-34-030 P2** — Add tests and CI wiring for the soak-test runner; document its failure modes.
- [ ] **IS-34-031 P1** — Define scope and implement the smallest working performance regression gate (compares captures against plan_35 baselines).
- [ ] **IS-34-032 P1** — Add tests and CI wiring for the performance regression gate; document its failure modes.
- [ ] **IS-34-033 P2** — Define scope and implement the smallest working crash reproducer (captures a minimal repro scene/input trace on failure).
- [ ] **IS-34-034 P2** — Add tests and CI wiring for the crash reproducer; document its failure modes.
- [ ] **IS-34-035 P2** — Define scope and implement the smallest working test fixture/data manager (shared save/mission/district fixtures for tests).
- [ ] **IS-34-036 P2** — Add tests and documentation for the test fixture/data manager.
- [ ] **IS-34-037 P2** — Define scope and implement combined flaky-test tracking and CI artifact reporting (owner/expiration for quarantined tests, one reporting surface for logs/traces/screenshots).
- [ ] **IS-34-038 P2** — Add tests and documentation for the flaky-test/artifact reporting tool.
