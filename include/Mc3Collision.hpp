// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <string>
#include <vector>

namespace MeshWorld {

// An axis-aligned world-space obstacle.  Y is an inclusive height range, so
// callers can avoid blocking a player with geometry entirely above or below
// their capsule.
struct CollisionBox {
    float min_x{0.0f}, max_x{0.0f};
    float min_z{0.0f}, max_z{0.0f};
    float min_y{0.0f}, max_y{0.0f};
};

enum class CollisionDiagnosticKind {
    UnresolvedDefinition,
    UnsupportedProxy,
    InvalidBounds,
};

// A non-fatal authoring diagnostic.  Collision extraction is deliberately
// permissive: a bad or future proxy does not prevent the rest of a chunk from
// rendering or from supplying valid player blockers.
struct CollisionDiagnostic {
    CollisionDiagnosticKind kind{CollisionDiagnosticKind::UnresolvedDefinition};
    std::string             definition_id;
    std::string             detail;
};

struct CollisionExtractionResult {
    std::vector<CollisionBox>       boxes;
    std::vector<CollisionDiagnostic> diagnostics;
};

// Extracts player blockers from an ordinary MC3 scene document in that
// document's local coordinate system.  Inline boxes remain backwards
// compatible: collision="box" is a blocker.  Reusable instances opt in via
// their resolved definition's assetMetadata.collisionProxy:
//
//   none or empty  -> passable decoration
//   box            -> transformed assetMetadata.boundsMin/Max blocker
//   any other text -> ignored and returned as an UnsupportedProxy diagnostic
//
// This is deliberately renderer-independent: it consumes only the canonical
// MC3 document model and does not include SDL, OpenGL, CNA, or SceneRenderer.
// `min_height_m` filters thin inline boxes and tiny assets such as road marks,
// curbs, lamps and decorative props even when old content accidentally marks
// them as collidable.
CollisionExtractionResult extract_mc3_collision_boxes(
    const MeshCraft::Mc3::Mc3Document& document, float min_height_m = 0.3f);

} // namespace MeshWorld
