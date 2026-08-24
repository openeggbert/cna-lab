#pragma once

#include "explore2d/Types.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace explore2d {

struct ItemDefinition final {
    std::string id;
    std::string label;
    std::string description;
    bool usable{true};
};

struct HotspotDefinition final {
    std::string id;
    std::string label;
    Rect interactionArea{};
    HotspotKind kind{HotspotKind::scenery};
    std::vector<Condition> visibleWhen;
    std::vector<Visual> visuals;
};

struct HazardDefinition final {
    std::string id;
    Rect area{};
    std::string deathMessage;
    std::vector<Condition> activeWhen;
};

struct ExitDefinition final {
    Direction direction{Direction::right};
    std::string destinationRoom;
    Vec2 spawn{};
    std::vector<Condition> availableWhen;
    std::string blockedMessage;
};

struct RoomDefinition final {
    std::string id;
    std::string label;
    Rgba background{20, 24, 28, 255};
    Vec2 defaultSpawn{32.0F, 220.0F};
    bool travelAnchor{};
    std::string travelLabel;
    std::vector<Visual> decorations;
    std::vector<Rect> solids;
    std::vector<HotspotDefinition> hotspots;
    std::vector<HazardDefinition> hazards;
    std::vector<ExitDefinition> exits;
};

struct InteractionRule final {
    Verb verb{Verb::examine};
    std::string targetId;
    std::optional<std::string> itemId;
    std::vector<Condition> when;
    std::vector<Message> messages;
    std::vector<Mutation> mutations;
    int priority{};
    std::string onceFlag;
};

class WorldDefinition final {
public:
    std::string title{"Untitled Explore2D"};
    std::string startRoom;
    std::map<std::string, ItemDefinition> items;
    std::map<std::string, RoomDefinition> rooms;
    std::vector<InteractionRule> interactions;

    WorldDefinition& addItem(ItemDefinition item);
    WorldDefinition& addRoom(RoomDefinition room);
    WorldDefinition& addInteraction(InteractionRule rule);

    [[nodiscard]] const RoomDefinition* room(std::string_view id) const noexcept;
    [[nodiscard]] const ItemDefinition* item(std::string_view id) const noexcept;
    [[nodiscard]] std::vector<std::string> validate() const;
};

} // namespace explore2d
