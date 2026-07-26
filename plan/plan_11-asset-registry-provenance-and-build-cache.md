# 11. Asset registry, provenance, and build cache

[Back to master plan](../plan.md)

Make every production asset traceable, licensed, validated, and incrementally built. License/provenance tracking matters regardless of project scope; a catalog-browser UI and content-signing infrastructure do not, and are cut.

- [ ] **IS-11-001 P0** — Make every production asset require an asset-registry entry.
- [ ] **IS-11-002 P0** — Define an allow-list of accepted licenses.
- [ ] **IS-11-003 P0** — Store a local copy or digest of relevant license evidence where permitted.
- [ ] **IS-11-004 P0** — Reject unknown-license assets from release packages.
- [ ] **IS-11-005 P1** — Add source URL, author, license, acquisition date, hash, modification, and attribution fields.
- [ ] **IS-11-006 P1** — Add commercial-use, redistribution, and AI-processing review fields.
- [ ] **IS-11-007 P1** — Add asset reviewer and approval status.
- [ ] **IS-11-008 P1** — Generate THIRD_PARTY_ASSETS.md from the registry.
- [ ] **IS-11-009 P1** — Detect unregistered files under production asset directories.
- [ ] **IS-11-010 P1** — Detect duplicate assets by content hash.
- [ ] **IS-11-011 P1** — Detect case-collision and path-portability problems.
- [ ] **IS-11-012 P1** — Create incremental rebuild decisions from source and tool hashes.
- [ ] **IS-11-013 P1** — Create clean-room provenance rules for AI-generated/AI-assisted assets.
- [ ] **IS-11-014 P1** — Create rejection reasons and quarantine directories.
- [ ] **IS-11-015 P1** — Create per-asset quality state: draft, validated, approved, shipping.
- [ ] **IS-11-016 P1** — Create budgets and warnings by asset class.
- [ ] **IS-11-017 P2** — Create thumbnail generation for model and texture assets.
- [ ] **IS-11-018 P2** — Create attribution-screen generation if licenses require it.
- [ ] **IS-11-019 P2** — Create release-manifest hash verification (no signing infrastructure).
- [ ] **IS-11-020 P2** — Create stale-source-link audit reports.
- [ ] **IS-11-021 P2** — Create an archive policy for replaced or revoked assets.

## Asset registry system

- [ ] **IS-11-022 P1** — Define the scope, public API, and versioned data format of the asset registry.
- [ ] **IS-11-023 P1** — Implement the smallest deterministic reference path for the asset registry.
- [ ] **IS-11-024 P1** — Add input validation and actionable failure reporting to the asset registry.
- [ ] **IS-11-025 P1** — Add focused unit tests for the asset registry.
- [ ] **IS-11-026 P1** — Add an integration scenario exercising the asset registry in a running build.
- [ ] **IS-11-027 P1** — Define save/checkpoint restoration rules so registry state survives interrupted builds.
- [ ] **IS-11-028 P2** — Add logging and debug inspection for the asset registry.
- [ ] **IS-11-029 P2** — Document usage examples and common failure modes for the asset registry.

## License policy validator

- [ ] **IS-11-030 P1** — Define the scope, public API, and versioned data format of the license policy validator.
- [ ] **IS-11-031 P1** — Implement the smallest deterministic reference path for the license policy validator.
- [ ] **IS-11-032 P1** — Add input validation and actionable failure reporting to the license policy validator.
- [ ] **IS-11-033 P1** — Add focused unit tests for the license policy validator.
- [ ] **IS-11-034 P1** — Add an integration scenario exercising the license policy validator in a running build.
- [ ] **IS-11-035 P2** — Add logging and debug inspection for the license policy validator.
- [ ] **IS-11-036 P2** — Define CPU/latency budgets for the license policy validator on a full asset scan.
- [ ] **IS-11-037 P2** — Document usage examples and common failure modes for the license policy validator.

## Asset dependency graph

- [ ] **IS-11-038 P1** — Define the scope, public API, and versioned data format of the asset dependency graph.
- [ ] **IS-11-039 P1** — Implement the smallest deterministic reference path for the asset dependency graph.
- [ ] **IS-11-040 P1** — Add input validation and actionable failure reporting to the asset dependency graph.
- [ ] **IS-11-041 P1** — Add focused unit tests for the asset dependency graph.
- [ ] **IS-11-042 P1** — Add an integration scenario exercising the asset dependency graph in a running build.
- [ ] **IS-11-043 P2** — Add logging and debug inspection for the asset dependency graph.
- [ ] **IS-11-044 P2** — Define CPU/memory budgets for the asset dependency graph at full-campaign asset scale.
- [ ] **IS-11-045 P2** — Document usage examples and common failure modes for the asset dependency graph.

## Incremental asset build cache

- [ ] **IS-11-046 P1** — Define the scope, public API, and versioned data format of the incremental build cache.
- [ ] **IS-11-047 P1** — Implement the smallest deterministic reference path for the incremental build cache.
- [ ] **IS-11-048 P1** — Add input validation and actionable failure reporting to the incremental build cache.
- [ ] **IS-11-049 P1** — Add focused unit tests for the incremental build cache.
- [ ] **IS-11-050 P1** — Add an integration scenario exercising the incremental build cache in a running build.
- [ ] **IS-11-051 P2** — Add logging and debug inspection for the incremental build cache.
- [ ] **IS-11-052 P2** — Profile the incremental build cache under a representative full-campaign rebuild.
- [ ] **IS-11-053 P2** — Document usage examples and common failure modes for the incremental build cache.

## Asset quarantine workflow

- [ ] **IS-11-054 P1** — Define the scope, public API, and versioned data format of the quarantine workflow.
- [ ] **IS-11-055 P1** — Implement the smallest deterministic reference path for the quarantine workflow.
- [ ] **IS-11-056 P1** — Add input validation and actionable failure reporting to the quarantine workflow.
- [ ] **IS-11-057 P1** — Add focused unit tests for the quarantine workflow.
- [ ] **IS-11-058 P1** — Add an integration scenario exercising the quarantine workflow in a running build.
- [ ] **IS-11-059 P2** — Add logging and debug inspection for the quarantine workflow.
- [ ] **IS-11-060 P2** — Document usage examples and common failure modes for the quarantine workflow.

## Release asset manifest

- [ ] **IS-11-061 P1** — Define the scope, public API, and versioned data format of the release asset manifest.
- [ ] **IS-11-062 P1** — Implement the smallest deterministic reference path for the release asset manifest.
- [ ] **IS-11-063 P1** — Add input validation and actionable failure reporting to the release asset manifest.
- [ ] **IS-11-064 P1** — Add focused unit tests for the release asset manifest.
- [ ] **IS-11-065 P1** — Add an integration scenario exercising the release asset manifest in a build/release flow.
- [ ] **IS-11-066 P2** — Add logging and debug inspection for the release asset manifest.
- [ ] **IS-11-067 P2** — Document usage examples and common failure modes for the release asset manifest.
