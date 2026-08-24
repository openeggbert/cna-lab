#include "explore2d/Session.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace explore2d {
namespace {

[[nodiscard]] int verbIndex(const Verb verb) noexcept {
    switch (verb) {
    case Verb::use: return 0;
    case Verb::examine: return 1;
    case Verb::take: return 2;
    case Verb::context: return 1;
    }
    return 1;
}

[[nodiscard]] Verb indexedVerb(int index) noexcept {
    index %= 3;
    if (index < 0) index += 3;
    switch (index) {
    case 0: return Verb::use;
    case 1: return Verb::examine;
    default: return Verb::take;
    }
}

} // namespace

AdventureSession::AdventureSession(const WorldDefinition& world, SessionConfig config)
    : world_{world}, config_{config},
      language_{world.localization.normalized(world.localization.defaultLanguage)}
{
    const auto errors = world_.validate();
    if (!errors.empty()) {
        throw std::invalid_argument{"invalid Explore2D world: " + errors.front()};
    }
    restart();
}

void AdventureSession::restart() {
    inventory_.clear();
    flags_.clear();
    counters_.clear();
    visitedRooms_.clear();
    unlockedTravel_.clear();
    choices_.clear();
    messageQueue_.clear();
    activeMessage_.reset();
    messageTargetId_.reset();
    pendingSoundEffects_.clear();
    activeAnimations_.clear();
    sceneElapsedSeconds_ = 0.0F;
    poseTimeRemaining_ = 0.0F;
    terminalMessage_ = {};
    pendingTarget_.reset();
    selectionIndex_ = 0;
    choicePurpose_ = ChoicePurpose::none;
    selectedVerb_ = Verb::examine;
    mode_ = SessionMode::world;
    enterRoom(world_.startRoom);
}

const RoomDefinition& AdventureSession::currentRoom() const {
    const auto* room = world_.room(currentRoomId_);
    if (room == nullptr) {
        throw std::logic_error{"current Explore2D room disappeared"};
    }
    return *room;
}

bool AdventureSession::hasItem(const std::string_view id) const {
    return inventory_.contains(std::string{id});
}

bool AdventureSession::flag(const std::string_view key) const {
    const auto it = flags_.find(std::string{key});
    return it != flags_.end() && it->second;
}

int AdventureSession::counter(const std::string_view key) const {
    const auto it = counters_.find(std::string{key});
    return it == counters_.end() ? 0 : it->second;
}

bool AdventureSession::conditionSatisfied(const Condition& condition) const {
    switch (condition.type) {
    case ConditionType::flagSet: return flag(condition.key);
    case ConditionType::flagClear: return !flag(condition.key);
    case ConditionType::hasItem: return hasItem(condition.key);
    case ConditionType::lacksItem: return !hasItem(condition.key);
    case ConditionType::counterAtLeast: return counter(condition.key) >= condition.value;
    case ConditionType::counterEquals: return counter(condition.key) == condition.value;
    case ConditionType::roomVisited: return visitedRooms_.contains(condition.key);
    }
    return false;
}

bool AdventureSession::allConditionsSatisfied(const std::vector<Condition>& conditions) const {
    return std::ranges::all_of(conditions, [this](const Condition& c) { return conditionSatisfied(c); });
}

bool AdventureSession::hotspotVisible(const HotspotDefinition& hotspot) const {
    return allConditionsSatisfied(hotspot.visibleWhen);
}

bool AdventureSession::hazardActive(const HazardDefinition& hazard) const {
    return allConditionsSatisfied(hazard.activeWhen);
}

Rect AdventureSession::playerRect() const noexcept {
    return {player_.position.x, player_.position.y, config_.playerSize.x, config_.playerSize.y};
}

bool AdventureSession::isSupported() const {
    const Rect p = playerRect();
    constexpr float epsilon = 1.5F;
    for (const Rect& solid : currentRoom().solids) {
        const bool horizontal = p.right() > solid.left() + 1.0F && p.left() < solid.right() - 1.0F;
        if (horizontal && std::abs(p.bottom() - solid.top()) <= epsilon) {
            return true;
        }
    }
    return false;
}

