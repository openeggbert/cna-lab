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

## 2026-08-24 — qualifying comparison archive enforcement

Qualifying regression comparisons now require both archived VRAM source bundles, not only their
two enriched JSON outputs. `--baseline-vram-bundle` and `--candidate-vram-bundle` each identify the
original profile, evidence manifest, and raw profiler artifact. The comparator invokes the same
full reconstruction verifier used by release summaries, stability-checks all eight inputs, protects
them from output aliases, and includes the six source hashes in the Markdown provenance table.

The synthetic integration binds two independent archives and reaches `NO REGRESSION`; after the
candidate raw artifact is changed, the same comparison exits 2 on its hash mismatch. Diagnostic
comparisons remain usable without archived sources. This prevents an enriched-only historical
comparison claim, but supplies neither a second physical measurement nor the missing M12 gate pass.

## 2026-08-24 — Markdown evidence rendering hardening

Release and comparison Markdown now share safe renderers for every dynamic value. Plain text has
HTML and inline Markdown syntax escaped; file names and identities use HTML-escaped `<code>` values,
table pipes are encoded, and non-printable filename characters are rendered as visible Unicode
escape text. A filename containing a backtick, `<b>`, a pipe, and a newline can no longer close its
code span, add a table column, inject markup, or split a provenance row.

The existing output-preservation tests exercise adversarial titles, hardware, and capture names in
both generators while retaining their 7/7 suite counts. Report 7/7, comparator 7/7, and VRAM 6/6
focused suites pass. The stored Xvfb report/self-comparison still yields `DIAGNOSTIC`/
`NO REGRESSION` with `df217f17…41cd0`; no graphical process ran and M12 remains open.

## 2026-08-24 — qualification repeatability-policy enforcement

The release report previously evaluated mixed captures individually and counted distinct contents,
but did not require the two runs to use the same resolution, presentation/timestep request, GPU
timer policy, representative workload, or external VRAM method. Two individually passing but
incompatible runs could therefore satisfy the repeatability count. The qualifying path now compares
those policy fields across every mixed capture and emits `FAIL` with the exact differing field.

The capture's own `budgets` object is now validated against every locked schema-8 threshold as well;
editing both captures to the same alternative policy exits 2 rather than evading a compatibility
check. The synthetic same-policy pair still reaches `PASS`. Focused tests prove 1280-vs-1920 and
profiler-version mismatch failures plus a changed 33.333 ms metadata refusal. Report 7/7,
comparator 7/7, VRAM 6/6, both retained real diagnostics, and the diagnostic self-comparison pass
without launching the game. No physical complete-VRAM capture exists, so M12 remains open.

## 2026-08-24 — cross-run archive independence

The three roles inside one VRAM bundle were already required to be distinct files/inodes, but a
second qualifying capture could still reuse a source from the first bundle. Qualifying release
reports and qualifying baseline/candidate comparisons now compare every source path/inode across
bundles and reject reuse or a hardlink alias with exit 2 before verification/evaluation.

The report integration constructs two otherwise valid independently enriched captures bound to one
raw profiler source. Comparator coverage replaces the candidate raw source with a hardlink to the
baseline artifact. Both are refused; the ordinary two-artifact synthetic flow still passes. Report
7/7, comparator 7/7, and VRAM 6/6 focused suites pass. Diagnostic self-comparison remains allowed,
and no graphical process or physical qualification was involved.

## 2026-08-24 — measurement-summary consistency

The shared schema-8 loader now validates all timing summaries, including measurements not directly
shown in the release table. Zero samples require zero average/p95/maximum; a one-sample block must
have all three values equal; average and p95 can never exceed maximum. Frame cadence receives a
stronger cross-check: histogram counts determine which threshold bucket contains the nearest-rank
`ceil(0.95 * samples)` observation, and the serialized p95 must be inside that bucket within
three-decimal rounding tolerance.

Tests reject a render p95 above maximum, a nonzero zero-sample GPU block, and a frame p95 moved from
its derived 16.667-33.333 ms bucket to 10 ms. Report 7/7, comparator 7/7, and VRAM 6/6 focused suites
pass. Both retained Xvfb captures pass the new invariant checks and the diagnostic self-comparison
retains `df217f17…41cd0`. Raw samples are still intentionally not archived, so this proves internal
consistency rather than recomputing exact p95 from first principles.

