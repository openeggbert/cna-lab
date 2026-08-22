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

The procedural renderer is intentionally temporary. Future production geometry should flow from MC3 through glTF/GLB into CNJ and be loaded through CNA content APIs. Game-specific streaming, physics, navigation, traffic, missions, dialogue, and cinematics remain outside CNA itself.
