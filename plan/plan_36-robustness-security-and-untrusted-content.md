# 36. Robustness, security, and untrusted content

[Back to master plan](../plan.md)

Treat downloaded/generated assets, saves, and mission data as untrusted inputs.

- [ ] **IG-36-001 P0** — Treat all downloaded and AI-generated assets as untrusted input.
- [ ] **IG-36-002 P0** — Bound all package counts, sizes, nesting, string lengths, and allocations.
- [ ] **IG-36-003 P0** — Bound the mission condition/action expression evaluator's depth, step count, and CPU/memory budget (no general scripting language is exposed, per the locked mission-framework decision in plan_24).
- [ ] **IG-36-004 P0** — Reject path traversal and absolute output paths in asset tools.
- [ ] **IG-36-005 P0** — Use atomic writes for saves and generated manifests.
- [ ] **IG-36-006 P1** — Validate UTF-8 and normalize identifiers consistently.
- [ ] **IG-36-007 P1** — Detect integer overflow in size/offset calculations.
- [ ] **IG-36-008 P1** — Detect decompression bombs and excessive expansion ratios.
- [ ] **IG-36-009 P1** — Set parser recursion/depth limits.
- [ ] **IG-36-010 P1** — Avoid executing arbitrary code from downloaded asset packages.
- [ ] **IG-36-011 P1** — Separate developer hot-reload paths from shipping content paths.
- [ ] **IG-36-012 P1** — Create secure temporary-file handling in tools.
- [ ] **IG-36-013 P1** — Create clear errors without leaking sensitive local paths in shipping builds.
- [ ] **IG-36-014 P1** — Create corruption recovery for caches and derived assets.
- [ ] **IG-36-015 P1** — Create dependency hash/signature checks for release packages.
- [ ] **IG-36-016 P1** — Run static analysis and sanitizers on parsers and converters.
- [ ] **IG-36-017 P1** — Create a vulnerability response and dependency update process.
- [ ] **IG-36-018 P1** — Document privacy boundaries for logs, crash reports, and analytics.
- [ ] **IG-36-019 P2** — Fuzz MC3 metadata, runtime packages, saves, dialogue, and mission data.
- [ ] **IG-36-020 P3** — Threat-model mod support before exposing writable/executable content (not planned for v1, matches the locked no-mod-support decision).
- [ ] **IG-36-021 P3** — Threat-model network features before adding any multiplayer/online service (not planned for v1).

- [ ] **IG-36-022 P1** — Implement bounds/limit checks in the runtime-package (CNJ/MCB) parser.
- [ ] **IG-36-023 P1** — Add fuzz/unit tests for the runtime-package parser against malformed input.
- [ ] **IG-36-024 P2** — Document the runtime-package parser's trust boundary and failure modes.

- [ ] **IG-36-025 P1** — Implement bounds/limit checks in the save-file parser.
- [ ] **IG-36-026 P1** — Add fuzz/unit tests for the save-file parser against malformed input (shares fixtures with plan_29's corruption tests).
- [ ] **IG-36-027 P2** — Document the save-file parser's trust boundary and failure modes.

- [ ] **IG-36-028 P1** — Implement asset-tool path-traversal and absolute-path rejection across every MC3/glTF/CNJ tool.
- [ ] **IG-36-029 P1** — Add unit tests for asset-tool path rejection.

- [ ] **IG-36-030 P1** — Implement dependency hash/signature verification for release packages.
- [ ] **IG-36-031 P1** — Add unit tests for release-manifest verification failure cases.

- [ ] **IG-36-032 P2** — Implement a small fuzz corpus and CI job covering the package, save, MC3, dialogue, and mission-data parsers.
- [ ] **IG-36-033 P2** — Document how to extend the fuzz corpus when a new parser is added.

- [ ] **IG-36-034 P2** — Implement a dependency version/CVE check as a CI step.
- [ ] **IG-36-035 P2** — Document the vulnerability response process and who owns it.

- [ ] **IG-36-036 P1** — Implement redaction of local paths and personal data from logs and crash reports.
- [ ] **IG-36-037 P1** — Add unit tests for log/crash-report redaction.
