# Third-party dependencies and references

People's MIT license covers only original People files. It does not relicense
dependencies, tools, references, or content.

## Runtime/build dependencies

| Component | Role | Checkout observed 2026-08-24 | License/status |
|---|---|---|---|
| CNA (`../cnanext`) | Public game/framework abstraction | `next`, executable verified at `b6cbfcd87c08a6e0172eaf866358bf95bec277b1` | Microsoft Public License; clean working tree |
| sharp-runtime (`../sharp-runtimenext`) | Supported runtime layer consumed by CNA | `next`, `54578590b328aa9612fe38bfddca9fd8ca795144`, `v0.1.0-beta.1` | MIT; clean working tree |
| CMake | Build generator | System tool | Governed by its own distribution terms |

CNA owns its backend and transitive third-party dependency selection. Consult
`../cnanext/THIRD_PARTY_NOTICES.md` and the notices shipped by an actual CNA
build before distribution. People game code does not call those backends
directly.

The initial planning inspection saw CNA at
`33ff296f5ffe42cfa9c3a2060da55a953f2a9f4e` with 31 pre-existing changes. It
advanced externally before executable verification. The recorded build SHA is
a verification snapshot, not a long-term dependency pin. Before a release,
build against the then-current clean, reviewed CNA commit and update this table.
During active development People follows the newest local `next` HEAD supplied
by dependency work. Historical SHAs remain evidence only, never rollback
targets.

## Architecture-only references

No code, data, art, audio, names, text, or binary resources from the following
projects are vendored or required.

### FreeSO

- Source: <https://github.com/riperiperi/FreeSO>
- Project structure notes:
  <https://github.com/riperiperi/FreeSO/wiki/Project-Structure>
- License: MPL-2.0 according to its repository.
- Boundary: FreeSO explicitly depends on original commercial game assets. That
  dependency model is incompatible with People requirements.
- Permitted use here: understand high-level separation of VM state, entities,
  primitives, routing, architecture, serialization, lot rendering, and debug
  tooling. Independently design and implement People behavior in C++23.

Copying FreeSO source into an MIT-only People file is prohibited unless a
dedicated legal review, source-file provenance record, and intentional license
strategy are completed first. No such strategy is currently accepted.

### Simitone

- Source: <https://github.com/riperiperi/Simitone>
- Relationship: a separate frontend whose repository includes a pinned FreeSO
  submodule and requires a legitimate commercial-game installation.
- License finding: the inspected top-level repository listing does not present
  a license file. The embedded FreeSO code remains MPL-2.0. Do not infer an MIT
  license from third-party summaries.
- Permitted use here: observe project-level concerns around neighborhood/UI
  integration and the difficulty of character/object presentation.

Treat Simitone code as not reusable until exact file-level licensing is
verified. People does not need that verification because no code is copied.

## Commercial-game references

Published manuals and observable gameplay may inform generic mechanics such as
needs, households, build/buy/live modes, relationships, careers, schedules,
object quality, and time management. They do not authorize copying expressive
content. Product trademarks belong to their owners and are used only when
identifying historical references.

Hard boundary:

```text
NO original game data
NO copied proprietary assets
NO original-file compatibility requirement
NO inherited branding
```

## Assets added later

Every admitted asset must be listed in a future machine-readable asset manifest
with creator/tool, source, creation date, prompt or source-model identifier,
processing steps, result files, license, and review status. Unknown,
noncommercial-only, attribution-free-without-proof, and otherwise ambiguous
assets do not ship.
