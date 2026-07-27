# 00. Repository foundation and completed scaffold

[Back to master plan](../plan.md)


Track what already exists and close the gap between source validation and a fully running CNA executable.

- [x] **IS-00-001 P0** — Create an original provisional project name and fictional city identity.
- [x] **IS-00-002 P0** — Create a standalone C++23 CMake project.
- [x] **IS-00-003 P0** — Integrate CNA and sharp-runtime as source dependencies.
- [x] **IS-00-004 P0** — Create persistent CMake presets for EasyGL, Vulkan, software, and release builds.
- [x] **IS-00-005 P0** — Limit documented builds to four parallel jobs.
- [x] **IS-00-006 P0** — Add compiler-warning and final-link helper modules.
- [x] **IS-00-007 P0** — Create a CNA Game-derived application class.
- [x] **IS-00-008 P0** — Create a procedural 3D prototype renderer using CNA vertex/index buffers and BasicEffect.
- [x] **IS-00-009 P0** — Create a prototype Iron City city block.
- [x] **IS-00-010 P0** — Create on-foot movement, sprinting, strafing, turning, and simple collision.
- [x] **IS-00-011 P0** — Create a kinematic drivable sedan with basic acceleration, drag, steering, and handbrake.
- [x] **IS-00-012 P0** — Create enter/exit vehicle interaction.
- [x] **IS-00-013 P0** — Create a dialogue flow and fallback dialogue data.
- [x] **IS-00-014 P0** — Create a prototype mission state machine.
- [x] **IS-00-015 P0** — Create save/load using sharp-runtime System::IO.
- [x] **IS-00-016 P0** — Create core tests for collision, vehicle motion, mission flow, dialogue, and persistence.
- [x] **IS-00-017 P0** — Create an MC3 source scene for the prototype block.
- [x] **IS-00-018 P0** — Validate the prototype MC3 scene against the supplied MC3 schema.
- [x] **IS-00-019 P0** — Create an MC3 to GLB to CNJ conversion helper script.
- [x] **IS-00-020 P0** — Add README, analysis, architecture, asset-pipeline, legal, contribution, and third-party documentation.
- [x] **IS-00-021 P0** — Add MIT license, .gitignore, .editorconfig, and .clang-format.
- [x] **IS-00-022 P0** — Add an asset license registry template.
- [x] **IS-00-023 P0** — Run C++23 syntax-only validation against the supplied CNA and sharp-runtime headers.
- [x] **IS-00-024 P0** — Build the supplied sharp-runtime source as a static library.
- [x] **IS-00-025 P0** — Execute a sharp-runtime file I/O smoke test.
- [x] **IS-00-026 P0** — Obtain or initialize a CNA checkout with SDL, SDL_image, and SDL_mixer submodules populated.
- [x] **IS-00-027 P0** — Obtain the EasyGL sibling repository for the primary EasyGL preset.
- [x] **IS-00-028 P0** — Complete the first fully linked Iron Shadows build against CNA (compile-software preset; a real-rendering-backend build/run is still open, see IS-00-029).
- [ ] **IS-00-029 P0** — Run the graphical prototype on a real rendering backend (EasyGL) and verify controls, mission completion, and save/load.
- [x] **IS-00-030 P0** — Run core tests through CTest in the integrated workspace.
- [ ] **IS-00-031 P1** — Capture the first known-good compiler, dependency revisions, backend, and platform matrix.
- [ ] **IS-00-032 P1** — Create a short gameplay capture of the procedural prototype for regression reference.
- [ ] **IS-00-033 P1** — Tag the first running scaffold as v0.1.0-prototype.

