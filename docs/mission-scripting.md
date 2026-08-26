# Mission scripting

How to write an Iron Shadows mission file: the schema, the typed variables a mission owns, the
expression language its conditions and actions are written in, and the limits enforced on all of it.

Missions live in `assets/missions/*.mission.json` and are loaded by
`IronGang::LoadMissionDefinition` (`src/Missions/MissionDefinition.cpp`). The committed example is
`assets/missions/prologue.mission.json`. Related plan entries: `plan/plan_24-mission-framework-and-scripting.md`
`IG-24-005`, `IG-24-007`, `IG-24-013`, `IG-24-014`, `IG-24-016`, `IG-24-031`-`IG-24-035`.

## What this is not

There is no embedded scripting language, VM, or engine API surface reachable from a mission file
(the locked decision behind `IG-24-013`). A mission expression can only read symbols the game
declared and combine them with arithmetic, comparison, and boolean operators. It has no statements,
no calls, no loops, and no assignment: the only thing that writes state is a declared `set` action.
Everything is type-checked when the file loads, so a malformed mission fails at load — never
halfway through a playthrough.

## File schema

```json
{
  "id": "prototype_delivery",
  "title": "The Quiet Delivery",
  "version": 2,
  "initialState": "introduction",
  "variables": [
    { "id": "cargo_secured", "type": "bool", "value": false },
    { "id": "handover_radius", "type": "float", "value": 3.0 }
  ],
  "states": [
    {
      "id": "introduction",
      "objective": "Listen to Mara (Enter advances dialogue)",
      "onEnter": [ { "action": "log", "message": "briefing started" } ],
      "when": "dialogue_finished",
      "next": "reach_vehicle"
    },
    { "id": "reach_vehicle", "objective": "Walk to the sedan" }
  ]
}
```

| Field | Meaning |
| --- | --- |
| `id`, `title` | Mission identity. `id` is what mission log lines are prefixed with. |
| `version` | Schema version. `1` is gate M7's original shape (a `condition` naming one engine signal); `2` adds `variables`, `when`, and `onEnter`. Version 1 files still load unchanged. |
| `initialState` | Must name a state in `states`. |
| `retry` | `checkpoint` (default) or `mission_start` — where `PrototypeMission::Retry()` puts the player after a failure. Version 2 only. |
| `variables` | This mission's own typed state — see below. Version 2 only. |
| `states[].id` | Unique within the mission. Any identifier you like — the save file stores this id, so states are not a fixed set. |
| `states[].objective` | The objective line the HUD shows while this state is current. |
| `states[].onEnter` | Actions run once, in file order, when the mission enters this state. Version 2 only. |
| `states[].when` | Bool expression evaluated every frame; when it becomes true the mission moves to `next`. `condition` is the version-1 spelling of the same field. A state declares `when`+`next`, or `transitions`, or neither — never a mix. |
| `states[].next` | The state to move to. A state with neither `when`/`next` nor `transitions` is terminal. |
| `states[].transitions` | Several ways out: `[{ "when": …, "next": … }, …]`, evaluated in file order, first match wins. Version 2 only, at most 8. |
| `states[].outcome` | `completed` or `failed` — what reaching this state means for the run. An outcome state ends the mission and must therefore have no `next`. Version 2 only. |
| `states[].reason` | Why the mission failed, shown to the player. Only allowed on a state whose outcome is `failed`. Version 2 only. |
| `states[].checkpoint` | `true` marks this state as a checkpoint: entering it records the state and the mission's variables for a later retry. A checkpoint state cannot also be an outcome. Version 2 only. |

## Branching

A state may declare more than one way out. They are evaluated **in file order every frame, and the
first condition that holds wins** — so put the branch that must take priority first:

```json
{
  "id": "drive_to_warehouse",
  "objective": "Drive into the green warehouse marker",
  "checkpoint": true,
  "transitions": [
    { "when": "police_chase_seconds > 25", "next": "busted" },
    { "when": "player_driving && vehicle_in_warehouse_goal && cargo_secured", "next": "completed" }
  ]
}
```

That is the committed prologue: arriving at the warehouse completes the delivery, unless the police
have been on you for more than 25 seconds, in which case the delivery is blown. `when`/`next` is
just the one-transition shorthand for the same thing.

## Ending a mission

Every mission must have at least one state that ends it. Say so explicitly:

```json
{ "id": "escaped", "objective": "You made it", "outcome": "completed" },
{ "id": "caught",  "objective": "They got you", "outcome": "failed" }
```