bool AdventureSession::horizontalBlocked(const Rect& candidate) const {
    for (const Rect& solid : currentRoom().solids) {
        if (candidate.intersects(solid)) {
            return true;
        }
    }
    return false;
}

const ExitDefinition* AdventureSession::exitFor(const Direction direction) const noexcept {
    const auto& exits = currentRoom().exits;
    const auto it = std::ranges::find_if(exits, [direction](const ExitDefinition& e) { return e.direction == direction; });
    return it == exits.end() ? nullptr : &*it;
}

bool AdventureSession::tryExit(const Direction direction) {
    const ExitDefinition* exit = exitFor(direction);
    if (exit == nullptr) return false;
    if (!allConditionsSatisfied(exit->availableWhen)) {
        if (!exit->blockedMessage.empty()) showSystemMessage(exit->blockedMessage);
        return false;
    }
    enterRoom(exit->destinationRoom, exit->spawn);
    return true;
}

void AdventureSession::enterRoom(const std::string_view roomId, const std::optional<Vec2> spawn) {
    const RoomDefinition* room = world_.room(roomId);
    if (room == nullptr) return;
    currentRoomId_ = room->id;
    player_.position = spawn.value_or(room->defaultSpawn);
    player_.verticalVelocity = 0.0F;
    player_.pose = PlayerPose::standing;
    poseTimeRemaining_ = 0.0F;
    sceneElapsedSeconds_ = 0.0F;
    activeAnimations_.clear();
    player_.grounded = isSupported();
    visitedRooms_.insert(room->id);
    if (room->travelAnchor) unlockedTravel_.insert(room->id);
}

void AdventureSession::walk(const Direction direction) {
    if (mode_ != SessionMode::world || (direction != Direction::left && direction != Direction::right)) return;
    const Facing desired = direction == Direction::left ? Facing::left : Facing::right;
    if (config_.turnBeforeWalk && player_.facing != desired) {
        player_.facing = desired;
        return;
    }
    player_.facing = desired;
    player_.pose = player_.grounded ? PlayerPose::standing : PlayerPose::jumping;
    const float dx = direction == Direction::left ? -config_.walkStep : config_.walkStep;
    Rect candidate = playerRect();
    candidate.x += dx;
    if (!horizontalBlocked(candidate)) player_.position.x += dx;

    const float worldLeft = ScreenMetrics::worldBounds.x;
    const float worldRight = ScreenMetrics::worldBounds.right();
    if (player_.position.x + config_.playerSize.x < worldLeft) {
        if (!tryExit(Direction::left)) player_.position.x = worldLeft;
    } else if (player_.position.x > worldRight) {
        if (!tryExit(Direction::right)) player_.position.x = worldRight - config_.playerSize.x;
    }
    if (!isSupported()) {
        player_.grounded = false;
        player_.pose = PlayerPose::jumping;
    }
    checkHazards();
}

void AdventureSession::jump() {
    if (!player_.grounded) return;
    player_.grounded = false;
    player_.verticalVelocity = config_.jumpVelocity;
    player_.pose = PlayerPose::jumping;
    queueSoundEffect(world_.presentation.sounds.jump);
}

void AdventureSession::contextNearbyOrJump() {
    const HotspotDefinition* hotspot = nearbyHotspotFor(Verb::context);
    if (hotspot != nullptr) {
        if (const InteractionRule* rule = findRule(Verb::context, hotspot->id, std::nullopt); rule != nullptr) {
            executeRule(*rule);
            return;
        }
    }
    jump();
}

void AdventureSession::jumpOrContext() {
    if (mode_ == SessionMode::message) {
        advanceMessage();
        return;
    }
    if (mode_ == SessionMode::choice || mode_ == SessionMode::map) {
        confirm();
        return;
    }
    if (mode_ == SessionMode::dead || mode_ == SessionMode::won) {
        restart();
        return;
    }
    contextNearbyOrJump();
}

