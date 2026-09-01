# Asset pipeline

## Intended source-to-runtime flow

```text
AI-assisted modelling / manual editing
             ↓
       Mesh Craft MC3 XML
             ↓
     mc3togltf → GLB/glTF
             ↓
 cna_tool_gltf_to_cnj → CNJ + binary sidecars
             ↓
   packaged Iron Shadows content
```

MC3 remains the editable source for constructional objects, buildings, roads, interiors, props, collision metadata, areas, and simple object animation. It is authored visually in **Mesh Craft**, a real 3D scene editor (Dear ImGui viewport, orbit camera, transform gizmos, CSG, extrude-along-path, scene hierarchy/properties panels, a first-person Walk Mode with collision, and a bounded live preview of area/trigger/timer event bindings) — not hand-typed XML. glTF/GLB is the interchange format. CNJ is the CNA-oriented runtime model representation.

MC3-specific mission, trigger, or semantic metadata can be lost when a scene is reduced to glTF geometry. Long term, the asset compiler should therefore emit a package containing both render data and a parallel metadata manifest rather than relying on glTF alone.

Use `scripts/build-assets.sh` after building Mesh Craft's `mc3togltf` and CNA's `cna_tool_gltf_to_cnj`. Generated files are ignored by Git until the packaging policy is finalized.
