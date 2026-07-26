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

MC3 remains the editable source for constructional objects, buildings, roads, interiors, props, collision metadata, areas, and simple object animation. glTF/GLB is the interchange format. CNJ is the CNA-oriented runtime model representation.

MC3-specific mission, trigger, or semantic metadata can be lost when a scene is reduced to glTF geometry. Long term, the asset compiler should therefore emit a package containing both render data and a parallel metadata manifest rather than relying on glTF alone.

Use `scripts/build-assets.sh` after building Mesh Craft's `mc3togltf` and CNA's `cna_tool_gltf_to_cnj`. Generated files are ignored by Git until the packaging policy is finalized.
