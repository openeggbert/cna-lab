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