## 2026-08-24 — strict non-finite JSON refusal

Python's standard JSON decoder accepts JavaScript-style `NaN`, `Infinity`, and `-Infinity` even
though JSON does not. The shared capture/manifest loader now rejects all three via its parse hook
before inspecting the schema, so hiding one in an unknown extension field cannot produce a
non-portable accepted archive. Existing duplicate-key refusal is unchanged.

Report coverage places `NaN` in an unused capture field, comparator coverage uses `Infinity`, and
the VRAM manifest test uses `-Infinity`; all exit 2. Focused suites remain report 7/7, comparator
7/7, and VRAM 6/6. This is parser integrity only and does not affect the physical M12 blocker.

## 2026-08-24 — representable JSON numeric range

Strict syntax did not by itself make every accepted JSON number safe for the C++ evidence schema.
Python decodes standard `1e400` syntax as infinity, and its arbitrary-precision integers can exceed
every type used by the producer before later float arithmetic raises `OverflowError`. The common
parse hooks now require finite floating-point results and integers within signed-negative or
unsigned-positive 64-bit bounds, including for unknown extension fields.

Report cases place `1e400` and a 101-digit integer in an unused field; both exit 2 without a
traceback. Report 7/7, comparator 7/7, VRAM 6/6, and both retained diagnostics pass without
launching the game. Full isolated CTest passes 8/8 with its smoke process inside Xvfb; no physical
M12 evidence changes.

## 2026-08-24 — representative qualification sample floor

Release qualification now binds its sample window to the documented `mixed --smoke 900` command.
The first drawn frame establishes only the interval baseline, so each mixed capture needs at least
899 frame-interval samples, at least 899 samples for every budgeted CPU metric, and at least 899 for
every render/physics/AI/audio workload summary. Shorter captures remain structurally readable and
useful as diagnostics, but cannot contribute to `PASS`.

A qualifying case reduces frame cadence, render CPU, and one audio-workload summary to four samples
and gets `FAIL`; the synthetic full-window pair still passes. Report 7/7, comparator 7/7, and VRAM
6/6 pass. The retained 539-interval Xvfb mixed capture remains `DIAGNOSTIC` with the expected new
short-window blocker, while the retained mission diagnostic is unchanged. Full isolated CTest
passes 8/8 with its smoke process inside Xvfb; no physical evidence was added.

## 2026-08-24 — qualifying comparison sample floor

The comparator's `qualifying` kind now uses the same representative-sample evaluator as release
reporting. A mixed baseline or candidate with fewer than 899 frame, budgeted-CPU, or workload
samples is rejected before metrics are compared, so archive completeness cannot turn a short p95
into a qualifying `NO REGRESSION` claim.

The VRAM integration rebinds an otherwise valid independent candidate after reducing it to four
frame intervals and proves comparison exit 2, then restores the full candidate and continues its
archive-hardlink/chronology checks. Report 7/7, comparator 7/7, and VRAM 6/6 pass. Diagnostic mode
remains intentionally flexible: the retained 539-interval Xvfb self-comparison still returns
`NO REGRESSION`. Full isolated CTest passes 8/8 with its smoke process inside Xvfb; no physical
evidence was added.

## 2026-08-24 — canonical evidence token encoding

The shared loader and VRAM binder no longer trim machine-readable evidence before validating it.
UTC timestamps must match their raw JSON text exactly; external source/scope and artifact file names
must be unpadded printable single lines; and SHA-256 tokens must be exactly 64 lowercase hexadecimal
characters. This removes semantically equivalent but byte-distinct spellings from the archive
contract while preserving trim normalization for human hardware/tool labels.

Report cases reject a padded capture timestamp and embedded artifact digest. VRAM cases independently
reject padded manifest scope, profile digest, artifact name, and timestamp. Report 7/7, comparator
7/7, VRAM 6/6, both retained diagnostics, and the retained diagnostic self-comparison pass without
launching the game. Full isolated CTest passes 8/8 with its smoke process inside Xvfb; no physical
evidence was added.

## 2026-08-24 — full representative-window Xvfb integration

