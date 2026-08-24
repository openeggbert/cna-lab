# Performance baseline

## 2026-08-24 — Linux EasyGL first-district capture

Environment: Linux, CNA `OPENGLES3`/EasyGL, OpenGL ES 3.2 Mesa 25.0.7, 1280x720, vertical sync
enabled, dummy SDL audio output, Release build. This is a development workstation capture, not yet
the locked minimum-hardware qualification required to close M12.

The committed profiler writes p95/average/maximum JSON for end-to-end frame cadence and CPU time
in update, physics, AI, audio control, rendering, district loading, and startup. It also reads the
Linux process high-water resident set and reports peak actor/body counts. VRAM coverage is
explicitly partial because CNA/EasyGL does not expose complete backend residency.

### Results

| Measurement | Intro/idle (300 frames) | Mixed walk/drive/load (900 frames, rerun) | Minimum budget |
| --- | ---: | ---: | ---: |
| Frame interval p95 | 16.876 ms | **57.705 ms (fail)** | 33.333 ms |
| Update CPU p95 | 0.120 ms | 0.405 ms | 8.0 ms |
| Physics CPU p95 | 0.095 ms | 0.164 ms | 3.0 ms |
| AI CPU p95 | 0.011 ms | 0.004 ms | 2.0 ms |
| Audio-control CPU p95 | 0.001 ms | 0.014 ms | 1.0 ms |
| Render-submission CPU p95 | 0.645 ms | 0.507 ms | 8.0 ms |
| District-load CPU p95 | 5.717 ms | 5.892 ms | 1000 ms |
| Peak resident RAM | 218.4 MiB | 220.5 MiB | 2 GiB |
| Tracked (partial) VRAM | 377.1 KiB | 244.7 KiB | 512 MiB |

A longer 1800-frame mixed capture reproduced the failure at 51.628 ms frame p95, with similarly
small CPU costs (render 0.532 ms, physics 0.168 ms, district load 6.269 ms). An earlier Debug mixed
capture passed the minimum at 28.925 ms p95. The Release failure is therefore reproducible but its
root cause is not established.

Host frame pacing is also unstable independently of the scenario. Two otherwise identical final
180-frame Release intro captures run back-to-back produced 51.381 ms and 16.897 ms p95, while
render-submission p95 stayed at 0.708 ms and 0.691 ms respectively. This rules out treating one
intro pass as stable qualification and weakens any claim that the mixed failure is caused by
movement alone.

The large gap between end-to-end frame interval and render-submission CPU points to GPU/present
back-pressure, compositor/v-sync behavior, or a camera/overdraw problem. This is an inference from
the measurements, not yet a diagnosis. M12 remains open until:

- the mixed workload passes 33.333 ms p95 on the named minimum target hardware;
- repeated captures establish stable frame pacing and diagnose or exclude the host compositor;
- intro, walking, and driving phases are separately measured to test the camera/overdraw theory;
- complete VRAM residency is measured rather than inferred from Iron Gang-owned allocations.

