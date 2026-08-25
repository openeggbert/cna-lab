# Validation record

## M12 profiling baseline (2026-08-24)

The `dev-easygl` and `release-easygl` presets now both configure, compile, launch, and render on
this workspace's host display (OpenGL ES 3.2 Mesa through CNA EasyGL). Iron Gang now has a bounded
`--profile <json>` capture and a deterministic `--profile-scenario mixed` workload covering walk,
drive, ambient AI, physics, audio control, and a real district transition. Unit tests cover p95/
average/maximum calculation and report policy; the real EasyGL runs validate the integrated path.

The first Release baseline does **not** close M12. Intro/idle passes the 30 FPS minimum at 16.876 ms
frame p95, but mixed movement/load captures reproduce a 51.628-57.705 ms p95 failure even though
all measured CPU subsystem p95 values are far inside budget. RAM passes at ~220 MiB and district
load passes at ~6 ms. Two final identical 180-frame intro runs also flipped back-to-back from
51.381 ms to 16.897 ms p95 with virtually unchanged render CPU, exposing independent host/display
frame-pacing instability. VRAM reporting is deliberately marked incomplete. Full evidence and the
next diagnostic question are in `docs/performance-baseline.md`.

Follow-up instrumentation adds isolated `intro`/`idle`/`walk`/`drive`/`mixed` scenarios,
`--vsync on|off`, requested scheduler/presentation metadata, and direct `present_cpu` timing around
CNA's `Game::EndDraw()`. The final graphical integration used an isolated Xvfb `:99` with
`WAYLAND_DISPLAY` removed and `SDL_VIDEODRIVER=x11` forced, preventing a visible Wayland window.
Mesa llvmpipe completed paired 540-frame full-mixed runs at 17.087 ms p95 (v-sync requested off)
and 16.898 ms p95 (requested on), each with two district-load samples. Present p95 was 11.604 ms
and 13.220 ms respectively. Xvfb has no real vblank and is unaccelerated, so these are diagnostic
integration results rather than hardware qualification; M12 remains open.

VRAM visibility was then extended without changing CNA: `VideoMemoryAccumulator` walks loaded CNJ
meshes, deduplicates their vertex/index buffers and built-in/generic-effect textures by identity,
and computes logical texture storage including mips and block compression. Unit tests cover exact
uncompressed, DXT, cube, and 3D texture-size calculations plus JSON category output. A Release
EasyGL 12-frame idle integration run on the same isolated Xvfb/X11 path reported 394,340 tracked
bytes: 386,168 game-owned, 8,160 imported model buffers, and 12 imported model textures. This is a
stronger lower bound, not full residency; backend programs, swapchain/depth/render targets,
transients, driver padding, and physical residency remain unknown, so `tracking_complete=false`
and M12 stays open.

The next M12 follow-up added a non-blocking GPU command-range timer without enabling CNA's entire
77-source GraphicsExt module: `GpuFrameTimer` uses the same renderer timer-query contract, guards
one pending query from overwrite, and never substitutes a CPU clock. JSON tests cover support
metadata, escaped unsupported reasons, discarded-result counts, and the `gpu_render` metric. A
full 540-frame Release EasyGL mixed run on isolated Xvfb/X11 produced 538 valid samples with GPU
p95 7.786 ms, Present CPU p95 12.189 ms, and frame p95 16.918 ms; a separate 120-frame idle run
reported GPU/frame p95 9.150/17.091 ms. One 32-bit all-ones EasyGL/metagl timer sentinel in each
run was explicitly counted and discarded rather than reported as a false 4.295-second GPU frame.
Release/development EasyGL, software plus all CTest targets, strict syntax checks, Emscripten/Web,
and the full mixed runtime flow pass. This proves the reporting path; llvmpipe is still not
qualifying hardware.

`IG-35-005` then added scoped render-workload counts to JSON schema 2. Unit coverage checks exact
nearest-rank statistics and scope metadata. A 540-frame Release EasyGL mixed run, again only on
isolated Xvfb/X11, sampled every frame and reported p95/max of 18 draw calls, 56 explicit
state-change calls, 1,768 declared vertices, 948 triangles, 16 geometry instances, and 67 submitted
visible objects. Transition frames reset to zero instead of leaking the previous frame's counts.
The report explicitly excludes HUD backend batching and driver state deduplication; “visible” means
submitted because no culling path exists yet. Software/CTest, syntax, Release/development EasyGL,
Web/Emscripten, and the mixed runtime flow all pass.

The next presentation diagnostic uses CNA's public platform GL-context acknowledgement rather than
linking Iron Gang directly to SDL. JSON schema 3 distinguishes requested/applied/unknown and warns
that platform acceptance is not physical-vblank proof. Two 60-frame Release EasyGL idle runs on
isolated Xvfb requested intervals 0 and 1; the platform rejected both (`apply_succeeded=false`,
`applied=null`). Their frame p95 values, 17.049/16.950 ms, are therefore correctly treated as an
invalid v-sync comparison instead of evidence that the settings are equivalent.

The district-load follow-up adds schema-4 per-transition phase, target-object, and memory-delta
evidence. Unit tests verify exact phase aggregation, counts, negative RSS delta and positive tracked
video-memory delta output. One isolated 540-frame Release EasyGL `mixed` run captured exactly one
WarehouseBlock -> Countryside transition: 0.045 ms world/static-physics activation + 0.219 ms
renderer rebuild/upload submission = 0.264 ms total, 25 procedural world objects, 5 target static
bodies, 0 B RSS delta, and -135,576 B tracked logical renderer-memory delta. District I/O,
decompression, and parse are correctly `null`: runtime districts are generated in memory and read
no package. Software plus 3/3 CTest, strict syntax, Release/development EasyGL, Web/Emscripten, and
the isolated real flow pass. Initialization is now solely `startup_cpu`, correcting the older
district p95 that accidentally included broad startup work.

The physics-profiler follow-up adds JSON schema 5 and an opt-in Jolt-seam snapshot. A deterministic
physics test proves exact body, fixed-step, public-raycast, character-update, and four-wheel-raycast
counts; it also verifies operation counters are consumed while current body state is retained.
Actual rigid-body and CharacterVirtual contacts are covered separately. One isolated 540-frame
Release EasyGL `mixed` run sampled 542 updates, reaching 9 bodies, 1 active rigid body, 2 character
contacts, 1 fixed step, 1 character collision update, and 4 wheel rays per update at p95/max where
applicable; physics CPU was 0.206 ms p95. Software plus 3/3 CTest, strict syntax, Release/development
EasyGL, Web/Emscripten, and the isolated real flow pass. Ordinary play keeps profiling disabled and
does not take the contact-counter mutex.

The ambient-AI follow-up adds schema 6 with per-update current traffic/pedestrian/fleeing/patrol
counts and exact traffic-update/obstacle, pedestrian-update/threat, police-witness/patrol loop
counts. `ai_cpu` now excludes the adjacent mission update so its scope matches the documented
ambient-AI budget. Unit coverage proves statistics/JSON and direct police instrumentation,
including two patrol iterations on the escalation tick. An isolated 540-frame Release EasyGL
`mixed` run sampled 543 updates: AI CPU 0.007 ms p95, with p95/max 2 traffic vehicles, 2
pedestrians, 4 obstacle checks, 2 threat checks, and 4 witness checks. This route correctly had no
fleeing pedestrian or active police patrol; the deterministic police test covers their nonzero
work. No road-graph/path-request numbers are invented while only fixed WaypointPaths exist.

The audio follow-up adds schema 7 with exact game-owned loaded-asset, retained-loop state, streamed-
asset, one-shot start, loop-control, and parameter-update counts. Unit coverage proves nearest-rank
statistics and the report's explicit observability boundary. An isolated 540-frame Release EasyGL
`mixed` run with dummy SDL audio sampled 544 updates: audio-control CPU was 0.019 ms p95, all four
one-shot footstep requests succeeded, and the one retained engine loop started and reached playing
state. CNA exposes neither fire-and-forget voice lifetime nor decoder/mixer callback, active backend
channel, or bus costs, so the report marks those unavailable instead of inventing zeroes. Software
plus 3/3 CTest, strict syntax, Release/development EasyGL, Web/Emscripten, and the isolated real flow
pass; no visible host display was used.

The frame-pacing follow-up adds schema 8 with five mutually exclusive histogram buckets, strict
>33.333 ms minimum-budget misses, >50 ms hitches, >100 ms severe hitches, and first-frame-after-
district-transition association. Unit coverage proves exact-threshold inclusivity and transition
indexing. An isolated 540-frame Release EasyGL `mixed` run sampled 539 intervals at 16.988 ms p95
and found one 55.936 ms hitch (0.186%), no severe hitch, and a non-hitch 17.345 ms transition
boundary. Software plus 3/3 CTest, strict syntax, Release/development EasyGL, Web/Emscripten, and
the isolated real flow pass. The result validates the detector but does not qualify llvmpipe/Xvfb
as physical target hardware or diagnose the one hitch; no visible host display was used.

The release-summary follow-up adds `scripts/performance_report.py`, a standard-library schema-8
Markdown generator that independently evaluates raw measurements against locked targets. It
requires two mixed captures with distinct canonical contents, explicit physical hardware identity,
Release OPENGLES3, acknowledged presentation, complete VRAM, and all direct budgets before emitting `PASS`; absent
qualification remains `DIAGNOSTIC`, while a declared qualification with verified archives but
measured blockers is `FAIL`. Missing or unverifiable archive sources are invalid input (exit 2), as
recorded by the later archive-enforcement follow-up. A new fourth CTest covers diagnostic Xvfb, a
synthetic two-capture pass, duplicate-copy refusal, swap/VRAM failures, stale schemas, and histogram
mismatch. The real isolated capture correctly remains
diagnostic with one-run, virtual-display, rejected-swap, and incomplete-VRAM blockers.

The content-budget follow-up adds versioned `assets/content-budgets.json` plus standard-library
`scripts/content_budget.py`. Exact committed baselines pass: district prototype 96 triangles/5
materials/0 textures, warehouse 12/1/0, grouped sedan 48/4/0, and test character 36/1/1. Bootstrap
triangle ceilings are 4x current geometry; material/texture-count reserves are documented and not
claimed as final production limits. `build-assets.sh` now requires a matching passing budget group
after XSD validation and before conversion. New `iron_gang_content_budget_tests` makes CTest 5/5
and proves real-source integration, actionable overflow, unsupported-MC3 rejection, malformed glTF
triangle rejection, and unregistered-source rejection. Texture resolution remains deliberately
outside this first policy until representative production textures exist; aggregate M12 VRAM
tracking remains authoritative.

The lifecycle-memory follow-up adds no-window `iron_gang_memory_soak_tests`, making CTest 6/6. Its
bounded CI case runs 200 mission-reset/save-read-resume completions and 400 district transitions;
after 20 warm-up cycles, current and peak RSS each grew only 4,096 B, current-RSS slope was 34
B/cycle, and WarehouseBlock returned to exactly 8 Jolt bodies after every round trip. A separate
5,000-cycle local run (10,000 transitions/save replay each cycle) also ended at +4,096 B current/
peak RSS with 0 B/cycle rounded trend and 8 bodies. The test enforces 8 MiB current, 16 MiB peak,
and 32 KiB/cycle trend allowances with a 60-second CTest timeout. It validates core ownership, not
render/audio/backend residency or physical-hardware M12 qualification.

The capture-comparison follow-up adds standard-library `scripts/performance_compare.py` and a
seventh CTest. Six CLI tests cover identical evidence, multiple simultaneous regressions, tolerance
overrides, hardware/kind mismatch, changed budget or GPU-sample availability, and incomplete
qualifying evidence. The latest real schema-8 Xvfb capture self-comparison reports all 15 available
metrics unchanged and exits 0. That is an integrated parser/report check only: meaningful temporal
comparison still requires a later compatible capture from the same named environment, and physical
M12 qualification still requires the separate release-summary rules.

The representative-mission follow-up adds `--profile-scenario mission`. Parser/name tests cover
the public scenario seam. A 120-frame isolated Xvfb negative integration correctly refused an
incomplete report; a 900-frame upper-bound run exited on real mission completion after 647 updates
and produced 642 frame intervals with 16.936 ms p95, one 69.503 ms hitch, no severe hitch, 167.9 MiB
peak RSS, and no accidental district transition. The schema-8 output passes both summary and
comparison tooling. This validates the full production mission route but remains a virtual-display
diagnostic, not physical M12 qualification.

The complete-VRAM follow-up adds `scripts/vram_evidence.py` and an eighth CTest. The binder requires
an original schema-8 profile, a versioned manifest, and the raw vendor/OS capture; SHA-256 binds both
artifacts, exact scope/process/time metadata is validated, inputs cannot be overwritten, and the
enriched total conservatively keeps the larger of logical bytes and external peak residency.
The CLI also reconstructs and verifies a later archived enriched capture without writing any file.
Six tests cover a synthetic report-qualified/verified flow plus semantic tampering, capture hash,
scope/time, raw-artifact, flooring, duplicate JSON keys, and overwrite failures. Report/comparison
tests validate evidence structure, hardware identity, profiler compatibility, and duplicate-key
refusal for generated capture input. No physical profiler artifact exists in this workspace, so the
real Xvfb evidence remains incomplete and this plumbing does not close M12.
The shared evidence validator additionally refuses any process basename other than
`iron_gang`/`iron_gang.exe`, a non-positive PID, and an end time that is not strictly later than the
start; binder and report tests cover the same downstream contract.

The capture-correlation follow-up adds a backward-compatible schema-8 `capture_session` object with
executable, PID-known state/value, and microsecond UTC start/end. Complete VRAM binding now requires
the external artifact manifest to name the same PID and a measurement interval enclosing the whole
game capture. Python fixtures cover PID/interval mismatch; the C++ report test covers emitted
metadata. Software, development/Release EasyGL, Web/Emscripten, strict syntax, and all 8 CTests pass.
A 60-frame Release EasyGL `idle` run on isolated Xvfb emitted a positive 1.198-second session and
remained correctly diagnostic; no host-visible window was used.

The qualification-archive follow-up makes the report itself re-run complete four-file VRAM
verification. Every declared qualifying enriched capture now requires one ordered
`--vram-bundle` containing its original profile, manifest, and raw profiler artifact. Missing
bundles/sources, count mismatches, raw-artifact mutation, semantic enriched-output changes, and an
enriched input changing during verification/parsing exit 2 before evaluation. The report and VRAM
CLI suites both remain 6/6, including a two-independent-bundle synthetic `PASS`; diagnostics remain
readable without bundles. This strengthens provenance but does not supply the still-missing
physical M12 capture.

Report-output preservation then closes the destructive alias case: `--output` is refused when it
equals or hardlinks to any capture/archive input, and valid output uses a same-directory atomic
replace. The seventh report CLI test proves the protected bytes remain identical for capture,
hardlink, and raw-artifact collisions and that a successful nested output leaves no temporary file.

Release summaries now embed exact evidence provenance: evaluated-capture SHA-256, capture PID/UTC
interval, and—when supplied—the verified original/manifest/raw-artifact names and hashes. Inputs
are re-hashed after parsing and again immediately before output. The real stored Xvfb session
renders its known PID/interval and hash
`df217f17b3cf32c3c279fbf582a3075a6bb61f759f9ec3d5d2b695be3da41cd0`; bundle fields correctly
remain absent for that diagnostic. Report 7/7 and VRAM 6/6 focused tests pass.

Presentation evidence is now structurally correlated too. The shared schema-8 loader requires a
0/1 `swap_interval.requested` matching `timing.vertical_sync_requested`; known/success/null states
must be coherent, and successful `applied` must equal the request. Report coverage rejects both
request-1/applied-0 and v-sync/request contradictions; comparator coverage independently reaches
the shared refusal. Report 7/7, comparator 6/6, VRAM 6/6, and both existing real Xvfb diagnostics
pass parsing with their honest rejected-acknowledgement blocker.

