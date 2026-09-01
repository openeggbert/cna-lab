// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "ChunkGenerator.hpp"

namespace MeshWorld {
class CrossroadGenerator final : public ChunkGenerator {
public:
    std::string generate(const ChunkContext& ctx) override;
};
} // namespace MeshWorld
