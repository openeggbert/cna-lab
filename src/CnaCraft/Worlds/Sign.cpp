#include "Sign.hpp"

#include <algorithm>

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

}
