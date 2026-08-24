# Realistic performance and hardware targets

## Why this document exists

An earlier informal estimate for this project was unnecessarily high because it
implicitly assumed something close to a modern open-world production: PBR
everywhere, many dynamic shadows, dozens of fully simulated NPCs and cars,
large-scale streaming, and large textures. **That level of cost is not a
property of CNA, nor a necessary consequence of this game.**

CNA itself can have very low overhead. The real requirements are determined
mainly by the scope and treatment of content, not by the framework.

## A more realistic target for this game

Assuming graphics roughly at the level of the original Mafia, or a lightly
modernized take on it:

### Minimum configuration

- CPU: dual-core, around 2 GHz
- RAM: **2-4 GB**
- GPU: OpenGL 3.3, DirectX 9/11, or Vulkan
- VRAM: **512 MB to 1 GB**
- Disk: approximately **2-8 GB**
- Resolution: 720p
- Performance target: 30 FPS

A game at this scope could run even on older integrated graphics, as long as
effects are not overused.

### Recommended configuration

- CPU: quad-core
- RAM: **4-8 GB**
- GPU: an older discrete card or a modern integrated GPU
- VRAM: **1-2 GB**
- Disk: approximately **5-15 GB**
- Resolution: 1080p
- Performance target: 60 FPS

For example, hardware around the level of:

- a newer-generation Intel HD/UHD iGPU,
- an AMD Vega iGPU,
- a GeForce GT 1030,
- a GTX 750,
- a Radeon RX 550,

should be sufficient for a reasonably built game at this scope.

## What the game itself might actually consume

A reasonable memory budget:

| Area                                      | Approximate RAM |
| ------------------------------------------ | ---------------: |
| CNA, sharp-runtime, and game code           |      100-300 MB |
| Active city sector                          |      200-600 MB |
| CPU-side textures and streaming cache       |      200-700 MB |
| NPCs, vehicles, animation, and physics      |      100-300 MB |
| Audio                                       |       50-250 MB |
| Other headroom                              |      200-500 MB |
| **Total**                                   |   **0.8-2.5 GB** |

So the game itself could plausibly fit within roughly **1-2 GB of RAM**, or up
to 3 GB in larger scenes. A stated requirement of 8 or 16 GB would really be a
recommendation for the whole modern operating system around it, not an actual
need of the game itself.

## VRAM

With reasonable textures:

- low: **256-512 MB**
- medium: **512 MB-1 GB**
- high: **1-2 GB**

This assumes:

- most textures sized 256x256 to 1024x1024;
- 2K textures reserved for important objects only;
- no unnecessary 4K textures;
- texture compression;
- mipmaps;
- shared materials;
- facade atlases;
- loading only nearby sectors.

## CPU

The original Mafia could simulate a city on single-core processors with far
less performance than today's machines. This project may carry more overhead
because it sits on more general-purpose libraries, but that is still not a
reason to require six modern cores.

A dual-core CPU can reasonably handle:

- the player;
- one vehicle simulated in detail;
- a handful of nearby cars;
- dozens of simple pedestrians;
- basic physics;
- missions and dialogue;
- audio;
- city rendering.

Four cores would comfortably allow separating:

1. the main game/render thread;
2. physics;
3. asset streaming;
4. AI, navigation, or other helper work.

## What would actually raise the requirements

High requirements would only appear once everything is pushed to its maximum
at once:

- hundreds of active NPCs;
- dozens of cars with full physics;
- dynamic shadows from every light source;
- large view distance with no LOD;
- 2K-4K textures on every object;
- PBR with many lights;
- SSAO, volumetric fog, SSR, and other heavy post-processing;
- skeletal animation for hundreds of characters at once;
- large-scale destruction;
- loading the entire city at once.

None of this is mandatory.

## Keeping the game genuinely lightweight

### Split the city into sectors

For example, 128x128 or 256x256 meters. Only sectors around the player need
to sit in RAM and VRAM at once.

