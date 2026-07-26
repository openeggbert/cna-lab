# 35. Performance, memory, and scalability

[Back to master plan](../plan.md)

Establish budgets and measure representative district workloads against the locked performance target (docs/performance-targets.md: baked lighting + one dynamic sun + limited shadows, ~2-4GB RAM, 512MB-1GB VRAM, 720p/30fps minimum, 1080p/60fps recommended, EasyGL as the shipped backend).

- [ ] **IS-35-001 P0** — Define target hardware and frame-rate/resolution goals from docs/performance-targets.md.
- [ ] **IS-35-002 P0** — Define initial CPU, GPU, memory, district-load, physics, AI, and audio budgets sized for one district (10-20 pedestrians, 5-10 physics vehicles, one sun, limited shadows).
- [ ] **IS-35-003 P0** — Instrument frame time by major subsystem (render, physics, AI, audio, district load).
- [ ] **IS-35-004 P0** — Record a baseline from the first running procedural prototype.
- [ ] **IS-35-005 P1** — Track draw calls, state changes, vertices, triangles, instances, and visible objects.
- [ ] **IS-35-006 P1** — Track texture, buffer, render-target, and transient GPU memory against the 512MB-1GB VRAM target.
- [ ] **IS-35-007 P1** — Track district load/unload I/O, decompression, parse, upload, and activation latency (a full level load between districts, not continuous streaming).
- [ ] **IS-35-008 P1** — Track active physics bodies, contacts, queries, and physics step time for the modest vehicle/pedestrian count.
- [ ] **IS-35-009 P1** — Track simplified pedestrian/traffic/police AI counts and update time.
- [ ] **IS-35-010 P1** — Track pedestrian waypoint-graph path requests, queue time, and failures.
- [ ] **IS-35-011 P1** — Track active audio voices, decoders, streams, and bus cost.
- [ ] **IS-35-012 P1** — Create representative walking, driving, interior, traffic, district-transition, and mission captures.
- [ ] **IS-35-013 P1** — Create memory high-water and leak checks across repeated district load/unload cycles.
- [ ] **IS-35-014 P1** — Create content budgets enforced by validators (triangle/texture/material budgets per building/vehicle/character).
- [ ] **IS-35-015 P1** — Create frame-pacing and hitch histograms, including at district-transition load boundaries.
- [ ] **IS-35-016 P1** — Create release performance reports against the locked minimum/recommended hardware targets.
- [ ] **IS-35-017 P1** — Optimize only measured bottlenecks and record before/after evidence.
- [ ] **IS-35-018 P1** — Keep low-detail AI and rendering paths visually coherent at LOD/culling distance.
- [ ] **IS-35-019 P2** — Evaluate multithreaded culling after single-threaded costs are measured.
- [ ] **IS-35-020 P2** — Evaluate job granularity and contention only if a measured bottleneck justifies it.
- [ ] **IS-35-021 P2** — Evaluate texture streaming only after actual residency pressure appears against the VRAM budget.
- [ ] **IS-35-022 P2** — Evaluate occlusion culling after district/frustum/LOD wins are measured.
- [ ] **IS-35-023 P2** — Evaluate data-oriented component storage for proven hot loops only.
- [ ] **IS-35-024 P2** — Create automated capture comparison over time.
- [ ] **IS-35-025 P1** — Define scope and implement the smallest working frame profiler (CPU/GPU time per subsystem, one sun + limited shadows, EasyGL only).
- [ ] **IS-35-026 P1** — Add unit tests and one real-flow integration test for the frame profiler.
- [ ] **IS-35-027 P1** — Add logging/overlay output and documentation for the frame profiler.
- [ ] **IS-35-028 P1** — Define scope and implement the smallest working memory tracker (RAM/VRAM high-water, per-category breakdown against the 2-4GB/512MB-1GB target).
- [ ] **IS-35-029 P1** — Add unit tests and one real-flow integration test for the memory tracker.
- [ ] **IS-35-030 P1** — Add logging/overlay output and documentation for the memory tracker.
- [ ] **IS-35-031 P1** — Define scope and implement the smallest working district-load profiler (load time, asset counts, memory delta per transition).
- [ ] **IS-35-032 P1** — Add unit tests and one real-flow integration test for the district-load profiler.
- [ ] **IS-35-033 P1** — Add logging/overlay output and documentation for the district-load profiler.
- [ ] **IS-35-034 P1** — Define scope and implement the smallest working physics profiler (body/contact/query counts, step time).
- [ ] **IS-35-035 P1** — Add unit tests and one real-flow integration test for the physics profiler.
- [ ] **IS-35-036 P2** — Add logging/overlay output and documentation for the physics profiler.
- [ ] **IS-35-037 P1** — Define scope and implement the smallest working AI profiler (pedestrian/traffic/police counts and update time).
- [ ] **IS-35-038 P1** — Add unit tests and one real-flow integration test for the AI profiler.
- [ ] **IS-35-039 P2** — Add logging/overlay output and documentation for the AI profiler.
- [ ] **IS-35-040 P2** — Define scope and implement the smallest working audio profiler (active voice/stream/bus cost).
- [ ] **IS-35-041 P2** — Add unit tests, logging output, and documentation for the audio profiler.
- [ ] **IS-35-042 P1** — Define scope and implement the smallest working performance report generator (per-release summary against locked hardware targets).
- [ ] **IS-35-043 P1** — Add unit tests and one real-flow integration test for the performance report generator.
- [ ] **IS-35-044 P2** — Document usage of the performance report generator.
- [ ] **IS-35-045 P2** — Define scope and implement the smallest working content budget validator (triangle/texture/material limits at authoring/import time).
- [ ] **IS-35-046 P2** — Add unit tests and CI wiring for the content budget validator.
- [ ] **IS-35-047 P2** — Document usage and failure modes of the content budget validator.
- [ ] **IS-35-048 P2** — Define scope and implement the smallest working frame-pacing/hitch detector (combines pacing histogram and hitch flagging into one tool).
- [ ] **IS-35-049 P2** — Add unit tests and one real-flow integration test for the frame-pacing/hitch detector.
- [ ] **IS-35-050 P2** — Document usage of the frame-pacing/hitch detector.
- [ ] **IS-35-051 P2** — Define scope and implement the smallest working memory-leak soak test (repeated district load/unload, mission replay, save/load cycles).
- [ ] **IS-35-052 P2** — Add CI wiring and documentation for the memory-leak soak test.