A fresh Release OPENGLES3 run executed the real `mixed --smoke 900 --vsync off` route entirely in
isolated Xvfb/X11. It produced 899 frame intervals, 900 render samples, 903 samples for each
update/physics/AI/audio timing and workload group, and one real WarehouseBlock -> Countryside
transition. This is the first retained real-flow capture to meet the new representative sample
floor rather than merely remaining structurally readable below it.

Frame average/p95/maximum were 16.879/17.127/64.649 ms. The one >50 ms hitch was not the transition
boundary; that boundary was 17.361 ms, and there were no >100 ms severe hitches. Update, physics,
AI, audio, and render CPU p95 were 0.304/0.242/0.007/0.023/1.440 ms. GPU Draw/Present p95 were
8.435/13.711 ms. The measured district phases were 0.044 ms world/physics plus 0.236 ms renderer
submission, totaling 0.280 ms. Peak RAM was 168.5 MiB and logical tracked VRAM about 0.2 MiB.

The retained file is `/tmp/iron-gang-m12-xvfb-mixed-900-20260824.json`, SHA-256
`63a62c8bc8a6601ed35f7eacbd369bb857d47f9e5c94ff0e5230c00176a3230c`. The release report parses it
without a short-window blocker and remains `DIAGNOSTIC` for the honest residual reasons: virtual
llvmpipe hardware, one mixed run, declined requested swap interval, and incomplete physical VRAM.
No visible/real-screen process was launched, and this does not close M12.

## 2026-08-24 — full-window Xvfb repeatability pair

A second independent Release EasyGL/Xvfb `mixed --smoke 900` run produced another 899 intervals,
900 render samples, complete workload coverage, and one real transition. Its PID/time interval is
separate from the first run, and its SHA-256 is
`a80043f052df0c7efc7adca6edd0980964c735b6ae9b377d345f3435cf73149e`.
The resulting two-run report no longer lists the minimum-two-mixed-captures blocker; its retained
Markdown hash is `8a4dd4afc3823783e7222967d7142c79d23597d14632a8e9e53460fcd4be0a36`.

The pair also demonstrates material virtual-environment variance. Frame p95 increased from 17.127
to 24.656 ms; render CPU from 1.440 to 1.861 ms; GPU Draw from 8.435 to 16.090 ms; and Present from
13.711 to 21.988 ms. The second run had a 113.286 ms non-transition severe hitch, while its
17.309 ms transition boundary and 0.347 ms district load remained fast. CPU, 30 FPS, RAM, and
district budgets still passed. The diagnostic comparator correctly returns exit 1/`REGRESSION`;
its artifact hash is `73323a1f79117ff871392780a77b1e1678dbb11d61c90a18763c6a05c24002cc`.

Both files remain `DIAGNOSTIC`: Xvfb/llvmpipe is virtual, both requested swap intervals were
declined, and neither capture has complete physical VRAM evidence. The variability strengthens the
existing requirement for two controlled physical runs rather than weakening it. No real-screen
process was launched.

## 2026-08-24 — capture-session independence

The synthetic release `PASS` previously used two metric-distinct JSON objects but left both at PID
123 with exactly the same 10:00:05-10:00:55 UTC capture interval. Canonical-content distinction
alone therefore overstated that fixture's run independence. The positive second capture now uses
PID 124 and a non-overlapping 11:00:05-11:00:55 interval, with matching separately bound external
evidence.

The qualifying report compares every mixed-session pair and emits `FAIL` on positive overlap;
qualifying regression comparison rejects an overlapping baseline/candidate pair with exit 2.
Release coverage proves the former same-session shape fails, and the VRAM integration rebinds an
overlapping candidate to reach the comparator check. Diagnostic self-comparison remains valid.
Report 7/7, comparator 7/7, and VRAM 6/6 focused suites pass; no physical run was created.

## 2026-08-24 — qualifying comparison chronology

Qualifying baseline/candidate comparison now requires the candidate session to start at or after
the baseline session ends. The existing overlap refusal remains a distinct error; a fully valid,
non-overlapping candidate captured earlier than the baseline is also invalid because its regression
direction would be mislabeled.

The VRAM integration rebinds the candidate to PID 4244 at 09:00:05-09:00:55 while retaining a
10:00:05 baseline and separate source archive, then proves exit 2. Its normal PID-4243 11:00:05
candidate still reaches `NO REGRESSION`. Focused suites remain report 7/7, comparator 7/7, and VRAM
6/6. Diagnostic self-comparison remains intentionally exempt.

