#pragma once

#include "explore2d/World.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace explore2d {

struct SessionSnapshot final {
    std::string roomId;
    PlayerState player{};
    Verb selectedVerb{Verb::examine};
    std::set<std::string> inventory;
    std::map<std::string, bool> flags;
    std::map<std::string, int> counters;
    std::set<std::string> visitedRooms;
    std::set<std::string> unlockedTravel;
};

struct ChoiceEntry final {
    std::string label;
    std::optional<std::string> targetId;
    std::optional<std::string> itemId;
};

class AdventureSession final {
public:
    AdventureSession(const WorldDefinition& world, SessionConfig config = {});

    void restart();
    void resumeFromCheckpoint();
    void tick(float seconds);
    void walk(Direction direction);
    void jumpOrContext();
    void cycleVerb(int delta = 1);
    void performSelectedVerb();
    void performVerb(Verb verb);
    void menuMove(int delta);
    void confirm();
    void cancel();
    void openMap();
    void advanceMessage();
    void showSystemMessage(LocalizedText text);
    [[nodiscard]] bool setLanguage(std::string_view languageId);
    [[nodiscard]] std::vector<std::string> takePendingSoundEffects();

    [[nodiscard]] SessionSnapshot snapshot() const;
    [[nodiscard]] bool restore(const SessionSnapshot& snapshot);

    [[nodiscard]] SessionMode mode() const noexcept { return mode_; }
    [[nodiscard]] Verb selectedVerb() const noexcept { return selectedVerb_; }
    [[nodiscard]] const PlayerState& player() const noexcept { return player_; }
    [[nodiscard]] const RoomDefinition& currentRoom() const;
    [[nodiscard]] std::string_view currentRoomId() const noexcept { return currentRoomId_; }
    [[nodiscard]] const std::set<std::string>& inventory() const noexcept { return inventory_; }
    [[nodiscard]] const std::set<std::string>& visitedRooms() const noexcept { return visitedRooms_; }
    [[nodiscard]] const std::set<std::string>& unlockedTravel() const noexcept { return unlockedTravel_; }
    [[nodiscard]] const std::vector<ChoiceEntry>& choices() const noexcept { return choices_; }
    [[nodiscard]] std::size_t selectionIndex() const noexcept { return selectionIndex_; }
    [[nodiscard]] const std::optional<Message>& activeMessage() const noexcept { return activeMessage_; }
    [[nodiscard]] const HintDefinition* currentHint() const noexcept;
    [[nodiscard]] std::string_view language() const noexcept { return language_; }
    [[nodiscard]] std::string_view localize(const LocalizedText& text) const noexcept {
        return text.resolve(language_);
    }
    [[nodiscard]] Vec2 messageAnchor() const noexcept;
    [[nodiscard]] bool messageAnchoredToTarget() const noexcept;
    [[nodiscard]] std::string_view terminalMessage() const noexcept { return localize(terminalMessage_); }
    [[nodiscard]] float sceneElapsedSeconds() const noexcept { return sceneElapsedSeconds_; }
    [[nodiscard]] std::optional<float> animationElapsed(std::string_view id) const noexcept;
    [[nodiscard]] const HotspotDefinition* nearbyHotspot() const noexcept;
    [[nodiscard]] bool hasItem(std::string_view id) const;
    [[nodiscard]] bool flag(std::string_view key) const;
    [[nodiscard]] int counter(std::string_view key) const;
    [[nodiscard]] bool conditionSatisfied(const Condition& condition) const;
    [[nodiscard]] bool allConditionsSatisfied(const std::vector<Condition>& conditions) const;
    [[nodiscard]] bool hotspotVisible(const HotspotDefinition& hotspot) const;
    [[nodiscard]] bool hazardActive(const HazardDefinition& hazard) const;
    [[nodiscard]] const SessionConfig& config() const noexcept { return config_; }

private:
    enum class ChoicePurpose : std::uint8_t { none, useItem, examineTarget };

    const WorldDefinition& world_;
    SessionConfig config_;
    std::string language_;
    std::string currentRoomId_;
    PlayerState player_{};
    Verb selectedVerb_{Verb::examine};
    SessionMode mode_{SessionMode::world};
    std::set<std::string> inventory_;
    std::map<std::string, bool> flags_;
    std::map<std::string, int> counters_;
    std::set<std::string> visitedRooms_;
    std::set<std::string> unlockedTravel_;
    std::vector<ChoiceEntry> choices_;
    std::size_t selectionIndex_{};
    ChoicePurpose choicePurpose_{ChoicePurpose::none};
    std::optional<std::string> pendingTarget_;
    std::vector<Message> messageQueue_;
    std::optional<Message> activeMessage_;
    LocalizedText terminalMessage_;
    std::optional<std::string> messageTargetId_;
    std::vector<std::string> pendingSoundEffects_;
    std::map<std::string, float> activeAnimations_;
    std::optional<SessionSnapshot> checkpoint_;
    float sceneElapsedSeconds_{};
    float poseTimeRemaining_{};

    [[nodiscard]] Rect playerRect() const noexcept;
    [[nodiscard]] const HotspotDefinition* nearbyHotspotFor(
        Verb verb,
        std::optional<std::string_view> itemId = std::nullopt) const noexcept;
    [[nodiscard]] bool hasApplicableRule(
        Verb verb,
        std::string_view targetId,
        std::optional<std::string_view> itemId = std::nullopt) const noexcept;
    [[nodiscard]] bool isSupported() const;
    [[nodiscard]] bool horizontalBlocked(const Rect& candidate) const;
    [[nodiscard]] const ExitDefinition* exitFor(Direction direction) const noexcept;
    [[nodiscard]] bool tryExit(Direction direction);
    void enterRoom(std::string_view roomId, std::optional<Vec2> spawn = std::nullopt);
    void rememberCheckpoint();
    void applyGravity(float seconds);
    void checkHazards();
    void beginMessages(const std::vector<Message>& messages, std::optional<std::string> targetId = std::nullopt);
    void queueSoundEffect(std::string_view id);
    void executeRule(const InteractionRule& rule);
    [[nodiscard]] const InteractionRule* findRule(
        Verb verb,
        std::string_view targetId,
        const std::optional<std::string>& itemId) const noexcept;
    void applyMutation(const Mutation& mutation);
    void beginUse();
    void beginExamine();
    void takeNearby();
    void contextNearbyOrJump();
    void jump();
    void finishChoice();
};

} // namespace explore2d