void AdventureSession::applyGravity(const float seconds) {
    if (mode_ != SessionMode::world || player_.grounded) return;
    const Rect before = playerRect();
    player_.verticalVelocity = std::min(config_.terminalVelocity, player_.verticalVelocity + config_.gravity * seconds);
    float nextY = player_.position.y + player_.verticalVelocity * seconds;
    Rect after{player_.position.x, nextY, config_.playerSize.x, config_.playerSize.y};

    if (player_.verticalVelocity >= 0.0F) {
        float landingY = std::numeric_limits<float>::max();
        for (const Rect& solid : currentRoom().solids) {
            const bool horizontal = after.right() > solid.left() + 1.0F && after.left() < solid.right() - 1.0F;
            const bool crossedTop = before.bottom() <= solid.top() + 1.0F && after.bottom() >= solid.top();
            if (horizontal && crossedTop) landingY = std::min(landingY, solid.top() - config_.playerSize.y);
        }
        if (landingY != std::numeric_limits<float>::max()) {
            nextY = landingY;
            player_.verticalVelocity = 0.0F;
            player_.grounded = true;
            if (player_.pose != PlayerPose::taking) player_.pose = PlayerPose::standing;
        }
    } else {
        for (const Rect& solid : currentRoom().solids) {
            if (after.intersects(solid) && before.top() >= solid.bottom() - 1.0F) {
                nextY = solid.bottom();
                player_.verticalVelocity = 0.0F;
                break;
            }
        }
    }

    player_.position.y = nextY;
    const float worldTop = ScreenMetrics::worldBounds.y;
    const float worldBottom = ScreenMetrics::worldBounds.bottom();
    if (player_.position.y + config_.playerSize.y < worldTop) {
        if (!tryExit(Direction::up)) {
            player_.position.y = worldTop;
            player_.verticalVelocity = 0.0F;
        }
    } else if (player_.position.y > worldBottom) {
        if (!tryExit(Direction::down)) {
            terminalMessage_ = world_.presentation.interfaceText.fellBeyondEdge;
            mode_ = SessionMode::dead;
            queueSoundEffect(world_.presentation.sounds.death);
        }
    }
}

void AdventureSession::checkHazards() {
    if (mode_ != SessionMode::world) return;
    const Rect p = playerRect();
    for (const HazardDefinition& hazard : currentRoom().hazards) {
        if (hazardActive(hazard) && p.intersects(hazard.area)) {
            terminalMessage_ = hazard.deathMessage;
            mode_ = SessionMode::dead;
            queueSoundEffect(world_.presentation.sounds.death);
            return;
        }
    }
}

void AdventureSession::tick(const float seconds) {
    if (seconds <= 0.0F) return;
    sceneElapsedSeconds_ += seconds;
    if (poseTimeRemaining_ > 0.0F) {
        poseTimeRemaining_ = std::max(0.0F, poseTimeRemaining_ - seconds);
        if (poseTimeRemaining_ == 0.0F && player_.pose == PlayerPose::taking) {
            player_.pose = player_.grounded ? PlayerPose::standing : PlayerPose::jumping;
        }
    }
    for (auto it = activeAnimations_.begin(); it != activeAnimations_.end();) {
        const auto animation = std::ranges::find_if(currentRoom().animations,
            [&it](const SceneAnimationDefinition& candidate) { return candidate.id == it->first; });
        if (animation == currentRoom().animations.end()) {
            it = activeAnimations_.erase(it);
            continue;
        }
        it->second += seconds;
        int durationTicks = 0;
        for (const AnimationFrame& frame : animation->frames) durationTicks += std::max(0, frame.durationTicks);
        const float duration = static_cast<float>(durationTicks) / qbasicTimerTicksPerSecond;
        if (!animation->loop && it->second >= duration) it = activeAnimations_.erase(it);
        else ++it;
    }
    applyGravity(seconds);
    checkHazards();
}

const HotspotDefinition* AdventureSession::nearbyHotspot() const noexcept {
    if (mode_ != SessionMode::world) return nullptr;
    const Rect p = playerRect();
    const HotspotDefinition* best = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    const Vec2 center{p.x + p.width * 0.5F, p.y + p.height * 0.5F};
    for (const HotspotDefinition& hotspot : currentRoom().hotspots) {
        if (!hotspotVisible(hotspot) || !p.intersects(hotspot.interactionArea)) continue;
        const Vec2 hc{hotspot.interactionArea.x + hotspot.interactionArea.width * 0.5F,
            hotspot.interactionArea.y + hotspot.interactionArea.height * 0.5F};
        const float dx = hc.x - center.x;
        const float dy = hc.y - center.y;
        const float d = dx * dx + dy * dy;
        if (d < bestDistance) {
            best = &hotspot;
            bestDistance = d;
        }
    }
    return best;
}