## 2026-08-24 — memory-summary consistency

The shared schema-8 loader now reconstructs the producer's memory relationships. Nonzero peak RSS
must agree with `memory.known`; RAM and tracked-VRAM `budget_pass` flags must equal decisions from
the locked 2 GiB and 512 MiB limits. The three logical VRAM categories must sum to raw
`tracked_bytes`, or to enriched `logical_tracked_bytes` once an external residency maximum replaces
the tracked total. Coverage remains a required printable single line.

Report negatives exercise false known/budget flags and mismatched logical category totals; the VRAM
binder validates both the raw input and its enriched output. Report 7/7, comparator 7/7, VRAM 6/6,
both retained real Xvfb captures, and full 8/8 CTest pass without launching the game. This rejects
internally contradictory evidence but supplies no physical M12 measurement.

## 2026-08-24 — derived-check consistency

The shared loader now correlates schema-8 `checks.minimum_frame_rate_pass`,
`recommended_frame_rate_pass`, `cpu_subsystems_pass`, and `district_load_pass` with measurement
sample availability and stored p95 values. District load must be `null` only with zero samples;
ordinary contradictory booleans are malformed evidence rather than ignored metadata.

At a stored p95 exactly equal to a three-decimal budget, either boolean remains allowed: C++ makes
the decision from its full-precision statistic before serializing the rounded p95, so the hidden
value cannot be reconstructed honestly. Report coverage proves four contradictions fail and a
rounded CPU-boundary case remains readable. Report 7/7, comparator 7/7, VRAM 6/6, and both retained
Xvfb captures pass without launching the game. Full 8/8 CTest also passes with its smoke process
isolated inside Xvfb; this adds no physical M12 measurement.

## 2026-08-24 — rounded-budget qualification consistency

Structural readability and release qualification now preserve the same information boundary. A
producer `false` at an exactly serialized frame, CPU, or district-load budget remains structurally
valid because hidden precision explains it, but it blocks qualification instead of being replaced
with a pass recomputed from the rounded p95. The release table's 30/60 FPS decisions also come from
the producer checks, with resolution still independently required for the recommended target.

Exact-boundary report cases cover frame 33.333 ms, update CPU 8.000 ms, and district load 1000.000
ms producer failures. Report 7/7, comparator 7/7, VRAM 6/6, and both retained Xvfb diagnostics pass
without launching the game. Full isolated CTest passes 8/8 with its smoke process inside Xvfb; this
adds no physical M12 measurement.

## 2026-08-24 — GPU timing metadata consistency

The shared loader now requires schema-8 GPU timing to remain explicitly non-blocking with exact
`draw_commands_excluding_present` scope and a non-negative integer discard count. A supported timer
must have an empty `unsupported_reason`; an unsupported timer must carry a printable non-empty one.
Four report negatives cover each contradiction class.

The loader does not derive support from `gpu_render.samples`: the generic C++ report writer permits
manually accumulated GPU measurements with an unsupported final context, a shape used by its unit
test. Report 7/7, comparator 7/7, VRAM 6/6, and both retained Xvfb diagnostics pass without launching
the game. Full 8/8 CTest also passes with its smoke process isolated inside Xvfb. This is
evidence-schema hardening, not a physical capture.

## 2026-08-24 — district-load detail consistency

The shared loader now treats `district_load.samples` as evidence for all three load measurement
rows. Detail count, phase sum, average, nearest-rank p95, and maximum must agree for world/physics,
renderer upload, and their total. A 0.001001 ms tolerance covers only independent three-decimal
serialization; a positive test exercises the maximum legitimate 0.001 ms difference.

Procedural scope, null package phases, and zero district-file count are fixed. Resident known state
and both signed resident/logical-VRAM deltas are re-derived from exact before/after bytes. Six report
negatives cover sum, count, statistics, known state, and both deltas. Report 7/7, comparator 7/7,
VRAM 6/6, and both retained zero-transition Xvfb diagnostics pass without launching the game. This
is followed by full 8/8 CTest with its smoke process isolated inside Xvfb. No physical transition
capture was added.

## 2026-08-24 — VRAM completeness-state field consistency