### Use LOD

For every building, vehicle, and character:

- a detailed model up close;
- a simpler model at medium distance;
- a very simple model or billboard far away.

### Bound the active simulation

For example:

- 10-20 pedestrians with detailed AI;
- other pedestrians with simple movement only;
- 5-10 vehicles with real physics;
- distant cars represented as spline-following objects;
- NPCs outside the player's vicinity represented only as saved state.

### Baked lighting

Instead of fully dynamic lighting:

- static lightmaps for buildings;
- one dynamic sun;
- a handful of important dynamic lights;
- simple ambient lighting;
- limited shadow casting.

### Instancing

Render many of the following with a single draw call each:

- lamps;
- windows;
- trees;
- signs;
- benches;
- fences.

### Restrained textures

A retro-realistic visual style can look good even with small textures.
AI-generated assets will need automatic optimization, since AI can otherwise
produce unnecessarily large meshes and textures without any limit.

## Recommended project targets

For **Iron Shadows**, set in **Iron City**, the recommended targets are:

### Absolute minimum

- 64-bit dual-core CPU
- 2 GB of RAM free for the game
- 512 MB VRAM or shared graphics memory
- OpenGL 3.3 or an equivalent backend
- 720p at 30 FPS

### Recommended

- Quad-core CPU
- 4 GB of RAM free
- 1-2 GB VRAM
- An SSD is not required, but it improves streaming
- 1080p at 60 FPS

### Development machine

Development itself is more demanding than running the game, since it runs
several of the following at the same time:

- CLion;
- the compiler;
- the debugger;
- Mesh Craft;
- model generation;
- converters;
- possibly Blender;
- the CNA game itself.

16-32 GB of RAM makes sense for a development machine, but that is not a
requirement for players.

## Conclusion

**CNA by itself does not cause high requirements.** On the contrary, it
should allow for a very lean native game without the overhead of a large,
general-purpose commercial engine. A previously assumed 8-16 GB of RAM and a
GTX 1060 would correspond to a significantly modernized variant with a large
city and advanced graphics.

For a game visually positioned between **Mafia 1, Mafia: The City of Lost
Heaven, and a lightly modernized indie 3D look**, a more realistic target is:

> **2-4 GB RAM, 512 MB to 1 GB VRAM, and an ordinary dual- to quad-core CPU.**

With disciplined optimization, the game could run on quite old hardware while
reaching hundreds of FPS on modern machines.

## Automated capture budgets

Gate M12 uses p95 rather than an average so a capture cannot hide frequent slow frames behind
fast ones. The first-district budgets enforced by `--profile` are:

| Metric | Minimum gate |
| --- | ---: |
| End-to-end frame interval at 1280x720 | 33.333 ms (30 FPS) |
| Whole game update CPU | 8.0 ms |
| Physics CPU | 3.0 ms |
| Traffic/pedestrian/police AI CPU | 2.0 ms |
| Game-owned audio control CPU | 1.0 ms |
| Render submission CPU | 8.0 ms |
| Synchronous district load + renderer rebuild | 1000 ms |
| Peak resident RAM | 2 GiB |
| VRAM | 512 MiB |

The recommended frame interval is 16.667 ms (60 FPS). The subsystem CPU rows are nested inside
the whole update/render work and therefore are not added together as independent frame costs.
`frame_interval` is the authoritative end-to-end measurement: it includes scheduling, vertical
sync, the preceding buffer presentation, and GPU back-pressure. `render_cpu` measures only command
submission and can be small while `frame_interval` is slow. `present_cpu` separately measures
CNA's `Game::EndDraw()`/`GraphicsDeviceManager::EndDraw()` path, including swap, v-sync waits, and
any rendering work the backend defers until presentation. It is diagnostic rather than a separate
budget because the end-to-end frame interval is still the gate.

