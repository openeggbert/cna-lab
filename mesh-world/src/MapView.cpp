// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "MapView.hpp"

#include <algorithm>

#include "Map/PlanetConstants.hpp"

namespace MeshWorld {

void MapView::zoom_in() {
    if (level_ >= Map::MAX_LEVEL) return;
    center_ = center_.child(0, 0);
    ++level_;
}

void MapView::zoom_out() {
    if (level_ <= 0) return;
    center_ = center_.parent();
    --level_;
}

void MapView::pan(std::int64_t dx_tiles, std::int64_t dy_tiles) {
    const std::int64_t max_index = (std::int64_t{1} << level_) - 1;
    center_.x = std::clamp<std::int64_t>(center_.x + dx_tiles, 0, max_index);
    center_.y = std::clamp<std::int64_t>(center_.y + dy_tiles, 0, max_index);
}

std::vector<Map::TileCoord> MapView::visible_tiles(int width, int height) const {
    std::vector<Map::TileCoord> out;
    if (width <= 0 || height <= 0) return out;

    const std::int64_t max_index = (std::int64_t{1} << level_) - 1;
    const std::int64_t before_w  = width / 2;
    const std::int64_t before_h  = height / 2;
    out.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    for (std::int64_t dy = -before_h; dy < static_cast<std::int64_t>(height) - before_h; ++dy) {
        const std::int64_t ty = center_.y + dy;
        if (ty < 0 || ty > max_index) continue;
        for (std::int64_t dx = -before_w; dx < static_cast<std::int64_t>(width) - before_w; ++dx) {
            const std::int64_t tx = center_.x + dx;
            if (tx < 0 || tx > max_index) continue;
            out.push_back({level_, tx, ty});
        }
    }
    return out;
}

bool MapView::should_show_label(const std::string& kind, int level) {
    if (kind == "continent") return level >= 0;   // Planet scale and finer
    if (kind == "country")   return level >= 3;    // Continent scale and finer
    if (kind == "capital")   return level >= 3;    // Continent scale and finer
    if (kind == "city")      return level >= 7;    // Region scale and finer
    if (kind == "town")      return level >= 9;    // Metro scale and finer
    if (kind == "village")   return level >= 12;   // City scale and finer
    return level >= 7;  // unrecognized kind: a reasonable middle ground
}

double MapView::scale_denominator(int level, int tiles_across, double screen_width_m) {
    if (tiles_across <= 0 || screen_width_m <= 0.0) return 0.0;
    const double ground_width_m = static_cast<double>(tiles_across) * Map::TileCoord::tile_size_m(level);
    return ground_width_m / screen_width_m;
}

} // namespace MeshWorld
