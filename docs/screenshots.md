# Capturing and reviewing frames

Until 2026-08-26 nothing in this repository had ever been looked at. Every "verified" claim about
the renderer, the camera, the HUD, or a character was made from assertions and log lines, because
the environment the game is built in has no display. `docs/validation.md` says so, repeatedly, under
"Not verified".

The software renderer needs no display, no GPU, and no driver. So the game can render a frame and
hand back the pixels anywhere it builds — which is what `--screenshot` does.

## Taking one

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./cmake-build-compile-software/iron_gang \
    --smoke 40 --screenshot frame.png --screenshot-frame 38
```

- `--screenshot <path>` writes one frame as an RGBA PNG.
- `--screenshot-frame <n>` picks which draw frame, 1-based, counting from the first `Draw()`.
  Default 1 — which is the very first frame of the intro cutscene.
- The frame is read back after the last draw call and **before** `Present()`, because reading the
  back buffer after a present is renderer-dependent.
- A capture that fails is logged and the game carries on. A diagnostic must never be able to take
  the run down with it.

`--smoke` must run at least as many frames as `--screenshot-frame`, or nothing is captured. Roughly:
one draw frame is one 60 Hz simulation step, so frame 300 is about five seconds into the game.

## Pinning a capture to a reproducible moment

`--screenshot-frame` counts **draw frames**, which is the nondeterministic axis. To capture a
specific moment, use `--screenshot-update <n>` together with a recorded input script — both are
keyed on the fixed 60 Hz simulation update:

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./cmake-build-compile-software/iron_gang \
    --play-input tests/input-scripts/prologue_opening.inputscript.json \
    --screenshot driving.png --screenshot-update 480
```

That run skips the cutscene, walks the player to the sedan, enters it, and captures the frame drawn
at or after update 480 — the same moment on any machine. See `docs/input-scripts.md`.

## What comes out

Two files. `frame.png`, and `frame.png.summary.json` beside it:

```json
{
  "width": 1280, "height": 720, "pixelCount": 921600,
  "nonSkyPixels": 918879, "nonSkyFraction": 0.997048,
  "distinctColours": 26, "distinctColoursCapped": false,
  "meanRed": 77.567087, "meanGreen": 68.063775, "meanBlue": 67.983671,
  "digest": "10147562532232898522"
}
```

The sidecar exists so a later run can be compared without an image library or a human eye. `digest`
is FNV-1a 64 over every RGBA byte: it answers "did this frame change at all", nothing more.

## Why there is no golden image

`--smoke N` is **not frame-deterministic**. CNA drives `Update()` from the wall clock and catches up
after a stall by calling it repeatedly, so how far the world has advanced by frame N differs between
runs and between machines. A golden-image comparison would fail everywhere, for reasons that have
nothing to do with a rendering regression.

What is stable is whether the frame is a rendered scene at all. `ScreenshotLooksRendered()`
(`include/IronGang/Graphics/ScreenshotSummary.hpp`) rejects an empty capture, a frame that is
nothing but the sky clear colour (the renderer drew nothing), and a frame of one or two flat colours
(a shader or format failure filling the screen). `iron_gang_screenshot_capture_tests` runs the real
binary and applies exactly that.

It deliberately does **not** reject a frame with no sky in it. The first real capture was the intro
cutscene's high establishing shot, which looks down at the street and is 99.7 % non-sky — a correct
frame that the first version of this predicate called suspicious. A camera angle is not a rendering
fault, and a check that cries wolf on valid content is worse than no check.

## What the first review found (2026-08-26)

Three frames were captured and looked at: the intro cutscene's establishing shot (frame 10), and
two gameplay frames from separate runs (frames 38 and 68).

Rendering correctly, confirmed visually for the first time:

- The follow camera sits behind and above the player, framing them centred, and the hand-off from
  the cutscene's terminal keyframe lands on that framing with no visible jump.
- The HUD's objective line and the dialogue subtitle draw legibly at the top left. The cutscene's
  own dialogue cue is visible in the establishing shot — end-to-end confirmation of the track added
  the same day.
- Road surface, lane markings, kerbs, sidewalks, foliage, buildings, traffic vehicles, and the
  parked sedan all draw in the right places and are distinguishable from one another.

Two defects that no test could have caught, and nobody could previously see:

- **Pedestrians render as a pair of red legs with no torso and no head**, floating slightly above
  the pavement. Consistent across both gameplay frames and every pedestrian in them. The player
  character in the same frame draws correctly (white torso and arms, red shirt, dark legs), so this
  is specific to how pedestrians are drawn, not to the character model itself.
- **A large flat pure-white quad fills the upper right** of both gameplay frames, with a hard
  straight edge and no shading. Its size and position are consistent between runs.

Neither is diagnosed here — this document records what the frames show. Both are open items in
`NEXT.md`.
