# Architecture

The repository deliberately separates framework code, reusable open-city systems, and game-specific content.

```text
sharp-runtime modules (`Core.Base`, `IO`, `Text.Json`, CNA's transitive closure)
      ↓
cnanext modules (`GraphicsCore` for the library, `Runtime` for the game)
      ↓
Iron Gang reusable systems (`iron_gang_core`) + Jolt Physics
      ↓
Iron Gang game application and original content
```

Iron Gang consumes the modular `../cnanext` checkout through named CMake targets, not CNA's
compatibility umbrella. `iron_gang_core` exposes `CNA::GraphicsCore`, uses the Sharp Runtime
`IO`/`Text.Json` components privately, and only the executable adds `CNA::Runtime`. CNA is added with
`EXCLUDE_FROM_ALL`, so unrelated Devices, GraphicsExt, C API, examples, and tools are not part of
the game's default build. Character playback uses `GraphicsCore`'s `AnimationPlayer` directly;
the former whole-repository `cna-extended` dependency is no longer needed.

The current prototype includes a small but real vertical path:

- CNA `Game` loop and input polling.
- CNA 3D vertex/index buffers and `BasicEffect` rendering.
- Procedural debug city geometry.
- Third-person on-foot movement and simple collision.
- Kinematic driveable sedan.
- Enter/exit interaction.
- Dialogue progression.
- Mission state progression.
- Save/load using sharp-runtime `System::IO`.
- Core tests that run without creating a window.

## Module boundaries, and what enforces them

The dependency direction above is a rule, not a description. `scripts/check_layering.py` checks it
on every `ctest` run (`iron_gang_layering_tests`), because a rule nothing checks is a convention
that holds until someone is in a hurry — and this one has already been broken once, when a public
header exposed sharp-runtime's `JsonDocument` and broke the test build.

| Rule | Why |
| --- | --- |
| A public header (`include/IronGang/**`) must not include `System/…` or `CNA/Internal/…` | Both are **private** dependencies of `iron_gang_core`. A public header naming their types forces every consumer to find them. |
| A public header must not reach into `src/` with a relative include | Private headers live under `src/` precisely so they are not part of the surface (`JsonDataFileInternal.hpp`, `JsonReadHelpers.hpp`). |
| Only the executable may include CNA::Runtime (`Game.hpp`, `GraphicsDeviceManager.hpp`) | `iron_gang_core` links `CNA::GraphicsCore` only. The moment a library source includes `Game.hpp`, the split stops being real. |

The executable's own module — `include/IronGang/Application/**` plus the sources CMake lists under
`add_executable(iron_gang …)` — is the single narrow exception to the third rule. Module membership
is **read from `CMakeLists.txt`**, so moving a source between targets is checked against the file
that decides it rather than against a second list that would drift.

The checker also refuses to pass vacuously: no public headers found, or a library with no sources,
is an error rather than a clean run. That is the failure mode where a moved directory silently
disables the whole thing.

**Global state.** An audit found exactly one piece of namespace-scope mutable state in the tree:
`Log`'s mutex-guarded `LogState`, which is the deliberate exception — a logger is the one service a
game legitimately reaches for from anywhere. It is not machine-enforced, because telling a static
member function from a global variable by regular expression is unreliable and there is one instance
to police.

## Fixed step and variable step

CNA owns the fixed step: `IronGangGame::Initialize` sets a 60 Hz target elapsed time, and CNA's loop
calls `Update()` once per step, catching up after a stall by calling it **repeatedly** rather than
by handing it a bigger delta.

That fixes the responsibilities (plan_04 `IG-04-005`):

| Half | Who | Rule |
| --- | --- | --- |
| Fixed step | `IronGangGame::Update` | The only place the world advances: physics, controllers, ambient AI, the mission, the autosave scheduler. |
| Variable step | `IronGangGame::Draw` / `EndDraw` | **Reads** state to present it. Never advances it. |

`SimulationClock` sits between the two and is the single place a frame delta is sanitized: it clamps
an extreme delta (so a stall costs smoothness rather than correctness) and keeps monotonic
simulation time that owes nothing to the wall clock. See its header for why it clamps rather than
subdividing, and what that means while CNA runs fixed-step.

The procedural renderer is intentionally temporary. Future production geometry should flow from MC3 through glTF/GLB into CNJ and be loaded through CNA content APIs. Game-specific streaming, physics, navigation, traffic, missions, dialogue, and cinematics remain outside CNA itself.
