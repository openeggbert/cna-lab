# 05. CNA integration and backend policy

[Back to master plan](../plan.md)

Use CNA correctly while limiting production to one real backend, not a multi-backend parity matrix.

- [x] **IG-05-001 P0** — Run the prototype on EasyGL and record all required sibling dependencies (cna, sharp-runtime, easy-gl, cna-extended).
- [x] **IG-05-002 P0** — Run core and smoke tests on the `compile-software` preset for fast CI compile-checking.
- [ ] **IG-05-003 P0** — Declare EasyGL the sole production backend; Vulkan is an optional secondary validation backend; software/headless is CI-only. All other CNA backends (D3D9, WebGPU, bgfx, SDL renderer, etc.) are explicitly out of scope for this game.
- [ ] **IG-05-004 P0** — Limit release blockers to the declared EasyGL production tier only.
- [ ] **IG-05-005 P1** — Run the prototype on Vulkan and record any behavioral differences from EasyGL (best-effort, not a release blocker).
- [ ] **IG-05-006 P1** — Create a backend capability log line at startup (backend name, key capability flags) — not a full capability database.
- [ ] **IG-05-007 P1** — Create graceful error messages for unsupported backend/capability combinations at startup.
- [ ] **IG-05-008 P1** — Centralize GraphicsDevice resource creation and disposal rules in one place.
- [ ] **IG-05-009 P1** — Document EasyGL device-lost/reset behavior and how Iron Gang responds.
- [ ] **IG-05-010 P1** — Validate window resize, fullscreen, DPI, focus, and suspend/resume behavior on EasyGL.
- [ ] **IG-05-011 P1** — Validate graphics-resource cleanup under repeated district load/unload (see plan_13).
- [ ] **IG-05-012 P1** — Add an integration test that boots the game, loads one district, and exits cleanly on the `compile-software` and EasyGL presets.
- [ ] **IG-05-013 P2** — Track any real CNA defects found with minimal reproductions outside the game.
- [ ] **IG-05-014 P2** — Prefer upstreaming general CNA fixes over maintaining hidden game-only patches.
- [ ] **IG-05-015 P2** — Write a short CNA/cna-extended revision upgrade checklist (what to re-run after bumping either dependency).
- [ ] **IG-05-016 P2** — Document input and audio behavior differences (if any) between EasyGL and Vulkan.
- [ ] **IG-05-017 P3** — Evaluate additional CNA backends only after the full campaign ships on EasyGL.
