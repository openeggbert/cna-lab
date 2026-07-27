# Third-party dependencies

Iron Shadows's original source code is MIT-licensed. Dependencies keep their own licenses and are not relicensed by this repository.

| Dependency | Role | Expected repository relationship | License handling |
|---|---|---|---|
| CNA | Game loop, graphics, input, audio, content | Sibling checkout | Keep CNA copyright and license notices |
| sharp-runtime | .NET-like C++ runtime services | Sibling checkout required by CNA | Keep sharp-runtime copyright and license notices |
| easy-gl | Recommended CNA OpenGL backend | Sibling checkout when using EASYGL | Keep easy-gl notices |
| cna-extended | ECS, Transform3 scene hierarchy, 3D collision/octree, skinned-model playback | Sibling checkout | Keep cna-extended (MIT) copyright and license notices |
| Jolt Physics | Rigid bodies, character controller, vehicle constraint, raycast/trigger queries (`plan/plan_15-physics-integration.md`) | Shared checkout at `~/deps/jolt`, pinned to tag `v5.6.0` (commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`) | MIT license (Copyright 2021 Jorrit Rouwe); keep Jolt copyright and license notices |
| Mesh Craft | Authoring MC3 scenes and conversion to glTF/GLB | External tool/sibling checkout; not linked into the prototype | Keep Mesh Craft notices and record generated-source provenance |
| SDL and CNA transitive dependencies | Platform, windowing, media, compression | Provided by CNA's build | Distribute required notices with releases |

No external textures, sounds, music, fonts, character models, vehicle models, or other downloaded content are currently bundled. Future additions must be entered in `assets/licenses/asset-registry.csv` and reviewed for commercial use, modification, attribution, and redistribution rights.
