# Next People development context

Last updated: 2026-08-25

## Immediate task

No task is `DOING`. Start `PEO-089`, the resident movement gate: direct the
resident across the furnished room in every one of the four camera views and
confirm that simulation coordinates never rotate with the presentation. The
walk clip, routing, and movement executor it depends on are all `DONE`, so the
gate is an exercise-and-record task rather than new subsystem work.

`--smoke-walk` already routes the demo resident on the first drawn frame and
traces the selected sprite; combining it with `--smoke-test` rotation cycling
is the cheapest way to gather the four-view evidence.

After that gate, the next real subsystem is the action foundation:
`PEO-079` explicit action states, `PEO-080` per-resident action queue, and
`PEO-082` the native interaction contract. Do not start `PEO-090` energy or
`PEO-091` bed sleep before `PEO-058`/`PEO-059` reservations and cleanup exist.

## Current verified state

- Branch: `develop`.
- `PEO-072` walk animation is committed and pushed; it is the completed task
  recorded by this handoff.
- HEADLESS: configure/build and 15/15 CTests passed.
- SDL_RENDERER/SDL3: configure/build and 15/15 CTests passed under Xvfb X11;
  `SDL_VIDEODRIVER=offscreen` also passed 15/15 without Xvfb.
- Observed movement: the real runtime selects `walk.<direction>.0` below 500
  travelled units, `walk.<direction>.1` from 500 to 999, restarts the cycle at
  1000, and returns to the idle clip on arrival. Confirmed in both the headless
  and the displayed binary, plus a mid-route Xvfb screenshot.
- CNA: branch `next`, HEAD `126ef4e7ce62f08dae1e19db210c31dcbe3fcf99`, working
  tree clean at the final rebuild.
- sharp-runtime: branch `next`, HEAD
  `768a8034a0c5942c27395b636293b369e7dd7d12`, working tree clean.
- No People/CNA/sharp-runtime blocker is open.

Recheck both dependency HEADs and worktrees before any build or commit. CNA
advanced twice during the previous session while another agent worked in it.
Never roll either checkout backward.

## This checkout is not beside its dependencies

`people-cna` currently lives in `.../openeggbert/_other/`, while `cnanext` and
`sharp-runtimenext` live one level up in `.../openeggbert/`. The documented
sibling layout therefore does not resolve here and both roots must be passed
explicitly:

```bash
-DPEOPLE_CNA_ROOT=/rv/data/development/github.com/openeggbert/cnanext
-DPEOPLE_SHARP_RUNTIME_ROOT=/rv/data/development/github.com/openeggbert/sharp-runtimenext
```

That override was broken until this session: `CMakeLists.txt` computed the
defaults with `get_filename_component` before the `set(... CACHE PATH ...)`
calls, so a normal variable shadowed the cache entry and any `-D` value was
discarded. Configure failed with the exact message that recommends the flag.
The defaults are now computed only when the variable is not already set.

## Desktop commands that passed

```bash
cmake -S . -B build-headless -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=HEADLESS \
  -DCNA_PLATFORM=HEADLESS \
  -DCNA_AUDIO_PLATFORM=NULL \
  -DPEOPLE_CNA_ROOT=/rv/data/development/github.com/openeggbert/cnanext \
  -DPEOPLE_SHARP_RUNTIME_ROOT=/rv/data/development/github.com/openeggbert/sharp-runtimenext \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build-headless -j12
ctest --test-dir build-headless --output-on-failure

cmake -S . -B build -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCNA_GRAPHICS_RENDERER=SDL_RENDERER \
  -DCNA_PLATFORM=SDL3 \
  -DCNA_AUDIO_PLATFORM=NULL \
  -DPEOPLE_CNA_ROOT=/rv/data/development/github.com/openeggbert/cnanext \
  -DPEOPLE_SHARP_RUNTIME_ROOT=/rv/data/development/github.com/openeggbert/sharp-runtimenext \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build -j12
xvfb-run -a -s '-screen 0 1280x720x24' \
  env SDL_VIDEODRIVER=x11 ctest --test-dir build --output-on-failure

./build-headless/People --smoke-frames 300 --smoke-walk
```

Build directories are `build/` and `build-headless/` inside the repository and
are git-ignored. Both are warm; reuse them instead of reconfiguring. ccache is
installed and both launchers are set.

## Architecture and known limitations

- Movement is renderer-independent fixed-point state: 1000 units/tile,
  125 units/tick, eight ticks/tile at 20 Hz.
- `MovementState::travelledUnits` is monotone for a whole route and survives a
  replan, so the walk cycle never snaps backward. Presentation reads it through
  the `const noexcept` `ProgressFor` accessor and can never mutate a route.
- The walk clip is deliberately the two-frame minimum of the version 1
  character progression. There is no animation graph, no blending, and no
  avatar customization.
- `--smoke-walk` is a temporary developer control, like right-click routing and
  the `F` door toggle. It hard-codes destination `12,5`, a free tile in the
  demo room, and traces one line per drawn frame.
- Right-click remains a direct movement command. Busy residents reject a second
  click until `PEO-079`/`PEO-080` establish action semantics.
- The runtime is still one lot, one floor, one resident, five procedural
  objects, with no motives, real interactions, autonomy, build/buy, or saves.
- All runtime art remains project-owned procedural placeholders recorded in
  `ASSET_PIPELINE.md`; this task added no external or generated asset.
