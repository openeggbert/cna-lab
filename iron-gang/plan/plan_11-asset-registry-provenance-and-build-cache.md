# 11. Asset registry, provenance, and build cache

[Back to master plan](../plan.md)

Make every production asset traceable, licensed, validated, and incrementally built. License/provenance tracking matters regardless of project scope; a catalog-browser UI and content-signing infrastructure do not, and are cut.

- [x] **IG-11-001 P0** — Make every production asset require an asset-registry entry. *(The validator enumerates runtime audio/config/dialogue/mission/cutscene content, MC3/glTF production sources, and the embedded bitmap font. `build-assets.sh` and CTest fail on any uncovered file.)*
- [x] **IG-11-002 P0** — Define an allow-list of accepted licenses. *(Version 1 permits only MIT, CC0, and Public Domain for today's shipping rows; additions require an explicit policy/code review.)*
- [x] **IG-11-003 P0** — Store a local copy or digest of relevant license evidence where permitted. *(Every row names a local evidence file and exact SHA-256. Original assets bind `LICENSE`; external font/audio bind reviewed primary-source records under `assets/licenses/evidence/`.)*
- [x] **IG-11-004 P0** — Reject unknown-license assets from release packages. *(Unknown licenses, missing approval, and denied commercial-use/redistribution rights exit 2; the same check gates asset conversion and CTest.)*
- [x] **IG-11-005 P1** — Add source URL, author, license, acquisition date, hash, modification, and attribution fields. *(All are mandatory or policy-conditionally mandatory in the exact version-1 CSV header.)*
- [x] **IG-11-006 P1** — Add commercial-use, redistribution, and AI-processing review fields. *(Canonical booleans are required; commercial use and redistribution must both be approved for shipping.)*
- [x] **IG-11-007 P1** — Add asset reviewer and approval status. *(Reviewer, `approval_status=approved`, and `quality_state=shipping` are mandatory.)*
- [x] **IG-11-008 P1** — Generate THIRD_PARTY_ASSETS.md from the registry. *(The deterministic per-file notice contains external asset/source/license/content hashes plus local evidence hashes and reviewers; CTest refuses stale output.)*
- [x] **IG-11-009 P1** — Detect unregistered files under production asset directories. *(Recursive production-set discovery caught the previously omitted runtime `config/game.json`; focused tests add an unknown WAV and require actionable refusal.)*
- [x] **IG-11-010 P1** — Detect duplicate assets by content hash. *(Distinct shipping rows may not share an asset SHA-256.)*
- [x] **IG-11-011 P1** — Detect case-collision and path-portability problems. *(Paths are normalized relative POSIX paths inside the project; symlink/escape and case-fold collisions are rejected.)*
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

- [x] **IG-11-022 P1** — Define the scope, public API, and versioned data format of the asset registry. *(Exact schema-version-1 CSV header plus `validate`/`--check-notice`/`--write-notice` CLI behavior is documented in `assets/README.md`.)*
- [x] **IG-11-023 P1** — Implement the smallest deterministic reference path for the asset registry. *(`scripts/asset_registry.py`, Python standard library only.)*
- [x] **IG-11-024 P1** — Add input validation and actionable failure reporting to the asset registry. *(Strict CSV shape, canonical types/paths/URLs/dates, hashes, evidence, rights, approval, coverage, duplicates, and output-alias safety exit 2 with a named cause.)*
- [x] **IG-11-025 P1** — Add focused unit tests for the asset registry. *(Seven CLI tests cover current integration, deterministic/atomic notice generation, missing coverage, asset/evidence tamper, policy/approval, duplicate/case collision, stale notice, and overwrite refusal.)*
- [x] **IG-11-026 P1** — Add an integration scenario exercising the asset registry in a running build. *(`build-assets.sh` validates registry + notice before conversion; `iron_gang_asset_registry_tests` validates the real committed project in CTest.)*
- [ ] **IG-11-027 P1** — Define save/checkpoint restoration rules so registry state survives interrupted builds.
- [x] **IG-11-028 P2** — Add logging and debug inspection for the asset registry. *(Successful CLI output reports approved/external counts and validate/write/check action; the generated notice is the reviewable inspection form.)*
- [x] **IG-11-029 P2** — Document usage examples and common failure modes for the asset registry. *(README, assets README, THIRD_PARTY, validation, and continuity docs.)*

## License policy validator

- [x] **IG-11-030 P1** — Define the scope, public API, and versioned data format of the license policy validator. *(The asset-registry version-1 contract owns license allow-list/evidence/rights/review state rather than duplicating a second policy file.)*
- [x] **IG-11-031 P1** — Implement the smallest deterministic reference path for the license policy validator. *(Shared in `scripts/asset_registry.py`.)*
- [x] **IG-11-032 P1** — Add input validation and actionable failure reporting to the license policy validator. *(Unknown/noncanonical license data, absent evidence, changed evidence, invalid external source/date, denied shipping rights, or incomplete review is refused.)*
- [x] **IG-11-033 P1** — Add focused unit tests for the license policy validator. *(The seven registry CLI tests include six policy mutations and evidence tamper.)*
- [x] **IG-11-034 P1** — Add an integration scenario exercising the license policy validator in a running build. *(Real-project CTest plus the pre-conversion `build-assets.sh` gate.)*
- [x] **IG-11-035 P2** — Add logging and debug inspection for the license policy validator. *(Counts plus generated notice/evidence table.)*
- [ ] **IG-11-036 P2** — Define CPU/latency budgets for the license policy validator on a full asset scan.
- [x] **IG-11-037 P2** — Document usage examples and common failure modes for the license policy validator. *(The same single registry workflow is documented rather than presenting license validation as a separate command.)*

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