`PrototypeMission::IsCompleted()` / `IsFailed()` / `IsFinished()` read the current state's outcome —
nothing keys off a particular state id. For compatibility, a file that declares no outcome at all
falls back to the pre-`outcome` rule: a terminal state literally named `completed` counts as the
success outcome. A file with neither is a load error, because nothing in it could ever finish.

## Failing, checkpoints, and retry

A mission fails by reaching a state with `"outcome": "failed"`, which may explain itself:

```json
{ "id": "busted", "objective": "Busted", "outcome": "failed",
  "reason": "The police took the shipment" }
```

`GetFailureReason()` returns that text; the HUD and window title show `Mission failed: <reason>`
in place of the objective line.

Mark the states worth returning to:

```json
{ "id": "loading", "objective": "Load the crates", "checkpoint": true,
  "onEnter": [ { "action": "set", "variable": "crates", "value": "crates + 3" } ],
  "when": "player_driving", "next": "delivered" }
```

Entering a checkpoint state records its id **and the mission's variables as they stand once its
entry actions have run**. `Retry()` then restores exactly that — without re-running those entry
actions, because their effects are already in the recorded values. Retrying does not consume the
checkpoint; you can fail and retry from it repeatedly.

In the running game, **R** retries a failed mission (and still resets the whole prototype when the
mission has not failed). `IronGangGame::RetryMission()` also restores the player, vehicle, and
district from a world snapshot taken when the checkpoint was recorded, and resets the police
response — otherwise the chase that failed the mission would fail it again within a frame. Both halves of a checkpoint are saved — the mission's state and variables, and the world it was
reached in — so a retry works straight after loading a save. A save written before the world half
existed restores only the mission half, and a retry then falls back to a full restart.

### Where to put a checkpoint

A checkpoint is only as good as the situation it drops the player back into, so:

* **Put one at the start of anything that can fail.** In the prologue that is
  `drive_to_warehouse`: the state whose branch leads to `busted`. A failure branch with no
  checkpoint above it means a full restart.
* **Put it where the world is survivable.** The player, the vehicle, and the district are recorded
  as they stand the instant the state is entered — mid-chase or mid-crash is a bad place to come
  back to.
* **Not on a state that ends the mission.** An outcome state cannot also be a checkpoint; the
  loader refuses it.
* **Not on every state.** Each checkpoint overwrites the previous one, so a checkpoint immediately
  before the failure branch defeats the point: the player returns to the moment they were already
  losing.
* **Mind the entry actions.** The checkpoint is recorded *after* the state's `onEnter` actions run,
  so a counter incremented there is not incremented again by a retry. Anything that must be redone
  on a retry belongs in a state entered *after* the checkpoint.

`"retry": "mission_start"` at the top level makes `Retry()` a full restart instead. A `checkpoint`
retry before any checkpoint has been reached is also a full restart, so a mission that declares no
checkpoint behaves identically under either policy.

`Reset()` is always a restart from the very beginning and discards the recorded checkpoint;
`Retry()` is the one that honours the policy.

The checkpoint is saved (`mission_checkpoint_state_id` plus one `mission_checkpoint_var.<name>`
line per variable) and restored. A checkpoint naming a state the loaded mission no longer defines
is dropped entirely — a retry that went nowhere would be worse than a restart — and a checkpoint
variable that no longer matches its declaration is dropped individually; both are reported.

## Variables

A variable is declared with a name, one of four types, and an initial value:

```json
{ "id": "deliveries_made", "type": "int", "value": 0 }
```

| Type | JSON literal | Notes |
| --- | --- | --- |
| `bool` | `true` / `false` | |
| `int` | `7` | 32-bit. |
| `float` | `3.0` | |
| `string` | `"Mara"` | |

`value` may be omitted and defaults to `false` / `0` / `0.0` / `""`. A value that is not of the
declared type is a load error, as is a name that collides with an engine fact.

Variables are per mission, live for one run, and:

* are restored to their declared values by a retry (`PrototypeMission::Reset()`);
* are written to the save file as `mission_var.<name>=<type>:<value>` lines and restored on load
  (the save also stores the current state as `mission_state_id=<id>`; a save from before free-form
  state ids stored a `mission_state=<0-4>` index instead and is migrated on read);
* keep their value across a district transition (the mission is not reloaded).

A save naming a variable the mission no longer declares — or whose type changed — is not a load
failure: the entry is skipped and reported (`[IronGang] save file mission variable ignored: …`).

