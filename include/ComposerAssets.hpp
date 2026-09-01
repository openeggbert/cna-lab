// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

namespace MeshWorld {

// R113 v1 (docs/world-composer-design.md §7) -- populates AssetRegistry
// from ObjectDefinitionLibrary's own definitions that carry real
// Mc3AssetMetadata. Call once at startup, after
// ObjectDefinitionLibrary::instance().load_all() -- mirrors
// register_builtin_materials()/register_builtin_styles()'s own
// "populate once, idempotent" convention.
void register_composer_assets();

} // namespace MeshWorld
