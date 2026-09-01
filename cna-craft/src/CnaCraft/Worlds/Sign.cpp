#include "Sign.hpp"

#include <algorithm>

#include "Chunk.hpp"

namespace CnaCraft::Worlds {

void SignStore::PlaceSign(int x, int y, int z, int face, const std::string& text) {
    const auto it = std::find_if(signs_.begin(), signs_.end(), [&](const Sign& s) {
        return s.x == x && s.y == y && s.z == z && s.face == face;
    });

    if (text.empty()) {
        if (it != signs_.end()) signs_.erase(it);
        return;
    }

    if (it != signs_.end()) {
        it->text = text;
    } else {
        signs_.push_back(Sign{x, y, z, face, text});
    }
}

bool SignStore::RemoveAllAt(int x, int y, int z) {
    const std::size_t before = signs_.size();
    signs_.erase(std::remove_if(signs_.begin(), signs_.end(),
                                 [&](const Sign& s) { return s.x == x && s.y == y && s.z == z; }),
                 signs_.end());
    return signs_.size() != before;
}

bool SignStore::RemoveAllInColumn(int cx, int cz) {
    const std::size_t before = signs_.size();
    signs_.erase(std::remove_if(signs_.begin(), signs_.end(),
                                 [&](const Sign& s) {
                                     return ChunkCoordOf(s.x) == cx && ChunkCoordOf(s.z) == cz;
                                 }),
                 signs_.end());
    return signs_.size() != before;
}

}