The shared schema-8 loader now rejects binder-only `logical_tracked_bytes` and `complete_evidence`
when `video_memory.tracking_complete=false`. Raw captures use `tracked_bytes` as the logical
category sum; enriched captures require both complete-state fields and their existing semantic
validation.

Two report negatives retain one stale field at a time and exit 2. The older incomplete diagnostic
and comparator fixtures were normalized by removing both enrichment fields rather than only
flipping the boolean. Report 7/7, comparator 7/7, VRAM 6/6, and both retained raw diagnostics pass.
Full isolated CTest passes 8/8 inside Xvfb; no capture was added.

## 2026-08-24 — workload-summary integrity

Schema-8 loading now requires all producer-defined render, physics, AI, and audio workload metrics
plus their fixed scope metadata. Each count summary rejects nonzero statistics without samples,
unequal one-sample statistics, average/p95 above maximum, and fractional p95/maximum values. A
capture can no longer drop workload context while retaining only top-level peaks.

Six report negatives cover missing section, scope mutation, zero/one corruption, invalid maximum,
and fractional count. Cross-metric sample equality and top-level peak equality are deliberately not
claimed: the generic writer exposes individual render/physics record calls and an independent final
context. Report 7/7, comparator 7/7, VRAM 6/6, and both retained Xvfb diagnostics pass without
launching the game. Full 8/8 CTest also passes with its smoke process isolated inside Xvfb. This
adds no physical workload capture.

## 2026-08-24 — base capture metadata completeness

The shared schema-8 loader now requires `startup_cpu`, the final previously optional C++ timing row,
with the same measurement-summary invariants as every other metric. Backend, build configuration,
and scenario must be printable non-empty lines; resolution dimensions and target-frame duration
must be positive and timing flags boolean.

The allowed identity values are deliberately not closed: the generic writer's C++ unit report uses
`TEST`/`Debug`/`unit_test`, while qualification still separately requires Release OPENGLES3 mixed
runs. Six report negatives cover the missing row, multiline/blank identity fields, and zero geometry
or target duration. Report 7/7, comparator 7/7, VRAM 6/6, and both retained diagnostics pass without
launching the game. Full 8/8 CTest also passes with its smoke process isolated inside Xvfb. This is
schema completeness, not new M12 evidence.

## 2026-08-24 — swap evidence-boundary consistency

The shared loader now fixes `swap_interval.proof` to platform `SetSwapInterval` acknowledgement that
explicitly is not physical vblank/compositor proof. A successful apply requires an empty
`unavailable_reason`; a failed or unknown apply requires a printable non-empty one. The synthetic
failure fixture now uses the same platform-declined reason as the application.

Three report negatives cover proof mutation, a reason attached to success, and a missing failure
reason. Report 7/7, comparator 7/7, VRAM 6/6, and both retained declined-Xvfb diagnostics pass
without launching the game. Full 8/8 CTest also passes with its smoke process isolated inside Xvfb.
No physical presentation evidence was created.

## 2026-08-24 — frame-pacing scope consistency

The shared schema-8 loader now fixes both frame-pacing semantic labels to the producer contract:
wall-clock intervals between consecutive `BeginFrame` calls (with the first frame establishing only
a baseline), and the first interval recorded after `RecordDistrictLoad` for a district boundary.
This prevents otherwise internally consistent counts from being relabelled as another timing scope.

Two report negatives mutate the ordinary and boundary scope independently. Report 7/7, comparator
7/7, VRAM 6/6, and both retained Xvfb diagnostics pass without launching the game. The complete
isolated Xvfb CTest passes 8/8 with its smoke process confined to Xvfb; no physical capture was
created.

## 2026-08-24 — VRAM coverage-scope consistency

Schema-8 loading now treats `video_memory.coverage` as an evidence boundary. A raw capture must use
the C++ producer's exact logical-resource list and explicit backend/residency omissions. An enriched
capture must use the bind tool's exact complete-process-residency statement and conservative
logical/external maximum rule. A repeated hand-edited scope can no longer pass merely because two
captures agree with each other.

Two report negatives independently replace the complete and logical scopes with broader claims.
Report 7/7, comparator 7/7, VRAM 6/6, and both retained Xvfb diagnostics pass without launching the
game. Full isolated Xvfb CTest passes 8/8 with its smoke process confined to Xvfb; no physical VRAM
capture was created.

## 2026-08-24 — process executable identity consistency

