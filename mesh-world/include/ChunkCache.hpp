// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstddef>
#include <string>
#include <optional>
#include "ChunkCoord.hpp"

namespace MeshWorld {

// File-based cache for generated mc3.xml content.
// Cache lives under the directory set in the constructor (default: "cache/chunks").
// Files are named "<x>_<y>.mc3.xml".
class ChunkCache {
public:
    // max_entries == 0 preserves the historical unbounded cache. A positive
    // cap makes the cache disposable: the oldest generated chunk is evicted
    // before adding a new one beyond the limit.
    explicit ChunkCache(std::string dir = "cache/chunks");
    ChunkCache(std::string dir, std::size_t max_entries);

    // Returns cached content if present, otherwise nullopt.
    std::optional<std::string> load(const ChunkCoord& c) const;

    // Stores content for coord. Creates parent directory if needed.
    void store(const ChunkCoord& c, const std::string& content) const;

    // True if a cache entry exists for coord.
    bool has(const ChunkCoord& c) const;

private:
    std::string path_for(const ChunkCoord& c) const;
    std::string dir_;
    std::size_t max_entries_{0};
};

} // namespace MeshWorld
