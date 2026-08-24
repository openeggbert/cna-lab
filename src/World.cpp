#include "explore2d/World.hpp"

#include <set>
#include <utility>

namespace explore2d {

Condition Condition::flag(std::string key) { return {ConditionType::flagSet, std::move(key), 0}; }
Condition Condition::notFlag(std::string key) { return {ConditionType::flagClear, std::move(key), 0}; }
Condition Condition::has(std::string item) { return {ConditionType::hasItem, std::move(item), 0}; }
Condition Condition::lacks(std::string item) { return {ConditionType::lacksItem, std::move(item), 0}; }
Condition Condition::counterAtLeast(std::string key, const int value) { return {ConditionType::counterAtLeast, std::move(key), value}; }
Condition Condition::counterEquals(std::string key, const int value) { return {ConditionType::counterEquals, std::move(key), value}; }
Condition Condition::visited(std::string room) { return {ConditionType::roomVisited, std::move(room), 0}; }

Mutation Mutation::setFlag(std::string key) { return {MutationType::setFlag, std::move(key), 0}; }
Mutation Mutation::clearFlag(std::string key) { return {MutationType::clearFlag, std::move(key), 0}; }
Mutation Mutation::addItem(std::string item) { return {MutationType::addItem, std::move(item), 0}; }
Mutation Mutation::removeItem(std::string item) { return {MutationType::removeItem, std::move(item), 0}; }
Mutation Mutation::setCounter(std::string key, const int value) { return {MutationType::setCounter, std::move(key), value}; }
Mutation Mutation::addCounter(std::string key, const int delta) { return {MutationType::addCounter, std::move(key), delta}; }
Mutation Mutation::unlockTravel(std::string room) { return {MutationType::unlockTravel, std::move(room), 0}; }
Mutation Mutation::moveTo(std::string room) { return {MutationType::moveToRoom, std::move(room), 0}; }
Mutation Mutation::kill(std::string message) { return {MutationType::killPlayer, std::move(message), 0}; }
Mutation Mutation::win(std::string message) { return {MutationType::winGame, std::move(message), 0}; }

WorldDefinition& WorldDefinition::addItem(ItemDefinition itemValue) {
    items.insert_or_assign(itemValue.id, std::move(itemValue));
    return *this;
}

WorldDefinition& WorldDefinition::addRoom(RoomDefinition roomValue) {
    rooms.insert_or_assign(roomValue.id, std::move(roomValue));
    return *this;
}

WorldDefinition& WorldDefinition::addInteraction(InteractionRule rule) {
    interactions.push_back(std::move(rule));
    return *this;
}

const RoomDefinition* WorldDefinition::room(const std::string_view id) const noexcept {
    const auto it = rooms.find(std::string{id});
    return it == rooms.end() ? nullptr : &it->second;
}

const ItemDefinition* WorldDefinition::item(const std::string_view id) const noexcept {
    const auto it = items.find(std::string{id});
    return it == items.end() ? nullptr : &it->second;
}

std::vector<std::string> WorldDefinition::validate() const {
    std::vector<std::string> errors;
    if (startRoom.empty() || room(startRoom) == nullptr) {
        errors.emplace_back("startRoom does not name an existing room");
    }

    std::set<std::string> hotspots;
    for (const auto& [roomId, roomValue] : rooms) {
        if (roomValue.id != roomId) {
            errors.push_back("room map key/id mismatch for " + roomId);
        }
        for (const auto& hotspot : roomValue.hotspots) {
            if (!hotspots.insert(hotspot.id).second) {
                errors.push_back("hotspot id must be globally unique: " + hotspot.id);
            }
        }
        for (const auto& exit : roomValue.exits) {
            if (room(exit.destinationRoom) == nullptr) {
                errors.push_back("room " + roomId + " exits to missing room " + exit.destinationRoom);
            }
        }
    }

    for (const auto& rule : interactions) {
        if (!hotspots.contains(rule.targetId)) {
            errors.push_back("interaction targets missing hotspot " + rule.targetId);
        }
        if (rule.itemId.has_value() && item(*rule.itemId) == nullptr) {
            errors.push_back("interaction references missing item " + *rule.itemId);
        }
    }
    return errors;
}

} // namespace explore2d
