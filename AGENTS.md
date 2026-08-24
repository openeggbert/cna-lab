# AGENTS.md — People

These instructions apply to the entire `people-cna` repository.

## Mission

People is an original household life simulator with a four-direction,
sprite-based 2D isometric runtime. Build a durable foundation while preserving
small playable milestones. The household loop, object interactions, routing,
motives, autonomy, social systems, economy, and emergent behavior matter more
than speculative framework layers.

## Required start-of-task workflow

Every coding agent must, in order:

1. read this file completely;
2. read the relevant sections of `analysis.md`;
3. inspect `plan.md` and select one coherent `PEO-*` task;
4. inspect the real public API in `../cnanext` before writing CNA calls;
5. confirm `../cnanext` is configured to use `../sharp-runtimenext`;
6. implement the smallest complete increment satisfying its acceptance criteria;
7. build the affected target;
8. run relevant tests and, for visual work, the applicable runtime smoke test;
9. update the task status and any affected documentation;
10. commit the verified task coherently on `develop`.

Do not mark a task `DONE` without recorded verification. If blocked, add or
update a `PEO-CNA-*` or `PEO-SR-*` blocker, mark the task `BLOCKED`, and choose
another independent task when useful work remains.

## Hard technical rules

- Use C++23 and CMake.
- Use CNA public APIs. Link CNA's supported aggregate target rather than
  reconstructing its private dependency graph.
- Use `../cnanext`, never `../cna`.
- Configure CNA's `CNA_SHARP_RUNTIME_ROOT` as `../sharp-runtimenext`, never
  `../sharp-runtime`.
- Track the newest local HEAD present in both `*next` repositories. Do not pin,
  reset, check out, or roll dependencies back to a previously recorded People
  verification SHA. Re-record exact SHAs and rebuild when another agent advances
  either checkout.
- Do not invent CNA APIs or CMake options. Search the current checkout first.
- Game code must not call SDL, OpenGL, Vulkan, Direct3D, Metal, native window
  APIs, or another backend to bypass CNA.
- Runtime world rendering remains 2D isometric sprites. Do not introduce a
  realtime 3D world, 3D asset dependency, perspective camera, or arbitrary
  camera rotation.
- Keep simulation state independent of rendering state and the render frame
  rate.
- Use deterministic fixed simulation ticks and explicit seeded random streams.
- Keep game logic headlessly testable where practical.
- Do not serialize raw C++ object memory. Persistent formats are explicit and
  versioned.

## Legal and content rules

- No original commercial-game data is permitted in source, generated output,
  tests, fixtures, documentation screenshots, or runtime requirements.
- Do not implement loaders or compatibility for proprietary game archives,
  object resources, behavior bytecode, neighborhoods, or saves as part of the
  primary project.
- Do not copy source from FreeSO or Simitone. They are architecture references;
  FreeSO is MPL-2.0 and Simitone includes FreeSO while its top-level licensing
  is not sufficiently explicit for reuse without a dedicated review.
- Do not reproduce a copyrighted asset one-for-one, including through a
  generative prompt.
- Do not add downloaded or generated assets without provenance and
  redistribution information in the asset manifest.
- New People code is MIT-licensed. This does not relicense CNA, sharp-runtime,
  reference projects, or third-party assets.
- `People` is a working title; preserve the pre-release trademark-clearance
  requirement.

## Architecture discipline

- Prefer a functioning vertical slice to empty abstractions.
- Begin interactions in native C++, then extract common deterministic
  primitives and data sequences after several objects prove the shape.
- Avoid global per-person/per-object scans every render frame. Introduce
  spatial or cached queries when measurements justify them.
- Objects advertise interactions and utility; final autonomy must not be a
  growing collection of motive-specific `if` statements.
- Route to explicit interaction slots and reserve exclusive resources.
- Release reservations on completion, cancellation, interruption, route
  failure, object deletion, and person deletion.
- Give asynchronous actions explicit states: queued, routing, executing,
  completed, failed, canceled, or interrupted.
- Wall topology lives on tile edges. Floors, walls, objects, rooms, and people
  remain logical simulation data even though presentation uses sprites.

## Source and test layout

- Application sources live under `game/` because People consumes CNA with
  `add_subdirectory` and must not collide with CNA's own layout validation.
- Framework-independent code lives in a small `people_core` library.
- Tests live under `tests/` and initially use a dependency-free executable so
  coordinate/simulation gates work without fetching a test framework.
- Public People headers use clear documentation for units, invariants, and
  ownership. Avoid comments that merely narrate the code.
- Run formatting tools only on files owned by the current task.

## Git workflow

- `main` holds the initial documentation/planning baseline.
- Implementation occurs on `develop`; do not merge to `main` during the
  initial autonomous campaign.
- Preserve unrelated working-tree changes, especially the current dirty
  `../cnanext` checkout.
- Stage task files by explicit path; never use `git add .` or `git add -A`.
- One coherent task or milestone per commit. Include its stable ID after the
  two prescribed bootstrap commit messages.
- Never push unless explicitly requested.

## Verification and handoff

At session end report the branch, HEAD, worktree state, commits, completed and
next tasks, build/test commands and actual results, runtime result, exact CNA
and sharp-runtime SHAs plus dirty state, blockers, playable functionality,
architecture decisions, asset provenance, and major risks. Never report a
command or visual result that was not actually observed.
