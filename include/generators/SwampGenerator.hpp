// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "ChunkGenerator.hpp"
namespace MeshWorld {
class SwampGenerator final : public ChunkGenerator {
public:
    std::string generate(const ChunkContext& ctx) override;
    std::vector<ModelPlacement> placements(const ChunkContext& ctx) const override;
};
} // namespace MeshWorld