Frame-pacing derived fields are now validated against their stored histogram rather than trusted
independently. Fixed bucket bounds, minimum-miss/hitch/severe counts and percentages, comparison
operators, district transition/load sample equality, measured/hitch count bounds, and boundary
maximum/hitch consistency are enforced. Report tests reject a forged hitch count, threshold, and
transition count; both retained Xvfb captures still parse with unchanged diagnostic output.

VRAM archive roles now require three distinct source files/inodes and a non-empty regular raw
profiler artifact. The binder/verifier and report-driven verification all inherit the check.
Expanded VRAM CLI coverage rejects an empty artifact, a raw-artifact hardlink to the original
profile, and an output hardlink to an input while proving source bytes remain unchanged; 6/6 tests
pass.

Capture/evidence times now require canonical `YYYY-MM-DDTHH:MM:SS[.ffffff]Z`. Date-only input, a
space instead of `T`, and seven fractional digits are refused, avoiding Python ISO parsing that
would otherwise accept reduced forms or truncate sub-microsecond evidence boundaries. Report 7/7,
VRAM 6/6, comparator 6/6, and both locally retained real Xvfb captures pass.

Hardware/tool identity strings and report/comparison titles are now normalized to non-empty
printable single lines. Blank report hardware, newline title, multiline VRAM hardware identity,
and multiline comparator hardware are refused before Markdown generation. Report 7/7, comparator
6/6, and VRAM 6/6 pass.

The comparator now hashes and rechecks baseline/candidate, embeds both digests in Markdown, refuses
`--output` path/hardlink aliases to either input, and writes atomically. Its new seventh CLI test
proves preservation and temporary-file cleanup. The real Xvfb self-comparison remains
`NO REGRESSION` and displays the expected `df217f17…41cd0` hash for both roles.

Qualifying comparisons now also require a verified original-profile/evidence-manifest/raw-artifact
bundle for both baseline and candidate. The comparator reconstructs each enriched input with
`vram_evidence.py --verify-enriched`, stability-checks and output-protects all archive members, and
records their hashes. The synthetic VRAM integration reaches `NO REGRESSION` with two independent
archives and then proves candidate-artifact mutation exits 2. Diagnostic comparison remains
bundle-optional. Comparator 7/7 and VRAM 6/6 focused suites pass; this is evidence hardening, not a
physical M12 capture.

Report and comparator Markdown now render dynamic text through one shared safe path. HTML-like
content and inline Markdown punctuation are escaped; filenames use HTML `<code>` with encoded table
pipes, and control characters are displayed as explicit Unicode escapes. Existing atomic-output
tests now include backticks, `<b>`, `*`, a pipe, and a newline in titles/hardware/capture names and
prove provenance rows remain data. Report 7/7, comparator 7/7, and VRAM 6/6 focused suites pass;
the real Xvfb diagnostic outputs retain their prior status/hash without launching the game.

Qualifying mixed captures now form one repeatability set rather than merely passing independently.
The report requires exact agreement on resolution, fixed-timestep/v-sync/swap request, GPU timer
policy, representative workload counts, complete-VRAM coverage, and external source/tool identity.
A 1280-vs-1920 synthetic pair and profiler-version mismatch both produce `FAIL`. Every stored
budget value is also checked against the locked schema-8 constants; changing 33.333 to 40 exits 2,
including through the comparator's shared loader. Report 7/7, comparator 7/7, VRAM 6/6, and both
retained Xvfb diagnostics pass; no graphical process was launched.

Archive independence is now checked across captures as well as inside each bundle. A qualifying
report/comparison refuses any original, manifest, or raw source that shares a path or hardlinked
inode with another bundle. The release integration uses two valid enriched outputs sharing one raw
source; comparator coverage uses a cross-bundle hardlink. Both exit 2, while diagnostic
self-comparison remains available. Report 7/7, comparator 7/7, and VRAM 6/6 focused suites pass.

All schema-8 measurement summaries now enforce zero-sample zero values, one-sample equality, and
average/p95 not exceeding maximum. Frame-pacing histogram counts additionally locate the
nearest-rank p95 bucket, and `frame_interval.p95_ms` must lie within its threshold bounds. Focused
tests reject all three contradiction classes. Report 7/7, comparator 7/7, VRAM 6/6, both retained
Xvfb captures, and the known-hash self-comparison pass; no graphical process was launched.

Frame-pacing validation now also locates the highest non-empty bucket and requires
`frame_interval.maximum_ms` within its bounds. A false 60 ms maximum exits 2; correcting the
comparator's older internally inconsistent maximum keeps all focused suites and retained
diagnostics passing. Full isolated 8/8 CTest also passes with its smoke process inside Xvfb.

District-boundary pacing now preserves its subset relationship: boundary hitches cannot outnumber
global hitches and boundary maximum cannot exceed the global frame maximum. Two contradictions
exit 2; an exact serialized 50.000 ms accepts either hidden-precision hitch state. Focused suites
and both retained diagnostics pass; full isolated 8/8 CTest also passes inside Xvfb.

Boundary maximum must now match a non-empty global histogram bucket as well. A 40 ms boundary in an
empty 33.333–50 ms range exits 2, while the exact-50.000 ms rounding case remains readable via an
adjacent populated bucket. Focused suites, retained diagnostics, and full isolated 8/8 CTest pass
inside Xvfb.

The common JSON loader now rejects non-standard `NaN`, `Infinity`, and `-Infinity` constants at
parse time, including in unknown fields, alongside its existing duplicate-key refusal. Report,
comparator, and VRAM manifest tests each cover one token and exit 2. Focused suites remain 7/7,
7/7, and 6/6; no runtime process was launched.

The same parse boundary now rejects standard-form floating-point tokens that decode outside finite
range and integers beyond the C++ producer's signed-negative/unsigned-positive 64-bit domain. An
ignored `1e400` and a 101-digit integer both exit 2 without a traceback. Focused suites remain 7/7,
7/7, and 6/6, and both retained diagnostics pass; full isolated CTest passes 8/8 with its smoke
process inside Xvfb.

Qualifying mixed captures now require the documented 900-draw-frame window: at least 899 frame
intervals after the baseline-only first draw, at least 899 samples for each budgeted CPU metric, and
at least 899 samples throughout all workload summaries. A four-sample frame/render/audio-workload
case produces report `FAIL`; the full synthetic pair remains `PASS`. Focused suites pass 7/7, 7/7,
and 6/6. The retained 539-interval mixed capture remains a valid diagnostic with the expected new
blocker; the mission diagnostic is unchanged. Full isolated CTest passes 8/8 with its smoke process
inside Xvfb.

Qualifying baseline/candidate comparison now invokes that same shared sample-window check. The VRAM
integration rebinds a valid independent candidate with only four frame intervals and proves exit 2
before metric comparison, then restores the full candidate for its remaining archive checks.
Focused suites pass 7/7, 7/7, and 6/6; the retained short Xvfb diagnostic self-comparison remains
`NO REGRESSION`. Full isolated CTest passes 8/8 with its smoke process inside Xvfb.

Machine evidence strings are now canonical at their raw JSON boundary. UTC values, external
source/scope, artifact file name, and SHA-256 tokens no longer gain validity from trimming. Padded
capture time/embedded digest report cases and padded scope/profile digest/artifact name/evidence time
VRAM cases all exit 2. Focused suites pass 7/7, 7/7, and 6/6; both retained diagnostics and their
diagnostic self-comparison pass. Full isolated CTest passes 8/8 with its smoke process inside Xvfb.

A new isolated Release EasyGL/Xvfb `mixed --smoke 900` integration then exercised the full sample
floor through the real game: 899 frame intervals, 900 render samples, 903 update/physics/AI/audio
samples, complete workload summaries, and one 0.280 ms district transition. Its report hash is
`63a62c8b…a3230c`; frame p95 is 17.127 ms with one non-transition hitch and no severe hitch. It has
no short-window blocker but remains `DIAGNOSTIC` for Xvfb/llvmpipe, one run, declined swap, and
incomplete physical VRAM. No real-screen window was opened.

A second independent 900-draw Xvfb mixed run supplies another 899 intervals and removes the
two-capture-count blocker from the pair report. Its hash is `a80043f0…3149e`; pair-report hash is
`8a4dd4af…e0a36`. Frame p95 varied from 17.127 to 24.656 ms and the second run contained one
113.286 ms severe non-transition hitch; comparator exit 1 correctly reports frame/render/GPU/
Present/miss-rate regressions (`73323a1f…002cc`). Both runs still pass the minimum direct budgets
but remain diagnostic for virtual hardware, declined swap, and incomplete physical VRAM. No
real-screen window was opened.

Qualifying repeatability now requires non-overlapping `capture_session` UTC intervals. The
synthetic `PASS` fixture was corrected from two metric-distinct objects sharing PID/time to PID 123
at 10:00 and PID 124 at 11:00 with separately bound evidence. The former overlapping form produces
report `FAIL`; a separately rebound overlapping comparator candidate exits 2. Diagnostic
self-comparison remains supported. Report 7/7, comparator 7/7, and VRAM 6/6 focused suites pass.

Qualifying comparison additionally enforces direction: candidate UTC start must be at or after
baseline UTC end. The VRAM integration binds a valid separate 09:00 candidate against the 10:00
baseline and proves exit 2, while the ordinary 11:00 candidate remains `NO REGRESSION`. Overlap
retains its own refusal and diagnostic self-comparison remains supported. Focused suites pass 7/7,
7/7, and 6/6.

Schema-8 memory summaries are also re-derived. Peak RSS must agree with `memory.known`; RAM/VRAM
budget flags must agree with the locked limits; and the three logical VRAM categories must sum to
raw `tracked_bytes` or enriched `logical_tracked_bytes`. Four report negatives reject contradictions,
and enrichment validates its result. Report 7/7, comparator 7/7, VRAM 6/6, both retained Xvfb
diagnostics, and full 8/8 CTest pass; no graphical process ran.

VRAM `coverage` is now fixed according to completeness instead of merely being printable. Raw
captures retain the exact logical-resource inclusions and backend/residency omissions; externally
enriched captures retain the exact complete-process-residency and conservative-maximum statement.
Two broader false claims exit 2. Focused suites, both retained diagnostics, and full isolated 8/8
CTest pass; its smoke process remained confined to Xvfb.

Incomplete VRAM state now forbids stale `logical_tracked_bytes` and `complete_evidence`; those
fields are binder-owned and only valid with `tracking_complete=true`. Two independent stale-field
cases exit 2, while normalized raw fixtures, focused suites, retained diagnostics, and full isolated
8/8 CTest pass inside Xvfb.

Process executable identity validation now checks the full path/name for a single printable line
before accepting its `iron_gang`/`iron_gang.exe` basename. Separate report and VRAM cases reject a
newline-prefixed path in capture-session and external-manifest data. Focused suites, both retained
diagnostics, and full isolated 8/8 CTest pass. Its Xvfb smoke remained separate from the
user-requested visible instance, which stayed running.

Root capture `schema_version` now requires an actual non-boolean JSON integer equal to 8. A
floating-point `8.0` no longer passes through Python's numeric equality. The new report negative,
all focused suites, both retained diagnostics, and full isolated 8/8 CTest pass with its smoke
process inside Xvfb.

Producer-authored `checks` are correlated with frame/CPU/district sample availability and p95 budget
direction. Frame minimum/recommended, aggregate CPU, and district-load contradictions exit 2;
district load is `null` only without samples. Exact serialized budget equality deliberately accepts
either boolean because C++ evaluates full precision before writing three decimals. Four negative
and one boundary report cases pass alongside comparator 7/7, VRAM 6/6, both retained diagnostics,
and full 8/8 CTest with its graphical smoke process isolated inside Xvfb.

Release qualification now retains the producer's full-precision decision at that rounded boundary:
a valid stored `false` for frame, aggregate CPU, or mixed-district load blocks qualification rather
than being overwritten by a comparison against the serialized p95. The 30/60 FPS table cells use
the same producer checks. Exact 33.333/8.000/1000.000 failure cases pass with report 7/7, comparator
7/7, VRAM 6/6, and both retained diagnostics; full isolated CTest passes 8/8 with its smoke process
inside Xvfb.

GPU timing metadata now requires `non_blocking:true`, exact Draw-excluding-Present scope, a
non-negative discard count, and empty/non-empty unsupported reason matching the support flag. Four
report contradictions exit 2; sample count intentionally does not imply support because the generic
C++ writer allows manual measurements. Focused suites, both retained diagnostics, and full isolated
8/8 CTest pass; the smoke process ran only inside Xvfb.

District-load detail now reproduces its world/physics, renderer-upload, and total measurement rows:
sample count, phase sum, average, nearest-rank p95, and maximum are correlated with only a 0.001001 ms
serialization tolerance. Fixed procedural/null-I/O metadata, asset counts, resident-known state,
and signed RAM/VRAM deltas are validated too. Six contradictions fail and one rounding-boundary case
passes; focused suites, both retained zero-transition diagnostics, and full isolated 8/8 CTest
remain clean, with the smoke process confined to Xvfb.

All producer-defined render/physics/AI/audio workload metrics and fixed scopes are now mandatory.
Count summaries enforce zero/one-sample, average/p95/maximum, and integral p95/maximum invariants.
Six missing/mutated/corrupt cases exit 2. Cross-metric count equality and peak==maximum are not
invented because the generic writer does not guarantee them; focused suites and both retained
diagnostics pass, followed by full isolated 8/8 CTest with its smoke process confined to Xvfb.

Base capture metadata now requires printable non-empty backend/build/scenario text, positive
resolution and target-frame duration, boolean timing flags, and the formerly omittable `startup_cpu`
row. Values stay extensible for the generic diagnostic writer; qualifying policy remains separate.
Six malformed shapes exit 2 while focused suites, both retained diagnostics, and full isolated 8/8
CTest pass; the smoke process remained confined to Xvfb.

Swap metadata now fixes its proof to platform acknowledgement that explicitly excludes physical
vblank/compositor proof. Success requires an empty reason; failure/unknown requires a printable
non-empty one. Three contradictions exit 2, the qualifying failure fixture carries a real reason,
and focused suites, both retained declined-Xvfb diagnostics, and full isolated 8/8 CTest pass; the
smoke process remained confined to Xvfb.

Frame-pacing metadata now fixes both sampling meanings to the C++ producer contract: consecutive
`BeginFrame` wall-clock intervals with a baseline-only first frame, and the first interval after
`RecordDistrictLoad` for a transition boundary. Two independently mutated scope cases exit 2;
focused suites, both retained Xvfb diagnostics, and full isolated 8/8 CTest pass. Its smoke process
remained inside Xvfb, with no use of the visible host display.

The user-requested district-map follow-up adds a real top-down overlay toggled by `Tab`; `M` has no
map binding. It projects the current district's authored `WorldBox` footprints and shows the player,
vehicle, mission target, district exit, north, a legend, and a straight player-to-exit guide. The
guide is not road-aware because no district road graph exists yet. `TestDistrictMapProjection`
proves exact X/Z-to-screen point and footprint mapping. Software build plus 3/3 CTest and strict
syntax pass. Two synthetic `Tab` presses inside isolated Xvfb/X11 were visually captured: the first
showed the complete map and the second restored the unobscured game view. No test used the visible
host display. Both `LeftShift` and `RightShift` remain mapped to sprint only in the on-foot input
branch.

The Linux M12 VRAM follow-up adds `scripts/drm_vram_capture.py`. A no-window surfaceless EGL probe
against the host AMD 780M confirmed the kernel exposes three descriptors with one shared
`drm-client-id` and amdgpu resident aliases for `vram`, `gtt`, and `cpu`; this probe opened no game
window. Synthetic coverage then proves descriptor/client deduplication, standard
`drm-resident-<region>` and amdgpu alias parsing, bytes/KiB/MiB conversion, all-region and
multi-client summation, and refusal of mismatched aliases, unsupported units, invalid device IDs,
or clients without resident fields. The binder also semantically reconstructs a built-in raw JSON
artifact before enrichment; mutating a sample total and updating its hash still exits 2. The
focused VRAM suite passes 9/9 and full isolated CTest passes 8/8. A three-draw Xvfb/SOFTWARE
integration produced its normal incomplete profile, then the wrapper exited 2 because the process
had no DRM resident samples; neither a raw artifact nor manifest was created. No physical-display
Iron Gang capture was launched, so this validates the measurement/evidence path without claiming
M12 qualification.