bool AdventureSession::hasApplicableRule(const Verb verb, const std::string_view targetId) const noexcept {
    for (const InteractionRule& rule : world_.interactions) {
        if (rule.verb != verb || rule.targetId != targetId) continue;
        if (!rule.onceFlag.empty() && flag(rule.onceFlag)) continue;
        if (!allConditionsSatisfied(rule.when)) continue;
        return true;
    }
    return false;
}

const HotspotDefinition* AdventureSession::nearbyHotspotFor(const Verb verb) const noexcept {
    if (mode_ != SessionMode::world) return nullptr;
    const Rect p = playerRect();
    const Vec2 center{p.x + p.width * 0.5F, p.y + p.height * 0.5F};
    const HotspotDefinition* bestActionable = nullptr;
    const HotspotDefinition* bestAny = nullptr;
    float bestActionableDistance = std::numeric_limits<float>::max();
    float bestAnyDistance = std::numeric_limits<float>::max();

    for (const HotspotDefinition& hotspot : currentRoom().hotspots) {
        if (!hotspotVisible(hotspot) || !p.intersects(hotspot.interactionArea)) continue;
        const Vec2 hc{hotspot.interactionArea.x + hotspot.interactionArea.width * 0.5F,
            hotspot.interactionArea.y + hotspot.interactionArea.height * 0.5F};
        const float dx = hc.x - center.x;
        const float dy = hc.y - center.y;
        const float d = dx * dx + dy * dy;
        if (d < bestAnyDistance) {
            bestAny = &hotspot;
            bestAnyDistance = d;
        }
        if (hasApplicableRule(verb, hotspot.id) && d < bestActionableDistance) {
            bestActionable = &hotspot;
            bestActionableDistance = d;
        }
    }

    // Context is special: ENTER should jump when no contextual interaction exists,
    // rather than being swallowed by nearby decorative scenery.
    if (verb == Verb::context) return bestActionable;
    return bestActionable != nullptr ? bestActionable : bestAny;
}

const InteractionRule* AdventureSession::findRule(
    const Verb verb,
    const std::string_view targetId,
    const std::optional<std::string>& itemId) const noexcept
{
    const InteractionRule* best = nullptr;
    for (const InteractionRule& rule : world_.interactions) {
        if (rule.verb != verb || rule.targetId != targetId || rule.itemId != itemId) continue;
        if (!rule.onceFlag.empty() && flag(rule.onceFlag)) continue;
        if (!allConditionsSatisfied(rule.when)) continue;
        if (best == nullptr || rule.priority > best->priority) best = &rule;
    }
    return best;
}

void AdventureSession::beginMessages(
    const std::vector<Message>& messages,
    std::optional<std::string> targetId)
{
    if (messages.empty()) return;
    messageTargetId_ = std::move(targetId);
    messageQueue_ = messages;
    activeMessage_ = messageQueue_.front();
    messageQueue_.erase(messageQueue_.begin());
    mode_ = SessionMode::message;
}

void AdventureSession::advanceMessage() {
    if (mode_ != SessionMode::message) return;
    if (!messageQueue_.empty()) {
        activeMessage_ = messageQueue_.front();
        messageQueue_.erase(messageQueue_.begin());
        queueSoundEffect(world_.presentation.sounds.interaction);
        return;
    }
    activeMessage_.reset();
    messageTargetId_.reset();
    mode_ = SessionMode::world;
}

void AdventureSession::showSystemMessage(LocalizedText text) {
    beginMessages({Message{std::move(text), MessageStyle::system}});
}

bool AdventureSession::setLanguage(const std::string_view languageId) {
    if (!world_.localization.supports(languageId)) return false;
    language_ = languageId;
    return true;
}

const HintDefinition* AdventureSession::currentHint() const noexcept {
    const HintDefinition* selected = nullptr;
    for (const HintDefinition& hint : world_.hints) {
        if (!allConditionsSatisfied(hint.when)) continue;
        if (selected == nullptr || hint.priority > selected->priority) selected = &hint;
    }
    return selected;
}