The shared schema-8/VRAM validator now requires the complete process executable path or name to be
one printable line before reducing it to the allowed `iron_gang`/`iron_gang.exe` basename. A
control-character prefix followed by an otherwise valid basename can no longer cross the capture or
external-evidence trust boundary.

Report coverage mutates `capture_session.process.executable`; VRAM coverage independently mutates
the external profiler manifest. Both exit 2 with source-specific diagnostics. Report 7/7,
comparator 7/7, VRAM 6/6, both retained diagnostics, and full isolated CTest 8/8 pass. Its Xvfb
smoke ran separately while the explicitly requested visible instance remained open; no evidence
was added.

## 2026-08-24 — root schema-version type consistency

The shared capture loader now requires `schema_version` to be a non-boolean JSON integer equal to
8. Python's numeric equality previously let a floating-point `8.0` pass the direct comparison even
though the versioned producer and external evidence schemas use integer tokens.

A report negative proves `8.0` exits 2 with the received value identified. Report 7/7, comparator
7/7, VRAM 6/6, both retained diagnostics, and full isolated CTest 8/8 pass. Its smoke process ran
inside Xvfb; no capture was added.

## 2026-08-24 — frame-maximum histogram consistency

The shared schema-8 loader now correlates `measurements.frame_interval.maximum_ms` with the highest
non-empty `frame_pacing.histogram` bucket, complementing the existing nearest-rank p95 check. The
same 0.0005 ms serialization tolerance handles a full-precision sample that rounds exactly onto a
three-decimal threshold.

A report negative claims a 60 ms maximum while all stored samples end in the <=33.333 ms bucket.
The comparator fixture itself exposed and corrected an older contradiction (five >16.667 ms
samples paired with a 16 ms maximum). Report 7/7, comparator 7/7, VRAM 6/6, and both retained
diagnostics pass. Full isolated CTest passes 8/8 with its smoke process inside Xvfb; no capture was
added.

## 2026-08-24 — district-boundary frame subset consistency

The shared schema-8 loader now enforces that district-transition boundary samples remain a subset
of all frame intervals. Boundary hitch count cannot exceed global hitch count, and boundary
`maximum_ms` cannot exceed `measurements.frame_interval.maximum_ms`.

An exactly serialized 50.000 ms boundary deliberately accepts either hitch state because C++ tests
the full-precision interval before serialization. Two report negatives cover the impossible count
and maximum; one positive case preserves the rounded threshold ambiguity. Fixing the base fixture's
older 17 ms boundary versus 16.8 ms frame maximum also made its histogram/statistics coherent.
Report 7/7, comparator 7/7, VRAM 6/6, and both retained diagnostics pass. Full isolated CTest passes
8/8 with its smoke process inside Xvfb; no capture was added.

## 2026-08-24 — district-boundary histogram occupancy

Boundary maximum validation now requires at least one non-empty global frame-pacing bucket whose
bounds contain the serialized value. This completes the subset check: a boundary interval cannot
exist in a duration range where the capture says no frame interval was observed.

A report negative uses a 40 ms boundary with zero samples in the 33.333–50 ms bucket while a
separate 60 ms hitch keeps the global maximum valid. It exits 2. The existing 50.000 ms rounded
positive case remains valid through either adjacent populated bucket. Report 7/7, comparator 7/7,
VRAM 6/6, and both retained diagnostics pass. Full isolated CTest passes 8/8 inside Xvfb; no
capture was added.

## 2026-08-24 — integrated Linux DRM residency capture path

`scripts/drm_vram_capture.py` now supplies the missing Linux OS-profiler path around a physical
EasyGL run. It polls `/proc/<pid>/fdinfo` for the exact child PID throughout the schema-8 capture,
uses `drm-pdev` plus `drm-client-id` (or a global client ID) to deduplicate repeated descriptors,
and sums all standard resident buffer-object regions. For older amdgpu output it accepts the
kernel-documented `drm-memory-<region>` alias, prefers the standard key when both exist, and rejects
different alias/standard values.

A short no-window surfaceless EGL probe on the host AMD Radeon 780M observed three fds for one DRM
client, each reporting 2,076 KiB VRAM, 4,096 KiB GTT, and 0 KiB CPU residency. Correct
deduplication therefore yields 6,172 KiB for that snapshot, not three times that number. This was a
format/semantics probe, not an Iron Gang performance capture.