At most `kMaxMissionVariables` (64) variables per mission.

## Engine facts

Facts are read-only signals the game refreshes every frame. A mission file cannot declare or assign
one. The prototype's set (`IronGang::CreatePrototypeMissionFacts`, `PrototypeMission.hpp`):

| Fact | Type | Meaning |
| --- | --- | --- |
| `dialogue_finished` | bool | The opening dialogue has been read to the end. |
| `player_driving` | bool | The player is currently driving the sedan. |
| `player_vehicle_distance` | float | XZ distance from the player to the sedan, in metres. |
| `player_near_vehicle` | bool | `player_vehicle_distance <= 3`. |
| `player_in_warehouse_goal` | bool | The player stands inside the delivery trigger. |
| `vehicle_in_warehouse_goal` | bool | The sedan is inside the delivery trigger. |
| `player_driving_in_warehouse_goal` | bool | Both of the previous two at once. |
| `police_alerted` | bool | The police have been dispatched or are chasing. |
| `police_chasing` | bool | A patrol car is actively chasing the player. |
| `police_chase_seconds` | float | How long the current chase has run. Resets when the chase resolves. |
| `vehicle_integrity` | float | 1 for an undamaged sedan, 0 for a wreck. See `docs/vehicles.md`. |
| `vehicle_disabled` | bool | The sedan is wrecked. It still rolls, slowly. |
| `current_district` | string | `warehouse_block` or `countryside`. |
| `player_in_delivery_goal` | bool | The player stands in this district's delivery zone. |
| `vehicle_in_delivery_goal` | bool | The sedan is in this district's delivery zone. |

The three composite facts exist so every version-1 mission file keeps loading. New missions should
prefer the primitives — `player_driving && vehicle_in_warehouse_goal` says what it means, and
`player_vehicle_distance <= handover_radius` puts the threshold in the mission file instead of the
engine.

Adding a fact is a code change. For a fact the mission runtime can derive from `Update()`'s own
arguments, declare it in `CreatePrototypeMissionFacts()` and set it in
`PrototypeMission::RefreshFacts()` (both in `src/Missions/PrototypeMission.cpp`). For a fact owned
by another subsystem — as the police facts are — declare it the same way but push it in from that
subsystem's owner with `PrototypeMission::SetFact()`, the way `IronGangGame::Update()` publishes the
`PoliceSystem` state each frame.

## Expression language

```text
expression := or
or         := and ( "||" and )*
and        := comparison ( "&&" comparison )*
comparison := sum ( ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) sum )?
sum        := product ( ( "+" | "-" ) product )*
product    := unary ( ( "*" | "/" ) unary )*
unary      := ( "!" | "-" ) unary | primary
primary    := number | 'string' | "true" | "false" | identifier | "(" expression ")"
```

* **Identifiers** are facts or variables. An unknown name is a compile error, never a silent `false`.
* **String literals use single quotes** (`contact == 'Mara'`) so they need no escaping inside JSON's
  own double quotes. There are no escape sequences: a literal is exactly the characters between the
  quotes.
* **Typing is static.** `&&`, `||`, `!` need bools. `+ - * /` and `< <= > >=` need numbers. `==` and
  `!=` compare two numbers, two bools, or two strings. Mixing an `int` and a `float` promotes the
  result to `float`; `int / int` truncates.
* **Comparisons do not chain.** `1 < 2 < 3` is rejected rather than silently meaning something else.
* **`&&` and `||` short-circuit**, so the right-hand side of a decided operator is never evaluated.
* **Division by zero** is an evaluation error, not an infinity: the mission logs it and stays put.

Examples:

```text
dialogue_finished
player_vehicle_distance <= handover_radius
player_driving && vehicle_in_warehouse_goal && cargo_secured
deliveries_made >= 3 || !player_driving
```

## Actions

`onEnter` runs once per entry into a state, in file order. Loading a save does **not** re-run them —
the state they were entered from already did.

```json
{ "action": "set", "variable": "deliveries_made", "value": "deliveries_made + 1" }
{ "action": "log", "message": "delivery complete" }
```

| Action | Fields | Behaviour |
| --- | --- | --- |
| `set` | `variable`, `value` | Evaluates the `value` expression and assigns it. The variable must be declared by this mission and the expression's type must match it exactly — both checked at load. |
| `log` | `message` | Writes `[IronGang][mission] <mission id>: <message>` through the game's existing logging path. |

