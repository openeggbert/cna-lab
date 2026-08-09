# 10. glTF, CNJ, MCB, and runtime packages

[Back to master plan](../plan.md)

Preserve both render data and gameplay metadata through conversion and packaging, for a single shipped campaign (no DLC/patch/hot-reload-package system — that is out of scope).

- [ ] **IG-10-001 P0** — Document exactly which MC3 semantics survive glTF export.
- [ ] **IG-10-002 P0** — Create a sidecar metadata path for semantics not represented in glTF.
- [x] **IG-10-003 P0** — Load one converted CNJ model in the game.
- [ ] **IG-10-004 P0** — Define a runtime package manifest with version, dependencies, hashes, and district ownership.
- [ ] **IG-10-004b P1** — Confirmed gap: `cna_tool_gltf_to_cnj` does not bake per-object glTF node transforms into vertex data (verified empirically: four identically-shaped, differently-positioned MC3 boxes produced byte-identical CNJ vertex buffers). A multi-object MC3 scene loaded as one CNJ `Model` therefore loses each object's relative position. Workaround in place for the sedan (four single-object MC3 files, composed with Iron Gang's own per-part transforms in `PrototypeRenderer`); a real fix (baking node-world-transform into vertex positions during conversion, or emitting a per-mesh-part transform in the CNJ `Model` format for `ContentManager` to apply) belongs upstream in CNA/Mesh Craft, not as a permanent Iron Gang workaround, since it will keep costing extra per-part MC3 files and manual composition code for every future multi-part prop otherwise.

## glTF validation stage

- [ ] **IG-10-005 P1** — Define the scope, responsibilities, and explicit non-goals of glTF validation.
- [ ] **IG-10-006 P1** — Define the public C++ API for glTF validation.
- [ ] **IG-10-007 P1** — Implement the smallest deterministic reference path for glTF validation before CNJ conversion.
- [ ] **IG-10-008 P1** — Add input validation and actionable failure reporting to glTF validation.
- [ ] **IG-10-009 P1** — Add focused unit tests for glTF validation.
- [ ] **IG-10-010 P1** — Add an integration scenario exercising glTF validation in a running conversion flow.
- [ ] **IG-10-011 P2** — Add logging, budgets, and profiling for glTF validation.
- [ ] **IG-10-012 P2** — Document usage examples and common failure modes for glTF validation.

## CNJ conversion stage

- [ ] **IG-10-013 P1** — Define the scope, responsibilities, and explicit non-goals of CNJ conversion.
- [ ] **IG-10-014 P1** — Define the public C++ API for CNJ conversion.
- [ ] **IG-10-015 P1** — Implement the smallest deterministic reference path for CNJ conversion.
- [ ] **IG-10-016 P1** — Create CNJ validation after conversion.
- [ ] **IG-10-017 P1** — Create deterministic output paths and names.
- [ ] **IG-10-018 P1** — Add focused unit tests for CNJ conversion.
- [ ] **IG-10-019 P1** — Add an integration scenario exercising CNJ conversion in a running game flow.
- [ ] **IG-10-020 P2** — Add logging, budgets, and profiling for CNJ conversion.
- [ ] **IG-10-021 P2** — Document usage examples and common failure modes for CNJ conversion.

## Runtime package compiler

- [ ] **IG-10-022 P1** — Define the scope, responsibilities, and explicit non-goals of the runtime package compiler.
- [ ] **IG-10-023 P1** — Define the public C++ API for the runtime package compiler.
- [ ] **IG-10-024 P1** — Create a direct MC3/MCB-to-runtime-metadata compiler path.
- [ ] **IG-10-025 P1** — Define runtime package chunking and compression.
- [ ] **IG-10-026 P1** — Define endianness, alignment, and version rules.
- [ ] **IG-10-027 P1** — Define package dependency cycles as build errors.
- [ ] **IG-10-028 P1** — Implement the smallest deterministic reference path for the package compiler.
- [ ] **IG-10-029 P1** — Add focused unit tests for the package compiler.
- [ ] **IG-10-030 P1** — Add an integration scenario exercising the package compiler in a running game flow.
- [ ] **IG-10-031 P2** — Add logging, budgets, and profiling for the package compiler.
- [ ] **IG-10-032 P2** — Document usage examples and common failure modes for the package compiler.