void AdventureSession::queueSoundEffect(const std::string_view id) {
    if (!id.empty()) pendingSoundEffects_.emplace_back(id);
}

std::vector<std::string> AdventureSession::takePendingSoundEffects() {
    std::vector<std::string> result = std::move(pendingSoundEffects_);
    pendingSoundEffects_.clear();
    return result;
}

bool AdventureSession::messageAnchoredToTarget() const noexcept {
    if (!activeMessage_.has_value() || !messageTargetId_.has_value()) return false;
    if (activeMessage_->speaker == MessageSpeaker::target) return true;
    if (activeMessage_->speaker == MessageSpeaker::player) return false;
    return activeMessage_->style == MessageStyle::speech;
}

Vec2 AdventureSession::messageAnchor() const noexcept {
    if (messageAnchoredToTarget()) {
        for (const HotspotDefinition& hotspot : currentRoom().hotspots) {
            if (hotspot.id == *messageTargetId_ && hotspotVisible(hotspot)) {
                return {hotspot.interactionArea.x + hotspot.interactionArea.width * 0.5F,
                    hotspot.interactionArea.y};
            }
        }
    }
    return {player_.position.x + config_.playerSize.x * 0.5F, player_.position.y};
}

std::optional<float> AdventureSession::animationElapsed(const std::string_view id) const noexcept {
    const auto it = activeAnimations_.find(std::string{id});
    if (it == activeAnimations_.end()) return std::nullopt;
    return it->second;
}

void AdventureSession::applyMutation(const Mutation& mutation) {
    switch (mutation.type) {
    case MutationType::setFlag:
        flags_[mutation.key] = true;
        break;
    case MutationType::clearFlag:
        flags_[mutation.key] = false;
        break;
    case MutationType::addItem:
        if (world_.item(mutation.key) != nullptr) inventory_.insert(mutation.key);
        break;
    case MutationType::removeItem:
        inventory_.erase(mutation.key);
        break;
    case MutationType::setCounter:
        counters_[mutation.key] = mutation.value;
        break;
    case MutationType::addCounter:
        counters_[mutation.key] += mutation.value;
        break;
    case MutationType::unlockTravel:
        if (world_.room(mutation.key) != nullptr) unlockedTravel_.insert(mutation.key);
        break;
    case MutationType::moveToRoom:
        enterRoom(mutation.key);
        break;
    case MutationType::playAnimation:
        activeAnimations_.insert_or_assign(mutation.key, 0.0F);
        break;
    case MutationType::killPlayer:
        terminalMessage_ = mutation.text;
        mode_ = SessionMode::dead;
        activeMessage_.reset();
        messageQueue_.clear();
        messageTargetId_.reset();
        queueSoundEffect(world_.presentation.sounds.death);
        break;
    case MutationType::winGame:
        terminalMessage_ = mutation.text;
        mode_ = SessionMode::won;
        activeMessage_.reset();
        messageQueue_.clear();
        messageTargetId_.reset();
        queueSoundEffect(world_.presentation.sounds.victory);
        break;
    }
}

void AdventureSession::executeRule(const InteractionRule& rule) {
    if (!rule.soundEffect.empty()) {
        queueSoundEffect(rule.soundEffect);
    } else {
        const bool warningMessage = std::ranges::any_of(rule.messages,
            [](const Message& message) { return message.style == MessageStyle::warning; });
        const bool addsItem = std::ranges::any_of(rule.mutations,
            [](const Mutation& mutation) { return mutation.type == MutationType::addItem; });
        if (warningMessage) queueSoundEffect(world_.presentation.sounds.warning);
        else if (addsItem) queueSoundEffect(world_.presentation.sounds.pickup);
        else queueSoundEffect(world_.presentation.sounds.interaction);
    }
    if (!rule.onceFlag.empty()) flags_[rule.onceFlag] = true;
    for (const Mutation& mutation : rule.mutations) applyMutation(mutation);
    if (mode_ == SessionMode::dead || mode_ == SessionMode::won) return;
    beginMessages(rule.messages, rule.targetId);
}

