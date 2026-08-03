# Third-party components

## Dear ImGui

`third_party/imgui/` contains Dear ImGui version 1.92.9b (docking branch), by Omar Cornut and
contributors, licensed under the MIT License. See `third_party/imgui/LICENSE.txt`.

Only Dear ImGui's core is vendored — `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
`imgui_widgets.cpp` and their headers. None of Dear ImGui's own platform or renderer backends are
included or built: cna-editor supplies its own, written against CNA's public API
(`src/viewport/CnaUiRenderer.cpp` and `src/viewport/CnaUiPlatform.cpp`).
