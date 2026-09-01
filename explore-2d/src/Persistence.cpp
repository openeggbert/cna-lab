#include "explore2d/Persistence.hpp"

#include <fstream>
#include <iomanip>
#include <string>

namespace explore2d {

bool saveSnapshot(const SessionSnapshot& s, const std::filesystem::path& path, std::string* error) {
    std::ofstream out{path, std::ios::trunc};
    if (!out) {
        if (error != nullptr) *error = "cannot open save file for writing";
        return false;
    }
    out << "EXPLORE2D_SAVE 1\n";
    out << "ROOM " << std::quoted(s.roomId) << '\n';
    out << "PLAYER " << s.player.position.x << ' ' << s.player.position.y << ' '
        << static_cast<int>(s.player.facing) << ' ' << s.player.verticalVelocity << ' '
        << (s.player.grounded ? 1 : 0) << '\n';
    out << "VERB " << static_cast<int>(s.selectedVerb) << '\n';
    for (const auto& item : s.inventory) out << "ITEM " << std::quoted(item) << '\n';
    for (const auto& [key, value] : s.flags) out << "FLAG " << std::quoted(key) << ' ' << (value ? 1 : 0) << '\n';
    for (const auto& [key, value] : s.counters) out << "COUNTER " << std::quoted(key) << ' ' << value << '\n';
    for (const auto& room : s.visitedRooms) out << "VISITED " << std::quoted(room) << '\n';
    for (const auto& room : s.unlockedTravel) out << "TRAVEL " << std::quoted(room) << '\n';
    out << "END\n";
    if (!out) {
        if (error != nullptr) *error = "failed while writing save file";
        return false;
    }
    return true;
}

LoadResult loadSnapshot(const std::filesystem::path& path) {
    std::ifstream in{path};
    if (!in) return {std::nullopt, "cannot open save file"};
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "EXPLORE2D_SAVE" || version != 1) {
        return {std::nullopt, "not an Explore2D save version 1"};
    }

    SessionSnapshot s;
    std::string token;
    bool sawRoom = false;
    bool sawPlayer = false;
    while (in >> token) {
        if (token == "END") break;
        if (token == "ROOM") {
            if (!(in >> std::quoted(s.roomId))) return {std::nullopt, "invalid ROOM record"};
            sawRoom = true;
        } else if (token == "PLAYER") {
            int facing = 0;
            int grounded = 0;
            if (!(in >> s.player.position.x >> s.player.position.y >> facing >> s.player.verticalVelocity >> grounded)) {
                return {std::nullopt, "invalid PLAYER record"};
            }
            s.player.facing = facing == 0 ? Facing::left : Facing::right;
            s.player.grounded = grounded != 0;
            sawPlayer = true;
        } else if (token == "VERB") {
            int verb = 0;
            if (!(in >> verb) || verb < 0 || verb > 2) return {std::nullopt, "invalid VERB record"};
            s.selectedVerb = static_cast<Verb>(verb);
        } else if (token == "ITEM") {
            std::string id;
            if (!(in >> std::quoted(id))) return {std::nullopt, "invalid ITEM record"};
            s.inventory.insert(std::move(id));
        } else if (token == "FLAG") {
            std::string key;
            int value = 0;
            if (!(in >> std::quoted(key) >> value)) return {std::nullopt, "invalid FLAG record"};
            s.flags[std::move(key)] = value != 0;
        } else if (token == "COUNTER") {
            std::string key;
            int value = 0;
            if (!(in >> std::quoted(key) >> value)) return {std::nullopt, "invalid COUNTER record"};
            s.counters[std::move(key)] = value;
        } else if (token == "VISITED") {
            std::string room;
            if (!(in >> std::quoted(room))) return {std::nullopt, "invalid VISITED record"};
            s.visitedRooms.insert(std::move(room));
        } else if (token == "TRAVEL") {
            std::string room;
            if (!(in >> std::quoted(room))) return {std::nullopt, "invalid TRAVEL record"};
            s.unlockedTravel.insert(std::move(room));
        } else {
            return {std::nullopt, "unknown save record: " + token};
        }
    }
    if (!sawRoom || !sawPlayer) return {std::nullopt, "save is missing mandatory records"};
    return {std::move(s), {}};
}

} // namespace explore2d
