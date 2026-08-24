# 35. Performance, memory, and scalability

[Back to master plan](../plan.md)

Establish budgets and measure representative district workloads against the locked performance target (docs/performance-targets.md: baked lighting + one dynamic sun + limited shadows, ~2-4GB RAM, 512MB-1GB VRAM, 720p/30fps minimum, 1080p/60fps recommended, EasyGL as the shipped backend).

- [x] **IG-35-001 P0** — Define target hardware and frame-rate/resolution goals from docs/performance-targets.md. *(Locked minimum: 720p/30 FPS, 2 GiB RAM, 512 MiB VRAM; recommended: 1080p/60 FPS. The automated first-district capture currently runs at 1280x720.)*
- [x] **IG-35-002 P0** — Define initial CPU, GPU, memory, district-load, physics, AI, and audio budgets sized for one district (10-20 pedestrians, 5-10 physics vehicles, one sun, limited shadows). *(Initial p95 budgets are explicit in `docs/performance-targets.md` and emitted in every JSON report. End-to-end frame interval remains the GPU/user-visible gate; supported EasyGL drivers additionally provide real asynchronous `gpu_render` command-range timing as a diagnostic.)*
- [x] **IG-35-003 P0** — Instrument frame time by major subsystem (render, physics, AI, audio, district load). *(`PerformanceProfiler` captures end-to-end frame interval plus update/render/Present/physics/AI/audio/district-load/startup CPU and a non-blocking real GPU Draw-range query where supported. Render CPU is command submission; GPU timing excludes Present; Present CPU measures CNA EndDraw/swap/backend flush; frame interval remains authoritative.)*
- [x] **IG-35-004 P0** — Record a baseline from the first running procedural prototype. *(Release EasyGL baseline recorded in `docs/performance-baseline.md`: intro passes 30 FPS, mixed workload fails; RAM/load pass; VRAM incomplete.)*
- [x] **IG-35-005 P1** — Track draw calls, state changes, vertices, triangles, instances, and visible objects. *(Done in JSON schema 2 as per-frame p95/average/maximum for exact Iron Gang 3D front-end submissions. Scope metadata excludes HUD backend batching and driver state deduplication; with no culling yet, every submitted scene object counts visible. The isolated 540-frame mixed run reports p95/max 18 draw calls, 56 explicit state calls, 1,768 vertices, 948 triangles, 16 geometry instances, and 67 visible objects.)*
- [ ] **IG-35-006 P1** — Track texture, buffer, render-target, and transient GPU memory against the 512MB-1GB VRAM target. *(Partial: game-owned meshes/lightmaps/HUD plus deduplicated imported CNJ vertex/index buffers and effect-bound textures are reported by category using logical public-API sizes. Backend programs, swapchain/depth/render targets, transients, driver padding, and physical residency remain unavailable, so reports still set `tracking_complete=false`.)*
- [x] **IG-35-007 P1** — Track district load/unload I/O, decompression, parse, upload, and activation latency (a full level load between districts, not continuous streaming). *(JSON schema 4 records each real full-level transition as world/physics unload+activation CPU, renderer rebuild/upload-submission CPU, and their total. Districts are currently procedural in-memory `PrototypeWorld`s, so I/O/decompression/parse are explicitly `null`/not applicable instead of fabricated zero-duration measurements; this must be extended if serialized district packages are introduced.)*
- [x] **IG-35-008 P1** — Track active physics bodies, contacts, queries, and physics step time for the modest vehicle/pedestrian count. *(JSON schema 5 retains the existing physics CPU timer and adds exact per-update Jolt-seam state/workload: total and active rigid bodies, active rigid-body contact manifolds, actual CharacterVirtual contacts, fixed steps, public gameplay raycasts, character collision updates, and vehicle wheel raycasts. Query categories remain separate so unlike operations are never combined into a misleading total.)*
- [x] **IG-35-009 P1** — Track simplified pedestrian/traffic/police AI counts and update time. *(Reports include peak traffic/pedestrian/police counts and AI update p95/average/maximum.)*
- [ ] **IG-35-010 P1** — Track pedestrian waypoint-graph path requests, queue time, and failures.
- [ ] **IG-35-011 P1** — Track active audio voices, decoders, streams, and bus cost.
- [ ] **IG-35-012 P1** — Create representative walking, driving, interior, traffic, district-transition, and mission captures. *(Partial: deterministic `intro`, `idle`, `walk`, `drive`, and `mixed` scenarios now isolate the first four relevant first-district workloads and mixed includes traffic plus a real district transition. Dedicated interior and mission captures remain.)*
- [ ] **IG-35-013 P1** — Create memory high-water and leak checks across repeated district load/unload cycles.
- [ ] **IG-35-014 P1** — Create content budgets enforced by validators (triangle/texture/material budgets per building/vehicle/character).
- [ ] **IG-35-015 P1** — Create frame-pacing and hitch histograms, including at district-transition load boundaries.
- [ ] **IG-35-016 P1** — Create release performance reports against the locked minimum/recommended hardware targets.
- [ ] **IG-35-017 P1** — Optimize only measured bottlenecks and record before/after evidence.
- [ ] **IG-35-018 P1** — Keep low-detail AI and rendering paths visually coherent at LOD/culling distance.
- [ ] **IG-35-019 P2** — Evaluate multithreaded culling after single-threaded costs are measured.
- [ ] **IG-35-020 P2** — Evaluate job granularity and contention only if a measured bottleneck justifies it.
- [ ] **IG-35-021 P2** — Evaluate texture streaming only after actual residency pressure appears against the VRAM budget.
- [ ] **IG-35-022 P2** — Evaluate occlusion culling after district/frustum/LOD wins are measured.
- [ ] **IG-35-023 P2** — Evaluate data-oriented component storage for proven hot loops only.
- [ ] **IG-35-024 P2** — Create automated capture comparison over time.
- [x] **IG-35-025 P1** — Define scope and implement the smallest working frame profiler (CPU/GPU time per subsystem, one sun + limited shadows, EasyGL only). *(Done: CPU subsystem/Present timings plus a true asynchronous GPU Draw-range query; unsupported drivers report why and never receive a fabricated CPU fallback.)*
- [x] **IG-35-026 P1** — Add unit tests and one real-flow integration test for the frame profiler. *(Done: exact statistic/report-policy tests plus isolated full-mixed and 120-frame GPU-timed EasyGL/Xvfb flows.)*
- [x] **IG-35-027 P1** — Add logging/overlay output and documentation for the frame profiler. *(Done through versioned JSON report output and `docs/performance-targets.md`/`performance-baseline.md`; a live overlay is unnecessary for the smallest automated profiler scope.)*
- [ ] **IG-35-028 P1** — Define scope and implement the smallest working memory tracker (RAM/VRAM high-water, per-category breakdown against the 2-4GB/512MB-1GB target). *(Partial: process RAM high-water and the three logical VRAM categories are implemented; a complete VRAM high-water needs backend residency data.)*
- [ ] **IG-35-029 P1** — Add unit tests and one real-flow integration test for the memory tracker. *(Partial: exact texture-size/JSON tests and an isolated EasyGL/Xvfb real-flow report pass; complete-residency behavior cannot yet be tested.)*
- [ ] **IG-35-030 P1** — Add logging/overlay output and documentation for the memory tracker. *(Partial: versioned JSON and performance documentation include category semantics and limitations; no live overlay or complete backend output.)*
- [x] **IG-35-031 P1** — Define scope and implement the smallest working district-load profiler (load time, asset counts, memory delta per transition). *(Every exit/save-load/reset district change records source/target/reason, phase and total CPU time, procedural world-object/static-body counts, current-RSS before/after/delta where available, and logical renderer video-memory before/after/delta. Startup is correctly separate from district-transition timing.)*
- [x] **IG-35-032 P1** — Add unit tests and one real-flow integration test for the district-load profiler. *(Exact phase aggregation, asset counts, signed positive/negative deltas, and not-applicable metadata are unit-tested. An isolated 540-frame Release EasyGL/Xvfb `mixed` flow captured exactly one real WarehouseBlock -> Countryside transition.)*
- [x] **IG-35-033 P1** — Add logging/overlay output and documentation for the district-load profiler. *(Done through schema-4 JSON and `docs/performance-targets.md`/`performance-baseline.md`; a live overlay adds no value for this infrequent, loading-screen-only event.)*
- [x] **IG-35-034 P1** — Define scope and implement the smallest working physics profiler (body/contact/query counts, step time). *(Profiling is opt-in with `--profile`; ordinary play avoids counter locks and collection. `PhysicsProfileSnapshot` consumes operation counters once per game update while retaining current body/contact state.)*
- [x] **IG-35-035 P1** — Add unit tests and one real-flow integration test for the physics profiler. *(`TestProfileSnapshotCountsStateAndConsumesOperations` proves exact bodies/steps/public rays/character updates/wheel rays plus live contacts and consume-vs-retain semantics. The isolated 540-frame Release EasyGL/Xvfb mixed flow sampled all 542 updates and reached the expected 9 bodies, 1 active body, 2 character contacts, 1 fixed step, 1 character update, and 4 wheel rays per sampled update.)*
- [x] **IG-35-036 P2** — Add logging/overlay output and documentation for the physics profiler. *(Done through schema-5 JSON plus `docs/performance-targets.md`, `performance-baseline.md`, and `validation.md`; a live overlay is unnecessary for this automated smallest scope.)*
- [ ] **IG-35-037 P1** — Define scope and implement the smallest working AI profiler (pedestrian/traffic/police counts and update time).
- [ ] **IG-35-038 P1** — Add unit tests and one real-flow integration test for the AI profiler.
- [ ] **IG-35-039 P2** — Add logging/overlay output and documentation for the AI profiler.
- [ ] **IG-35-040 P2** — Define scope and implement the smallest working audio profiler (active voice/stream/bus cost).
- [ ] **IG-35-041 P2** — Add unit tests, logging output, and documentation for the audio profiler.
- [ ] **IG-35-042 P1** — Define scope and implement the smallest working performance report generator (per-release summary against locked hardware targets).
- [ ] **IG-35-043 P1** — Add unit tests and one real-flow integration test for the performance report generator.
- [ ] **IG-35-044 P2** — Document usage of the performance report generator.
- [ ] **IG-35-045 P2** — Define scope and implement the smallest working content budget validator (triangle/texture/material limits at authoring/import time).
- [ ] **IG-35-046 P2** — Add unit tests and CI wiring for the content budget validator.
- [ ] **IG-35-047 P2** — Document usage and failure modes of the content budget validator.
- [ ] **IG-35-048 P2** — Define scope and implement the smallest working frame-pacing/hitch detector (combines pacing histogram and hitch flagging into one tool).
- [ ] **IG-35-049 P2** — Add unit tests and one real-flow integration test for the frame-pacing/hitch detector.
- [ ] **IG-35-050 P2** — Document usage of the frame-pacing/hitch detector.
- [ ] **IG-35-051 P2** — Define scope and implement the smallest working memory-leak soak test (repeated district load/unload, mission replay, save/load cycles).
- [ ] **IG-35-052 P2** — Add CI wiring and documentation for the memory-leak soak test.