A subsequent no-window Release EasyGL integration used `SDL_VIDEODRIVER=offscreen` with both
`DISPLAY` and `WAYLAND_DISPLAY` removed. It created a real OPENGLES3 context on the AMD Radeon 780M
and let the wrapper collect 100 complete samples for the exact Iron Gang PID. The peak was
51,986,432 B across one deduplicated amdgpu client (49,020,928 B GTT, 2,965,504 B VRAM, 0 B CPU);
the raw artifact, manifest, original profile, and enriched profile passed full binder
reconstruction. The 30-draw `idle` run is intentionally diagnostic: it is short, render CPU p95 is
20.024 ms, and offscreen presentation is not physical vblank evidence. New report coverage proves
that `offscreen`, `headless`, and `surfaceless` labels cannot be promoted with
`--qualifying-hardware`.

Two subsequent full Release EasyGL `mixed --smoke 900` offscreen captures each supplied 899 frame
intervals, complete workloads, one real district transition, and independently reconstructed DRM
archives. Both have the same 58,273,792 B (55.57 MiB) peak, while amdgpu placement differs between
54,296,576 B GTT + 3,977,216 B VRAM and 50,331,648 B GTT + 7,942,144 B VRAM; the stable sum proves
why all resident regions are included. Frame p95 is 18.003/17.461 ms, district load 0.599/0.710 ms,
RAM 177.1 MiB, and both local rows are `PASS`. A qualifying-intent audit verifies all six source
archives and reports exactly one blocker: the offscreen hardware label. The diagnostic comparator
flags only the district-boundary frame's 17.680 -> 20.306 ms increase; every ordinary timing,
memory, and hitch metric passes. This completes the memory-tracker real-flow integration without
claiming physical-display M12 qualification.

## Current modular dependency baseline (2026-08-22)

Iron Gang now configures against the sibling `../cnanext` and modular `../sharp-runtime`
checkouts. Its own CMake graph names `CNA::GraphicsCore`, `CNA::Runtime`,
`SharpRuntime::IO`, and `SharpRuntime::Text.Json`; CNA is added `EXCLUDE_FROM_ALL`, so its
unrelated Devices, GraphicsExt, C API, examples, and tools are absent from Iron Gang's default
`all` target. The former `cna-extended` animation wrapper was replaced by a small game-owned
state over CNA's `AnimationPlayer`, preserving the existing 0.25-second clip crossfade.

Validated against `cnanext` 0.1.0-alpha.1 and `sharp-runtime` 0.1.0-alpha.1:

- a fresh `compile-software` configure and full build completed successfully;
- the default build graph contains only Iron Gang, its selected CNA/Sharp Runtime module closure,
  and Jolt—not unused CNA devices, extensions, examples, or tools;
- `./scripts/check-syntax.sh` passed all 23 Iron Gang source/test translation units;
- all three CTest targets passed; and
- `iron_gang --smoke 5` loaded both districts' prototype assets, the skinned character and its
  animation clips, textures, and audio, then exited successfully.

The historical entries below describe the milestone state at the time each gate was completed;
references there to the old `../cna`/`cna-extended` graph are not current build instructions.

## Completed for this scaffold

