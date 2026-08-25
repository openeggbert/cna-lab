# Linux release packaging

## Scope

This document covers the Linux + EasyGL distributable archive: how to build it, what it
verifies, and what it deliberately does not prove. It is the `release-easygl` slice of gate
M14 (`plan/plan_39-vertical-slice-gates.md` `IG-39-015`), not the whole gate — see
"Boundaries" below.

## Prerequisites

- A working `release-easygl` configure/build, per `README.md`'s "Required workspace layout"
  (modular CNA checkout with `sharp-runtime`/`easy-gl` siblings, Jolt Physics at `~/deps/jolt`).
- `cmake`, `ninja`, `ccache`, `python3`, `readelf`, and `ldd` on `PATH`. `readelf`/`ldd` come
  from `binutils`/`glibc` and are already required by every mainstream Linux distribution.
- Enough disk headroom for `cmake-build-release-easygl/` (a persistent, reused build
  directory — never point this at `/tmp` or a per-session scratch directory).

## Building the archive

```bash
./scripts/build-release.sh
```

This runs, in order:

1. `scripts/preflight.sh release-easygl` — confirms the dependency layout above.
2. `scripts/asset_registry.py --check-notice THIRD_PARTY_ASSETS.md` — refuses a stale
   generated asset notice before anything is built.
3. `cmake --preset release-easygl` / `cmake --build --preset release-easygl` — an ordinary
   incremental configure/build of the persistent `cmake-build-release-easygl/` directory.
4. `scripts/release_archive.py` — stages, validates, archives, and smoke-tests the package
   (see "What the archive validator checks" below).

The build's ccache defaults to `cmake-build-release-easygl/ccache` (override with
`IRON_GANG_CCACHE_DIR`) so it works without a globally writable cache directory.

Running `release_archive.py` directly (for example, to re-archive an already-built tree
without rebuilding) accepts `--project-root`, `--build-dir`, `--output-dir`, and
`--source-date-epoch`:

```bash
./scripts/release_archive.py \
  --project-root . \
  --build-dir cmake-build-release-easygl
```

## Output

- `cmake-build-release-easygl/dist/iron-gang-<version>-linux-<arch>.tar.gz`
- `cmake-build-release-easygl/dist/iron-gang-<version>-linux-<arch>.tar.gz.sha256`

The archive is built reproducibly: entries are sorted by path, and ownership/timestamps are
normalized to `SOURCE_DATE_EPOCH` (the last commit's timestamp by default, or the
`SOURCE_DATE_EPOCH` environment variable, or `--source-date-epoch`). Re-running the archive
step against an unchanged install produces a byte-identical file and checksum.

## What the archive validator checks

`release_archive.py` fails closed (non-zero exit, no archive written) if any of the following
do not hold:

- **Package identity.** The CMake cache must show `CMAKE_BUILD_TYPE=Release`,
  `CNA_GRAPHICS_RENDERER=OPENGLES3`, `CNA_ENABLE_VIDEO=OFF`, and a `Linux` target system —
  this script only supports the Linux EasyGL Release identity.
- **Runtime-only layout.** The game executable, all four repository notices (`README.md`,
  `LICENSE`, `THIRD_PARTY.md`, `THIRD_PARTY_ASSETS.md`), this document, ten dependency license
  files, every shipped runtime asset (audio/config/cutscene/dialogue/mission data and the
  generated CNJ models), and the private `libSDL3.so.0`/`libSDL3_mixer.so.0` runtime libraries
  must all be present. Development-only content — headers, the Jolt static archive/CMake
  package, MC3/glTF authoring sources, and generated GLB — must be absent.
- **Linked-library policy.** `readelf -d` on the installed executable must show the two SDL
  `NEEDED` entries and the relative `$ORIGIN/../lib/iron-gang` RUNPATH, must not show a
  leftover absolute build-workspace RUNPATH, and must not show any of the four direct FFmpeg
  libraries (`libavcodec`/`libavformat`/`libavutil`/`libswresample`) that CNA's `AUTO` video
  default would otherwise pull in — Iron Gang does not use CNA's video playback.
- **Resolved linkage.** `ldd` must resolve both SDL libraries from inside the package (not a
  build-tree or system copy) and must not report any unresolved library.
- **A real smoke run.** With `DISPLAY`/`WAYLAND_DISPLAY` unset, `SDL_VIDEODRIVER=offscreen`,
  and `SDL_AUDIODRIVER=dummy`, the installed executable is launched with `--smoke 5
  --vsync off` from inside the package directory, and its output must show every shipped CNJ
  model and all three WAV files loading successfully.

This full validate/verify pass runs twice per build: once against the freshly installed
package, and once again against the package re-extracted from the archive it just produced —
proving the archive itself, not just the pre-archive staging directory, is correct.

## Failure modes

| Failure | Likely cause |
|---|---|
| `CMake cache is missing <KEY>` / wrong `CMAKE_BUILD_TYPE`/`CNA_GRAPHICS_RENDERER`/`CNA_ENABLE_VIDEO` | Ran against a non-`release-easygl` build directory, or a stale cache from before this policy existed — reconfigure `release-easygl`. |
| `missing or empty <label>` | `cmake --install` did not run, or a required source file (a dependency license, this document) is missing from the source tree. |
| `development-only content leaked into runtime package` | A CMake install-rule regression started shipping headers, the Jolt SDK, or authoring sources. |
| `installed executable does not require <library>` / `has an unresolved shared library` | CNA's SDL prebuilt layout changed, or the private-library install rule did not run. |
| `unused video dependency leaked into installed executable` | `CNA_ENABLE_VIDEO` reverted to `AUTO`/`ON` for this configure. |
| `installed executable retains a build-workspace RUNPATH` | The `INSTALL_RPATH`/`BUILD_RPATH` CMake target properties were changed or removed. |
| `installed smoke run did not load the packaged runtime assets` | A runtime asset was dropped from the install rules, or the smoke run crashed before finishing — check the captured stdout/stderr in the raised error. |
| `unsafe or unexpected archive member` / `unsafe archive link target` (only reachable via `extract_verified_archive`, exercised by `tests/test_release_archive.py`) | A malicious or corrupt archive attempted a path escape; this is a safety check, not a normal build failure. |

Any of these leaves no archive (or a `.tmp` file that is cleaned up) — a partial or
unverified package is never left at the final archive path.

## Testing

```bash
python3 tests/test_release_archive.py scripts/release_archive.py
```

Four focused cases run against synthetic fixture packages (no real build required): layout
policy (valid / missing license / development-content leak), deterministic archive creation
plus a symlink round trip, path-traversal rejection on extraction, and locked package-identity
policy. CMake also registers this as the `iron_gang_release_archive_tests` CTest case.

## Boundaries

This slice proves the Linux EasyGL archive/install/smoke path on the machine that built it. It
does **not** prove:

- an external clean checkout on a separate machine, following only the documented steps
  (`IG-39-015`, the full M14 gate);
- a full interactive playthrough of the packaged build (the smoke run is five headless frames,
  not a played mission);
- Windows packaging — the CMake install rules for the Windows SDL DLLs exist but are untested;
- signed or published releases (`IG-37-013` signed checksums, `IG-37-020` release publication);
- any physical-display graphics/audio verification — see `docs/performance-baseline.md` for
  that separate, still-open boundary (gate M12).