The wrapper creates the raw JSON artifact and evidence manifest automatically only after the child
exits successfully, the profile exists, its PID matches the child, and the sampler interval
encloses the profile interval. `vram_evidence.py` recognizes this built-in artifact and
reconstructs source fields, region maxima for duplicate descriptors, client/sample totals, and the
overall peak before binding. Focused VRAM coverage passes 9/9, including a sample-total mutation
whose manifest hash was recomputed; full isolated CTest passes 8/8. A short Xvfb/SOFTWARE wrapper
integration then wrote only the game's ordinary incomplete schema-8 profile and exited 2 with no
DRM samples, leaving both raw-artifact and manifest paths absent. No real physical game window or
qualifying artifact was created; M12 remains open pending two controlled hardware runs with
acknowledged presentation.

## 2026-08-24 — first real Iron Gang DRM artifact (offscreen diagnostic)

SDL's offscreen video driver can create the Release EasyGL OPENGLES3 context on this host's AMD
Radeon 780M with both visible-display environment variables removed. A 30-draw `idle` run through
the new wrapper therefore exercised the actual game, Mesa/amdgpu, schema-8 profiler, DRM sampler,
manifest generator, binder, and release reporter end to end without opening a window.

The sampler made 139 attempts at 5 ms intervals and retained 100 complete process snapshots; two
fd read races were counted and their partial snapshots excluded. Fds 5, 6, and 7 consistently
identified the same `0000:c3:00.0/1815` client and were counted once. Peak process DRM residency was
51,986,432 B (49.58 MiB): 49,020,928 B GTT, 2,965,504 B VRAM, and 0 B CPU. The binder reconstructed
every raw source field and derived total, then verified the enriched capture. Exact hashes:

- original profile: `c7feeede61827de9e8ad9d24c758d91e348c0457b7af0fd701fc3f4c41289716`;
- raw artifact: `978cf4843ea33e68113a5d153d945afd77495017347a32e7b34bf886ceaf0930`;
- evidence manifest: `124b4c843b5a7616945a6338e41e5646927d6f8f3da7c69b82cf32df5fb4bd55`;
- enriched profile: `0fcc17419e1d8e4694d130173a14478b3a19c1403c1ae248d01f4388b0ca8729`;
  and
- diagnostic report: `fd1cb3ba8f7d44f2ddfdbe81b5d4cc187200cec48187a91a23e1d7e2a17304c1`.

Frame p95 was 20.276 ms, GPU Draw-range p95 0.155 ms, Present CPU p95 0.052 ms, and peak RAM
172.6 MiB. Render CPU p95 was 20.024 ms because this short window includes warm-up and fails its
8 ms budget. More importantly, offscreen `SetSwapInterval(0)` acknowledgement is not physical
display/vblank proof. The report now classifies offscreen/headless/surfaceless labels as diagnostic
even when real GPU residency and platform swap acknowledgement exist. This artifact validates the
complete VRAM path but cannot close M12.

## 2026-08-24 — full-window offscreen DRM repeatability

Two independent Release EasyGL `mixed --smoke 900` runs repeated the no-window AMD path with the
locked representative window and one real WarehouseBlock -> Countryside transition each. The
source sessions are non-overlapping, every original/manifest/raw bundle is independent, and both
enriched profiles pass semantic reconstruction.

| Measurement | Offscreen mixed 1 | Offscreen mixed 2 | Minimum budget |
| --- | ---: | ---: | ---: |
| Frame interval p95 | 18.003 ms | 17.461 ms | 33.333 ms |
| Update / physics CPU p95 | 0.543 / 0.504 ms | 0.544 / 0.478 ms | 8 / 3 ms |
| AI / audio / render CPU p95 | 0.008 / 0.024 / 1.265 ms | 0.008 / 0.025 / 1.194 ms | 2 / 1 / 8 ms |
| GPU Draw / Present CPU p95 | 0.118 / 0.050 ms | 0.113 / 0.052 ms | diagnostic |
| District load / following frame | 0.599 / 17.680 ms | 0.710 / 20.306 ms | 1000 ms load |
| Hitches / severe hitches | 0 / 0 | 0 / 0 | diagnostic |
| Peak resident RAM | 177.1 MiB | 177.1 MiB | 2 GiB |
| Complete DRM residency | 55.57 MiB | 55.57 MiB | 512 MiB |