At most `kMaxMissionStateActions` (16) actions per state. The remaining `IG-24-007` verbs
(spawn/despawn/enable/disable/move/play/wait/branch) need entity and timer concepts the prototype
does not have yet and are still open.

## Limits

All of these are compile-time or evaluation-time failures, never hangs
(`include/IronGang/Missions/MissionExpression.hpp`):

| Limit | Value |
| --- | --- |
| Expression source length | 512 characters |
| Tokens per expression | 128 |
| Operations per expression | 96 |
| Nesting depth | 16 |
| Evaluation steps | 256 |
| Variables per mission | 64 |
| Entry actions per state | 16 |

## Logging

Every state transition is logged with the condition that fired it:

```text
[IronGang][mission] prototype_delivery: reach_vehicle -> enter_vehicle (player_vehicle_distance <= handover_radius)
```

A condition or action that fails at runtime is logged the same way and leaves the mission where it
is, rather than advancing or silently failing.

## Failure modes

| Symptom | Cause |
| --- | --- |
| `Mission state "x" condition "…": unknown identifier "y" … at column N` | The expression names a fact or variable that does not exist. Check spelling against the fact table above. |
| `… evaluates to int, not bool` | A `when` expression must be a bool; a bare numeric fact is not a condition. |
| `Mission state "x" assigns a float expression to int variable "y"` | Types must match exactly; write `1` rather than `1.0` for an `int`. |
| `Mission file declares "variables", which requires "version": 2` | Bump `version` to 2. |
| `Mission state "x" declares a condition … but no "next" state` | A terminal state must not have a condition. |
| `Mission file has no state that ends the mission` | Give one state `"outcome": "completed"`. |
| `Mission state "x" declares outcome "completed" and a "next" state` | An outcome ends the mission; drop the `next`. |
| `Mission state "x" declares a failure "reason" without "outcome": "failed"` | Only a failing state can explain a failure. |
| `Mission state "x" is both a checkpoint and an outcome` | A state that ends the mission cannot be retried from. |
| `Mission state "x" declares more than one of "transitions"/"when"/"condition"` | Pick one spelling: `transitions` for several ways out, `when`+`next` for one. |
| `Mission state "x" needs both a condition and a "next" state, or neither` | Half a transition goes nowhere. |
| `save file mission state "x" is not defined by the loaded mission` | The save was written against a different mission file. The mission keeps its current state rather than resuming into a state that does not exist. |
| The game logs `… -- using built-in fallback mission.` | The file failed to load; the message above it says why. The game keeps running on the hardcoded prologue. |

## Campaigns

`assets/missions/campaign.json` says which missions exist and what unlocks them:

```json
{
  "version": 1,
  "missions": [
    { "id": "prototype_delivery", "title": "The Quiet Delivery", "path": "missions/prologue.mission.json" },
    { "id": "countryside_run", "title": "Out of Town", "path": "missions/countryside_run.mission.json",
      "requires": ["prototype_delivery"] }
  ]
}
```

A mission with no `requires` is available from the start; the game runs the first available one and,
on completion, marks it done and starts whatever that unlocks.

The loader refuses every shape that describes a campaign nobody could finish: an unsupported
version, an empty or duplicate id, an empty path, a prerequisite naming a mission that is not in the
file, a mission requiring itself, **a dependency cycle** — reported as the path that loops, since
"cycle detected" tells an author nothing — and a campaign where every mission has a prerequisite,
which can never start.

Campaign **progress is not saved yet**, so loading a save restarts the campaign at its first
mission (`IG-24-049`).

## Tests

`tests/CoreTests.cpp`: `TestMissionValidationRejectsMalformedData`,
`TestMissionExpressionEvaluatesTypedOperations`, `TestMissionExpressionRejectsMalformedInput`,
`TestMissionVariablesEnforceTypes`, `TestMissionEntryActionsRunOncePerEntry`,
`TestMissionVariablesSurviveSaveLoad`, `TestMissionStateIdsAreNotAFixedSet`,
`TestMissionCheckpointRetryAndFailureReason`, `TestMissionCheckpointSurvivesSaveLoad`,
`TestCampaignGraphUnlocksAndRejectsCycles`, `TestCountrysideMissionRunsAndFailsOnAWreck`,
`TestMissionBranchesOnFirstMatchingTransition`, `TestPrologueFailsAndRetriesUnderPoliceChase`,
`TestSaveMigratesLegacyMissionState`, plus the flow tests `TestMissionFlow` and
`TestMissionLoadsCommittedFile`.
