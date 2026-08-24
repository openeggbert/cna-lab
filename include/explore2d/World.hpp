#pragma once

#include "explore2d/QBasicSound.hpp"
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

struct AnimationFrame final {
    int durationTicks{1};
    std::vector<Visual> visuals;
};

struct SceneAnimationDefinition final {
    std::string id;
    bool autoplay{true};
    bool loop{true};
    std::vector<Condition> visibleWhen;
    std::vector<AnimationFrame> frames;
};

struct RoomDefinition final {
    std::string id;
    std::string label;
    PaletteColor background{PaletteColor::black};
    Vec2 defaultSpawn{32.0F, 220.0F};
    bool travelAnchor{};
    std::string travelLabel;
    std::vector<Visual> decorations;
    std::vector<Rect> solids;
    std::vector<HotspotDefinition> hotspots;
    std::vector<SceneAnimationDefinition> animations;
    std::vector<HazardDefinition> hazards;
    std::vector<ExitDefinition> exits;
};

// Games supply original procedural artwork and wording, while Explore2D keeps
// the recognisable SCREEN 9-era presentation and menu behaviour consistent.
struct TitleScreenDefinition final {
    PaletteColor background{PaletteColor::black};
    PaletteColor border{PaletteColor::brightYellow};
    std::vector<PaletteColor> titleColors{
        PaletteColor::brightCyan,
        PaletteColor::brightMagenta,
        PaletteColor::brightYellow,
        PaletteColor::brightGreen,
    };
    std::string subtitle{"AN EXPLORE2D ADVENTURE"};
    std::string byline{"CREATED WITH EXPLORE2D"};
    std::string startLabel{"NEW GAME"};
    std::string loadLabel{"LOAD GAME"};
    std::string quitLabel{"QUIT"};
    std::vector<Visual> artwork;
};

struct SoundBindings final {
    std::string title;
    std::string menuMove;
    std::string menuConfirm;
    std::string interaction;
    std::string pickup;
    std::string jump;
    std::string warning;
    std::string death;
    std::string victory;
    std::string save;
    std::string load;
};

struct PresentationDefinition final {
    TitleScreenDefinition title;
    SoundBindings sounds;
    std::string inventoryHeading{"CARRYING"};
    std::string creditLine{"CREATED WITH EXPLORE2D"};
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
    std::string soundEffect;
};

class WorldDefinition final {
public:
    std::string title{"Untitled Explore2D"};
    std::string startRoom;
    std::map<std::string, ItemDefinition> items;
    std::map<std::string, RoomDefinition> rooms;
    std::map<std::string, ToneEffectDefinition> soundEffects;
    std::vector<InteractionRule> interactions;
    PresentationDefinition presentation;

    WorldDefinition& addItem(ItemDefinition item);
    WorldDefinition& addRoom(RoomDefinition room);
    WorldDefinition& addInteraction(InteractionRule rule);
    WorldDefinition& addSoundEffect(ToneEffectDefinition soundEffect);

    [[nodiscard]] const RoomDefinition* room(std::string_view id) const noexcept;
    [[nodiscard]] const ItemDefinition* item(std::string_view id) const noexcept;
    [[nodiscard]] const ToneEffectDefinition* soundEffect(std::string_view id) const noexcept;
    [[nodiscard]] std::vector<std::string> validate() const;
};

} // namespace explore2d
