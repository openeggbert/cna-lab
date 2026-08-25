# Configuration

`assets/config/game.json` holds the game's tunable values. It is read once at startup by
`IronGang::LoadGameConfig` (`include/IronGang/Core/GameConfig.hpp`, `src/Core/GameConfig.cpp`).
Plan entries: `plan/plan_04-core-runtime-services.md` `IG-04-001`, `IG-04-006`, `IG-04-018`.

This is **developer tuning, not user settings**. Nothing here is written back, and the player
changes none of it from inside the game; player-facing settings are separate work
(`plan_29` `IG-29-005`).

## Schema

```json
{
  "projectName": "Iron Shadows",
  "cityName": "Iron City",
  "prototypeYear": 1932,
  "autosaveIntervalSeconds": 180,
  "autosaveMinimumSpacingSeconds": 20,
  "notes": "free text, ignored"
}
```

| Key | Type | Default | Used for |
| --- | --- | --- | --- |
| `projectName` | string | `Iron Shadows` | The window title's prefix. |
| `cityName` | string | `Iron City` | The district map panel's header, with the year. |
| `prototypeYear` | int, 1800–2200 | `1932` | The same header. |
| `autosaveIntervalSeconds` | number ≥ 0 | `180` | Seconds of unblocked play between periodic autosaves; `0` disables them without disabling checkpoint and district autosaves. |
| `autosaveMinimumSpacingSeconds` | number ≥ 0 | `20` | Shortest gap between two autosaves, so triggers landing together write one file. |
| `notes` | string | — | Accepted and ignored, so the file can carry a comment. |

The autosave defaults come from `AutosaveScheduler`'s own constants rather than being repeated in
`GameConfig`, so the two cannot drift apart.

## What happens when something is wrong

A broken or partial configuration costs the tuning, never the run:

| Situation | Result |
| --- | --- |
| File missing | Every default applies. Reported as a warning, not an error — the defaults are a complete, playable configuration. |
| Unknown key | Ignored, and **named in a warning**. This is the common configuration mistake (`projectNmae`), and silently ignoring it is how a mistuned build goes unnoticed. |
| Value of the wrong JSON type | That key keeps its default; warning names the key. |
| Number outside its range | That key keeps its default; warning names the range. |
| Negative seconds | Clamped to `0` with a warning — the author meant "off", and `0` is exactly that. |
| Empty string | Keeps the default; warning. |
| `autosaveMinimumSpacingSeconds` > `autosaveIntervalSeconds` | Loads, with a warning: legal, but it means the interval never fires when it says it will. |
| Malformed JSON, or a root that is not an object | **The only failure.** `LoadGameConfig` returns false, leaves the caller's configuration untouched, and the game logs it and runs on its defaults. |

Warnings are printed at startup as `[IronGang] configuration: …`.

## Adding a tunable

1. Add the member to `GameConfig` **with the default the game already uses** — the default belongs
   in one place, and that place is the struct.
2. Read it in `LoadGameConfig` with the matching `ReadString`/`ReadInt`/`ReadSeconds` helper, or a
   new one if the type is new. Validate the range there, not at the use site.
3. Add the key to `IsKnownKey`, or loading the file that uses it warns about it.
4. Add it to `assets/config/game.json` and update that file's hash in
   `assets/licenses/asset-registry.csv` (`python3 scripts/asset_registry.py --check-notice
   THIRD_PARTY_ASSETS.md` verifies).
5. Extend `TestGameConfigLoadsValidatesAndFallsBack` in `tests/CoreTests.cpp` — it asserts that the
   committed file loads with **zero** warnings, which is what keeps the shipped configuration
   honest.
6. Update the tables above.

## Not implemented yet

Environment-variable or command-line overrides of individual tunables (`IG-04-020`), reloading the
file while the game runs, and per-platform or per-build-configuration files. Command-line options
that are not tunables (`--assets`, `--smoke`, `--profile`, `--vsync`) are parsed in `src/main.cpp`
(`IG-04-008`).