The first run has one 44.673 ms frame (a minimum-budget miss, below the strict >50 ms hitch
threshold); the second maximum is 23.425 ms. Both local report rows are `PASS`.

The identical 58,273,792 B residency peaks have different amdgpu placement at their peak samples:
54,296,576 B GTT + 3,977,216 B VRAM in the first and 50,331,648 B GTT + 7,942,144 B VRAM in the
second. This is direct evidence that checking local VRAM alone would be unstable and incomplete;
the committed policy's all-region sum stays stable.

The diagnostic pair report has only the expected unasserted/offscreen blockers. Re-running it with
qualifying intent verifies all archives, chronology, repeatability policy, presentation
acknowledgement, sample floors, workloads, budgets, and transitions, then produces `FAIL` with one
blocker only: the diagnostic offscreen label. The comparator reports a regression solely for the
district-boundary frame (+2.626 ms exceeds its 1.768 ms allowed increase); all other metrics pass.

Key hashes:

- first original/raw/evidence/complete: `ae452b03…b5c86`, `b81abdec…fb09d`,
  `f574a340…f80aa`, `e0247d27…1c7a2`;
- second original/raw/evidence/complete: `e8e5b57b…99814`, `d6014bb5…9fe6`,
  `4803085b…2eeba`, `71c5f4cc…a393f`;
- diagnostic pair report: `6ccfca4a…0498f`;
- diagnostic comparison: `2627f562…0c74`; and
- qualifying-intent audit: `4474b358…1af9`.

The memory tracker and its real-flow integration are now implemented and proven. M12 itself remains
open because an offscreen surface cannot establish the required physical display/vblank behavior.

## 2026-08-24 — machine-readable native-window classification

The schema-8 producer now records CNA's native-window system and whether its typed native handle is
usable. This closes a classification weakness in the prior policy: a label containing `offscreen`
was rejected, but the capture itself could not disprove a misleading physical-display label.
Older diagnostics remain readable; qualification now requires the additive evidence to be present
and `available:true`. The proof text explicitly excludes physical-display, vblank, and compositor
claims because an X11 or Wayland handle alone cannot establish them.

A 30-draw EasyGL `idle` validation used the AMD Radeon 780M with
`SDL_VIDEODRIVER=offscreen`, dummy audio, and both visible-display environment variables removed.
OpenGL ES 3.2 initialized and `SetSwapInterval(0)` succeeded, while the new block independently
reported `system: Headless` and `available: false`. A diagnostic report whose hardware label was
deliberately changed to `AMD Radeon 780M physical-display claim` still emitted
`CNA reports no usable native graphical window (Headless)`. Exact hashes are
`dd680607af0f2da4c43c2e153e22b8478ed3a9450c426e3299ec0df70185544c` for the capture and
`9bb7b2bfa4da9aff1019cea0b9e3411d670f76604e8e12555a10b7e4752e568d` for the report.
No visible window or qualifying physical-display evidence was created.

## 2026-08-24 — current OpenGL runtime identity

The schema-8 EasyGL producer now resolves `glGetString` through CNA's current context service and
stores `GL_VENDOR`, `GL_RENDERER`, and `GL_VERSION`. The report and comparator require this additive
identity for qualification, require repeated runs to agree, and refuse known software runtimes
without relying on the operator's hardware label. The proof string explicitly excludes physical
display claims.

A six-draw no-window run on the same AMD path reported vendor `AMD`, renderer
`AMD Radeon 780M (radeonsi, phoenix, LLVM 19.1.7, DRM 3.61, 6.12.100+deb13-amd64)`, and version
`OpenGL ES 3.2 Mesa 25.0.7-2+deb13u1`; profile hash `6bbf8127…da2ae0`. A separate six-draw isolated
Xvfb run reported `Mesa`, `llvmpipe (LLVM 19.1.7, 256 bits)`, and the same API/Mesa version. Its
profile/report hashes are `b1550832…c8559` and `3f1abf5a…629ad`. Although that report was labelled
`Discrete GPU physical-display claim`, it still emitted the machine-derived software-renderer
blocker. Both integrations avoided the visible host display and remain diagnostic.
