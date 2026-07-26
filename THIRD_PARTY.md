# Third-party dependencies

Iron Shadows's original source code is MIT-licensed. Dependencies keep their own licenses and are not relicensed by this repository.

| Dependency | Role | Expected repository relationship | License handling |
|---|---|---|---|
| CNA | Game loop, graphics, input, audio, content | Sibling checkout | Keep CNA copyright and license notices |
| sharp-runtime | .NET-like C++ runtime services | Sibling checkout required by CNA | Keep sharp-runtime copyright and license notices |
| easy-gl | Recommended CNA OpenGL backend | Sibling checkout when using EASYGL | Keep easy-gl notices |
| Mesh Craft | Authoring MC3 scenes and conversion to glTF/GLB | External tool/sibling checkout; not linked into the prototype | Keep Mesh Craft notices and record generated-source provenance |
| SDL and CNA transitive dependencies | Platform, windowing, media, compression | Provided by CNA's build | Distribute required notices with releases |

No external textures, sounds, music, fonts, character models, vehicle models, or other downloaded content are currently bundled. Future additions must be entered in `assets/licenses/asset-registry.csv` and reviewed for commercial use, modification, attribution, and redistribution rights.
