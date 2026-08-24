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
