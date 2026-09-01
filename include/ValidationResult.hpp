// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include <string>
#include <vector>

namespace MeshWorld {

// R108 -- stats fields below (generator_id, object_count, material_count/
// materials_used, light_count) turn the walk validate() already does over
// the XML tree into reusable per-chunk diagnostics (see ChunkDiagnostics,
// ChunkPipeline::get()) instead of being thrown away after the ok/errors/
// warnings check. Best-effort counts, not an exact renderer-side triangle
// budget: object_count/material_count only cover elements the validator
// itself understands (OBJECT_ELEMENTS), and light_count only the four
// <lights> child tags MeshCraft's Mc3XmlWriter emits.
struct ValidationResult {
    bool                     ok = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    // From <metadata>'s generator.id / generator_id (empty if missing/unparsed).
    std::string              generator_id;
    // Count of elements inside <objects> that the validator recognizes
    // (box/plane/cylinder/sphere/cone/ground/instance/icosphere).
    int                      object_count{0};
    // Distinct non-empty "material" attribute values seen anywhere in the doc.
    std::vector<std::string> materials_used;
    int                      material_count{0};
    // Count of <ambient>/<directional>/<spot>/<point> children under <lights>.
    int                      light_count{0};

    // R130b (mesh_world_revival.md §21.4 "performance validation") -- these
    // three fields extend the existing best-effort-count contract above,
    // not a new parallel struct (see this iteration's plan Key Decision 3).
    //
    // Count of <instance> elements specifically, split out of the more
    // generic object_count above (which counts every OBJECT_ELEMENTS tag,
    // instance included).
    int                      instance_count{0};
    // Total triangle count for the whole document, via Mc3MeshBuilder --
    // reuses the EXACT SAME computation ChunkPipeline::get() already makes
    // at src/ChunkPipeline.cpp:306 for ChunkDiagnostics, so callers that
    // only have a ValidationResult (MeshWorldValidate, this validator's own
    // other callers) get it without a second, separate mesh-build pass.
    int                      triangle_count{0};
    // Conservative placeholder, NOT a real batching-aware estimate: defined
    // as == object_count (1 draw call per placed object) because
    // WorldRenderer.cpp doesn't batch or instance anything today. Revisit
    // once R120 (GPU-driven rendering) lands real batching.
    int                      draw_call_estimate{0};

    void add_error(std::string msg)   { errors.push_back(std::move(msg)); ok = false; }
    void add_warning(std::string msg) { warnings.push_back(std::move(msg)); }
};

} // namespace MeshWorld
