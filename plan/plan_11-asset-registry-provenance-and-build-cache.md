# 11. Asset registry, provenance, and build cache

[Back to master plan](../plan.md)

Make every production asset traceable, licensed, validated, and incrementally built. License/provenance tracking matters regardless of project scope; a catalog-browser UI and content-signing infrastructure do not, and are cut.

- [ ] **IG-11-001 P0** — Make every production asset require an asset-registry entry.
- [ ] **IG-11-002 P0** — Define an allow-list of accepted licenses.
- [ ] **IG-11-003 P0** — Store a local copy or digest of relevant license evidence where permitted.
- [ ] **IG-11-004 P0** — Reject unknown-license assets from release packages.
- [ ] **IG-11-005 P1** — Add source URL, author, license, acquisition date, hash, modification, and attribution fields.
- [ ] **IG-11-006 P1** — Add commercial-use, redistribution, and AI-processing review fields.
- [ ] **IG-11-007 P1** — Add asset reviewer and approval status.
- [ ] **IG-11-008 P1** — Generate THIRD_PARTY_ASSETS.md from the registry.
- [ ] **IG-11-009 P1** — Detect unregistered files under production asset directories.
- [ ] **IG-11-010 P1** — Detect duplicate assets by content hash.
- [ ] **IG-11-011 P1** — Detect case-collision and path-portability problems.
- [ ] **IG-11-012 P1** — Create incremental rebuild decisions from source and tool hashes.
- [ ] **IG-11-013 P1** — Create clean-room provenance rules for AI-generated/AI-assisted assets.
- [ ] **IG-11-014 P1** — Create rejection reasons and quarantine directories.
- [ ] **IG-11-015 P1** — Create per-asset quality state: draft, validated, approved, shipping.
- [ ] **IG-11-016 P1** — Create budgets and warnings by asset class.
- [ ] **IG-11-017 P2** — Create thumbnail generation for model and texture assets.
- [ ] **IG-11-018 P2** — Create attribution-screen generation if licenses require it.
- [ ] **IG-11-019 P2** — Create release-manifest hash verification (no signing infrastructure).
- [ ] **IG-11-020 P2** — Create stale-source-link audit reports.
- [ ] **IG-11-021 P2** — Create an archive policy for replaced or revoked assets.

## Asset registry system

- [ ] **IG-11-022 P1** — Define the scope, public API, and versioned data format of the asset registry.
- [ ] **IG-11-023 P1** — Implement the smallest deterministic reference path for the asset registry.
- [ ] **IG-11-024 P1** — Add input validation and actionable failure reporting to the asset registry.
- [ ] **IG-11-025 P1** — Add focused unit tests for the asset registry.
- [ ] **IG-11-026 P1** — Add an integration scenario exercising the asset registry in a running build.
- [ ] **IG-11-027 P1** — Define save/checkpoint restoration rules so registry state survives interrupted builds.
- [ ] **IG-11-028 P2** — Add logging and debug inspection for the asset registry.
- [ ] **IG-11-029 P2** — Document usage examples and common failure modes for the asset registry.

## License policy validator

- [ ] **IG-11-030 P1** — Define the scope, public API, and versioned data format of the license policy validator.
- [ ] **IG-11-031 P1** — Implement the smallest deterministic reference path for the license policy validator.
- [ ] **IG-11-032 P1** — Add input validation and actionable failure reporting to the license policy validator.
- [ ] **IG-11-033 P1** — Add focused unit tests for the license policy validator.
- [ ] **IG-11-034 P1** — Add an integration scenario exercising the license policy validator in a running build.
- [ ] **IG-11-035 P2** — Add logging and debug inspection for the license policy validator.
- [ ] **IG-11-036 P2** — Define CPU/latency budgets for the license policy validator on a full asset scan.
- [ ] **IG-11-037 P2** — Document usage examples and common failure modes for the license policy validator.

## Asset dependency graph

- [ ] **IG-11-038 P1** — Define the scope, public API, and versioned data format of the asset dependency graph.
- [ ] **IG-11-039 P1** — Implement the smallest deterministic reference path for the asset dependency graph.
- [ ] **IG-11-040 P1** — Add input validation and actionable failure reporting to the asset dependency graph.
- [ ] **IG-11-041 P1** — Add focused unit tests for the asset dependency graph.
- [ ] **IG-11-042 P1** — Add an integration scenario exercising the asset dependency graph in a running build.
- [ ] **IG-11-043 P2** — Add logging and debug inspection for the asset dependency graph.
- [ ] **IG-11-044 P2** — Define CPU/memory budgets for the asset dependency graph at full-campaign asset scale.
- [ ] **IG-11-045 P2** — Document usage examples and common failure modes for the asset dependency graph.

## Incremental asset build cache

- [ ] **IG-11-046 P1** — Define the scope, public API, and versioned data format of the incremental build cache.
- [ ] **IG-11-047 P1** — Implement the smallest deterministic reference path for the incremental build cache.
- [ ] **IG-11-048 P1** — Add input validation and actionable failure reporting to the incremental build cache.
- [ ] **IG-11-049 P1** — Add focused unit tests for the incremental build cache.
- [ ] **IG-11-050 P1** — Add an integration scenario exercising the incremental build cache in a running build.
- [ ] **IG-11-051 P2** — Add logging and debug inspection for the incremental build cache.
- [ ] **IG-11-052 P2** — Profile the incremental build cache under a representative full-campaign rebuild.
- [ ] **IG-11-053 P2** — Document usage examples and common failure modes for the incremental build cache.

## Asset quarantine workflow

- [ ] **IG-11-054 P1** — Define the scope, public API, and versioned data format of the quarantine workflow.
- [ ] **IG-11-055 P1** — Implement the smallest deterministic reference path for the quarantine workflow.
- [ ] **IG-11-056 P1** — Add input validation and actionable failure reporting to the quarantine workflow.
- [ ] **IG-11-057 P1** — Add focused unit tests for the quarantine workflow.
- [ ] **IG-11-058 P1** — Add an integration scenario exercising the quarantine workflow in a running build.
- [ ] **IG-11-059 P2** — Add logging and debug inspection for the quarantine workflow.
- [ ] **IG-11-060 P2** — Document usage examples and common failure modes for the quarantine workflow.

## Release asset manifest

- [ ] **IG-11-061 P1** — Define the scope, public API, and versioned data format of the release asset manifest.
- [ ] **IG-11-062 P1** — Implement the smallest deterministic reference path for the release asset manifest.
- [ ] **IG-11-063 P1** — Add input validation and actionable failure reporting to the release asset manifest.
- [ ] **IG-11-064 P1** — Add focused unit tests for the release asset manifest.
- [ ] **IG-11-065 P1** — Add an integration scenario exercising the release asset manifest in a build/release flow.
- [ ] **IG-11-066 P2** — Add logging and debug inspection for the release asset manifest.
- [ ] **IG-11-067 P2** — Document usage examples and common failure modes for the release asset manifest.
