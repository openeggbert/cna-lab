// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ChunkCache.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace MeshWorld {
namespace {

// WorldStreamer owns two independent ChunkCache instances, one per worker,
// that intentionally share one directory. Serialize the infrequent
// create/prune/write operation so an eviction cannot race another worker's
// newly-created file. Reads remain lock-free.
std::mutex g_chunk_cache_write_mutex;

void prune_before_store(const std::string& dir, std::size_t max_entries) {
    if (max_entries == 0) return;

    std::error_code ec;
    std::vector<fs::directory_entry> entries;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".xml") {
            entries.push_back(*it);
        }
        ec.clear();
    }
    if (entries.size() < max_entries) return;

    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        std::error_code a_ec;
        std::error_code b_ec;
        const auto a_time = a.last_write_time(a_ec);
        const auto b_time = b.last_write_time(b_ec);
        if (a_ec != b_ec) return !a_ec; // unreadable timestamps are newest
        return a_time < b_time;
    });

    const std::size_t remove_count = entries.size() - max_entries + 1;
    for (std::size_t i = 0; i < remove_count; ++i) {
        fs::remove(entries[i].path(), ec);
        ec.clear(); // Cache eviction is best-effort, never a generation error.
    }
}

} // namespace

ChunkCache::ChunkCache(std::string dir)
    : ChunkCache(std::move(dir), 0) {}

ChunkCache::ChunkCache(std::string dir, std::size_t max_entries)
    : dir_(std::move(dir)), max_entries_(max_entries) {}

std::string ChunkCache::path_for(const ChunkCoord& c) const {
    return dir_ + "/" + c.to_string() + ".mc3.xml";
}

std::optional<std::string> ChunkCache::load(const ChunkCoord& c) const {
    std::ifstream f(path_for(c));
    if (!f.is_open()) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void ChunkCache::store(const ChunkCoord& c, const std::string& content) const {
    if (dir_.empty()) return;
    std::lock_guard lock(g_chunk_cache_write_mutex);
    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (ec) return;

    const std::string path = path_for(c);
    // Forced regeneration still calls store(). Avoid physical writes when it
    // produced exactly the same deterministic chunk, which is the normal
    // case and otherwise needlessly wears the cache volume.
    if (std::ifstream existing{path}; existing.is_open()) {
        std::ostringstream previous;
        previous << existing.rdbuf();
        if (previous.str() == content) return;
    }

    // Only app callers opt into a cap. Export tools retain their complete
    // output semantics because their default remains unlimited.
    if (!fs::exists(path, ec)) prune_before_store(dir_, max_entries_);
    std::ofstream f(path, std::ios::trunc);
    if (f.is_open()) f << content;
}

bool ChunkCache::has(const ChunkCoord& c) const {
    return fs::exists(path_for(c));
}

} // namespace MeshWorld
