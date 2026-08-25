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