Reproduction commands and the exact budget semantics are in
[`performance-targets.md`](performance-targets.md#automated-capture-budgets).

## 2026-08-24 — isolated Xvfb diagnostic follow-up

The profiler now supports separate `intro`, `idle`, `walk`, `drive`, and `mixed` scenarios,
`--vsync on|off`, requested timing metadata, and a dedicated `present_cpu` measurement around
CNA's virtual `Game::EndDraw()` path. This directly measures buffer swap/v-sync/backend flush time
instead of inferring all of it from the gap between frame cadence and Draw submission.

Per the workspace automation constraint, the follow-up ran on an isolated Xvfb display with
`WAYLAND_DISPLAY` removed and `SDL_VIDEODRIVER=x11` forced. `glxinfo -B` identified the renderer as
unaccelerated Mesa llvmpipe. A full 540-frame mixed run completed its walking, driving, and district
transition phases in both requested presentation modes:

| Measurement | Xvfb v-sync requested off | Xvfb v-sync requested on |
| --- | ---: | ---: |
| Frame interval p95 | 17.087 ms | 16.898 ms |
| Render-submission CPU p95 | 1.289 ms | 1.389 ms |
| Present CPU p95 | 11.604 ms | 13.220 ms |
| Update CPU p95 | 0.292 ms | 0.255 ms |
| Physics CPU p95 | 0.233 ms | 0.196 ms |
| District-load CPU p95 | 5.165 ms | 4.932 ms |
| District-load samples | 2 | 2 |

The paired results are effectively the same at end-to-end frame level. This is expected because
Xvfb has no real vertical-retrace signal; `vertical_sync_requested` does not claim the driver
accepted it. llvmpipe also performs substantial deferred work in Present, so this is not comparable
to the earlier hardware-backed capture. The result validates the new diagnostic path and full mixed
automation without opening a visible window, but it does not qualify M12.

## 2026-08-24 — imported-resource accounting follow-up

The partial VRAM report now traverses every loaded CNJ model through CNA's public model API,
deduplicates shared vertex/index buffers and effect-bound textures by object identity, and computes
logical texture storage across complete mip chains and compressed-format block rounding. It also
reports game-owned, imported-buffer, and imported-texture categories separately.

A 12-frame Release EasyGL integration check, run only on the isolated Xvfb/X11 display described
above, reported:

| Tracked logical allocation | Bytes |
| --- | ---: |
| Iron Gang-owned meshes, lightmaps, and HUD atlas | 386,168 |
| Imported CNJ vertex/index buffers | 8,160 |
| Imported effect-bound textures | 12 |
| **Tracked total** | **394,340** |

The 12 texture bytes are three distinct 1x1 RGBA GPU objects produced from the character model's
single source image and held by its imported effects; shared object instances would be counted
once. This short run validates accounting rather than frame pacing. `tracking_complete` remains
false because backend effect programs,
swapchain/depth/render-target/transient allocations, driver padding, and physical residency remain
unobservable. The earlier table's tracked totals predate imported-resource accounting and should
not be compared as a VRAM-growth measurement.

## 2026-08-24 — non-blocking EasyGL GPU timer follow-up

The profiler now places a real CNA renderer timer query around each frame's Draw command range,
from before Clear through the HUD and explicitly excluding Present. Results are collected only when
available in a later frame; no query result is waited on and a pending query is never overwritten.
The JSON reports support/unsupported reason and the count of discarded samples alongside the new
`gpu_render` measurement.

A 120-frame idle run and a full 540-frame mixed walking/driving/district-transition run used the
Release EasyGL build on the isolated Xvfb/X11 path with v-sync requested off:

| Measurement | Idle, 120 frames | Mixed, 540 frames |
| --- | ---: | ---: |
| Frame interval p95 | 17.091 ms | 16.918 ms |
| Render-submission CPU p95 | 1.914 ms | 1.387 ms |
| Present CPU p95 | 13.604 ms | 12.189 ms |
| GPU Draw-range p95 | 9.150 ms | 7.786 ms |
| GPU Draw-range average / maximum | 6.207 / 15.615 ms | 4.341 / 13.250 ms |
| GPU samples | 118 valid, 1 discarded | 538 valid, 1 discarded |

The mixed run also retained the expected small non-rendering costs: update/physics p95 were
0.267/0.208 ms and two district-load samples had a 5.036 ms p95.

The discarded result was EasyGL/metagl's 32-bit all-ones timer saturation value
(`4,294,967,295 ns`), first observed as an impossible 4,294.967 ms result while the complete host
frame was at most tens of milliseconds. `GpuFrameTimer` now recognizes that explicit sentinel,
keeps a discard count as evidence, and excludes it from statistics. llvmpipe implements the timer
extension, but remains unaccelerated software GL; these values validate collection and make future
hardware captures materially more diagnostic, but do not replace named-hardware qualification.
The historic 51-58 ms hardware-backed runs predate this metric and must be rerun before their
Draw/GPU/Present split is known.

## 2026-08-24 — scoped 3D workload counters

JSON schema 2 adds per-frame counts for the exact 3D work Iron Gang submits through its procedural
meshes and CNA `Model` parts. Effect passes and vertex/index/blend binding calls are counted at the
front-end seam; the report does not invent driver state transitions after backend deduplication.
HUD `SpriteBatch` internal batching is likewise excluded. With no frustum/occlusion culling in the
prototype, every submitted scene object is labelled visible and the policy is embedded in JSON.

The full 540-frame Release EasyGL mixed run on isolated Xvfb/X11, v-sync requested off, reported:

| Per-frame 3D workload | Average | p95 | Maximum |
| --- | ---: | ---: | ---: |
| Draw calls | 13.730 | 18 | 18 |
| State-change calls | 43.052 | 56 | 56 |
| Declared vertices | 1,548.711 | 1,768 | 1,768 |
| Triangles | 826.044 | 948 | 948 |
| Geometry instances | 13.293 | 16 | 16 |
| Submitted visible objects | 59.891 | 67 | 67 |

All 540 frames contributed a sample. The non-integral averages include the district-transition
loading frames, whose 3D counts correctly reset to zero; p95/max describe the populated district
workload. These measurements are a diagnostic baseline, not content-budget pass/fail checks.

## 2026-08-24 — swap-interval acknowledgement follow-up

JSON schema 3 no longer leaves `--vsync` as requested-only metadata. Once the EasyGL context is
current, profiling repeats the same interval through CNA's public platform GL-context seam and
records whether the platform reports it applied. This acknowledgement is stronger than echoing a
preference, but deliberately does not claim a physical vblank/compositor result.

Two 60-frame Release EasyGL idle runs used only isolated Xvfb/X11:

| Measurement | Requested interval 0 | Requested interval 1 |
| --- | ---: | ---: |
| Platform apply succeeded | no | no |
| Reported applied interval | unknown (`null`) | unknown (`null`) |
| Frame interval p95 | 17.049 ms | 16.950 ms |
| Present CPU p95 | 13.301 ms | 12.169 ms |

This explains the earlier paired Xvfb result more precisely: the virtual platform rejected both
settings, so similar timing is not evidence that “v-sync on” and “off” perform the same. Future
hardware comparisons must first require `swap_interval.apply_succeeded=true`; only then can timing
be attributed to an accepted request, and even then external compositor behavior remains separate.

## 2026-08-24 — district-load phase and memory-delta follow-up

JSON schema 4 replaces the district-load profiler's opaque aggregate with per-transition evidence.
It separately records world/static-physics unload+activation CPU and renderer static-geometry/
lightmap rebuild-upload CPU, then derives the existing budgeted total from those two values. Every
sample also identifies source, target, reason, target procedural-object/static-body counts, current
RSS delta, and logical tracked renderer-video-memory delta. Startup remains under `startup_cpu` and
is no longer incorrectly included as a district-load sample.

That last correction explains the older tables' roughly 5-6 ms district p95 and two samples in one
mixed run: one sample was actually broad initialization work, not a district transition. Those
historical values remain useful only as evidence of the old profiler behavior and are superseded
for district-load qualification by schema 4.

A 540-frame Release EasyGL `mixed` run, executed only on isolated Xvfb/X11 with v-sync requested
off, captured exactly one real WarehouseBlock -> Countryside transition:

| Transition evidence | Value |
| --- | ---: |
| World/static-physics unload + activation CPU | 0.045 ms |
| Renderer rebuild/upload-submission CPU | 0.219 ms |
| **Derived total** | **0.264 ms** |
| Target procedural world objects | 25 |
| Target static physics bodies (including ground) | 5 |
| Current resident-memory delta | 0 B |
| Tracked logical renderer-video-memory delta | -135,576 B |

The target is generated in memory and reads zero district files, so I/O, decompression, and parse
durations are reported as `null`/not applicable rather than measured zero. Xvfb remains unsuitable
for graphics-hardware qualification, but this CPU/resource-lifecycle path is the real integrated
transition and passes the 1000 ms district-load budget by a wide margin.

## 2026-08-24 — physics workload follow-up

JSON schema 5 adds opt-in physics state and operation counters at the Iron Gang/Jolt boundary
without changing simulation behavior. State counters retain total/active rigid bodies, live
rigid-body contact manifolds, and actual CharacterVirtual contacts. Operation counters are consumed
once per game update and separately report fixed steps, public gameplay raycasts, character
collision updates, and vehicle wheel raycasts. The existing `physics_cpu` timer remains the
budgeted duration; unlike query types are not collapsed into a synthetic total.

A 540-frame Release EasyGL `mixed` run, executed only on isolated Xvfb/X11 with v-sync requested
off, produced 542 update samples:

| Physics workload per update | Average | p95 | Maximum |
| --- | ---: | ---: | ---: |
| Rigid bodies | 8.652 | 9 | 9 |
| Active rigid bodies | 0.906 | 1 | 1 |
| Rigid-body contact manifolds | 0.000 | 0 | 0 |
| Character contacts | 1.041 | 2 | 2 |
| Fixed steps | 0.930 | 1 | 1 |
| Public gameplay raycasts | 0.000 | 0 | 0 |
| Character collision updates | 0.930 | 1 | 1 |
| Vehicle wheel raycasts | 3.720 | 4 | 4 |

Physics CPU averaged 0.136 ms, with 0.206 ms p95 and 2.605 ms maximum. Zero rigid-body manifolds is
expected for this route: the player uses CharacterVirtual contacts and the sedan uses a raycast
vehicle. Zero public raycasts is also real—the current integrated route makes none outside those
separately counted character/wheel paths. Xvfb still cannot qualify graphics hardware, but these
CPU-side Jolt counters exercise the real mixed gameplay flow and establish the workload baseline.

## 2026-08-24 — ambient-AI workload follow-up

JSON schema 6 scopes `ai_cpu` to the existing traffic, pedestrian, witness, and police update path
(mission progression is now outside that timer) and adds current state plus exact per-update loop
counts. A 540-frame Release EasyGL `mixed` run, executed only on isolated Xvfb/X11 with v-sync
requested off, produced 543 update samples:

| Ambient-AI workload per update | Average | p95 | Maximum |
| --- | ---: | ---: | ---: |
| Traffic vehicles | 1.904 | 2 | 2 |
| Pedestrians | 1.904 | 2 | 2 |
| Fleeing pedestrians | 0.000 | 0 | 0 |
| Police patrols | 0.000 | 0 | 0 |
| Traffic updates | 1.768 | 2 | 2 |
| Traffic obstacle checks | 3.094 | 4 | 4 |
| Pedestrian updates | 1.768 | 2 | 2 |
| Pedestrian threat checks | 1.326 | 2 | 2 |
| Police witness checks | 2.652 | 4 | 4 |
| Police patrol updates | 0.000 | 0 | 0 |

AI CPU averaged 0.004 ms, with 0.007 ms p95 and 0.021 ms maximum. The zero flee/patrol state is
real for this deterministic mixed route: it drives without entering a pedestrian's threat radius
or triggering the simplified offense thresholds. Deterministic police coverage separately proves
nonzero witness work and exactly two patrol updates on an escalation tick. The state-vs-operation
average difference is also intentional: district-transition frames retain the currently spawned
actors while their AI operations are suspended, followed by a Countryside arrival with no ambient
actors. No road/path-request metrics are reported because only fixed WaypointPaths exist today.

## 2026-08-24 — game-owned audio workload follow-up

JSON schema 7 retains the budgeted game-owned `audio_cpu` timer and adds only the audio state and
commands observable through CNA. A 540-frame Release EasyGL `mixed` run, executed only on isolated
Xvfb/X11 with dummy SDL audio and v-sync requested off, produced 544 update samples:

| Audio workload per update | Average | p95 | Maximum |
| --- | ---: | ---: | ---: |
| Loaded sound assets | 3.000 | 3 | 3 |
| Retained loop instances | 1.000 | 1 | 1 |
| Retained loops playing | 0.779 | 1 | 1 |
| Streamed game assets | 0.000 | 0 | 0 |
| One-shot play requests | 0.007 | 0 | 1 |
| Successful one-shot starts | 0.007 | 0 | 1 |
| Loop play commands | 0.002 | 0 | 1 |
| Loop stop commands | 0.000 | 0 | 0 |
| Loop parameter updates | 1.423 | 2 | 2 |

Audio-control CPU averaged 0.010 ms, with 0.019 ms p95 and 0.095 ms maximum. The four one-shot
requests in the capture were footsteps and all four succeeded. The one retained engine-loop
instance started once and played through the driving phase; it received volume and pitch updates
on each active driving update. No stop command is expected because the deterministic route remains
in the vehicle until the district transition suspends audio control.

Zero streamed assets is exact for Iron Gang's current content: it owns three `SoundEffect`s and no
streaming audio object. It is not a claim about backend decoder activity. CNA exposes neither the
lifetime of fire-and-forget one-shot voices nor decoder/mixer callback, active backend channel, or
bus costs, so schema 7 labels those unavailable instead of reporting false zeroes. Xvfb/dummy audio
validates the integrated command and report path, not audible quality or physical audio hardware.

## 2026-08-24 — frame-pacing and hitch follow-up

JSON schema 8 derives a stable histogram and explicit hitch counts from the existing wall-clock
frame intervals. A 540-frame Release EasyGL `mixed` run, executed only on isolated Xvfb/X11 with
dummy SDL audio and v-sync requested off, produced 539 intervals:

| Frame-pacing bucket | Count | Share |
| --- | ---: | ---: |
| <=16.667 ms (recommended budget) | 46 | 8.534% |
| >16.667 and <=33.333 ms (minimum budget) | 492 | 91.280% |
| >33.333 and <=50 ms | 0 | 0.000% |
| >50 and <=100 ms (hitch) | 1 | 0.186% |
| >100 ms (severe hitch) | 0 | 0.000% |

Frame interval averaged 16.871 ms, with 16.988 ms p95 and 55.936 ms maximum. The one interval over
the 33.333 ms minimum budget was also the capture's sole >50 ms hitch; no severe hitch occurred.
The one real WarehouseBlock -> Countryside transition had a measured first-following interval of
17.345 ms, so the 55.936 ms hitch was not at that transition boundary.

This single llvmpipe/Xvfb run validates classification, report plumbing, and boundary association;
it does not explain the hitch or qualify physical frame pacing. Xvfb has no real vblank and remains
unsuitable for closing M12. Controlled repeated captures on named minimum hardware are still
required, using the same strict thresholds and a platform-acknowledged swap interval.

## 2026-08-24 — release-summary generator follow-up

`scripts/performance_report.py` now validates schema-8 input and produces a Markdown release
summary from one or more captures. It recomputes the locked frame/CPU/RAM/VRAM/load decisions from
raw measurements, verifies that pacing histogram counts match frame samples, and has three
deliberately distinct overall states: `DIAGNOSTIC` without an explicit physical-hardware assertion,
`FAIL` for a declared qualification with blockers, and `PASS` only for complete evidence.

Running it against the isolated 539-interval capture above with hardware labelled `Xvfb llvmpipe
diagnostic` correctly produced `DIAGNOSTIC`, not a pass. Its blockers were the absent
`--qualifying-hardware` assertion, only one mixed capture instead of two distinct runs, a known
virtual/software display, rejected swap-interval application, and incomplete backend VRAM
tracking. The summary still preserves the useful 16.988 ms p95, one hitch, 17.345 ms transition
boundary, RAM, and logical VRAM evidence in its per-capture table.

A synthetic two-capture test proves the strict physical path can reach `PASS` only with Release
OPENGLES3, acknowledged presentation, complete in-budget VRAM, and all direct minimum budgets
passing. The current integration binds two independent archive bundles; separate tests prove a
copied capture under a second path, missing archive, mutated raw artifact, stale schema,
inconsistent histograms, rejected swap application, and incomplete VRAM cannot be promoted.
Repeatability uses canonical JSON object contents rather than paths or formatting. The CLI cases
are wired into CTest as `iron_gang_performance_report_tests`; generating a report successfully never
by itself closes M12.

## 2026-08-24 — automated capture-comparison follow-up

`scripts/performance_compare.py` now turns two compatible schema-8 captures into a deterministic
Markdown regression table. It compares 15 available p95/pacing/memory values and returns exit 1
when any candidate increase exceeds both its metric-specific absolute tolerance and the configured
relative tolerance. Workload counts are included as non-failing context. Exit 2 is reserved for
invalid or incomparable evidence rather than mislabelling environment changes as regressions.

Compatibility is intentionally strict: hardware identity, diagnostic/qualifying kind, scenario,
resolution, backend/build, timing and swap state, GPU timer scope/support, budget/hitch definitions,
RAM observability, VRAM completeness/coverage, and optional metric availability must match. The
qualifying path additionally rejects virtual/software displays, missing platform presentation
acknowledgement, unknown RAM, and incomplete VRAM.

The latest real schema-8 Xvfb/llvmpipe capture was compared with itself as a diagnostic integration
check. All 15 values matched exactly and the generated report returned `NO REGRESSION`; this proves
real-report parsing, compatibility validation, metric selection, and Markdown/exit-code plumbing.
It is deliberately not a comparison over two points in time and does not qualify the virtual
hardware. Unit coverage separately proves regression exit 1, configurable tolerance acceptance,
hardware/kind refusal, budget and sample-availability refusal, and incomplete qualifying evidence.

## 2026-08-24 — representative mission-capture follow-up

The new schema-8 `mission` scenario preserves and programmatically advances the real opening
dialogue, lets the real cutscene complete, walks the Jolt-backed character to the sedan, enters via
the production interaction/animation state, drives the Jolt-backed sedan, and finishes only when
the data-driven mission observes the warehouse trigger. It exits at that exact success boundary.
Report generation refuses a too-short `--smoke` run, so a mission-labelled JSON cannot silently
represent a partial playthrough.

A 120-frame isolated Xvfb negative run ended before completion, returned the actionable error, and
wrote no JSON. The complete Release EasyGL run used a 900-frame upper bound but ended naturally at
647 updates and 642 measured frame intervals:

| Metric | Mission capture |
| --- | ---: |
| Frame interval p95 / maximum | 16.936 / 69.503 ms |
| Update / physics / AI / audio CPU p95 | 0.293 / 0.227 / 0.009 / 0.019 ms |
| Render / GPU Draw-range / Present p95 | 1.502 / 8.642 / 13.721 ms |
| Minimum-budget misses / hitches / severe hitches | 1 / 1 / 0 |
| District transitions | 0 |
| Peak resident RAM | 167.9 MiB |
| Peak ambient workload | 9 physics bodies, 2 traffic vehicles, 2 pedestrians, 0 police |

The capture parses through both the release-summary generator (`DIAGNOSTIC`) and a comparator
self-check (`NO REGRESSION`). It supports the mission portion of `IG-35-012`; it does not supply
the still-nonexistent interior workload or qualify Xvfb/llvmpipe, presentation, or complete VRAM.

## 2026-08-24 — bounded lifecycle memory-soak follow-up

The new no-window `iron_gang_memory_soak_tests` repeatedly combines mission reset/replay, real
mid-mission save/read/resume/completion, and full WarehouseBlock/Countryside round trips in one
process. It verifies the WarehouseBlock returns to exactly 8 static Jolt bodies after every pair of
transitions and samples Linux current/high-water RSS after a fixed warm-up.

| Lifecycle soak | CI-sized run | Extended local run |
| --- | ---: | ---: |
| Cycles | 200 | 5,000 |
| District transitions | 400 | 10,000 |
| Mission replay + save/load cycles | 200 | 5,000 |
| Warm-up cycles | 20 | 20 |
| RSS checkpoints | 10 | 10 |
| Current RSS baseline -> final | 7,962,624 -> 7,966,720 B | 8,032,256 -> 8,036,352 B |
| Current RSS delta | +4,096 B | +4,096 B |
| Peak RSS delta | +4,096 B | +4,096 B |
| Linear current-RSS trend | 34 B/cycle | 0 B/cycle (rounded) |
| Final WarehouseBlock bodies | 8 | 8 |

Both runs pass the bounded allowances (8 MiB current growth, 16 MiB high-water growth, 32 KiB per
cycle trend). These numbers are process-local diagnostics and can vary by allocator/build; the
thresholded result and absence of continued growth matter more than the absolute 7.6-7.7 MiB RSS.
This lifecycle test excludes rendering, audio hardware, backend allocations, and physical GPU
residency, so it does not replace the still-required integrated target-hardware M12 qualification.

## 2026-08-24 — external complete-VRAM evidence contract

An audit of the public CNA/EasyGL seam found no complete per-process GPU-residency counter. The
available OpenGL extension families expose adapter-global capacity/free memory at best; `apitrace`
tracks API resources rather than physical residency, and no usable vendor per-process profiler is
installed in this workspace. Treating any of those as the missing complete measurement would make
the M12 result misleading.

`scripts/vram_evidence.py` now supplies the smallest honest external seam. It binds one original
schema-8 capture, one evidence manifest, and one raw vendor/OS profiler artifact by SHA-256, checks
the exact `complete_process_gpu_residency_peak` scope and UTC/process metadata, and writes a new
enriched capture atomically. It never overwrites an input. The complete tracked total is
`max(Iron Gang logical bytes, external peak residency)`, so an unexpectedly smaller external value
cannot weaken the existing lower bound.

The report and comparator independently validate the embedded evidence structure. Reports require
an exact hardware-label match; comparisons additionally require matching profiler name/version,
source, and measurement scope. The same CLI can re-verify an archive by reconstructing the expected
enriched JSON from its original capture, manifest, and raw artifact and comparing the complete
object. Six focused CLI tests cover the qualifying synthetic/verification path, semantic tamper
refusal, capture hash refusal, scope/time refusal, raw-artifact mutation, conservative flooring,
duplicate-key ambiguity, and every input-overwrite path. This is contract/plumbing coverage only:
no qualifying physical artifact was invented, and the real Xvfb captures correctly remain
`tracking_complete=false`. M12 still needs repeated mixed captures plus authoritative
complete-residency artifacts on named physical minimum hardware.

The manifest process is also constrained to an `iron_gang`/`iron_gang.exe` basename, positive PID,
and strictly positive start/end interval. The binder and downstream report use the same validator,
so changing those fields after binding cannot turn unrelated or instantaneous evidence into a pass.

The generated schema-8 profile now carries its own `capture_session` PID and microsecond UTC
interval. Complete evidence must use that PID and enclose that full interval, closing the previous
gap where an operator-provided manifest could not be correlated to the profiled process run. A new
60-frame Release EasyGL `idle` integration ran only on isolated Xvfb and emitted PID `1059289` with
`2026-08-24T12:31:41.991744Z` through `2026-08-24T12:31:43.189643Z`. The report parsed it as
`DIAGNOSTIC`; Xvfb, rejected swap acknowledgement, and incomplete VRAM remain unchanged blockers.

## 2026-08-24 — qualification archive enforcement

The release-summary path no longer accepts an enriched complete-VRAM JSON as sufficient evidence
by itself. Every capture in a declared physical qualification must have an ordered
`--vram-bundle ORIGINAL EVIDENCE ARTIFACT`; the report invokes `vram_evidence.py
--verify-enriched` and checks that the enriched input does not change across verification and
parsing. Missing sources, mismatched counts, altered raw artifacts, stale manifests, cross-bound
captures, and semantically edited enriched outputs exit 2 before any gate result is emitted.

The synthetic integration now creates and verifies two independent four-file bundles. Focused
coverage also proves that omitting the bundles or mutating a raw artifact after binding is refused.
Ordinary diagnostic reports intentionally remain readable without archive sources. No physical
profiler artifact is available in this workspace, so this closes an evidence-integrity gap rather
than M12 itself.

The report output path is now part of the same preservation contract. It cannot equal or hardlink
to an enriched capture or any bundle source, and successful Markdown output is staged beside its
destination before an atomic replace. A focused regression test proves direct-capture, capture-
hardlink, and raw-artifact collisions leave their bytes unchanged and that a normal nested output
leaves no temporary file behind.

The Markdown release artifact now preserves the identity it evaluated rather than only capture
basenames. Its provenance table includes each evaluated JSON SHA-256 and capture-session PID/UTC
interval plus all three verified bundle names and hashes when present. All captures and source
files are re-hashed after parsing and again immediately before output. Running the updated tool on
the locally retained real Xvfb session produced capture hash
`df217f17b3cf32c3c279fbf582a3075a6bb61f759f9ec3d5d2b695be3da41cd0`, PID `1059289`, and the
previously recorded `12:31:41.991744Z`–`12:31:43.189643Z` interval; absent bundle columns are
explicitly `—` because that diagnostic has no external evidence.

The schema-8 presentation contract is now checked rather than inferred from any non-null integer.
`requested` is restricted to 0/1 and must agree with `vertical_sync_requested`; a successful
`applied` must equal it exactly, while failed/unknown results require null state consistently. A
hand-edited request-1/applied-0 capture and a v-sync/request contradiction both exit 2. The shared
loader gives the comparator and VRAM binder the same protection. Both locally retained Xvfb
diagnostics still parse normally and retain their rejected-swap blocker.

Frame-pacing summaries are now resistant to independent derived-field edits. The parser verifies
all 16.667/33.333/50/100 ms bucket bounds, re-derives minimum-budget misses, hitches, severe hitches,
and their percentages, and correlates transition-boundary counts with `district_load_cpu` samples
and maximum/hitch state. Tests reject a changed hitch count, changed hitch threshold, and transition
count disconnected from load samples. Both locally retained Xvfb diagnostics pass the stricter
parser with their existing one-hitch results unchanged.

The external-VRAM source roles are now physically distinct as well as hash-bound. The binder and
archive verifier reject any original/manifest/raw-artifact pair referring to the same file or
hardlinked inode, and require the raw profiler artifact to be a non-empty regular file. Output
hardlinks to an input are refused too. Existing CLI coverage now exercises an empty artifact, a
raw-artifact hardlink to the original profile, and an output hardlink while confirming no source
bytes change. This is a sanity boundary, not certification of the external tool's semantics.

UTC correlation now accepts one canonical, precision-safe representation only:
`YYYY-MM-DDTHH:MM:SS[.ffffff]Z`. Python's broader ISO parser otherwise accepts date-only/space
forms and truncates fractions beyond microseconds; the latter could move an actual evidence
boundary across a capture boundary without changing the parsed value. Tests reject all three
cases. Report 7/7, VRAM 6/6, comparator 6/6, and both retained real diagnostics pass.

Human-readable identities can no longer alter the Markdown structure. Release/comparison hardware
labels and titles are normalized to non-empty printable single lines; VRAM hardware identity and
tool name/version obey the same rule before binding. Tests reject blank report hardware, a newline
title, a multiline external hardware identity, and multiline comparator hardware. This also makes
the exact report-to-manifest hardware match canonical rather than whitespace-dependent.

Regression comparison now preserves and protects its own input identity too. Its Markdown records
baseline/candidate SHA-256, rechecks both files after parsing and immediately before output, refuses
direct or hardlink output aliases, and atomically replaces valid output. The locally retained Xvfb
self-comparison still reports `NO REGRESSION` and now shows
`df217f17b3cf32c3c279fbf582a3075a6bb61f759f9ec3d5d2b695be3da41cd0` for both roles. The seventh
comparator CLI test proves direct/hardlink preservation and clean nested output.