void AdventureSession::beginUse() {
    const HotspotDefinition* hotspot = nearbyHotspotFor(Verb::use);
    if (hotspot == nullptr) {
        showSystemMessage(world_.presentation.interfaceText.nothingToUseOn);
        return;
    }
    choices_.clear();
    for (const std::string& itemId : inventory_) {
        const ItemDefinition* item = world_.item(itemId);
        if (item != nullptr && item->usable) {
            choices_.push_back({std::string{localize(item->label)}, std::nullopt, itemId});
        }
    }
    if (choices_.empty()) {
        showSystemMessage(world_.presentation.interfaceText.nothingUsable);
        return;
    }
    pendingTarget_ = hotspot->id;
    selectionIndex_ = 0;
    choicePurpose_ = ChoicePurpose::useItem;
    mode_ = SessionMode::choice;
}

void AdventureSession::beginExamine() {
    choices_.clear();
    if (const HotspotDefinition* hotspot = nearbyHotspotFor(Verb::examine); hotspot != nullptr) {
        choices_.push_back({std::string{localize(hotspot->label)}, hotspot->id, std::nullopt});
    }
    for (const std::string& itemId : inventory_) {
        if (const ItemDefinition* item = world_.item(itemId); item != nullptr) {
            choices_.push_back({std::string{localize(item->label)}, std::nullopt, itemId});
        }
    }
    if (choices_.empty()) {
        showSystemMessage(world_.presentation.interfaceText.nothingToExamine);
        return;
    }
    selectionIndex_ = 0;
    choicePurpose_ = ChoicePurpose::examineTarget;
    mode_ = SessionMode::choice;
}

void AdventureSession::takeNearby() {
    const HotspotDefinition* hotspot = nearbyHotspotFor(Verb::take);
    if (hotspot == nullptr) {
        showSystemMessage(world_.presentation.interfaceText.nothingToTake);
        return;
    }
    if (const InteractionRule* rule = findRule(Verb::take, hotspot->id, std::nullopt); rule != nullptr) {
        const float playerCenter = player_.position.x + config_.playerSize.x * 0.5F;
        const float targetCenter = hotspot->interactionArea.x + hotspot->interactionArea.width * 0.5F;
        player_.facing = targetCenter < playerCenter ? Facing::left : Facing::right;
        player_.pose = PlayerPose::taking;
        poseTimeRemaining_ = 0.45F;
        executeRule(*rule);
    } else {
        showSystemMessage(world_.presentation.interfaceText.cannotTake);
    }
}

void AdventureSession::performVerb(const Verb verb) {
    if (mode_ != SessionMode::world) return;
    selectedVerb_ = verb == Verb::context ? selectedVerb_ : verb;
    switch (verb) {
    case Verb::use: beginUse(); break;
    case Verb::examine: beginExamine(); break;
    case Verb::take: takeNearby(); break;
    case Verb::context: contextNearbyOrJump(); break;
    }
}

void AdventureSession::performSelectedVerb() { performVerb(selectedVerb_); }

void AdventureSession::cycleVerb(const int delta) {
    if (mode_ != SessionMode::world) return;
    selectedVerb_ = indexedVerb(verbIndex(selectedVerb_) + delta);
    queueSoundEffect(world_.presentation.sounds.menuMove);
}

void AdventureSession::menuMove(const int delta) {
    if ((mode_ != SessionMode::choice && mode_ != SessionMode::map) || choices_.empty()) return;
    const auto size = static_cast<int>(choices_.size());
    int index = static_cast<int>(selectionIndex_) + delta;
    index %= size;
    if (index < 0) index += size;
    selectionIndex_ = static_cast<std::size_t>(index);
    queueSoundEffect(world_.presentation.sounds.menuMove);
}