## Runtime package reader

- [ ] **IG-10-033 P1** — Define the scope, responsibilities, and explicit non-goals of the runtime package reader.
- [ ] **IG-10-034 P1** — Define the public C++ API for the runtime package reader.
- [ ] **IG-10-035 P1** — Implement the smallest deterministic reference path for the package reader.
- [ ] **IG-10-036 P1** — Create package read limits against malformed counts and sizes.
- [ ] **IG-10-037 P1** — Create source-to-runtime traceability in package metadata.
- [ ] **IG-10-038 P1** — Add focused unit tests for the package reader.
- [ ] **IG-10-039 P1** — Add an integration scenario exercising the package reader in a running game flow.
- [ ] **IG-10-040 P2** — Add logging, budgets, and profiling for the package reader.
- [ ] **IG-10-041 P2** — Create backward-compatible package-reader tests across format versions.
- [ ] **IG-10-042 P2** — Document usage examples and common failure modes for the package reader.

## Package inspection (small CLI helper, not a GUI editor)

- [ ] **IG-10-043 P1** — Define the scope of a command-line package inspector (dump manifest, contents, and sizes).
- [ ] **IG-10-044 P1** — Implement the smallest deterministic reference path for package inspection.
- [ ] **IG-10-045 P2** — Add focused unit tests for package inspection.
- [ ] **IG-10-046 P2** — Document usage examples for package inspection.

## Content-addressed conversion cache

- [ ] **IG-10-047 P1** — Define the scope of a content-addressed cache keyed by source hash for conversion outputs.
- [ ] **IG-10-048 P1** — Implement the smallest deterministic reference path for the conversion cache.
- [ ] **IG-10-049 P1** — Add input validation and actionable failure reporting to the conversion cache.
- [ ] **IG-10-050 P1** — Add focused unit tests for the conversion cache, including cache-invalidation cases.
- [ ] **IG-10-051 P2** — Add logging and debug inspection for the conversion cache.
- [ ] **IG-10-052 P2** — Document usage examples for the conversion cache.

## Shared typed-asset package builder

One shared builder architecture handles texture, animation, collision, navigation, and audio package data through a common interface; only genuinely type-specific steps get their own task.

- [ ] **IG-10-053 P1** — Define the scope, responsibilities, and explicit non-goals of the shared typed-asset package builder.
- [ ] **IG-10-054 P1** — Define the public C++ API and per-type extension points for the shared builder.
- [ ] **IG-10-055 P1** — Implement the smallest deterministic reference path for the shared builder using texture data.
- [ ] **IG-10-056 P1** — Extend the shared builder to animation package data.
- [ ] **IG-10-057 P1** — Extend the shared builder to collision package data.
- [ ] **IG-10-058 P1** — Extend the shared builder to navigation package data.
- [ ] **IG-10-059 P1** — Extend the shared builder to audio package data.
- [ ] **IG-10-060 P1** — Add input validation and actionable failure reporting to the shared builder.
- [ ] **IG-10-061 P1** — Add focused unit tests covering all five data types through the shared builder.
- [ ] **IG-10-062 P1** — Add an integration scenario exercising the shared builder in a running game flow.
- [ ] **IG-10-063 P2** — Add logging, budgets, and profiling for the shared builder.
- [ ] **IG-10-064 P2** — Document usage examples and common failure modes for the shared builder.

## Format evaluation and scope decisions

- [ ] **IG-10-065 P2** — Evaluate whether MCB remains a source cache or becomes part of shipping data.
- [ ] **IG-10-066 P2** — Evaluate mesh compression after load time and disk pressure are measured.
- [ ] **IG-10-067 P2** — Evaluate texture container formats per target platform.
- [ ] **IG-10-068 P3** — Evaluate streaming individual package chunks directly from archives, only if load-time profiling shows a need.