On a renderer/driver with a real timer-query implementation, `gpu_render` measures the GPU command
range from the pre-Clear start of `Draw()` through the HUD, excluding Present. The query is polled
asynchronously in a later frame and never waited on; while a result is pending, that single timer
is not reused, so a slow GPU can produce fewer samples without the profiler introducing a stall.
`gpu_timing.supported` and `unsupported_reason` distinguish unavailable timing from zero work. The
current EasyGL/metagl seam returns a 32-bit nanosecond result; its all-ones saturation sentinel is
discarded and counted in `gpu_timing.discarded_samples` instead of contaminating statistics.
`gpu_render` remains diagnostic rather than a separate pass/fail budget: frame interval is still
the user-visible gate and includes GPU back-pressure plus presentation behavior.

JSON schema 2 also records per-frame `render_workload` counts. These are exact for Iron Gang's 3D
front-end submissions: `draw_calls`, explicit EffectPass/buffer/blend `state_change_calls`, declared
`vertices`, triangle primitives, high-level `geometry_instances`, and `visible_objects`. They do
not pretend to be driver counters: Clear, Present, backend state deduplication, and HUD
`SpriteBatch`'s renderer-internal batching are outside the scope. The prototype currently performs
no frustum or occlusion culling, so “visible” means submitted scene objects; every candidate is
counted until a real visibility rejection path exists. These counts have no pass/fail threshold
yet. They establish the baseline needed before draw batching, instancing, or culling work can be
justified and later compared.

Every report also records whether v-sync was requested, whether CNA's fixed timestep was enabled,
and the target frame duration. JSON schema 3's `swap_interval` separately records the 0/1 request
and CNA platform `SetSwapInterval` acknowledgement. `apply_succeeded=true` means the current GL
platform accepted that setting; it is still not proof of a physical vertical-retrace signal or
compositor pacing. A declined or unqueryable path reports `applied:null` plus an explicit reason.
WebGL is always unqueryable here because browser presentation is compositor-controlled.

JSON schema 4 makes district loading a per-transition record instead of one opaque stopwatch.
`district_world_physics_cpu` covers destruction of the old static bodies, construction/activation
of the target procedural world, target static-body creation, and (for an exit-trigger transition)
arrival placement. `district_renderer_upload_cpu` is CPU time to rebuild the target static mesh and
lightmap and issue its resource uploads; it is not a GPU-completion timer. Their sum remains the
budgeted `district_load_cpu`. Application initialization is measured only by `startup_cpu`, not
mislabelled as a district transition.

Each `district_load.samples[]` entry carries its reason, source/target district, target procedural
world-object and static-body counts, Linux current-RSS before/after/signed delta, and tracked
renderer video-memory before/after/signed delta. The video-memory delta has the same logical,
partial semantics as the global tracker and omits the constant HUD/map UI textures. Current districts have
no serialized runtime package: their transition constructs `PrototypeWorld` in memory, so
`io_ms`, `decompression_ms`, and `parse_ms` are `null` with an explicit not-applicable reason.
They must become real measured phases if district files/packages are added later; `null` must not
be converted to a misleading measured zero.

JSON schema 5 adds `physics_workload` sampled once per game `Update()`. `bodies` and
`active_rigid_bodies` are current Jolt rigid-body state; they deliberately exclude the separately
managed `CharacterVirtual`. `rigid_body_contact_manifolds` is the current set of Jolt body-contact
pairs, while `character_contacts` counts actual contacts reported by each CharacterVirtual's most
recent collision update. These are different collision mechanisms and remain separate.

The remaining physics fields count operations since the preceding sample and reset when captured:
`fixed_steps`, public gameplay `public_raycasts`, `character_collision_updates`, and
`vehicle_wheel_raycasts`. Wheel-ray counts are exact for the current vehicle setup because each of
its four wheels is configured for a collision test on every active and inactive step. The profiler
does not fabricate one generic “query count” by adding semantically different operations. Physics
CPU p95 remains the budgeted time; these workload counters explain that time and provide a stable
baseline for later content growth.