void AdventureSession::finishChoice() {
    if (choices_.empty() || selectionIndex_ >= choices_.size()) {
        cancel();
        return;
    }
    const ChoiceEntry selected = choices_[selectionIndex_];
    choices_.clear();
    selectionIndex_ = 0;
    mode_ = SessionMode::world;

    if (choicePurpose_ == ChoicePurpose::useItem && pendingTarget_.has_value() && selected.itemId.has_value()) {
        const InteractionRule* rule = findRule(Verb::use, *pendingTarget_, selected.itemId);
        pendingTarget_.reset();
        choicePurpose_ = ChoicePurpose::none;
        if (rule != nullptr) executeRule(*rule);
        else showSystemMessage(world_.presentation.interfaceText.doesNotWork);
        return;
    }

    if (choicePurpose_ == ChoicePurpose::examineTarget) {
        choicePurpose_ = ChoicePurpose::none;
        if (selected.itemId.has_value()) {
            if (const ItemDefinition* item = world_.item(*selected.itemId); item != nullptr) {
                beginMessages({Message{item->description, MessageStyle::inspect}});
                return;
            }
        }
        if (selected.targetId.has_value()) {
            if (const InteractionRule* rule = findRule(Verb::examine, *selected.targetId, std::nullopt); rule != nullptr) {
                executeRule(*rule);
                return;
            }
        }
        showSystemMessage(world_.presentation.interfaceText.noticeNothing);
    }
}

void AdventureSession::openMap() {
    if (mode_ != SessionMode::world) return;
    choices_.clear();
    for (const auto& [roomId, room] : world_.rooms) {
        if (!room.travelAnchor || !unlockedTravel_.contains(roomId)) continue;
        const LocalizedText& label = room.travelLabel.empty() ? room.label : room.travelLabel;
        choices_.push_back({std::string{localize(label)}, roomId, std::nullopt});
    }
    if (choices_.empty()) {
        showSystemMessage(world_.presentation.interfaceText.noTravelDestinations);
        return;
    }
    std::ranges::sort(choices_, [](const ChoiceEntry& a, const ChoiceEntry& b) { return a.label < b.label; });
    selectionIndex_ = 0;
    mode_ = SessionMode::map;
}

void AdventureSession::confirm() {
    if (mode_ == SessionMode::message) {
        advanceMessage();
        return;
    }
    if (mode_ == SessionMode::choice) {
        finishChoice();
        return;
    }
    if (mode_ == SessionMode::map && !choices_.empty()) {
        const ChoiceEntry selected = choices_[selectionIndex_];
        choices_.clear();
        selectionIndex_ = 0;
        mode_ = SessionMode::world;
        if (selected.targetId.has_value()) enterRoom(*selected.targetId);
    }
}

void AdventureSession::cancel() {
    if (mode_ == SessionMode::message) {
        activeMessage_.reset();
        messageQueue_.clear();
        messageTargetId_.reset();
        mode_ = SessionMode::world;
        return;
    }
    if (mode_ == SessionMode::choice || mode_ == SessionMode::map) {
        choices_.clear();
        selectionIndex_ = 0;
        pendingTarget_.reset();
        choicePurpose_ = ChoicePurpose::none;
        mode_ = SessionMode::world;
    }
}

SessionSnapshot AdventureSession::snapshot() const {
    return {currentRoomId_, player_, selectedVerb_, inventory_, flags_, counters_, visitedRooms_, unlockedTravel_};
}

bool AdventureSession::restore(const SessionSnapshot& snapshotValue) {
    if (world_.room(snapshotValue.roomId) == nullptr) return false;
    for (const std::string& itemId : snapshotValue.inventory) {
        if (world_.item(itemId) == nullptr) return false;
    }
    currentRoomId_ = snapshotValue.roomId;
    player_ = snapshotValue.player;
    player_.pose = player_.grounded ? PlayerPose::standing : PlayerPose::jumping;
    selectedVerb_ = snapshotValue.selectedVerb;
    inventory_ = snapshotValue.inventory;
    flags_ = snapshotValue.flags;
    counters_ = snapshotValue.counters;
    visitedRooms_ = snapshotValue.visitedRooms;
    unlockedTravel_ = snapshotValue.unlockedTravel;
    visitedRooms_.insert(currentRoomId_);
    mode_ = SessionMode::world;
    choices_.clear();
    messageQueue_.clear();
    activeMessage_.reset();
    messageTargetId_.reset();
    pendingSoundEffects_.clear();
    activeAnimations_.clear();
    sceneElapsedSeconds_ = 0.0F;
    poseTimeRemaining_ = 0.0F;
    terminalMessage_ = {};
    pendingTarget_.reset();
    choicePurpose_ = ChoicePurpose::none;
    selectionIndex_ = 0;
    return true;
}

} // namespace explore2d
