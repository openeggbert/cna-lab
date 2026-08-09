# 00. Repository foundation and completed scaffold

[Back to master plan](../plan.md)


Track what already exists and close the gap between source validation and a fully running CNA executable.

- [x] **IG-00-001 P0** — Create an original provisional project name and fictional city identity.
- [x] **IG-00-002 P0** — Create a standalone C++23 CMake project.
- [x] **IG-00-003 P0** — Integrate CNA and sharp-runtime as source dependencies.
- [x] **IG-00-004 P0** — Create persistent CMake presets for EasyGL, Vulkan, software, and release builds.
- [x] **IG-00-005 P0** — Limit documented builds to four parallel jobs.
- [x] **IG-00-006 P0** — Add compiler-warning and final-link helper modules.
- [x] **IG-00-007 P0** — Create a CNA Game-derived application class.
- [x] **IG-00-008 P0** — Create a procedural 3D prototype renderer using CNA vertex/index buffers and BasicEffect.
- [x] **IG-00-009 P0** — Create a prototype Iron City city block.
- [x] **IG-00-010 P0** — Create on-foot movement, sprinting, strafing, turning, and simple collision.
- [x] **IG-00-011 P0** — Create a kinematic drivable sedan with basic acceleration, drag, steering, and handbrake.
- [x] **IG-00-012 P0** — Create enter/exit vehicle interaction.
- [x] **IG-00-013 P0** — Create a dialogue flow and fallback dialogue data.
- [x] **IG-00-014 P0** — Create a prototype mission state machine.
- [x] **IG-00-015 P0** — Create save/load using sharp-runtime System::IO.
- [x] **IG-00-016 P0** — Create core tests for collision, vehicle motion, mission flow, dialogue, and persistence.
- [x] **IG-00-017 P0** — Create an MC3 source scene for the prototype block.
- [x] **IG-00-018 P0** — Validate the prototype MC3 scene against the supplied MC3 schema.
- [x] **IG-00-019 P0** — Create an MC3 to GLB to CNJ conversion helper script.
- [x] **IG-00-020 P0** — Add README, analysis, architecture, asset-pipeline, legal, contribution, and third-party documentation.
- [x] **IG-00-021 P0** — Add MIT license, .gitignore, .editorconfig, and .clang-format.
- [x] **IG-00-022 P0** — Add an asset license registry template.
- [x] **IG-00-023 P0** — Run C++23 syntax-only validation against the supplied CNA and sharp-runtime headers.
- [x] **IG-00-024 P0** — Build the supplied sharp-runtime source as a static library.
- [x] **IG-00-025 P0** — Execute a sharp-runtime file I/O smoke test.
- [x] **IG-00-026 P0** — Obtain or initialize a CNA checkout with SDL, SDL_image, and SDL_mixer submodules populated.
- [x] **IG-00-027 P0** — Obtain the EasyGL sibling repository for the primary EasyGL preset.
- [x] **IG-00-028 P0** — Complete the first fully linked Iron Gang build against CNA (compile-software preset; a real-rendering-backend build/run is still open, see IG-00-029).
- [ ] **IG-00-029 P0** — Run the graphical prototype on a real rendering backend (EasyGL) and verify controls, mission completion, and save/load.
- [x] **IG-00-030 P0** — Run core tests through CTest in the integrated workspace.
- [ ] **IG-00-031 P1** — Capture the first known-good compiler, dependency revisions, backend, and platform matrix.
- [ ] **IG-00-032 P1** — Create a short gameplay capture of the procedural prototype for regression reference.
- [ ] **IG-00-033 P1** — Tag the first running scaffold as v0.1.0-prototype.