JSON schema 6 adds `ai_workload` once per game `Update()`. Current-state fields report traffic
vehicles, pedestrians, pedestrians in their timed flee state, and active police patrols. Operation
fields report the exact loops that actually ran: traffic updates and obstacle comparisons,
pedestrian updates and player-vehicle threat-distance checks, police witness candidates tested,
and patrol cars moved. AI-suspended district-transition updates retain current state but record
zero operations.

`ai_cpu` now ends before mission-state progression, so its scope matches the budget row: ambient
traffic, pedestrian, witness, and police work only. The current movers follow fixed
`WaypointPath`s; there is no district road graph, asynchronous path-request queue, queue latency,
or failure state to report. Those fields must not be fabricated as zero—add them with the real
road/path system required by IG-35-010.

JSON schema 7 adds `audio_workload` once per game `Update()`. It records the exact game-owned audio
surface available through CNA: loaded `SoundEffect` assets, retained loop instances and their
playing state, streamed game assets, one-shot play requests/successes, loop play/stop commands, and
loop volume/pitch updates. `audio_cpu` remains the budgeted time for these game-owned control calls.

`tracked_playing_loop_voices` covers only retained `SoundEffectInstance`s. CNA exposes no lifetime
query for fire-and-forget voices started by `SoundEffect::Play()`, nor decoder time, mixer callback
time, active backend channel count, or bus cost. Report metadata marks those fields unavailable;
they must not be inferred from the successful-play count or represented by misleading zeroes.

JSON schema 8 adds `frame_pacing` derived from the existing wall-clock `frame_interval` samples.
Its mutually exclusive histogram buckets end at the recommended 16.667 ms budget, minimum 33.333
ms budget, 50 ms hitch threshold, and 100 ms severe-hitch threshold, followed by an unbounded final
bucket. A minimum-budget miss is strictly greater than 33.333 ms, a hitch strictly greater than 50
ms, and a severe hitch strictly greater than 100 ms; exact threshold values stay in the lower bucket.

Each synchronous `RecordDistrictLoad` marks the index of the first frame-interval sample recorded
after it. `district_transition_boundaries` reports total transitions, boundaries actually measured
before capture end, how many crossed the 50 ms hitch threshold, and their maximum. A transition on
the final unpresented update remains counted but unmeasured (`measured_samples < transitions`)
instead of borrowing a neighboring frame or inventing zero duration.

The current CNA/EasyGL API does not expose complete GPU residency. The report records the exact
logical size of known Iron Gang-owned meshes/lightmaps/HUD resources plus imported CNJ vertex and
index buffers and textures bound by CNA's built-in or generic effects. Imported resources are
deduplicated by object identity; texture sizes include their complete mip chains and block-format
rounding. The JSON separates `game_owned_bytes`, `imported_model_buffer_bytes`, and
`imported_model_texture_bytes` beneath `tracked_bytes`.

`tracking_complete` nevertheless remains `false`: backend effect programs, swapchain/depth and
other render-target or transient allocations, driver padding, and physical residency are not
available through the public API. A backend counter or external GPU capture is still required to
qualify the VRAM gate; the tracked budget check is only an early lower-bound guard.

### Binding complete external VRAM evidence

Neither CNA/EasyGL nor the generic OpenGL extensions available to Iron Gang expose complete
per-process residency. Global adapter capacity/free-memory counters and API-object tracers are not
equivalent: they omit or conflate driver allocations, residency, and other processes. A qualifying
capture therefore needs an authoritative vendor/OS profiler whose documented scope is the peak
complete graphics residency of the specific Iron Gang process.

Keep the profiler's raw artifact and write a small evidence manifest alongside the original
schema-8 capture:

```json
{
  "schema_version": 1,
  "measurement_scope": "complete_process_gpu_residency_peak",
  "hardware_identity": "<CPU, GPU, driver, display/compositor identity>",
  "tool": {"name": "<profiler>", "version": "<version>"},
  "process": {"executable": "iron_gang", "pid": 1234},
  "measurement": {
    "peak_resident_bytes": 268435456,
    "started_utc": "2026-08-24T10:00:00Z",
    "ended_utc": "2026-08-24T10:15:00Z"
  },
  "profile_capture_sha256": "<sha256 of original profile JSON>",
  "source_artifact": {
    "file_name": "<raw profiler artifact file name>",
    "sha256": "<sha256 of raw profiler artifact>"
  }
}
```

Generate the two hashes with `sha256sum`, then bind and validate all three inputs without modifying
the original capture:

```bash
sha256sum runtime/performance/m12-mixed-01.json runtime/performance/vendor-capture-01.bin
./scripts/vram_evidence.py \
  --capture runtime/performance/m12-mixed-01.json \
  --evidence runtime/performance/m12-vram-evidence-01.json \
  --artifact runtime/performance/vendor-capture-01.bin \
  --output runtime/performance/m12-mixed-01-complete.json
```

Before generating or publishing a release report, re-verify the archived four-file bundle:

```bash
./scripts/vram_evidence.py \
  --capture runtime/performance/m12-mixed-01.json \
  --evidence runtime/performance/m12-vram-evidence-01.json \
  --artifact runtime/performance/vendor-capture-01.bin \
  --verify-enriched runtime/performance/m12-mixed-01-complete.json
```

The binder verifies schema, rejects duplicate JSON object keys, checks exact capture and
raw-artifact hashes, positive process/peak values, UTC interval ordering, and the exact measurement
scope. The enriched `tracked_bytes` is the
conservative maximum of Iron Gang's logical total and the external peak; only that output sets
`tracking_complete=true`. The release-summary hardware label must exactly match
`hardware_identity`, and capture comparison also requires the same profiler name, version, source,
and scope on both sides. Verification reconstructs the expected enriched capture from the three
source files and requires semantic equality of the entire JSON object, not just its VRAM fields;
it therefore detects a stale, cross-bound, or subsequently edited enriched profile.

This contract binds evidence; it does not certify a profiler's semantics or fabricate a
measurement. `apitrace`, adapter-global free-memory queries, Xvfb/llvmpipe, and a hand-authored
manifest without an authoritative complete-residency artifact remain non-qualifying. Archive the
original profile JSON, raw profiler artifact, evidence manifest, and enriched JSON together; later
report generation validates embedded hashes and metadata but cannot recreate a missing raw file.

A repeatable representative capture is:

```bash
./scripts/preflight.sh release-easygl
cmake --preset release-easygl
cmake --build --preset release-easygl
SDL_AUDIODRIVER=dummy ./cmake-build-release-easygl/iron_gang \
  --smoke 900 \
  --profile runtime/performance/m12-release-easygl.json \
  --profile-scenario mixed
```

Supported `--profile-scenario` values are `intro`, `idle`, `walk`, `drive`, `mixed`, and `mission`. `intro`
keeps the real opening sequence. `idle`, `walk`, and `drive` skip it and hold one isolated workload;
`mixed` skips it, walks for two fixed-time seconds, drives, and performs a real district transition
after eight fixed-time seconds. The scenarios exist only for profiling; ordinary play and ordinary
`--smoke` behavior are unchanged. `--vsync on|off` selects the requested presentation interval.

`mission` keeps the real opening sequence, advances one real dialogue line per simulated second,
lets the 2.5-second cutscene finish naturally, walks the physics character to the sedan, enters via
the real interaction and half-second animation path, drives the Jolt vehicle, and finishes on the
real warehouse mission trigger. It exits at the exact `Completed` boundary so continued motion
cannot contaminate the capture with the nearby district exit. `--smoke` is its upper safety bound;
if that bound expires first, report generation fails and no incomplete JSON is written.

