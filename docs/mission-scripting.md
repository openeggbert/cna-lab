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
| `variables` | This mission's own typed state — see below. Version 2 only. |
| `states[].id` | Unique within the mission. `PrototypeMission` additionally restricts the whole set to the five ids its save format encodes; see its header. |
| `states[].objective` | The objective line the HUD shows while this state is current. |
| `states[].onEnter` | Actions run once, in file order, when the mission enters this state. Version 2 only. |
| `states[].when` | Bool expression evaluated every frame; when it becomes true the mission moves to `next`. `condition` is the version-1 spelling of the same field — a state must not use both. |
| `states[].next` | The state to move to. A state with no `next` is terminal and must not declare a condition. |

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
* are written to the save file as `mission_var.<name>=<type>:<value>` lines and restored on load;
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

The three composite facts exist so every version-1 mission file keeps loading. New missions should
prefer the primitives — `player_driving && vehicle_in_warehouse_goal` says what it means, and
`player_vehicle_distance <= handover_radius` puts the threshold in the mission file instead of the
engine.

Adding a fact is a code change: declare it in `CreatePrototypeMissionFacts()` and set it in
`PrototypeMission::RefreshFacts()`. Both live in `src/Missions/PrototypeMission.cpp`.

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
| The game logs `… -- using built-in fallback mission.` | The file failed to load; the message above it says why. The game keeps running on the hardcoded prologue. |

## Tests

`tests/CoreTests.cpp`: `TestMissionValidationRejectsMalformedData`,
`TestMissionExpressionEvaluatesTypedOperations`, `TestMissionExpressionRejectsMalformedInput`,
`TestMissionVariablesEnforceTypes`, `TestMissionEntryActionsRunOncePerEntry`,
`TestMissionVariablesSurviveSaveLoad`, plus the flow tests `TestMissionFlow` and
`TestMissionLoadsCommittedFile`.
