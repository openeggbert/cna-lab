# Architecture

The repository deliberately separates framework code, reusable open-city systems, and game-specific content.

```text
sharp-runtime
      ↓
     CNA + future CNA EXT
      ↓
Iron Shadows reusable systems (`iron_shadows_core`)
      ↓
Iron Shadows game application and original content
```

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