For isolated automation that must not open a visible Wayland window, force SDL onto X11 as well as
setting a virtual `DISPLAY`; otherwise SDL may prefer an inherited `WAYLAND_DISPLAY`:

```bash
env -u WAYLAND_DISPLAY \
  SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
  xvfb-run -a -s "-screen 0 1280x720x24" \
  ./cmake-build-release-easygl/iron_gang \
    --smoke 540 \
    --profile runtime/performance/m12-xvfb-mixed.json \
    --profile-scenario mixed \
    --vsync off
```

Xvfb on this workspace uses unaccelerated Mesa llvmpipe and has no real vblank. Such runs validate
automation, report plumbing, and CPU/software-GL behavior, but cannot close the EasyGL hardware
frame-rate or VRAM gates.

### Release summary generator

Turn one or more schema-8 captures into a deterministic Markdown summary with:

```bash
./scripts/performance_report.py \
  --hardware "Xvfb llvmpipe diagnostic" \
  runtime/performance/m12-xvfb-mixed.json
```

Without `--qualifying-hardware`, the overall state is always `DIAGNOSTIC`. For a real qualification,
name the controlled physical target explicitly, provide at least two mixed captures with distinct
canonical performance contents, and write the release artifact:

```bash
./scripts/performance_report.py \
  --hardware "<CPU, GPU, driver, display/compositor identity>" \
  --qualifying-hardware \
  --output runtime/performance/m12-release-summary.md \
  runtime/performance/m12-mixed-01.json \
  runtime/performance/m12-mixed-02.json
```

`--qualifying-hardware` is an operator assertion, not automatic hardware detection. The generator
still rejects labels identifying Xvfb/llvmpipe/software rasterization and requires Release
OPENGLES3, at least 1280x720, a successfully acknowledged swap interval, direct p95 budget passes,
known RAM, complete VRAM accounting within budget, and a real passing district transition in each
mixed capture. It reports `FAIL` when a declared qualification misses any condition and exits zero
because the report was generated successfully; malformed/stale input exits 2. `PASS` is therefore
a strict evidence summary, while successful command execution alone is not a gate result.
Duplicate object keys are malformed input rather than last-value-wins aliases; this applies to both
generated profiles and external evidence manifests.
Renaming, copying, or changing only JSON whitespace cannot turn one capture into the two independent
runs required for repeatability. Canonical performance identity is independent of file path and key
ordering and normalizes externally bound VRAM metadata, so rebinding the same original profile to a
second manifest/artifact also remains one run.

### Capture regression comparison

Compare a candidate schema-8 capture with a historical baseline using:

```bash
./scripts/performance_compare.py \
  --baseline runtime/performance/m12-baseline.json \
  --candidate runtime/performance/m12-candidate.json \
  --baseline-hardware "<same CPU, GPU, driver, display/compositor identity>" \
  --candidate-hardware "<same CPU, GPU, driver, display/compositor identity>" \
  --baseline-kind qualifying \
  --candidate-kind qualifying \
  --output runtime/performance/m12-comparison.md
```

Hardware identities and capture kinds must match exactly. The tool also requires matching backend,
build configuration, scenario, resolution, timing, requested/applied presentation state, GPU timer
support/scope, budget and hitch-threshold definitions, RAM observability, VRAM completeness/
coverage, and optional GPU/load/transition measurement availability. A qualifying comparison also
rejects virtual/software display labels, unacknowledged presentation, unknown RAM, and incomplete
VRAM. Use `--baseline-kind diagnostic --candidate-kind diagnostic` for Xvfb or other non-qualifying
engineering runs; diagnostic and qualifying evidence can never be mixed.

