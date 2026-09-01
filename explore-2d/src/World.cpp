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

Mutation Mutation::setFlag(std::string key) { return {MutationType::setFlag, std::move(key), 0, {}}; }
Mutation Mutation::clearFlag(std::string key) { return {MutationType::clearFlag, std::move(key), 0, {}}; }
Mutation Mutation::addItem(std::string item) { return {MutationType::addItem, std::move(item), 0, {}}; }
Mutation Mutation::removeItem(std::string item) { return {MutationType::removeItem, std::move(item), 0, {}}; }
Mutation Mutation::setCounter(std::string key, const int value) { return {MutationType::setCounter, std::move(key), value, {}}; }
Mutation Mutation::addCounter(std::string key, const int delta) { return {MutationType::addCounter, std::move(key), delta, {}}; }
Mutation Mutation::unlockTravel(std::string room) { return {MutationType::unlockTravel, std::move(room), 0, {}}; }
Mutation Mutation::moveTo(std::string room) { return {MutationType::moveToRoom, std::move(room), 0, {}}; }
Mutation Mutation::playAnimation(std::string animation) { return {MutationType::playAnimation, std::move(animation), 0, {}}; }
Mutation Mutation::kill(LocalizedText message) { return {MutationType::killPlayer, {}, 0, std::move(message)}; }
Mutation Mutation::win(LocalizedText message) { return {MutationType::winGame, {}, 0, std::move(message)}; }

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

WorldDefinition& WorldDefinition::addSoundEffect(ToneEffectDefinition soundEffectValue) {
    soundEffects.insert_or_assign(soundEffectValue.id, std::move(soundEffectValue));
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

const ToneEffectDefinition* WorldDefinition::soundEffect(const std::string_view id) const noexcept {
    const auto it = soundEffects.find(std::string{id});
    return it == soundEffects.end() ? nullptr : &it->second;
}

std::vector<std::string> WorldDefinition::validate() const {
    std::vector<std::string> errors;
    std::set<std::string> languageIds;
    for (const LanguageDefinition& language : localization.languages) {
        if (language.id.empty()) errors.emplace_back("language id must be non-empty");
        else if (!languageIds.insert(language.id).second) {
            errors.push_back("language id must be unique: " + language.id);
        }
    }
    if (localization.languages.empty()) errors.emplace_back("at least one language must be defined");
    else if (!languageIds.contains(localization.defaultLanguage)) {
        errors.push_back("defaultLanguage does not name a supported language: " + localization.defaultLanguage);
    }
    if (startRoom.empty() || room(startRoom) == nullptr) {
        errors.emplace_back("startRoom does not name an existing room");
    }

    std::set<std::string> hotspots;
    std::set<std::string> animations;
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
        for (const SceneAnimationDefinition& animation : roomValue.animations) {
            if (animation.id.empty() || !animations.insert(animation.id).second) {
                errors.push_back("animation id must be non-empty and globally unique: " + animation.id);
            }
            if (animation.frames.empty()) errors.push_back("animation has no frames: " + animation.id);
            for (const AnimationFrame& frame : animation.frames) {
                if (frame.durationTicks <= 0) errors.push_back("animation frame duration must be positive: " + animation.id);
            }
        }
    }

    for (const auto& [soundId, sound] : soundEffects) {
        if (sound.id != soundId) errors.push_back("sound effect map key/id mismatch for " + soundId);
        if (sound.steps.empty()) errors.push_back("sound effect has no tone steps: " + soundId);
        if (sound.volume < 0.0F || sound.volume > 1.0F) errors.push_back("sound effect volume is outside 0..1: " + soundId);
        for (const ToneStep& step : sound.steps) {
            if ((step.frequencyHz != 0 && (step.frequencyHz < 37 || step.frequencyHz > 32767)) || step.durationTicks <= 0) {
                errors.push_back("sound effect contains an invalid QBasic SOUND step: " + soundId);
                break;
            }
        }
    }

    const SoundBindings& sounds = presentation.sounds;
    const std::string* bindings[] = {&sounds.title, &sounds.menuMove, &sounds.menuConfirm,
        &sounds.interaction, &sounds.pickup, &sounds.jump, &sounds.warning, &sounds.death,
        &sounds.victory, &sounds.save, &sounds.load};
    for (const std::string* binding : bindings) {
        if (!binding->empty() && soundEffect(*binding) == nullptr) {
            errors.push_back("presentation references missing sound effect " + *binding);
        }
    }

    for (const auto& rule : interactions) {
        if (!hotspots.contains(rule.targetId)) {
            errors.push_back("interaction targets missing hotspot " + rule.targetId);
        }
        if (rule.itemId.has_value() && item(*rule.itemId) == nullptr) {
            errors.push_back("interaction references missing item " + *rule.itemId);
        }
        if (!rule.soundEffect.empty() && soundEffect(rule.soundEffect) == nullptr) {
            errors.push_back("interaction references missing sound effect " + rule.soundEffect);
        }
        for (const Mutation& mutation : rule.mutations) {
            if (mutation.type == MutationType::playAnimation && !animations.contains(mutation.key)) {
                errors.push_back("interaction references missing animation " + mutation.key);
            }
        }
    }
    return errors;
}

} // namespace explore2d