- Every Iron Gang `.cpp` file passed a C++23 syntax-only compile against the actual supplied CNA and sharp-runtime headers with software-backend definitions.
- The prototype MC3 scene passed the supplied Mesh Craft `mc3.xsd`.
- `./scripts/preflight.sh` confirms CNA, sharp-runtime, EasyGL, and cna-extended siblings, plus populated CNA-vendored SDL/SDL_image/SDL_mixer.
- The full `compile-software` preset configured and built (780 targets, `-j4`, ccache), including `cna-extended` linking against the parent-provided `CNA` target as designed.
- `iron_gang_core_tests` linked against the real project sources, CNA, sharp-runtime, and `CNA_EXTENDED`, and all tests (collision, vehicle, mission, dialogue, save round-trip) passed via `ctest --preset compile-software`.
- CMake target and backend names used by Iron Gang were checked against CNA's and cna-extended's current CMake files.
- The full MC3 -> GLB -> CNJ pipeline ran end to end for a real production asset: `assets/source/mc3/warehouse.mc3.xml` validated against `mc3.xsd`, converted via Mesh Craft's `mc3togltf` and CNA's `cna_tool_gltf_to_cnj` (both already built in this workspace), producing `warehouse.cnj` + binary vertex/index sidecars. `./cmake-build-compile-software/iron_gang --smoke 30` loaded it through `Content.Load<Model>()` (confirmed by the `[IronGang] Loaded generated warehouse.cnj` log line), drew it in place of the procedural warehouse box, and exited cleanly; `ctest --preset compile-software` still passes, confirming the mission/collision logic is unaffected.
- The same pipeline was repeated for the sedan as four single-object MC3 files (`vehicle_body`/`vehicle_cabin`/`vehicle_windshield`/`vehicle_wheel`, one MC3 file per part because `cna_tool_gltf_to_cnj` does not bake per-object node transforms -- see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md` `IG-10-004b`), composed by `PrototypeRenderer` with Iron Gang's own per-part transforms; the smoke run logs `[IronGang] Loaded generated vehicle_{body,cabin,windshield,wheel}.cnj` and `ctest` still passes.
- Jolt Physics v5.6.0 (shared checkout at `~/deps/jolt`) was added as a dependency and built successfully as part of the `compile-software` preset (GPU-compute shader options disabled; not needed for CPU rigid-body/character/vehicle physics). `IronGang::Physics::PhysicsWorld` (`src/Physics/PhysicsWorld.cpp`) hides every Jolt type behind a PIMPL boundary. `tests/PhysicsTests.cpp` (`iron_gang_physics_tests`, run via `ctest`) exercises and passes 5 scenarios: a raycast hitting a known static floor, a dynamic box settling under gravity, a trigger volume firing enter/exit events, a capsule character controller stopped by a wall while remaining grounded, and a 4-wheel `VehicleConstraint` vehicle settling onto its suspension and then driving forward under throttle input.
- `PlayerController` and `VehicleController` were migrated to be driven by `PhysicsWorld` instead of the old fixed-height/kinematic math, with world geometry collision coming from real static bodies (`PrototypeWorld::BuildPhysicsStaticBodies`, including a dedicated ground-plane body -- a real bug: the render-only ground box is deliberately `collidable=false` for the unrelated XZ-only `CanOccupy` check, so it was silently never getting a physics body until this was added). `tests/CoreTests.cpp` gained `TestPlayerMotion` (walks forward from spawn, and confirms walking into the hotel's static collider for 5 simulated seconds does not tunnel through it) and an updated `TestVehicleMotion` (confirms the physics-driven vehicle accelerates on the real district's ground plane); both pass via `ctest --preset compile-software`. A standalone diagnostic (not committed) confirmed the sedan settles with all four wheels in ground contact and accelerates smoothly. **Not verified**: actual driving/handling *feel* (acceleration curve, top speed, steering sharpness) and visual alignment of the rendered mesh with the physics capsule/chassis, since this environment has no display or interactive input -- see `NEXT.md`.
- Gate M5 (second district): `IronGang::DistrictManager` (`src/World/DistrictManager.cpp`) was added to own the currently loaded `PrototypeWorld` and its static physics bodies, and a second, genuinely different `Countryside` district was added alongside `WarehouseBlock`, each with its own exit trigger back to the other. `PhysicsWorld::GetBodyCount()` was added (test/diagnostic only, wraps `JPH::PhysicsSystem::GetNumBodies()`) so `tests/CoreTests.cpp`'s new `TestDistrictTransition` could assert that two full round trips (warehouse -> countryside -> warehouse -> countryside) leave the physics body count exactly where it started each time it revisits the same district -- this caught a test-design mistake (asserting equal body counts after a *one-way* swap, which is wrong since the two districts have a different number of static bodies) before it was corrected to only assert equality on round trips. `SaveSnapshot`/`SaveGame` gained a `districtId` field (additive, no save-format version bump) and `TestSaveRoundTrip` now covers it. A full `./scripts/check-syntax.sh` pass and a `./cmake-build-compile-software/iron_gang --smoke 120` run (exit 0, correct warehouse/vehicle CNJ loading logged) both passed after this change. **Not verified**: actually walking/driving through an exit trigger interactively (no display access), and the loading screen's visual appearance beyond a dark clear + "Loading..." window title.
- Gate M6 (one skinned character, partial): `cna-extended` (a sibling repo, touched with the owner's explicit go-ahead for this one addition) gained `ModelAnimationComponentEXT`/`ModelAnimationSystem3DEXT` -- an ECS component/system pair wrapping cna's general-purpose `Model`+`AnimationPlayer` skinned path, which the existing `SkinnedModelComponentEXT`/`AnimationSystem3DEXT` do not (they only wrap the separate Avatar-specific `SkinnedModelEXT` data model, not the path cna's own glTF/CNJ import tools actually populate for a skinned asset). Verified there: 3 new unit tests (`ModelAnimationSystem3DEXTTests.cpp`, clip start/switch/hard-cut/unknown-name behavior against a real `AnimationPlayer`) plus a new `RenderSystem3DEXTTest.ModelAnimationComponent_PosesAndDraws` real-pixel-render case, all reusing cna's own proven-good `kSkinnedAnimatedGltf` fixture; full `cna-extended` suite re-run clean (2367 tests, 1 unrelated pre-existing parallel-run flake). For Iron Gang itself: a hand-authored 3-bone test character (`assets/source/gltf/gen_test_character_gltf.py` generates `assets/source/gltf/test_character.gltf` -- Mesh Craft/MC3 has no rigging/skinning authoring support, confirmed by checking `mc3.xsd` for any skin/bone/joint concept and finding none) was converted via `cna_tool_gltf_to_cnj` to `assets/generated/models/cnj/test_character.cnj` and verified by a standalone diagnostic program (not committed) that loaded it through `ContentManager`/`AnimationPlayer` directly and confirmed the "Walk" clip's leg-swing math matches hand-derived pivot-rotation values (`y≈0.084`, `z≈±0.380` at the swing extremes) and that "Idle" holds bind pose. `PrototypeRenderer` gained a minimal, dedicated `CNA::Extended::ECS::World` (just `ModelAnimationSystem3DEXT`, one entity) to drive the real component/system pair, and draws the resulting model directly via `Model::Draw()` (the same pattern already used for `warehouseModel_`/`vehicleModels_`) after pushing bone transforms onto its `SkinnedEffect`, rather than introducing `RenderSystem3DEXT`/`Camera3DEXT` (Iron Gang has no other ECS-driven rendering to justify that). A first attempt at this asset with no material/texture at all crashed at runtime (`TextureEnabled=true but texture0 is null` -- a skinned mesh with no material still gets `TextureEnabled=true` from cna's importer); fixed by adding a trivial 1x1 white texture (the same bytes cna's own test fixtures use) to every primitive. `./scripts/check-syntax.sh`, the full `ctest` suite, and a `--smoke 30` run (exit 0, `[IronGang] Loaded generated test_character.cnj` logged, no crash) all passed after the fix -- note this environment's shared multi-tenant machine was under heavy contention while validating this change (`load average` ~5 on 16 cores from concurrent unrelated sessions), which made the CPU software rasterizer visibly slower per frame; this was confirmed to be real system load, not a hang, by running longer and observing continued forward progress (236 frames in 60 real seconds) rather than a stuck call. **Not verified**: dialogue-pose/vehicle-entry-exit animations (not implemented), and any interactive/visual check of how the character actually looks or moves, since this environment has no display.
- Gate M6 clip blending (follow-up, same session): `ModelAnimationComponentEXT`/`ModelAnimationSystem3DEXT` (cna-extended) gained `BlendDurationEXT`/`BlendFromSkinTransformsEXT`/`BlendedSkinTransformsEXT` -- a per-bone `Matrix::Lerp` crossfade from a frozen outgoing-pose snapshot to the new clip's live pose over `BlendDurationEXT` seconds (default 0.25s; 0 = hard cut), replacing the earlier hard-cut-only behavior. `RenderSystem3DEXT` and Iron Gang's `PrototypeRenderer` were updated to read `BlendedSkinTransformsEXT` instead of `PlayerEXT.GetSkinTransforms()` directly. Verified in `cna-extended`: 2 new tests using a hand-verified two-named-clip glTF fixture (cross-checked against a real `AnimationPlayer` via a throwaway diagnostic before being embedded) proving the blend math numerically at t=0/halfway/finished, plus a `BlendDurationEXT=0` hard-cut case; full suite re-run clean at 2369/2369, including the previously-flaky `TexturePackerFileReaderTests` case (confirming that earlier failure was a parallel-run isolation flake, not a real regression). Verified in Iron Gang: full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` passes on every file, and a `--smoke 20` run exits 0 with correct asset-loading logs. **Not verified**: how the crossfade actually looks during a real Idle<->Walk transition, since this environment has no display.
- Gate M6 dialogue pose (follow-up, same session): `gen_test_character_gltf.py` gained a third "Dialogue" clip -- a static "parade rest" leg stance rotated about the local Z axis (a different axis than Walk's X-axis swing, so it reads as a genuinely distinct pose, not a frozen mid-walk frame) -- regenerated into `test_character.cnj` (now 3 clips). `IronGangGame::Update` calls `renderer_.UpdateCharacterAnimation(deltaSeconds, "Dialogue")` whenever `dialogue_.IsActive() && !playerDriving_ && !transitioning`, alongside the existing Walk/Idle call (which only fires when dialogue is NOT active, so the two never race). Verified: a standalone diagnostic (not committed) loaded the regenerated CNJ through `ContentManager`/`AnimationPlayer` and confirmed the Dialogue pose's foot position matches hand-derived pivot-rotation math exactly (`(-0.0747, 0.00876, 0)` for the left foot at an 8° Z-axis rotation about the hip pivot). Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, and a `--smoke 20` run exits 0 -- notably, smoke mode never dismisses the opening dialogue, so this run exercised the new Dialogue clip continuously for its entire duration, not just at t=0. **Not verified**: how the pose actually looks, since this environment has no display.
- Gate M6 vehicle entry/exit animation (follow-up, same session, gate M6 now fully done): `gen_test_character_gltf.py` gained two more one-shot clips, "EnterVehicle" (standing -> sitting, both legs bending forward together via `quat_x`, unlike Walk's alternating phase) and "ExitVehicle" (the reverse), each authored as a 1-second clip but only ever played for 0.5s (`IronGangGame::kVehicleTransitionSeconds`) so the motion is still visibly in progress -- not already at its end pose -- when the game switches away, and so `LoopEXT`'s default-true modulo wraparound never triggers. `test_character.cnj` regenerated with 5 clips total. Added a small `VehicleTransitionState` (`None`/`Entering`/`Exiting`) state machine to `IronGangGame`: entering the sedan starts `Entering` and keeps the character visible/on-foot-input-suppressed while "EnterVehicle" plays, only flipping `playerDriving_` true (hiding the character) once the clip finishes; exiting flips `playerDriving_` false immediately (character visible right where the car is) and starts `Exiting` while "ExitVehicle" plays. `HandleInteraction()` ignores a new interaction while a transition is already in progress; `LoadPrototype()`/`ResetPrototype()` both reset the state to `None` for safety. Verified: two standalone diagnostics (not committed) loaded the regenerated CNJ and confirmed both clips' foot-position math at t=0/0.5s matches hand-derived pivot-rotation values exactly (e.g. `EnterVehicle@0.5` and `ExitVehicle@0.5` both land at the same halfway pose, as expected by symmetry). Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, and a `--smoke 20` run exits 0. **Not verified**: the vehicle-transition *state machine itself* (as opposed to the underlying clip math) was checked by careful manual code review of `HandleInteraction()`/`Update()`'s new branches, not by an automated test -- smoke mode never presses 'E' and `IronGangGame` is not unit-testable headlessly today. Also not verified: how any of this actually looks, since this environment has no display.
- Gate M7 (one data-driven mission, done at prototype fidelity): `assets/missions/prologue.mission.json` (a pre-existing stub file from the original scaffold, previously just documentation of an "intended future form") is now the real, active mission definition -- 5 states, each with objective text, a named transition condition, and a next-state id. New `MissionDefinition`/`LoadMissionDefinition` (`include/`/`src/Missions/MissionDefinition.hpp/.cpp`) parse and validate it using sharp-runtime's own `System::Text::Json` (`JsonDocument`/`JsonElement`, backed by vendored `nlohmann::json`) -- already a linked dependency, no new library added. Validation rejects (with an actionable error): duplicate state ids, an `initialState`/`next` that doesn't match any state id, an unrecognized condition name, and an empty state list. `PrototypeMission` now resolves its current state's objective text and transition condition against a loaded `MissionDefinition` instead of a hardcoded switch statement, while keeping `PrototypeMissionState` (a fixed enum) for `SaveGame`'s existing int-based format and public-API compatibility -- `LoadMission()` additionally rejects any mission file introducing a state id outside that fixed set. `IronGangGame::Initialize` loads the real file, falling back to an identical hardcoded default (matching `DialogueSystem::LoadFallbackPrologue()`'s convention) on any failure. Verified: the pre-existing `TestMissionFlow` (hardcoded default) still passes unchanged; two new tests -- `TestMissionLoadsCommittedFile` (loads the real committed file and drives it through the identical reach-vehicle/enter-vehicle/drive-to-warehouse/completed flow) and `TestMissionValidationRejectsMalformedData` (6 cases: dangling `next`, dangling `initialState`, duplicate id, unknown condition, empty states, missing file -- all correctly rejected -- plus one well-formed minimal mission that still loads correctly afterward, proving failures don't corrupt the loader's own state) -- both pass. A standalone diagnostic (not committed) confirmed the real committed file parses to the exact expected 5-state structure. Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` passes on every file (including the new `IRON_GANG_SOURCE_ASSET_DIR` compile definition added to both the CMake test target and the syntax-check script so the real committed file can be validated by path), and a `--smoke 20` run exits 0 with no mission-load fallback message (confirming the real file loaded successfully at runtime, not just in the standalone test). **Not verified**: save/load resuming correctly mid-mission with the new data-driven system was reasoned through (the int-enum save format and `SetState()`/`GetState()` are unchanged) but not covered by a new dedicated test, and there is no display to check the objective-text window title actually updates correctly on screen.
- Gate M8 (one in-engine cutscene, done at prototype fidelity, camera-track-only scope): new `CutscenePlayer`/`CutsceneSequence` (`include/`/`src/Cutscenes/CutscenePlayer.hpp/.cpp`, `CutsceneSequence.hpp/.cpp`) play a hand-written, versioned JSON camera keyframe sequence, parsed/validated the same way as `MissionDefinition` (sharp-runtime's `System::Text::Json`, inline validation rejecting an empty keyframe list, a first keyframe not at time 0, non-strictly-ascending keyframe times, and a duration shorter than the last keyframe). `assets/cutscenes/prologue_intro.cutscene.json`: a 2.5s pan from an establishing shot of the warehouse delivery target `(25, 12, -34)` looking at `(0, 2, -34)` to `(0, 4.65, 27.5)` looking at `(0, 1.25, 20)` -- the second keyframe computed by hand to exactly match `Draw()`'s own `target = playerPos + (0,-0.45,0); camera = target - forward*7.5 + (0,3.4,0)` formula at the player's spawn point (`(0, 1.70, 20)`, yaw 0), so the cut back to the normal follow camera has no visible pop. `IronGangGame::Initialize()` starts it alongside the opening dialogue; `Update()` ticks it every frame (gated on `!transitioning`, independent of dialogue's own pace) and treats a second Enter press (once dialogue has already finished) as a skip; `Draw()` overrides `view`/`target` with the cutscene's interpolated camera whenever `IsActive()`; player/vehicle input and physics stepping are frozen while it plays via the same `!dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning` gate already used for dialogue. `LoadPrototype()`/`ResetPrototype()` both force `cutscene_.Skip()` defensively. Verified: `TestCutscenePlayerAdvancesAndFinishes` (fixed-time updates, asserts exact linear-interpolation numbers at the halfway point and the exact terminal keyframe once finished), `TestCutscenePlayerSkipAppliesTerminalState` (Skip() partway through must produce the identical terminal camera state a natural finish would), `TestCutsceneValidationRejectsMalformedData` (5 malformed-data cases + a missing file, all correctly rejected, plus one well-formed sequence that still loads afterward) -- all pass. A standalone diagnostic (not committed) confirmed the real committed file parses to the exact intended keyframe values. Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, a `--smoke 20` run (covers only the first ~0.3 simulated seconds, mid-cutscene) and a `--smoke 200` run (covers ~3.3 simulated seconds, past the cutscene's 2.5s duration, confirming it naturally finishes and the game keeps running normally afterward) both exit 0 with no cutscene-load fallback message. **Not verified**: the actual Enter-press skip path in a real running game (smoke mode's dialogue never becomes inactive on its own, so the skip branch was only exercised by the standalone unit tests, not an end-to-end game run), and any visual/interactive check of how the pan actually looks, since this environment has no display.
- Gate M9 (traffic, pedestrians, one police-response scenario, done at first-pass/prototype fidelity, including the gate's own literal "ten-minute soak" wording): new `WaypointPath`/`AdvanceAlongPath()` (`include/`/`src/World/WaypointPath.hpp/.cpp`) -- a hand-authored ordered polyline plus a `loop` flag, not a graph -- is a shared path-following helper used by both new `TrafficVehicle` and new `Pedestrian` (`include/`/`src/Gameplay/`). `TrafficVehicle::Update()` accelerates toward a cruise speed (6 units/s²) and brakes (12 units/s²) when an externally-computed obstacle distance falls inside a 10-unit braking distance / 3-unit minimum gap; `IronGangGame`'s new `DistanceAheadIfInLane()` helper supplies that distance from other traffic vehicles and the player's own vehicle. `Pedestrian::Update()` walks a fixed sidewalk path at 1.6 units/s, overridden by a 4-second "flee directly away from the last-known threat position" state (2.5x speed, continuing off its own timer even after the threat is no longer reported present) when the player's vehicle comes within 6 units. New `PoliceSystem::Update()` runs `Clear -> Dispatched -> Chasing`: Clear triggers on a witnessed offense (player speeding >70 km/h, or within 2.5 units of a witness, while any traffic-vehicle/pedestrian position is within a 15-unit "witness radius" -- a simplified proximity check, not a real vision-cone/line-of-sight test); Dispatched holds for a fixed 2 seconds; Chasing drives up to 2 patrol cars straight toward the player's current position (ignoring roads) at 9 units/s, escalating to a second car after 20 seconds still chasing (the one locked escalation tier), and resolves back to Clear once the closest patrol car has stayed beyond 40 units for 3 sustained seconds. `PrototypeWorld::BuildWarehouseBlock()` hand-authors one 4-point traffic loop (both lanes of `road_north_south`, X=±3) and two 2-point sidewalk paths (X=±7.5, matching `sidewalk_west`/`sidewalk_east`); `BuildCountryside()` was not touched, so it has neither, by design. `IronGangGame` gained `RespawnTrafficAndPedestrians()` (spawns 2 traffic vehicles + 2 pedestrians, resets `police_`; called from `Initialize()`, district arrival, load, and reset, since none of this ambient state is part of `SaveGame`), ticks all three systems every frame gated only on `!transitioning` (they keep running through dialogue/cutscenes), and a new `PrototypeRenderer::DrawTraffic()` draws them as colored boxes distinct from the player's own sedan/character meshes. The window title appends "Police dispatched..."/"WANTED" while not Clear. Verified: 4 new deterministic unit tests in `tests/CoreTests.cpp` -- `TestWaypointPathAdvancesAndWraps`, `TestTrafficVehicleAcceleratesAndBrakes`, `TestPedestrianFleesAndResumesPath`, and `TestPoliceSystemFullCycle` (the big one: exercises the FULL `Clear -> Dispatched -> Chasing -> escalate -> resolve` cycle against hand-computed, standalone-diagnostic-confirmed position/timer values, plus two negative cases -- not driving, and a witness outside the radius -- both correctly never triggering a chase). A standalone diagnostic (not committed) caught a wrong test assumption before it shipped: starting a mover exactly AT its first waypoint (as `Reset()` does) immediately wraps to the second waypoint on the very first `AdvanceAlongPath()` call, since distance-to-current-target is already 0 -- the first draft of `TestWaypointPathAdvancesAndWraps` assumed no wrap on that first call and failed until corrected to match the actual (correct) behavior. Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, two short `--smoke` runs (30 and 90 draw frames, ~10s and ~25s wall-clock), and the gate's own literal "ten-minute soak" requirement: a `--smoke 3000` run, launched in the background and timed via `date +%s` before/after, ran for 980 real seconds (~16.3 minutes) with `trafficVehicles_`/`pedestrians_`/`police_` all ticking every frame throughout, exiting cleanly (exit 0, no crash, no error, no asset-fallback message in the log). Confirmed via `cna/src/Microsoft/Xna/Framework/Game.cpp` that `Game::Tick()` uses a fixed 1/60s timestep accumulator fed by real elapsed wall-clock time (`IsFixedTimeStep_` defaults true, `TargetElapsedTime_` = 1/60s), so simulated game time tracks real wall-clock time 1:1 for this workload -- a ten-minute soak genuinely requires close to ten real minutes of runtime, there is no way to compress it via a larger `--smoke` frame count alone. **Not verified**: memory-leak growth specifically over the soak run (only crash/stall-freedom was checked, no periodic memory sampling was added), and, as with every other visual milestone this session, there is no display to check how any of this actually looks.

- Gate M10 (production assets/collision, baked lighting, one dynamic sun, limited shadows, audio, UI -- all five pieces done at first-pass/prototype fidelity): **UI** -- `include/`/`src/UI/BitmapFont.hpp`/`.cpp` builds a real `SpriteFont` at runtime from the public-domain font8x8 bitmap glyphs (CNA has no XNB font pipeline), drawn via `SpriteBatch` each frame in `IronGangGame::Draw()` (objective/speed/dialogue/wanted/status), replacing the window-title-only display. **Dynamic sun** -- `include/IronGang/Graphics/SunLight.hpp`'s `ComputeSunBrightness()`/`ComputeBrightnessForNormal()` compute a CPU brightness scalar from a single fixed sun direction (confirmed `BasicEffect`/`SkinnedEffect`/etc.'s built-in `DirectionalLight0` lighting is a no-op on the SOFTWARE backend by reading its own source), applied via `PrototypeRenderer::DrawMesh()`'s new `tint` parameter (procedural boxes) and a new `SetModelDiffuseColor()` helper (CNJ `Model` content) to every dynamic actor (player/vehicle/traffic/pedestrians/police) each frame -- confirmed `DiffuseColor` DOES apply unconditionally on this backend, independent of the lighting no-op. **Limited shadows** -- new `PrototypeRenderer::DrawShadowDecal()` draws a flat, dark, alpha-blended "blob shadow" beneath the player and their own vehicle only (confirmed the SOFTWARE backend has no shadow-map support at all and real shadow-mapping is not achievable without modifying CNA itself). **Baked lighting** -- new `include/`/`src/Graphics/LightmapMesh.hpp`/`.cpp`: `LightmapMeshBuilder` builds the static city mesh (procedural boxes only) with 24 vertices per box (one per face, no sharing, since each face needs its own UV), baking one flat-shaded lightmap tile per face from the shared sun direction into a real texture atlas (fixed 32-column grid of 4x4 tiles, each face sampling the exact tile center to avoid bilinear bleed), sampled via a new `PrototypeRenderer::lightmapEffect_` (`DualTextureEffect`, confirmed fully implemented on the SOFTWARE backend: `finalColor = vertexColor * (texture0*2) * texture1 * diffuseColor`) with `texture0` a flat near-gray identity texture and `texture1` the real baked atlas; MC3-sourced models stay out of scope (no lightmap UV channel in that pipeline). **Audio** -- real CC0 sound from Nox Sound Design's "Essentials Series" pack (itch.io, 988MB); WebSearch/WebFetch confirmed the license but the actual download needed a JS-driven payment-bypass step neither `curl` nor this environment's (unconnected) browser tools could complete, so the user downloaded it manually and handed it off. Three files extracted (renamed only) into `assets/audio/` (`engine_loop.wav`, `horn.wav`, `footstep.wav`), recorded in `assets/licenses/asset-registry.csv`; no ambience/siren (not in the pack, and a second CC0 source hit the same download friction). `IronGangGame` gained `engineSound_`/`engineSoundInstance_`/`footstepSound_`/`hornSound_` (`SoundEffect(const std::string&)`, SDL3_mixer decodes WAV natively, no XNB step), each optional with the same try/catch-with-fallback convention as every other optional asset; the looped engine sound ties to `playerDriving_` with speed-scaled volume/pitch, the horn fires on H while driving, footsteps fire on a fixed-interval timer while walking. Verified: `TestSunBrightnessMatchesHandComputedValue`, `TestLightmapMeshBuilderBakesPerFaceBrightness` (both against hand-computed/Python-cross-checked values), `TestBitmapFontGlyphAtlas`, the full `ctest` suite, `./scripts/check-syntax.sh`, `--smoke` runs, and -- since `--smoke` mode never drives or walks -- a standalone diagnostic (not committed) that directly exercised `SoundEffectInstance`/`SoundEffect` playback against the real audio files through the real SDL3_mixer pipeline, confirming correct state transitions and successful playback with no exceptions (this environment's audio hardware initialized successfully, so `NoAudioHardwareException`'s fallback was reasoned about but not actually exercised). **Measured a real ~4-5x per-frame slowdown** from the lightmap's two-bilinear-texture-sample draw path on this environment's CPU software rasterizer (a `--smoke 20` run went from ~10s to ~26s wall-clock), not yet profiled against `docs/performance-targets.md` (real gate M12 scope). **Not verified**: how any of this actually looks or sounds, since this environment has no display and cannot play audio for a human to hear.

- Gate M11 (mission happy-path/failure/retry/save-load/cutscene-skip automation, all ten sub-tasks `IG-39-060`-`069` done): almost no new production code -- mostly proving what already exists end to end. New tests in `tests/CoreTests.cpp`: `TestSaveLoadMidMissionPlaythrough` (saves mid-mission, loads into a fresh `PrototypeMission`, proves it can still complete -- not just that the state enum round-trips), `TestCutsceneSkipDoesNotBlockMissionProgression` (confirms `PrototypeMission::Update()`'s architectural independence from cutscene state with a regression test), `TestMissionResetActsAsRetry` (this prototype's one mission has no real failure/branching state -- `plan_24`'s own locked scope -- so "retry" is proven via `Reset()` returning to the initial state and completing again), `TestVehicleStatePersistsIndependentlyOfPlayer` ("vehicle-loss recovery" reinterpreted at the level that actually exists, since no combat/damage system exists yet: a save made on foot far from a parked vehicle restores both positions independently), and `TestDistrictTransitionPreservesMissionState` (a full district round trip mid-mission leaves mission state untouched). Fresh-start playthrough and missing-optional-asset behavior were already covered by pre-existing tests. Soak test: a `--smoke 3000` background run exercising the full M10-era rendering/audio path; the lightmap draw path made it far slower per frame than the M9 baseline, so it was manually stopped (`TaskStop`, not a crash) after 65 minutes (3925s) of continuous, error-free execution -- more than six times the gate's own "ten minutes" wording. Performance capture: `/usr/bin/time -v ./iron_gang --smoke 60` measured 55988 KB (~55MB) maximum resident set size (far under the ~2-4GB budget) and ~1.55s/frame average, dominated by the lightmap's two-bilinear-texture-sample draw path plus HUD text drawing -- not meaningfully comparable to the 30-60 FPS target, since this CPU software rasterizer backend was never intended to be performant (it exists only because this environment has no GPU/display); real frame-time verification needs a `dev-easygl`/`dev-vulkan` build, still unverified here. License audit: found and fixed two real gaps -- `assets/licenses/asset-registry.csv` was missing rows for `assets/missions/prologue.mission.json` and `assets/cutscenes/prologue_intro.cutscene.json` (both original content, same convention as the already-tracked dialogue file), and `THIRD_PARTY.md` still claimed no external content was bundled, which became false as of gate M10 (font8x8 Public Domain font, Nox Sound Design CC0 audio) -- both corrected. Verified: full `compile-software` rebuild (clean), all three `ctest` targets pass (5 new tests), `./scripts/check-syntax.sh` clean.

## M12 native-window evidence

The M12 native-window evidence follow-up adds CNA's validated native-window classification to every
new schema-8 profile. Report/comparator focused suites pass 7/7 each, full isolated CTest passes
8/8 including the C++ writer assertion, syntax validation passes, and the EasyGL executable builds
cleanly. A real AMD offscreen run with `DISPLAY` and `WAYLAND_DISPLAY` removed emitted
`Headless/false` despite a successful GL context and swap acknowledgement; an innocently named
physical-display report still blocked it. A separate isolated Xvfb profile emitted `X11/true`,
confirming the two platform paths remain distinct. Neither run used the visible host display or
supplies physical qualification.

The following M12 runtime-identity pass records `GL_VENDOR`, `GL_RENDERER`, and `GL_VERSION` from
the current EasyGL context. A no-window AMD run reported the Radeon 780M/radeonsi, while isolated
Xvfb reported Mesa llvmpipe. The latter remained software-blocked under the intentionally misleading
label `Discrete GPU physical-display claim`, demonstrating that classification no longer depends
on operator prose. Old diagnostics remain readable but missing runtime identity cannot qualify.
A complete short DRM-wrapper follow-up then retained 100 samples, bound a 49.57 MiB peak, verified
the enriched profile, and reported Radeon runtime plus `Headless/false`; all direct metrics passed
while the presentation classification remained diagnostic. No visible display was used.
A subsequent pair of full Release `mixed --smoke 900` DRM runs passes every direct minimum with
17.147/17.907 ms frame p95, no minimum misses or hitches, stable 55.574 MiB complete residency, and
`NO REGRESSION` comparison. Both archives verify independently. The qualifying audit fails only on
the offscreen label and each capture's machine-derived `Headless` state. No visible display was used.
This real raw/manifest/enriched/report output plus the existing documentation closes the final
memory-tracker logging task `IG-35-030`; a live overlay is intentionally outside the bounded
profiler scope. The physical M12 gate remains open.
A further Release AMD offscreen `--vsync on` integration recorded requested/applied interval 1 with
successful acknowledgement, 17.122 ms frame p95, and a verified 49.57 MiB DRM peak. Its independent
window evidence remained `Headless/false`, so the visible `1 / yes` report row correctly failed
local qualification. This validates the non-vblank proof boundary without using a visible display.
The final physical-pair workflow is now covered by `iron_gang_m12_capture_pair_tests`. Four isolated
fake-tool integrations prove exact two-run ordering and the locked `mixed --smoke 900`/v-sync
arguments, qualifying comparison after PASS, diagnostic comparison plus preserved artifacts after
FAIL, regression exit 1, and all-output collision refusal before the first tool starts. The focused
CTest passes 1/1 and the complete compile-software suite passes 9/9; no game or visible display is
used by this orchestration test.
A real follow-up invoked the orchestrator with both desktop display variables removed and
`SDL_VIDEODRIVER=offscreen`. All six stages completed: two independent 899-interval AMD EasyGL
captures, both DRM bindings, qualifying-intent report, and diagnostic comparison. Both four-file
archives independently re-verify, each records 58,273,792 B complete residency, all direct minimum
budgets pass, qualification has only the explicit offscreen label plus two `Headless` blockers, and
comparison says `NO REGRESSION`. The designed process exit is 1 (valid gate failure), not workflow
error 2. No visible display was available to the processes.

The frame-outlier correlation follow-up adds bounded top-eight context to new schema-8 profiles:
every retained existing frame interval includes its zero-based aggregate sample index and the
scenario phase/update at the `BeginFrame` ending it. C++ tests prove top-eight retention, descending
order, stable ties, phase/update storage, and JSON output. The report's expanded 8/8 suite validates
good rendering plus duplicate/negative/out-of-range indices, wrong maximum/order, non-canonical
phase/update, and record-count failures; comparator 8/8 proves both retained lists render. Older
schema-8 diagnostics remain accepted without the additive block.

Release EasyGL and compile-software rebuild, complete 9/9 CTest, and strict syntax validation pass.
A real AMD offscreen paired run with both desktop display variables absent then exercised producer,
DRM binding, report, and comparison end to end. Its two 899-interval captures record
17.997/17.868 ms p95, 30.114/26.204 ms maximum, no >33.333 ms miss or hitch, and stable 55.574 MiB
complete residency. The maxima correlate to `mixed_drive` samples 534/208 and scenario updates
536/210; the earlier isolated ~76 ms hitch did not reproduce. Comparison is `NO REGRESSION` and the
qualifying-intent report fails only for the deliberate offscreen label plus two machine-derived
`Headless` states. No visible display was used and physical M12 remains open.

## On-foot movement gets momentum (2026-08-26)

plan_16 `IG-16-005` advanced to partial (acceleration, deceleration, and turning inertia done;
slope handling not). plan_16 stood at 1 of 80 with the note that on-foot movement "already works" —
it did, in the sense that a keypress **was** full speed and a release **was** a dead stop. That
reads as a cursor, not a person, and it is the most-felt half of a game where you walk to the car.

**What changed.** `Locomotion` (`include/IronGang/Gameplay/Locomotion.hpp`) eases forward/strafe
velocity and turn rate toward what the input asks for:

* **Stopping is quicker than starting** (26 vs 18 m/s²), on purpose. People lean into a walk and
  plant their feet to halt, and a character who stops faster than he starts feels responsive rather
  than sluggish — the opposite ordering is what makes momentum feel like ice.
* **Deceleration is chosen per axis**, so releasing forward while still strafing does not brake the
  strafe.
* **Diagonal input is clamped**, so moving diagonally is not faster than moving straight.
* **A teleport drops momentum** (`Reset`/`SetPosition`), so a respawn cannot carry speed into
  wherever the character lands.
* `MoveToward` snaps to the target when within one step rather than overshooting — overshoot at a
  low frame rate is exactly how a character jitters around a standstill.

The model is pure arithmetic with no physics, input, or rendering in it, so the feel is unit-tested
rather than eyeballed. `PlayerController` now asks it for a velocity and hands that to the same
`CharacterVirtual` as before; `GetSpeed()`/`IsMoving()` are exposed for a future animation blend or
footstep timer.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**;
`TestLocomotionAcceleratesAndDecelerates` covers one frame not reaching walking pace, walking pace
being reached in under half a second and settling exactly (no overshoot, no creep), sprint raising
and releasing easing back without snapping, stopping measured **from a clean walk** and required to
be quicker than starting, diagonal clamping while still using both axes, releasing forward not
braking the strafe, turn rate easing in and reversing through the intervening rates, reverse input
not flipping the velocity in one frame, `Stop()` and zero-length frames, and tuning being honoured.

A full `--profile-scenario mission --smoke 1200` run still completes: the walk to the sedan now
takes momentum into account and still reaches the handover radius.

**A test bug worth recording:** the first version measured "frames to stop" from wherever the sprint
assertions had left the character — roughly sprint speed — and compared it against "frames to walk
from rest". It reported 15 vs 14 and failed. The code was right; the measurement was comparing two
different starting speeds. Fixed by measuring the stop from a clean walk.

**Boundaries.** No slope handling: `CharacterVirtual` resolves slopes as geometry, but nothing
changes speed uphill or downhill and there is no slide threshold. No step-up/ledge/falling states
(`IG-16-006`), no crouch, no camera work, and the locomotion constants are compile-time defaults
rather than a tunable file.

## The sedan can be wrecked (2026-08-26)

plan_17 `IG-17-015` advanced to partial (impact damage and disabled states done; wheel damage has
nothing to damage yet). This closes the gap `NEXT.md` had been carrying since gate M11: "no
combat/damage system at all, so vehicle-loss has no real mechanic behind it".

**How impacts are detected, and why.** From the vehicle's **own speed history**, not from contact
reports: a frame in which speed drops faster than any brake could manage *is* a collision. That
needs nothing new from the physics layer and cannot miss a contact Jolt resolved internally. The
separation is wide on purpose — a good car brakes at about 1 g (0.17 m/s per 60 Hz frame), the
threshold sits at about 4 g, and a 20 m/s crash stops in a frame or two — so ordinary driving can
never scratch the paint.

Integrity runs 1 to 0, accumulates across impacts, floors at 0, and scales the engine's pull down
toward `minimumSpeedFactor` as it falls. **A wrecked sedan still steers and rolls**: being stranded
in a wreck is a situation, being unable to move at all is a trap. Reversing into something counts
(magnitudes are compared), changing direction through zero does not.

`VehicleDamage` is pure arithmetic with no physics, input, or rendering in it, which is why the
whole model is unit-tested. Thresholds are tunable in `sedan.vehicle.json`'s new `damage` block;
integrity is saved (an older save loads an intact car — the friendlier of the two defaults) and
published to missions as `vehicle_integrity`/`vehicle_disabled`, so a mission can now fail on a
wrecked car. The HUD shows `DAMAGE n%`, then `WRECKED`.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**;
`TestVehicleDamageDistinguishesCrashesFromBraking` covers a full 1.1 g braking stop leaving the car
untouched, accelerating/holding/coasting/zero-length frames doing nothing, a wall at speed doing
damage, a harder impact costing more than a softer one, accumulation to a wreck with integrity
flooring at 0, a wrecked car keeping exactly its minimum speed factor and still rolling, reversing
into a wall counting while a direction change through zero does not, repair/restore/clamping
(including NaN falling back to undamaged rather than poisoning the model), a raised threshold
ignoring an impact it used to count while a harder crash still registers, and the save round trip
plus the older-save default.

The end-to-end check that matters most: a full `--profile-scenario mission --smoke 900` run records
**zero** impacts and still completes the delivery. A damage model that fired on normal driving would
have wrecked the car before the warehouse.

**Boundaries.** No wheel damage — wheels have no per-wheel state to damage. No visual damage, no
smoke, no repair mechanic beyond a mission reset, and nothing in the committed prologue fails on a
wrecked car yet (the fact exists; no mission uses it). A stale comment in
`TestVehicleStatePersistsIndependentlyOfPlayer` claiming "no vehicle-destruction mechanic exists"
was corrected rather than left to mislead.

## Data files are bounded before a parser sees them (2026-08-26)

plan_36 `IG-36-009` closed; `IG-36-003` and `IG-36-005` recorded as already done under plan_24 and
plan_29; `IG-36-002`/`IG-36-006` advanced to partial. Three loaders written over the last few
iterations — mission, game configuration, vehicle tuning — each opened a file, read all of it, and
handed the text to a JSON parser with **no size, encoding, or depth limit**. The duplication was
mine; so was the gap.

**What changed.** `ReadBoundedJsonText` (`include/IronGang/Core/JsonDataFile.hpp`) now stands in
front of all three, checking in this order:

* **Size**, read from the filesystem — so a 900 MB file is refused *without being loaded into
  memory*. Refusing it after allocating it would be a strange kind of protection. The limit is
  1 MiB, roughly a hundred times the largest file the game ships.
* **UTF-8**, rejecting stray continuation bytes, truncated sequences, overlong encodings,
  surrogates, and anything above U+10FFFF. Overlong forms matter specifically because they are how
  one character gets two spellings, and a validated identifier stops equalling the compared one.
* **Nesting depth**, counted over the raw text (ignoring brackets inside strings, honouring
  escapes), so a document deep enough to exhaust the stack is refused **before** the
  recursive-descent parser runs. Unbalanced brackets and unterminated strings fall out of the same
  pass.

These files are hand-authored today, but the project's own scope decision (`IG-36-001`) is to treat
generated and downloaded assets as untrusted, and "we wrote it ourselves" stops being true the first
time a mission is produced by a tool.

**An architecture point that shaped the API.** The first version exported a `JsonDataFile` struct
holding a `JsonDocument` — which broke the test build, because sharp-runtime's `Text.Json` is a
**private** dependency of `iron_gang_core` and that header dragged it into every consumer. The
public header now names no JSON types at all (bounds, `IsValidUtf8`, `MeasureJsonNestingDepth`,
`ReadBoundedJsonText`), and the document-returning half lives in `src/Core/JsonDataFileInternal.hpp`,
outside `include/`. The compiler caught a real layering violation, and the fix is the better API.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**;
`TestJsonDataFileIsBoundedBeforeParsing` covers an ordinary read, a missing file, a non-object root
refused by a loader that needs one, an over-1-MiB file, a document nested 20 levels past the limit,
invalid UTF-8 in a real file, eight direct `IsValidUtf8` cases (including overlong, surrogate,
past-U+10FFFF, and truncated), seven depth-counter cases (including brackets inside strings, an
escaped quote, and each unbalanced direction), and — the part that matters — the depth bound
applying **through all three loaders**, not just when called directly. `scripts/check-syntax.sh`,
`git diff --check`, and a `--smoke 60` run with zero warnings or errors.

**Boundaries.** Still unbounded: CNJ/glTF model and texture data, WAV audio, and the save file, all
read without a size check — the save at least has a checksum and a backup, the assets have nothing.
No Unicode **normalization** anywhere (`IG-36-006`'s other half). Error messages still contain full
local paths, which `IG-36-013` says shipping builds should not leak.

## The sedan's numbers move into data (2026-08-26)

plan_17 `IG-17-003` closed, `IG-17-002` recorded as already done, `IG-17-001`/`IG-17-004` given
honest partial notes. plan_17 had 0 of 97 entries done while a Jolt raycast vehicle had been driving
the game since gate M4 — the file was simply never updated.

**What changed.** `assets/vehicles/sedan.vehicle.json` (schema version 1) now holds the chassis mass
and half extents, wheel radius/width/positions, and the forward/reverse speed limits that
`VehicleController.cpp` used to hard-code. `VehicleConfig`/`LoadVehicleConfig` follow the same
failure contract as the mission, game-config, and save loaders: a missing file, an unknown key
(named **with its section**, so `chassis.masss` is findable), a wrong type, an out-of-range value,
or a wheel list that is not exactly four entries are warnings that keep defaults; only malformed
JSON, a non-object root, or an unsupported version is a failure, and a failure leaves the caller's
configuration untouched.

Two validation choices worth stating: a **zero mass or zero-radius wheel can never reach the physics
body** (those do not degrade gracefully in a solver, they explode), and **one malformed wheel entry
defaults all four** rather than leaving three applied and one defaulted, which would be a wheelbase
nobody designed.

**A real bug found while wiring this up.** The loader was initially called after
`vehicle_.Reset()` — which creates the physics body — so mass, chassis, and wheel geometry would
have been baked in *before* the file was read, and the tuning would have silently applied nothing
but the speed limits. The load now runs before `districtManager_.Initialize()`, and
`VehicleController::Configure` logs a warning if it is called once the body exists, so the same
mistake cannot be silent again.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**;
`TestVehicleConfigLoadsValidatesAndFallsBack` covers the missing file, a full round trip of every
field, unusable numbers keeping defaults, a short wheel list, a malformed wheel entry defaulting all
four, an unknown key named with its section, the reverse-faster-than-forward warning, malformed JSON
and an unsupported version failing while leaving the caller's config alone, and — the assertion that
proves this change did not alter driving — the **committed** sedan loading with zero warnings and
carrying exactly the values the code used to hard-code, chassis and wheel offsets included. A
`--smoke 60` run logs no vehicle warning, confirming the real file is read at startup. Three
allowlists needed the new `assets/vehicles` directory: the asset registry's production directories,
the CMake install set, and `release_archive.py`'s packaged-asset list; the registry test's fixture
and its "15 approved shipping assets" expectation were updated with it (now 16).

**Boundaries.** `chassis.halfExtents` and `wheels.positions` must keep matching
`PrototypeRenderer`'s body box and wheel offsets, and **nothing validates that** — the physics
chassis and the drawn car are built by different code, and a mismatch shows up only as floating or
sunken wheels. Suspension, steering response, and braking still use Jolt's defaults and are
therefore deliberately absent from the schema: a key the game does not read would be worse than no
key. No gears, engine curve, per-surface grip, damage, lights, or second vehicle; no runtime reload.

## Pedestrians stop walking through each other (2026-08-26)

plan_20 `IG-20-010` closed. This fixes a defect the entry below **introduced**: six pedestrians per
two-point sidewalk, half of them walking each way, meant they passed straight through one another —
invisible at a population of two, obvious at twelve.

**What changed.** Stacking and clipping are different problems, so there are two mechanisms:

* **Lanes.** `Pedestrian::SetLaneOffset` shifts a pedestrian off the path centreline, always to the
  right of its own heading, so the two directions of travel occupy two lanes and pass each other.
  The offset moves the position **everything outside sees** — rendering, witness checks, congestion
  queries — so it is a real position, not a drawing trick.
* **Yielding.** `Pedestrian::Update` now takes the distance to the nearest pedestrian ahead *in its
  own lane* and slows from 2.0 m to a full stop at 0.7 m: the same shape as `TrafficVehicle`'s
  following distance, at walking scale. A queue forms instead of a pile, and a stopped pedestrian
  keeps facing the way it was going rather than turning into a huddle.

A **fleeing** pedestrian ignores clearance on purpose — someone running from a car does not queue —
and that is asserted, not left implicit.

The lane test moved out of `IronGangGame.cpp`, where it was a private helper with the traffic lane
width baked into it, into a shared `DistanceAheadInLane` (`include/IronGang/Gameplay/LaneClearance.hpp`)
that traffic and pedestrians both call with their own half-width: 2.0 m for cars, 0.55 m for people,
which is exactly what lets two pedestrians pass shoulder to shoulder without braking each other.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; two new core tests.
`TestLaneClearanceSeesOnlyWhatIsAhead` covers ahead/behind/co-located/inside/outside the lane, that
height is ignored, and that the same offset blocks in a traffic lane but not a walking lane.
`TestPedestriansDoNotWalkThroughEachOther` walks two pedestrians head-on for 400 frames and asserts
their closest approach stays above 0.5 m, that their lane offsets land on opposite sides of the
centreline, that a follower behind a stopped pedestrian ends up between 0.2 m and 2.5 m behind it
(stopped short, but actually queued up rather than halting at a distance), that it resumes once the
way clears — a yield that deadlocks would be worse than the clipping it replaced — and that a
fleeing pedestrian moves with zero clearance.

A real `--profile-scenario mixed --smoke 400` run still reports 12 pedestrians and 4 vehicles.

**Cost, stated rather than hidden:** the congestion scan is O(n²) — 144 checks per ambient update at
12 pedestrians — and `ai_cpu` maximum moved from 0.022 ms to 0.094 ms (p95 unchanged at 0.002 ms).
That is 350× under a 33 ms frame, so it is affordable now, but it is deliberately **not** a profiler
counter yet: adding one revises the performance report's schema and its comparator contract, which
is more than this scan is worth today. A streamed population (`IG-20-008`) will need both a counter
and a spatial index.

**Boundaries.** Pedestrians still do not avoid traffic or each other laterally (they yield in a
line, they do not step around); `IG-20-004` remains partial. They are still colored boxes with no
walk animation (`IG-20-003`).

## A deterministic RNG, and a city with people in it (2026-08-26)

plan_04 `IG-04-011`/`012` closed; plan_20 `IG-20-001` and plan_21 `IG-21-001` closed — both P0
gameplay tasks that had been sitting at "only 2 spawned, not 10-20" and "only 2 spawned, not 3-5"
since gate M9.

**The RNG.** `RandomSource` (`include/IronGang/Core/RandomSource.hpp`, `src/Core/RandomSource.cpp`)
is splitmix64, hand-written rather than `<random>` **on purpose**: `std::mt19937` is specified
exactly, but `std::uniform_int_distribution` and `std::uniform_real_distribution` are not, so the
same seed produces different numbers on different standard libraries. "Seed 42" in a bug report, a
save, or a profiling run has to mean the same thing everywhere, so the range mapping is written out
here. `NextIndex` rejection-samples instead of taking a modulo — that bias is what shows up as the
same option being picked too often. `Derive(label)` gives an independent stream **without consuming
from the parent**, so adding a caller cannot shift an existing system's sequence out from under it.

**The city.** `WarehouseBlock` now spawns 12 pedestrians (six per sidewalk path, spread along it by
`Pedestrian::Reset`'s new start offset rather than stacking on its endpoint, speeds 1.1–2.0, both
directions of travel represented) and 4 traffic vehicles (one per corner of the oval, cruise speeds
5–7). The varied traffic speeds matter beyond appearance: identical speeds hold their spacing
forever, so the following-distance braking that already exists would never actually fire.

The whole population derives from a seed mixed with the district id, so a district repopulates
**identically** every time — a retry, a load, and a profiling run all see the same city. That is not
a detail: without it the performance scenarios would stop being comparable to each other.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; two new core tests.
`TestRandomSourceIsDeterministicAndUniform` replays four golden draws (so changing the generator is
a deliberate act, not a silent one), checks that different seeds diverge, that `Derive` neither
consumes from its parent nor collides across labels while reproducing for the same label, that every
range holds across 2000 draws including the degenerate ones (bound 0 and 1, empty and reversed
ranges), and smoke-tests for gross bias with 8000 draws over 8 buckets.
`TestPedestrianSpawnOffsetSpreadsAlongPath` checks that a zero offset preserves gate M9's exact
behaviour, that six offsets give six positions no two of which are within a metre, that an offset
past the segment clamps rather than overshooting, and — the one that matters for how it looks — that
a pedestrian spawned mid-segment keeps walking the way it was headed instead of turning round on its
first step, from both endpoints.

A real `--profile-scenario mixed --smoke 400` run reports exactly 12 pedestrians and 4 traffic
vehicles, with `ai_cpu` average 0.001 ms, p95 0.002 ms, maximum 0.022 ms, at most 16 traffic
obstacle checks, 12 pedestrian threat checks, and 16 police witness checks per update, and
`update_cpu` p95 0.429 ms — the six-fold population increase is far below anything the budgets can
see.

**Consequence recorded rather than glossed over:** every M12 capture in
`docs/performance-baseline.md` was taken with 2 pedestrians and 2 vehicles. A comparison that crosses
this change is comparing two different workloads, including the comparator's `NO REGRESSION`
verdict, so a dated note now sits at the top of that file saying a fresh baseline pair is needed
before those numbers can be treated as current.

**Boundaries.** Pedestrians are still colored boxes with no skinned model or walk animation
(`IG-20-003`), they do not avoid each other, and there is no spawn/despawn by attention — 12 is a
fixed population, not a streamed one. Traffic still has no signals, turning, or lane graph. The
seed is a compile-time constant rather than a `game.json` tunable.

## A simulation clock between CNA and the world (2026-08-25)

plan_04 `IG-04-003`/`004`/`005`/`007` closed.

**What changed.** `IronGangGame::Update` took `GameTime`'s raw elapsed value and fed it straight to
physics, movement, AI, the mission, and the autosave scheduler. `SimulationClock`
(`include/IronGang/Core/SimulationClock.hpp`, `src/Core/SimulationClock.cpp`) now sits in between
and does exactly two things:

* **Clamps an extreme delta** to 100 ms. A stall then costs smoothness — the world runs slower than
  wall time — instead of costing correctness: the player teleporting through a wall, the sedan
  skipping its trigger, a mission condition that was only briefly true being missed. The refused
  wall time is accumulated in `GetDroppedSeconds()` rather than silently vanishing, and the game
  logs the first clamp **once**, not on every frame after it.
* **Stays monotonic**: elapsed simulation time is the sum of the deltas actually taken, so a
  negative, NaN, or infinite delta from a broken or wrapped platform timer advances the world by 0
  rather than backwards — while still counting the frame as having happened.

It deliberately clamps rather than subdividing, because CNA already drives `Update()` on a fixed
step and subdividing here would step the world twice per engine step.

**Stated honestly, because reading the CNA source changed the claim:** `Game.cpp`'s fixed-step loop
hands `Update()` a constant `TargetElapsedTime` (16.67 ms) and catches up by calling `Update()`
repeatedly, so **this clamp never fires in the current configuration**. It is a guard, not an active
correction — and it becomes load-bearing the moment the game runs variable-step, where CNA's own cap
is `Game::MaxElapsedTime` = 500 ms, five times more than this game's movement and physics can absorb
in a single step. The plan entry and the header both say so rather than implying a bug was fixed.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**;
`TestSimulationClockClampsStallsAndStaysMonotonic` covers an ordinary delta passing through, a 2.5 s
stall clamped with the refused time accounted for, elapsed time being the sum of what ran,
negative/NaN/infinite/zero deltas each yielding 0 and leaving elapsed time untouched while counting
the frame, a 200-frame mixed run asserting monotonicity and step bounds every frame, a configurable
maximum, a zero/negative/NaN maximum ignored rather than freezing time, and `Reset()` clearing the
totals while keeping the configured maximum. `scripts/check-syntax.sh` and `git diff --check` clean;
a `--smoke 120` run logs no clamp warning, which is the expected result given the fixed step above.
The fixed/variable-step split is now documented in `docs/architecture.md`.

**Boundaries.** No interpolation between fixed steps for rendering, so the picture is only as smooth
as the step; no pause/time-scale (a cutscene still runs the world at 1×); the clamp threshold is a
compile-time constant rather than a `game.json` tunable, since nothing has needed to change it.

## Structured logging replaces 24 ad-hoc stderr lines (2026-08-25)

plan_04 `IG-04-002`/`017` closed; `IG-04-008` advanced. Every previous entry in this session added
another hand-written `std::cerr << "[IronGang] …"` line; there were 24, with no severity, no
category, and no way to turn any of them down.

**What changed.** `IronGang::Log` (`include/IronGang/Core/Log.hpp`, `src/Core/Log.cpp`): four
ordered severities, eight categories, `[IronGang][<category>][<severity>] <message>` on stderr.
Every call site was migrated — which is also what turned several of them from an undifferentiated
print into the **error** or **warning** they always were (a mission condition that cannot be
evaluated, a failed autosave, a fact that could not be published). Gameplay text the prototype
prints for the player (dialogue) stays on stdout and is not log output.

Design points worth keeping:

* **A disabled category is silent at every severity, errors included.** Turning a category off means
  "I do not want to hear from this subsystem"; a half-off category would be more confusing than
  either state.
* **The sink is called outside the state mutex.** It is caller-supplied, may be slow, and must never
  be able to deadlock the game by logging from inside itself. The mutex is there so the planned
  background loader thread (`IG-04-014`) can log safely.
* **The level comes from data**: `game.json`'s `logSeverity`, overridden for a single run by
  `--log-level`, which wins because someone passing it is debugging *this* run. An unrecognized name
  is rejected at startup with the valid list rather than silently falling back.
* `scripts/release_archive.py`'s packaged smoke check now matches the **message text only**. Pinning
  "the packaged build loads these assets" is the claim worth keeping; pinning the log prefix was an
  accident of how the check was first written.

**Verification (no display).** Strict-warning build clean; **CTest 11/11** (including the release
archive test, whose expectations changed); `TestLogSeverityAndCategoryFiltering` covers severity
filtering in both directions, a disabled category swallowing an error, `IsEnabled` agreeing with
what `Write` actually does, the exact formatted line, name/parse round-trips for every severity and
category, unknown names rejected, and `Reset` restoring the defaults so one test cannot leave the
next deaf. Real runs confirm the end-to-end path: a default `--smoke 60` prints the six
`[IronGang][assets|audio][info] Loaded …` lines, `--log-level error` prints **none** of them, and
`--log-level nonsense` exits with `--log-level must be debug, info, warning, or error`. The config
test now also covers `logSeverity` round-tripping and an unrecognized name keeping the default.

**Boundaries.** No timestamps, no log file, no rotation, no in-game console, and no per-category
configuration key — `Log::SetCategoryEnabled` exists for code and tests, and a key can be added when
someone actually needs to silence a subsystem from data. Command-line parsing is still a hand-written
if/else chain with no shared option table (`IG-04-008`).

## Configuration loader: game.json is finally read (2026-08-25)

plan_04 `IG-04-001`/`006`/`018` closed; plan_29 `IG-29-005` advanced. `assets/config/game.json` has
existed since the original scaffold with a `notes` field admitting it was "reserved for a future
configuration loader" — nothing had ever read it, and the previous two entries both had to record
"the autosave interval is a compile-time constant because nothing reads the config yet".

**What changed.** `GameConfig`/`LoadGameConfig` (`include/IronGang/Core/GameConfig.hpp`,
`src/Core/GameConfig.cpp`) read the file through sharp-runtime's `System::Text::Json`. The design
rule is that **a broken or partial configuration costs the tuning, never the run**:

* Every member's initializer *is* the default, so there is one place a default lives.
* A missing file, an unknown key, a wrong-typed value, an out-of-range number, and an empty string
  are all warnings that keep the default. Only malformed JSON or a non-object root fails, and a
  failure leaves the caller's configuration untouched rather than half-applied.
* Unknown keys are **named** in the warning. Silently ignoring `projectNmae` is how a mistuned
  build goes unnoticed for a week.
* Negative seconds clamp to 0 rather than being rejected: the author meant "off".
* A minimum spacing longer than the interval loads with a warning — legal, but it means the
  interval never fires when it says it will.

The values drive the window title (`projectName`, replacing a hardcoded "Iron Gang" — which also
serves `docs/renaming.md`), the district map header (`cityName`, `prototypeYear`), and the autosave
scheduler's interval and spacing. The autosave defaults come from `AutosaveScheduler`'s own
constants rather than being repeated in `GameConfig`, so the two cannot drift.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**;
`TestGameConfigLoadsValidatesAndFallsBack` covers the missing file, a full round trip, an unknown
key (with the correct keys still applying), three wrong-typed values, three out-of-range values, the
spacing/interval warning, malformed JSON and a non-object root leaving the caller's config
untouched, and — the case that keeps the shipped file honest — the **committed**
`assets/config/game.json` loading with **zero** warnings and the expected identity. A `--smoke 120`
run prints no configuration warnings, confirming the real file is read at startup rather than only
in tests. `scripts/check-syntax.sh` and `git diff --check` clean; the config file's registry hash
updated. Schema, failure table, and the procedure for adding a tunable are in
`docs/configuration.md`.

**Boundaries.** No environment-variable or command-line overrides of individual tunables
(`IG-04-020`), no hot reload, no per-platform files. This is developer tuning only: **user**
settings the player changes in game still have no system (plan_28's menus) and therefore nowhere to
be stored, which is the open half of `IG-29-005`.

## Autosave scheduling and refusing to save at unsafe moments (2026-08-25)

plan_29 `IG-29-010`/`011`/`036`/`037` closed. Closes the "a checkpoint is only as durable as the
player's last manual save" gap the entry below left.

**What changed.**

* `AutosaveScheduler` and `SaveBlockReason` (`include/IronGang/Persistence/AutosavePolicy.hpp`,
  `src/Persistence/AutosavePolicy.cpp`) decide *when* a save may happen; the game decides what to
  write. Triggers: a new mission checkpoint, a finished district transition, and a 180-second
  interval as the backstop between them.
* **A request at an unsafe moment is held, not dropped.** An autosave asked for during a cutscene
  fires the instant the cutscene ends. **Triggers that land together produce one save**: a
  20-second minimum spacing collapses them, deferring rather than discarding the second request.
* Saving is refused while a cutscene is playing (the camera is not the gameplay camera), during
  dialogue (the line index is not saved), during a district transition (the world being written is
  the one being unloaded), and while getting in or out of the car (the player is neither on foot
  nor driving). F5 there says `Can't save: <reason>` instead of writing a save that would come back
  wrong or silently doing nothing.
* Autosaves use their own slot (`runtime/iron_gang_prototype.autosave`), so they never overwrite a
  save the player made by hand. F9 loads whichever slot is newer via the new
  `SaveGame::ChooseMostRecent` — "load" means "resume" — and the status line says which it was.
* `IronGangGame` gained `CaptureWorldState()`/`CaptureSnapshot()`, so the manual save, the autosave,
  and the checkpoint world all record state through one path rather than three copies.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; two new core tests.
`TestAutosaveSchedulingAvoidsUnsafeMoments` covers the interval not firing early, a request held
across 60 blocked frames and firing the instant it is safe, minimum spacing collapsing two triggers
while deferring the second, trigger priority deciding only the reported label, a repeated request
served once rather than queued, `Reset()` abandoning a pending request, a blocked interval, a zero
interval disabling periodic autosaves without disabling event ones, and the full block-reason
precedence with player-facing text asserted non-empty for every reason and trigger.
`TestLoadChoosesTheMostRecentSave` covers no saves, one save, a newer autosave, a newer manual save,
and a missing candidate — setting timestamps explicitly rather than trusting filesystem resolution.

An end-to-end run confirms the write path: `--profile-scenario mixed --smoke 1600` (the game runs a
fixed 60 Hz step, so that is ~26.7 s of simulated time) exited 0 having written **two** autosaves —
`runtime/iron_gang_prototype.autosave` plus `…autosave.bak`, both 919 bytes with a current-format
header, four minutes apart in wall-clock terms. That is the deferred `DistrictArrival` trigger and
then a later one, and it incidentally exercises backup rotation on the autosave slot as well as the
manual one. No `autosave failed` line appears in the run log. Note the `--profile-scenario mission`
run does **not** produce one: it completes and exits at about 15 s of simulated time, inside the
minimum spacing, which is the scheduler behaving as designed rather than a failure.

**Boundaries.** The 180-second interval and 20-second spacing are compile-time constants;
`assets/config/game.json` is still not read by anything (plan_04's configuration loader). One
autosave slot, one backup generation each — no rotating history, profiles, or slots
(`IG-29-006`/`032`). Autosaving is not asynchronous (`IG-29-012`), so the write happens on the game
thread; at this save size that is microseconds, but it is not free.

## Checkpoints record the world they were reached in (2026-08-25)

plan_29 `IG-29-009`/`029`/`030`/`031` closed; `IG-29-007` advanced. Closes the gap the entry below
left: a checkpoint carried the mission's state and variables, but not where the player and vehicle
stood, so `RetryMission()` after loading a save fell back to a full restart.

**What changed.**

* `SaveSnapshot` now **derives from** a new `WorldStateSnapshot` (player position/yaw, vehicle
  position/yaw/speed, driving flag, district). The live world and a checkpoint's world are one type
  rather than two declarations of the same seven fields, every existing `snapshot.playerPosition`
  still compiles, and `WorldStateSnapshot world = snapshot;` takes exactly the world half.
* `SaveSnapshot::missionCheckpointWorld` is an `optional<WorldStateSnapshot>`, written as additive
  `checkpoint_*` keys — **no format-version bump**, which is precisely what the previous entry's
  versioning work made safe. It is all-or-nothing on read: a partial block is dropped rather than
  half-applied, because putting the player somewhere and the vehicle nowhere is worse than a
  restart.
* `IronGangGame` keeps the world half in an `optional`, saves and restores it, and `RetryMission()`
  now works straight after a load. `LoadPrototype()` and `RetryMission()` share one new
  `ApplyWorldSnapshot()` instead of two copies of the same restore sequence — which also means the
  cutscene-skip and traffic-respawn safeguards can no longer drift apart between the two paths.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; new
`TestCheckpointWorldSurvivesSaveLoad` covers the round trip (positions, yaws, speed, driving flag,
and a checkpoint district deliberately different from the live one, to prove the two do not bleed
into each other), a save with no checkpoint world loading with none, and a partial world half being
dropped while the mission half stays intact. `TestPrologueFailsAndRetriesUnderPoliceChase` remains
the scripted-failure integration test for the mission half. `scripts/check-syntax.sh` clean,
`git diff --check` clean, `--smoke 200` exits 0 with no errors. Checkpoint placement conventions for
mission authors are documented in `docs/mission-scripting.md`; the new save keys in
`docs/save-format.md`.

**Boundaries.** Wanted state is still deliberately not saved, so a load (or a retry) starts with the
police clear — a design choice from plan_22, not an oversight. Ambient traffic and pedestrians are
respawned rather than restored. Autosave, profiles, and slots remain untouched, so a checkpoint is
still only as durable as the player's last manual save.

## Save format integrity: versioning, atomic write, backup, checksum (2026-08-25)

plan_29 `IG-29-001`/`002`/`003`/`004`/`022`/`023`/`025` closed; `IG-29-015`/`016`/`024` advanced.
Done **before** extending the save further (the checkpoint world half the entry below flagged),
because adding fields to a format with no version, no atomicity, and no integrity check compounds
the problem rather than closing it.

**What changed.** `SaveGame` was a single `WriteAllText` of a `format=iron-gang-save-v1` document
whose reader `at()`-ed required keys — a torn write, a truncated file, or a missing field lost the
save or surfaced as `map::at`. Now:

* **Format version 2**, with `kMinSaveFormatVersion`/`kCurrentSaveFormatVersion` and a parsed,
  range-checked `format=iron-gang-save-v<N>` first line. A newer version is refused by name rather
  than half-read; version 1 still loads and is converted on read (`mission_state`'s int index →
  `mission_state_id` through the exact table the deleted enum had), with
  `SaveReadDiagnostics::formatVersion` reporting which version the file was.
* **Atomic write.** The document goes to `<path>.tmp`, the existing save rotates to `<path>.bak`,
  then the temporary is renamed into place — two renames within one directory. A failure at any
  point leaves the previous save intact where it was or as the backup, never a half-written file at
  the save's own path. A leftover temporary from an interrupted run is ignored and replaced.
* **One rolling backup**, with automatic fallback: if the primary file is missing, corrupt, or
  unsupported, `Read()` uses `<path>.bak` and reports it through `SaveReadDiagnostics::usedBackup`
  and `primaryError`. `IronGangGame::LoadPrototype` logs the reason and shows "Loaded backup save" —
  recovering silently would hide that the player lost the progress between the two.
* **Checksum.** Line two is `checksum=<FNV-1a 64-bit hex>` over every byte after it. Documented as
  damage detection, not tamper protection. Missing required fields are now reported by name.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; new
`TestSaveFormatRobustness` covers the round trip, no surviving temporary file, backup rotation and
contents, a flipped byte falling back to the backup with the reason reported, truncation with a
damaged backup failing cleanly, a future version refused, a version-1 file migrating, a missing
field named, a leftover garbage temporary being replaced, and — standing in for a full disk — a
write into a read-only directory that must fail and leave the existing save byte-for-byte intact
(asserted only when the write really failed, so the case is skipped rather than falsely passing
under root). `TestMissionVariablesSurviveSaveLoad`'s malformed-variable case was rewritten as a
version-1 document, because appending junk to a version-2 save is now correctly rejected as
corrupt — the old test was only passing because nothing checked integrity. `scripts/check-syntax.sh`
clean, `git diff --check` clean, `--smoke 300` exits 0. Schema and rules documented in
`docs/save-format.md`, linked from `README.md`.

**Boundaries.** No migration registry (`IG-29-026`) — with two versions the conversion is one branch,
and `docs/save-format.md` says when to add it. No profiles/slots, autosave, settings separation,
thumbnails, or CLI inspector. The world half of a mission checkpoint is still not saved, so a retry
straight after a load restarts the mission; that is the next task (`IG-29-009`/`029`).

## Branching missions, wanted-state facts, and a real failure/retry loop (2026-08-25)

plan_24 `IG-24-043` closed; `IG-24-006`/`007`/`024` advanced; plan_22 `IG-22-007` advanced. This is
the integration scenario the previous entry deliberately left open: **nothing in the running game
could fail**, so failure and retry had unit tests only.

**What changed.**

* **Branching.** A state may declare `"transitions": [{ "when": …, "next": … }, …]`, evaluated in
  file order every frame with the first matching condition winning. `when`+`next` is now the
  one-entry shorthand for the same list, and mixing the two spellings is a load error. At most 8
  per state. `MissionStateDefinition::condition`/`next` were replaced by that list, so the runtime
  has exactly one notion of where a state goes.
* **Wanted-state facts.** `PoliceSystem::GetChaseSeconds()` is new; `police_alerted`,
  `police_chasing`, and `police_chase_seconds` are declared facts. They are *pushed in* by the game
  through the new `PrototypeMission::SetFact()` rather than derived from `Update()`'s arguments,
  because `PoliceSystem` belongs to the game — that is the extension point later subsystems use.
* **The committed prologue can now fail.** `drive_to_warehouse` is a checkpoint and branches:
  `police_chase_seconds > 25` → `busted` (`"outcome": "failed"` with its own reason), otherwise the
  delivery completes. The HUD shows `Mission failed: … | R: retry`.
* **In-game retry.** `R` retries a failed mission (and still resets the whole prototype otherwise).
  `IronGangGame::RetryMission()` restores the mission half from `PrototypeMission::Retry()` and the
  world half — player, vehicle, district — from a snapshot `CaptureMissionCheckpointWorld()` takes
  the moment the mission records a new checkpoint, and resets the police response, without which the
  chase that failed the mission would fail it again within a frame.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; two new core tests.
`TestMissionBranchesOnFirstMatchingTransition` covers order-dependent branching, the
first-match-wins rule when both conditions hold, and `SetFact` rejecting a wrong type or an
undeclared fact. `TestPrologueFailsAndRetriesUnderPoliceChase` is the integration scenario: it
drives the **committed** `assets/missions/prologue.mission.json` against the **real** `PoliceSystem`
frame by frame — speed past a witness, get chased, fail on the mission's own branch with its own
reason, retry back to the checkpoint with `cargo_secured` still set, confirm a cleared chase does
**not** immediately re-fail the mission, and complete the delivery afterwards counting exactly one.
Seven new rejection cases cover the transition spellings. A `--profile-scenario mission` run through
the real game loop logs `checkpoint "drive_to_warehouse"` and still completes normally when no chase
occurs — the new failure branch does not disturb the deterministic performance workload.
`scripts/check-syntax.sh` and `git diff --check` clean; the prologue's registry hash updated again.

**Boundaries.** The save carries the mission half of a checkpoint but not the world half, so a retry
straight after loading a save falls back to a full restart (persisting the world half belongs with
plan_29's checkpoint work). Branching is on world state only — no player-chosen branch exists
(`IG-24-024`'s remaining half). Surrender and arrest are still missing from `PoliceSystem`
(`IG-22-007`).

## Mission failure reasons, checkpoints, and retry policies (2026-08-25)

plan_24 `IG-24-009`/`010`/`041`/`042`/`044`/`045` closed; `IG-24-002`/`019`/`029` extended;
`IG-24-043` left open on purpose (see Boundaries). Builds directly on the outcome states added in
the entry below, which could mark a mission failed but gave it nowhere to go afterwards.

**What changed.**

* A failing state explains itself: `"reason": "The police took the shipment"` alongside
  `"outcome": "failed"`. `PrototypeMission::GetFailureReason()` returns it, and both the on-screen
  HUD and the window title show `Mission failed: <reason>` in place of the objective line (one
  shared `MissionStatusLine` helper, so the two paths cannot drift). A reason on a non-failing state
  is a load error — nothing would ever read it.
* A state may declare `"checkpoint": true`. Entering it records its id **and the mission's variables
  as they stand once its entry actions have run**. That ordering is the point: `Retry()` restores
  the recorded values without re-running those actions, so a counter incremented on entry is not
  incremented twice by a retry. A checkpoint state may not also be an outcome (a state that ends the
  mission cannot be retried from).
* `"retry": "checkpoint"` (default) or `"mission_start"` at the mission's top level. `Retry()`
  honours the policy and falls back to a full restart when no checkpoint has been reached yet, so a
  mission declaring no checkpoint behaves identically under either. Retrying does not consume the
  checkpoint — repeated failures return to the same place. `Reset()` remains the unconditional
  restart and discards the checkpoint.
* The checkpoint is persisted: `mission_checkpoint_state_id` plus one
  `mission_checkpoint_var.<name>=<type>:<value>` line per recorded variable. Restoration is
  fail-safe — a checkpoint naming a state the loaded mission no longer defines is dropped entirely
  (a retry that went nowhere would be worse than a restart), and a variable whose declaration or
  type changed is dropped individually; both are reported through the same warning path the game
  already prints for mission variables.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; two new core tests —
`TestMissionCheckpointRetryAndFailureReason` (reach a checkpoint, fail, retry, and land back on the
checkpoint with the recorded counter rather than the declared one or a doubly-incremented one; the
same mission under `mission_start` restarts; a checkpoint retry with no checkpoint yet restarts;
a retry does not consume the checkpoint) and `TestMissionCheckpointSurvivesSaveLoad` (round trip,
undefined-state checkpoint dropped with a warning, type-changed checkpoint variable dropped with a
warning, checkpoint-free save loads as having none) — plus seven new rejection cases in
`TestMissionValidationRejectsMalformedData` (unknown retry policy, retry/checkpoint/reason in a
version-1 file, reason without a failed outcome, checkpoint that is also an outcome, non-boolean
checkpoint). `--profile-scenario mission` still completes the whole mission through the real game
loop; `scripts/check-syntax.sh` clean; `git diff --check` clean.

**Boundaries.** `IG-24-043`'s integration scenario is deliberately still open: **nothing in the
running game can fail yet**, so no `--smoke`/`--profile-scenario` run exercises failure or retry —
only the unit tests do. The obvious way to close it is a real fail condition driven by the existing
`PoliceSystem` chase state, which would also close part of `IG-24-006`'s wanted-state conditions.
The prologue mission is unchanged by this entry: it declares no checkpoint and cannot fail.

## Free-form mission state ids, outcomes, and save migration (2026-08-25)

plan_24 `IG-24-018` closed; `IG-24-002`/`009`/`019`/`029` advanced. Follows directly from the
expression-evaluator entry below, which left `PrototypeMission` still restricting a mission file to
the five state ids the int-based save format encoded — so no mission with a new state could be
authored. That restriction is gone.

**What changed.**

* `PrototypeMissionState` (the fixed `Introduction`…`Completed` enum) is **deleted**. The mission's
  authoritative state is now `std::string stateId_`; the API is `GetStateId()`, `IsInState(id)`, and
  `SetStateId(id)` — the last of which returns false, leaving the mission untouched, for an id the
  loaded mission does not define, rather than stranding it in a state with no objective, condition,
  or way out.
* A state may declare `"outcome": "completed"` or `"failed"` (schema version 2). `IsCompleted()`,
  `IsFailed()`, and `IsFinished()` read that — no engine code keys off a particular state id any
  more, and a mission can now *fail*, which is the groundwork `IG-24-009`/`010` needed. An outcome
  state must be terminal, and a mission with no state that can end it is a load error. A file that
  declares no outcome at all falls back to the pre-`outcome` rule (a terminal state named
  `completed` counts as success), so every version-1 file still loads.
* The save writes `mission_state_id=<id>`. A save in the earlier format (a 0-4 index into the
  deleted enum) is migrated on read through that enum's exact mapping; an out-of-range index, or a
  save with neither field, is rejected rather than silently clamped to state 0. This is `IG-24-018`'s
  persisted-state half.
* `IronGangGame`'s one remaining enum comparison became `mission_.IsInState("enter_vehicle")` (the
  deterministic mission profiling scenario), and its load path reports a save state the loaded
  mission does not define.

**Verification (no display).** Strict-warning build clean; **CTest 11/11**; two new core tests —
`TestMissionStateIdsAreNotAFixedSet` (a mission whose states are `briefing`/`stakeout`/`escaped`/
`caught` loads, transitions, reports `completed` then `failed` outcomes, refuses an undefined state
id, and round-trips its free-form id through the save) and `TestSaveMigratesLegacyMissionState`
(legacy index 3 → `drive_to_warehouse`, index 0 → `introduction`, out-of-range rejected, no-state
rejected, `mission_state_id` wins over a legacy index) — plus four new rejection cases in
`TestMissionValidationRejectsMalformedData` (unknown outcome, outcome with a `next`, outcome in a
version-1 file, a mission nothing can end). The `--profile-scenario mission` run still completes the
whole mission through the real game loop. `scripts/check-syntax.sh` clean; asset-registry notice
verified after the prologue mission's hash changed again (it now declares `"outcome": "completed"`
explicitly rather than relying on the compatibility rule).

**Boundaries.** A failure *reason*, a retry policy, and checkpoints distinct from a plain save are
still open (`IG-24-009`/`010`/`041`-`045`), and nothing in the prologue mission can fail yet, so the
failure path is exercised by tests rather than by gameplay.

## Mission variables and the condition/action expression evaluator (2026-08-25)

plan_24 `IG-24-013`/`014`/`016`/`029`/`030`/`031`/`032`/`033`/`035` closed; `IG-24-002`/`005`/`006`/
`007`/`018`/`019`/`034` advanced to partial with itemized remainders. Nothing about gates M12 or M14
changed.

**What was added.** Mission conditions are no longer one fixed engine signal named by string. Four
new translation units under `src/Missions/` (`MissionValue`, `MissionContext`, `MissionExpression`,
plus the rewritten `MissionDefinition`) give a mission file typed variables and a real, engine-
evaluated expression language:

* `MissionValue` — a four-type value (`bool`/`int`/`float`/`string`) whose `ToText()`/`Parse()`
  round-trip exactly, which is what the plain-text save file relies on (float uses the shortest
  round-trip form via `std::to_chars`).
* `MissionContext` — one symbol table holding read-only engine *facts* and mission-owned
  *variables*, each declared with an initial value that fixes its type. A wrong-typed assignment,
  a duplicate/shadowing declaration, writing a fact from a mission, or exceeding the 64-variable
  cap are all rejected rather than coerced.
* `MissionExpression` — tokenizer, recursive-descent parser, **static type check**, and evaluator
  over a flat node array. `|| && ! == != < <= > >= + - * /`, unary minus, parentheses, literals
  (strings single-quoted so they need no escaping inside JSON), and identifiers bound to declared
  symbols. Chained comparison is rejected; `&&`/`||` short-circuit; int/float mixing promotes to
  float; division by zero is an evaluation error, not an infinity. Limits: 512 source characters,
  128 tokens, 96 operations, depth 16, 256 evaluation steps — recursion is impossible because the
  grammar has no calls.
* `MissionDefinition` — schema version 2 (`variables`, `when`, `onEnter`), with version 1 still
  loading unchanged because every gate-M7 condition name is still a declared bool fact. Entry
  actions are `set` (type-checked assignment) and `log`.
* `PrototypeMission` — declares the seven prototype facts (`dialogue_finished`, `player_driving`,
  `player_vehicle_distance`, `player_near_vehicle`, `player_in_warehouse_goal`,
  `vehicle_in_warehouse_goal`, `player_driving_in_warehouse_goal`), refreshes them every `Update()`,
  runs entry actions exactly once per entry, and logs every transition with the condition that fired
  it. `Reset()` is a retry (restores declared values, re-runs the initial state's actions);
  `SetState()` is a save restore (does not).
* `SaveGame` — additive `mission_var.<name>=<type>:<value>` lines, one per variable, so a string
  value may contain any character but a newline. A saved variable the mission no longer declares, a
  changed type, or a malformed line is skipped and reported instead of failing the load.

The committed `assets/missions/prologue.mission.json` moved to version 2 and now declares four
variables and uses expressions: `player_vehicle_distance <= handover_radius` puts the handover
threshold in the mission file instead of the engine (it was a hardcoded `9.0F` squared-distance
literal), and `player_driving && vehicle_in_warehouse_goal && cargo_secured` replaces the fused
`player_driving_in_warehouse_goal` signal. Its registry hash in
`assets/licenses/asset-registry.csv` was updated to `388fa781…8b60`.

**Verification (all no-display, offscreen SDL).**

* `cmake --build --preset compile-software` — clean, no warnings under the project's strict flags.
* `ctest --preset compile-software` — **11/11 passed**, including `iron_gang_asset_registry_tests`
  after the mission-file hash update (it failed first, exactly as designed, on the stale hash).
* `iron_gang_core_tests` — 5 new cases pass alongside the existing 27:
  `TestMissionExpressionEvaluatesTypedOperations` (operators, precedence, promotion, string
  comparison, short-circuiting, live re-evaluation, `ResetVariables`),
  `TestMissionExpressionRejectsMalformedInput` (20 malformed sources plus the length/depth/token
  limits, divide-by-zero at evaluation, `EvaluateBool` refusing a non-bool, and the empty-expression
  case), `TestMissionVariablesEnforceTypes` (declaration/assignment/capture rules and
  `ToText()`/`Parse()` round trips for all four types), `TestMissionEntryActionsRunOncePerEntry`
  (actions run once per entry, not per frame; a condition reading a mission variable fires; `Reset`
  behaves as a retry), and `TestMissionVariablesSurviveSaveLoad` (save round trip, order preserved,
  unknown-name and changed-type warnings, malformed-line tolerance).
  `TestMissionValidationRejectsMalformedData` grew from 6 to 17 rejection cases.
* `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./cmake-build-compile-software/iron_gang --smoke
  4000 --profile … --profile-scenario mission` — the mission completes through the **real game
  loop** on the new evaluator, logging all four transitions and the completion action:
  `introduction -> reach_vehicle (dialogue_finished)`,
  `reach_vehicle -> enter_vehicle (player_vehicle_distance <= handover_radius)`,
  `enter_vehicle -> drive_to_warehouse (player_driving)`,
  `drive_to_warehouse -> completed (player_driving && vehicle_in_warehouse_goal && cargo_secured)`,
  `delivery complete`. A plain `--smoke 600` run shows no fallback message, confirming the
  version-2 file loads rather than silently falling back to the built-in mission.
* `scripts/build-web.sh`'s configure/build (Emscripten 6.0.3, WEBGL2) — links
  `web/build/iron_gang.{js,wasm,data}` with only a pre-existing CNA warning, confirming that the
  new `std::to_chars`/`std::from_chars` float paths compile and link under Emscripten's libc++ as
  well as the host toolchain. It needed both `IRON_GANG_CNA_DIR` and `IRON_GANG_SHARP_RUNTIME_DIR`
  passed explicitly, because the checkout has moved a directory deeper than the preset defaults
  assume — see `NEXT.md`.
* `scripts/check-syntax.sh` — clean over every `.cpp`.
* `scripts/asset_registry.py --check-notice THIRD_PARTY_ASSETS.md` — 15 approved shipping assets,
  4 external, notice verified.

A condition that faults at runtime (a divide by zero, say) faults on every frame, so the failure is
logged once per state entry rather than once per frame; `EnterState`/`SetState` clear the guard.

**Boundaries.** Per-campaign variable scope, failure/retry states, checkpoints distinct from a plain
save, and the remaining `IG-24-006`/`IG-24-007` conditions and actions are still open — see each
entry's own note in `plan/plan_24-mission-framework-and-scripting.md`. `PrototypeMission` still
restricts a mission file to the five state ids its int-based save format encodes, so a mission with
a new state cannot be authored yet. Nothing here was visually verified: this environment has no
display.

## M14 Linux release-archive slice (2026-08-25)

A prior session implemented and did a first verification pass of a Linux EasyGL release
archive, then stopped for an urgent handoff before documentation, a final post-edit
verification pass, and a commit (see the "URGENT HANDOFF" entry that was in `NEXT.md`). This
entry closes that out: documentation was added, and every verification step below was
re-run against the final, documented state of the change.

`CMakeLists.txt` now records the target OS/architecture in the CMake cache, sets
`CNA_ENABLE_VIDEO=OFF` (this game does not use CNA's video playback; `AUTO` was pulling in
four direct FFmpeg shared-library edges from a development machine that happened to have it
installed), disables Jolt's own `install()` rules (which would otherwise ship its static
archive, headers, and CMake package), switches to GNU install directory conventions, and gives
the Linux executable a relative `$ORIGIN/../lib/iron-gang` install RPATH. The install set is
runtime-only (audio/config/cutscene/dialogue/mission data and generated CNJ models, license
notices, and a private copy of CNA's SDL3/SDL3_mixer shared libraries); ten dependency license
texts are installed under `share/iron-gang/licenses/` (two of them, `stb.txt` and
`nlohmann-json.txt`, are new repository-owned copies under `third_party/licenses/`). Windows
DLL install rules exist but are untested. `src/main.cpp` no longer bakes the source checkout's
absolute asset path into the executable; native builds resolve assets relative to the
executable (installed `../share/iron-gang/assets`, then build-tree `../assets`, then `assets`),
Emscripten keeps its virtual `assets` path, and the platform-specific executable-path lookup is
compiled out entirely on Emscripten.

New `scripts/release_archive.py` stages the package via `cmake --install`, validates the exact
Linux Release/OPENGLES3/video-off identity and runtime-only layout, uses `readelf`/`ldd` to
reject FFmpeg, unresolved or wrong-source SDL linkage, absolute workspace RPATHs, and a missing
relative RPATH, runs an offscreen/dummy-audio five-frame smoke expecting every shipped CNJ model
and WAV to load, then creates a sorted/normalized/reproducible `.tar.gz`, safely re-extracts it
(rejecting path traversal), re-validates and re-smokes the extracted copy, and atomically writes
a `.sha256` file. New `scripts/build-release.sh` runs preflight and asset-notice validation,
configures/builds the `release-easygl` preset, and creates the archive, with ccache defaulting
inside the build tree. New `docs/release-packaging.md` documents prerequisites, the build
command, output paths, every validation stage, a failure-mode table, and the Linux-only/
unsigned/single-machine boundary; it is itself installed and required by the archive validator.
New `tests/test_release_archive.py` covers layout policy (valid/missing-license/development-
pollution), deterministic archive creation plus a symlink round trip, path-traversal rejection,
and locked package-identity policy; CMake registers it as the `iron_gang_release_archive_tests`
CTest case.

Full verification, all in this environment with no visible display or physical-window launch
(`SDL_VIDEODRIVER=offscreen`, dummy audio, `DISPLAY`/`WAYLAND_DISPLAY` unset for every smoke
run):

- `python3 -m py_compile scripts/release_archive.py tests/test_release_archive.py` passes.
- The focused suite passes 4/4: `python3 tests/test_release_archive.py scripts/release_archive.py`.
- `bash -n scripts/build-release.sh` and `git diff --check` are both clean.
- The reconfigured `compile-software` preset builds, and its no-display CTest passes 11/11
  (including `iron_gang_release_archive_tests`).
- `./scripts/check-syntax.sh` passes for all 27 tracked translation units.
- `./scripts/build-web.sh` succeeds; the only warning is CNA's pre-existing `Song.hpp`
  `GetHashCode` override warning, unrelated to this change, confirming the new
  Emscripten-only `GetExecutablePath` compile-out guard is warning-free.
- `./scripts/build-release.sh` was run twice end to end (preflight, asset-notice check,
  `release-easygl` configure/build, archive). Both runs produced the byte-identical archive
  `iron-gang-0.1.0-linux-x86_64.tar.gz` (25,312,754 bytes) with SHA-256
  `bfcf9cea58d014f0644bbaddef95bbf5f0a7c4b24ed9e84fad35b08fa52a48d8`. This hash differs from
  the pre-documentation hash recorded during the earlier interrupted session, as that session's
  own note anticipated: `docs/release-packaging.md` is now part of the installed/archived
  notice set, which legitimately changes package contents. The archive's internal validation
  (layout, `readelf`/`ldd` linkage policy, and the offscreen/dummy-audio smoke run) passed
  against both the freshly staged install and the re-extracted archive on every run.

This closes the Linux EasyGL slice of gate M14 (`plan/plan_39-vertical-slice-gates.md`
`IG-39-070`) and the license-notice/archive-builder tasks in `plan/plan_37-platforms-packaging-
release-and-operations.md` (`IG-37-004`, `IG-37-063`-`IG-37-068`). It does not close the whole
of M14 (`IG-39-015`): no external machine has built this from a clean checkout using only the
documented steps, there is no full interactive playthrough of the packaged build, Windows
packaging is untested, and the archive is unsigned and unpublished. It also does not touch M12,
which remains open solely for controlled physical-display qualification.

## M13 asset provenance and notice gate

The current first-district production set is now enforced by `scripts/asset_registry.py`, not a
manual CSV convention. The exact version-1 registry contains 15 approved shipping rows (11 original,
4 external) and now includes the previously omitted runtime `assets/config/game.json`. Each row
binds both content and local license evidence by SHA-256 and records source/acquisition where
external, allow-listed license, modification/attribution/commercial/redistribution/AI review,
reviewer, approval, shipping state, and notes. Recursive coverage includes runtime audio/config/
cutscene/dialogue/mission content, MC3/glTF production sources, and the code-embedded bitmap font;
generated GLB/CNJ files inherit source/converter provenance.

Primary evidence was refreshed from the authors' own sources on 2026-08-24. The font8x8 README and
`font8x8_basic.h` identify the data as Public Domain; the Nox Sound Design product page identifies
all sounds as CC0, allows commercial use without attribution/restrictions, and reports no generative
AI. Review records preserve the observed upstream SHA-256/source facts under
`assets/licenses/evidence/`, and registry rows bind those local files by a second hash.

`THIRD_PARTY_ASSETS.md` is generated atomically and deterministically with per-file source/license/
content hash and evidence/reviewer tables. `THIRD_PARTY.md` links it, CMake installs both notices,
and `build-assets.sh` refuses conversion when the registry or notice is stale. Seven focused tests
cover the real registry, deterministic generation, unknown-file coverage, asset/evidence tamper,
license/rights/approval policy, duplicate/case collision, stale output, and input-overwrite refusal.
The complete virtual-display-free CTest passes 10/10. A clean install to
`/tmp/iron-gang-m13-install-20260824` includes a byte-identical generated notice. This closes M13
asset provenance/notices; actual dependency-license copies and clean runnable packaging remain M14.

## Full CNA-linked build status

A full Iron Gang executable (`iron_gang`) links successfully in this workspace using the `compile-software` preset against `../cnanext` and `../sharp-runtime`. The CNA-vendored SDL/SDL_image/SDL_mixer submodules are populated here; `cna-extended` is no longer required. The `dev-easygl` preset now selects CNA's public `OPENGLES3` renderer name (whose implementation is EasyGL), while `dev-vulkan` selects `VULKAN`. Both compile-software and Release EasyGL are exercised here; Vulkan remains unverified in this environment.

## Reproduction

```bash
./scripts/preflight.sh compile-software
./scripts/check-syntax.sh
MESH_CRAFT_SOURCE_DIR=../mesh-craft ./scripts/validate-mc3.sh
```

After dependencies are complete:

```bash
./scripts/configure.sh dev-easygl
./scripts/build.sh dev-easygl
./scripts/test.sh dev-easygl
./scripts/run.sh dev-easygl --smoke 120
```