The candidate is a regression only when its increase is greater than both the relative tolerance
and the applicable absolute tolerance. Defaults are 10%, 0.5 ms for frame/GPU/Present/transition
frame, 0.1 ms for CPU subsystems, 1 ms for district load, 8 MiB for RAM/VRAM, and 0.25 percentage
points for minimum-budget misses/hitches. Every tolerance has a named CLI override. The Markdown
table covers frame, update/physics/AI/audio/render/Present/GPU/load p95, miss/hitch rates, peak RAM,
tracked VRAM, and the district-transition frame; workload counts are context and do not fail the
comparison by themselves.

Exit 0 means no measured regression beyond tolerance, exit 1 means at least one regression, and
exit 2 means invalid or incompatible evidence. A no-regression result is not an M12 budget pass;
use the release-summary generator for the gate decision.

### Bootstrap source-content budgets

`assets/content-budgets.json` is the versioned first-pass policy consumed by
`scripts/content_budget.py`. The ceilings are guardrails around today's deliberately simple source
assets, not claimed final production limits:

| Asset group | Category | Current triangles / limit | Current materials / limit | Current textures / limit |
| --- | --- | ---: | ---: | ---: |
| Prototype city block | District prototype | 96 / 384 | 5 / 20 | 0 / 8 |
| Warehouse | Building | 12 / 48 | 1 / 4 | 0 / 4 |
| Four-part sedan | Vehicle | 48 / 192 | 4 / 8 | 0 / 8 |
| Test character | Character | 36 / 144 | 1 / 4 | 1 / 4 |

Triangle ceilings provide 4x the current committed geometry. Material limits provide 4x headroom
except the grouped sedan's 2x reserve. Texture limits reserve a small number of conventional
authored resources per group; they count texture resources only. They do **not** yet constrain each
texture's dimensions or decoded bytes because the current MC3 building/vehicle sources have no
textures and the character has only a 1x1 crash-prevention fixture—not representative production
evidence. Aggregate tracked texture memory remains under the M12 VRAM guard, and per-texture size
limits must be added when the first representative production textures are authored.

Validate all registered source assets or the budget group containing one import input:

```bash
./scripts/content_budget.py
./scripts/content_budget.py --source assets/source/mc3/vehicle_body.mc3.xml
```

`scripts/build-assets.sh` runs the filtered validation after MC3 schema validation and before
conversion, so a new source must first receive a reviewed policy entry. Exit 0 means every selected
group passes, exit 1 is an actionable limit failure, and exit 2 is invalid policy/source data. MC3
boxes/cubes and glTF triangle lists/strips/fans have exact counting rules. Any other MC3 primitive
or non-triangle glTF mode fails with a request to add exact triangulation logic; the validator never
guesses a cost.

### Bounded lifecycle memory soak

`iron_gang_memory_soak_tests` is a no-window M12 lifecycle guard. Every cycle:

1. resets and advances the prototype mission to its driving checkpoint;
2. writes and reads a real save, resumes it in a fresh mission runtime, and completes the mission;
3. performs WarehouseBlock -> Countryside -> WarehouseBlock transitions; and
4. verifies Jolt's body count returns to the exact WarehouseBlock baseline.

The CTest target runs 200 cycles with a 60-second timeout. After a 20-cycle warm-up, Linux samples
current RSS at ten fixed checkpoints and peak RSS at baseline/end. It fails above 8 MiB current-RSS
growth, 16 MiB high-water growth, or a +32 KiB/cycle least-squares trend. These deliberately loose
page/allocator allowances catch unbounded lifecycle retention without treating normal allocator
caching as a leak. Platforms without `/proc/self/status` still execute all lifecycle/body checks
and report `rss_known=false` instead of fabricating memory values.

Run the CI-sized or a longer local soak with:

```bash
ctest --preset compile-software -R iron_gang_memory_soak_tests --output-on-failure
./cmake-build-compile-software/iron_gang_memory_soak_tests --cycles 5000
```

This is not a rendering, GPU residency, audio-device, or physical-hardware soak; it cannot close
the remaining M12 graphics gate. It isolates repeated district/mission/save ownership in a fast,
deterministic process so lifecycle regressions fail CI before a longer integrated run.
