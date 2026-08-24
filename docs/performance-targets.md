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
and the target frame duration. `vertical_sync_requested` describes the requested presentation
parameters; it is not proof that a virtual display or driver accepted a real swap interval.

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

Supported `--profile-scenario` values are `intro`, `idle`, `walk`, `drive`, and `mixed`. `intro`
keeps the real opening sequence. `idle`, `walk`, and `drive` skip it and hold one isolated workload;
`mixed` skips it, walks for two fixed-time seconds, drives, and performs a real district transition
after eight fixed-time seconds. The scenarios exist only for profiling; ordinary play and ordinary
`--smoke` behavior are unchanged. `--vsync on|off` selects the requested presentation interval.

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
