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
    LocalizedText label;
    LocalizedText description;
    bool usable{true};
};

struct HotspotDefinition final {
    std::string id;
    LocalizedText label;
    Rect interactionArea{};
    HotspotKind kind{HotspotKind::scenery};
    std::vector<Condition> visibleWhen;
    std::vector<Visual> visuals;
};

struct HazardDefinition final {
    std::string id;
    Rect area{};
    LocalizedText deathMessage;
    std::vector<Condition> activeWhen;
};

struct ExitDefinition final {
    Direction direction{Direction::right};
    std::string destinationRoom;
    Vec2 spawn{};
    std::vector<Condition> availableWhen;
    LocalizedText blockedMessage;
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
    LocalizedText label;
    PaletteColor background{PaletteColor::black};
    Vec2 defaultSpawn{32.0F, 220.0F};
    bool travelAnchor{};
    LocalizedText travelLabel;
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
    LocalizedText subtitle{"AN EXPLORE2D ADVENTURE"};
    LocalizedText byline{"CREATED WITH EXPLORE2D"};
    LocalizedText startLabel{"NEW GAME"};
    LocalizedText loadLabel{"LOAD GAME"};
    LocalizedText settingsLabel{"SETTINGS"};
    LocalizedText quitLabel{"QUIT"};
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

// Engine-owned wording is also game-configurable and localizable. A game may
// leave translations out; LocalizedText then falls back to these English
// strings rather than displaying a missing-key marker.
struct InterfaceTextDefinition final {
    LocalizedText inventoryEmpty{"(NOTHING)"};
    LocalizedText verbUse{"USE"};
    LocalizedText verbExamine{"EXAMINE"};
    LocalizedText verbTake{"TAKE"};
    LocalizedText useWhat{"USE WHAT?"};
    LocalizedText confirmCancel{"ENTER / ESC"};
    LocalizedText travelMap{"TRAVEL MAP"};
    LocalizedText travelHelp{"ARROWS + ENTER   ESC BACK"};
    LocalizedText messageAdvance{"ENTER"};
    LocalizedText missionComplete{"MISSION COMPLETE"};
    LocalizedText missionFailed{"MISSION FAILED"};
    LocalizedText restartPrompt{"ENTER TO RESTART"};
    LocalizedText paused{"GAME PAUSED"};
    LocalizedText resume{"RESUME GAME"};
    LocalizedText settings{"SETTINGS"};
    LocalizedText returnToTitle{"RETURN TO TITLE"};
    LocalizedText language{"LANGUAGE"};
    LocalizedText back{"BACK"};
    LocalizedText settingsHelp{"LEFT / RIGHT CHANGE   ESC BACK"};
    LocalizedText nothingToUseOn{"There is nothing close enough to use an item on."};
    LocalizedText nothingUsable{"You are not carrying anything usable."};
    LocalizedText nothingToExamine{"There is nothing here that catches your eye."};
    LocalizedText nothingToTake{"There is nothing within reach to take."};
    LocalizedText cannotTake{"You cannot take that."};
    LocalizedText doesNotWork{"That does not seem to work here."};
    LocalizedText noticeNothing{"You notice nothing unusual."};
    LocalizedText noTravelDestinations{"No travel destinations have been discovered yet."};
    LocalizedText gameSaved{"Game saved."};
    LocalizedText saveFailed{"Save failed."};
    LocalizedText loadFailed{"Load failed."};
    LocalizedText loadWorldMismatch{"Load failed: save does not match this world."};
    LocalizedText gameLoaded{"Game loaded."};
    LocalizedText fellBeyondEdge{"You fell beyond the edge of the screen."};
};

struct PresentationDefinition final {
    TitleScreenDefinition title;
    SoundBindings sounds;
    InterfaceTextDefinition interfaceText;
    LocalizedText inventoryHeading{"CARRYING"};
    LocalizedText creditLine{"CREATED WITH EXPLORE2D"};
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
    LocalizedText title{"Untitled Explore2D"};
    std::string startRoom;
    std::map<std::string, ItemDefinition> items;
    std::map<std::string, RoomDefinition> rooms;
    std::map<std::string, ToneEffectDefinition> soundEffects;
    std::vector<InteractionRule> interactions;
    LocalizationDefinition localization;
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
