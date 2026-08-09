# Architecture

The repository deliberately separates framework code, reusable open-city systems, and game-specific content.

```text
sharp-runtime
      ↓
     CNA (materials, shadows, instancing, post-processing already available)
      ↓
cna-extended (ECS, Transform3 hierarchy, 3D collision/octree, skinned-model playback)
      ↓
Iron Gang reusable systems (`iron_gang_core`)
      ↓
Iron Gang game application and original content
```

`cna-extended` is a sibling dependency (`../cna-extended`), not a fork of CNA. It supplies
scene/ECS boilerplate a small team should not rewrite, but it does not provide materials,
shadows, instancing, or post-processing — those already exist in CNA itself
(`PbrEffect`, `SkinnedPbrEffect`, shadow-mapping and instancing examples) and should be
integrated directly rather than reimplemented under a separate "CNA EXT" layer.

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
