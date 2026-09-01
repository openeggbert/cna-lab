# Logging

Every diagnostic the game emits goes through `IronGang::Log`
(`include/IronGang/Core/Log.hpp`, `src/Core/Log.cpp`). Plan entries:
`plan/plan_04-core-runtime-services.md` `IG-04-002`, `IG-04-017`.

## Line format

```text
[IronGang][mission][info] prototype_delivery: reach_vehicle -> enter_vehicle (player_driving)
[IronGang][assets][warning] warehouse.cnj not found -- using procedural warehouse box. …
```

`[IronGang][<category>][<severity>] <message>`, one line each, written to **stderr**. Gameplay text
the prototype prints for the player (dialogue lines) goes to stdout and is not log output, so a run
can have its diagnostics redirected without losing the game's own voice.

Nothing outside the log should depend on this exact format. `scripts/release_archive.py` checks its
packaged smoke run for the *message* text only, deliberately — "the packaged build loads these
assets" is the claim worth pinning, not "the prefix never changes".

## Severities

| Severity | Meaning |
| --- | --- |
| `debug` | Detail useful while working on a subsystem. Off by default. |
| `info` | Something a player-visible run should mention: an asset loaded, a mission advanced, a save was written. |
| `warning` | The game carried on, but not the way the data asked for. A warning should name **what was ignored and what happened instead** — "using built-in fallback mission", "keeping the default". |
| `error` | Something the caller asked for did not happen at all. |

Messages below the minimum severity are dropped. The default minimum is `info`.

## Categories

| Category | Covers |
| --- | --- |
| `app` | The application shell itself. |
| `assets` | Models, textures, and the procedural fallbacks used when one is missing. |
| `audio` | Sound loading and playback. |
| `config` | `assets/config/game.json` — see `docs/configuration.md`. |
| `cutscene` | Cutscene loading and playback. |
| `dialogue` | Dialogue loading. |
| `mission` | Mission transitions, entry actions, checkpoints, retries, and condition faults. |
| `save` | Saving, loading, migration, backup recovery, and autosaves. |

A **disabled category is silent at every severity, errors included**. Turning a category off means
"I do not want to hear from this subsystem", and a half-off category would be more confusing than
either state.

## Choosing the level

* `assets/config/game.json`: `"logSeverity": "debug" | "info" | "warning" | "error"`.
* `--log-level <name>` on the command line **overrides the file for that run** — someone passing it
  is debugging this run specifically. An unrecognized name is rejected at startup with the list of
  valid ones, rather than silently falling back.

Category filtering has no configuration key yet; `Log::SetCategoryEnabled` exists for code and
tests. Add a key when someone actually needs to silence a subsystem from data.

## Adding a category or severity

1. Add the enum value in `Log.hpp` (a category goes **before** `Save`, or `kCategoryCount` in
   `Log.cpp` — which is derived from the last value — stops covering it).
2. Add its name to `LogCategoryName`/`LogSeverityName`; `ParseLogCategory` walks the names, so
   parsing follows automatically for categories.
3. Extend `TestLogSeverityAndCategoryFiltering` in `tests/CoreTests.cpp` — it round-trips every
   name through parse, so a missing name fails there.
4. Add a row to the table above.

A category is only worth adding when someone would plausibly want to watch it alone. Categories
exist to be filtered; a category nobody would filter is just a word in a prefix.

## Testing against the log

`Log::SetSink` replaces the destination with a callback, which is how
`TestLogSeverityAndCategoryFiltering` asserts on messages without touching stderr. Call
`Log::Reset()` afterwards — it restores stderr, the `info` minimum, and every category — so one
test cannot leave the next one deaf.

## Not implemented yet

No timestamps, no log file, no rotation, no in-game console, and no per-category configuration key.
The state is process-wide and guarded by a mutex, so a future background loader thread
(`IG-04-014`) can log safely; the sink is called outside that lock so a slow or re-entrant sink
cannot deadlock the game.
