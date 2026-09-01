# Recorded input, and QA repro cases

Until 2026-08-26 no automated run in this repository had ever pressed a key. `--smoke` renders
frames and lets the world tick, but nothing reached an input-driven path: advancing dialogue,
skipping a cutscene, pressing E to get into the sedan. Those existed only as unit tests calling the
systems directly, and `docs/validation.md` said so repeatedly under "Not verified".

An input script fixes that. It is a small JSON file that says what is held down, and when.

## Playing one

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./cmake-build-compile-software/iron_gang \
    --play-input tests/input-scripts/prologue_opening.inputscript.json
```

With no `--smoke`, the run ends when the script does. That is what makes a repro's *length*
reproducible: it is written in updates, and it stops when those updates run out.

## Recording one

```bash
./cmake-build-compile-software/iron_gang --record-input repro.inputscript.json --record-input-id my_bug
```

Play until you have reproduced the problem, quit, and the script is written on exit.

## The format

```json
{
  "id": "prologue_opening",
  "version": 1,
  "steps": [
    { "update": 60, "held": ["confirm"] },
    { "update": 61, "held": [] },
    { "update": 150, "held": ["move_forward"] },
    { "update": 400, "held": [] }
  ]
}
```

Two decisions are worth stating, because both are the reason this works at all:

**Steps name actions, not keys.** `"confirm"`, not `"Enter"`. Rebinding a key is exactly the sort of
change that would otherwise silently turn every recorded repro into a different repro. The ids are
the same ones the settings file uses (`GameActionId`), and one that no longer exists fails the load
rather than doing nothing at that moment.

**Steps are keyed on the simulation update, not the draw frame.** The simulation runs a fixed 60 Hz
step, so "at update 320, hold Confirm" means the same thing on a fast machine and a slow one. How
many *draw frames* a run produces depends entirely on how fast it renders — with the software
renderer that is roughly six frames a second, and with a GPU it is hundreds.

Steps are **sparse**: one entry per update where the held set changes. Everything in between repeats
the last entry, so an idle minute costs one step rather than 3600.

Playback **replaces** the keyboard rather than merging with it. A repro that a stray keypress could
alter is not a repro.

## Validation

The loader refuses an unsupported version, an empty id, an empty step list (a script that would play
back nothing), steps out of ascending update order, duplicate or negative update indices, a step
missing `update` or `held`, an unknown action id, and unknown fields. Each of those is a repro case
that would otherwise run and quietly do something other than what it says.

## The committed repro

`tests/input-scripts/prologue_opening.inputscript.json` drives the whole opening: Confirm at update
60 skips the intro cutscene, two more Confirms walk through the rest of the conversation,
`move_forward` from update 150 walks to the sedan, and Interact at update 430 gets in.
`iron_gang_input_script_tests` runs it against the real binary and asserts the game reached the
states that only input can cause — `introduction -> reach_vehicle -> enter_vehicle ->
drive_to_warehouse`. It is the first end-to-end verification of the cutscene skip path, which had
been listed as unverified since the cutscene was written.

If you edit that file, the unit test `TestCommittedPrologueReproScriptIsUsable` requires it to keep
pressing Confirm three times and Interact once — otherwise it stops being the repro it claims to be.

## What this does not do

No mouse or gamepad input is recorded, because the game has no such input path yet. Playback is
deterministic in *what happens at update N*, not in how many updates a given wall-clock second
contains — a run that stalls does fewer updates in the same time, it does not do different ones.
