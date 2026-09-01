// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <string>
#include <vector>

namespace MeshWorld {

// R132 -- extracts a self-contained, definition-focused MC3 document from
// an already-resolved library document. Roots and every definition reached
// through instance, variant, or asset-metadata LOD references are retained;
// document-owned materials, textures, and SVG textures referenced by that
// retained object graph are retained too. Runtime-script references are
// rejected: their dynamic def:place() dependencies cannot be made standalone
// without entering R104's deliberately separate script-execution scope.
// Library/import identity is
// intentionally removed: the result is a scene/document ready for ordinary
// MC3 or MCB compilation, not another reusable library.
//
// References to absent definitions are rejected rather than
// silently producing a non-standalone result. Material names absent from the
// source document remain legal external material-registry references, the
// same convention existing MC3 scene generation uses.
MeshCraft::Mc3::Mc3Document prune_mc3_dependencies(
    const MeshCraft::Mc3::Mc3Document& resolved_library,
    const std::vector<std::string>& root_definition_ids);

} // namespace MeshWorld
