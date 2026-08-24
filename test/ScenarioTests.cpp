#include "BlackPineWorld.hpp"

#include "explore2d/Session.hpp"

#include <cassert>
#include <iostream>
#include <ranges>
#include <string_view>

namespace e2d = explore2d;

static void moveSession(e2d::AdventureSession& session, const char* room, e2d::Vec2 position) {
    auto s = session.snapshot();
    s.roomId = room;
    s.player.position = position;
    s.player.verticalVelocity = 0.0F;
    s.player.grounded = true;
    s.visitedRooms.insert(room);
    assert(session.restore(s));
}

static void dismiss(e2d::AdventureSession& session) {
    while (session.mode() == e2d::SessionMode::message) session.advanceMessage();
}

static void chooseItem(e2d::AdventureSession& session, const std::string_view itemId) {
    assert(session.mode() == e2d::SessionMode::choice);
    const auto& choices = session.choices();
    std::size_t wanted = choices.size();
    for (std::size_t i = 0; i < choices.size(); ++i) {
        if (choices[i].itemId && *choices[i].itemId == itemId) {
            wanted = i;
            break;
        }
    }
    assert(wanted < choices.size());
    while (session.selectionIndex() != wanted) session.menuMove(1);
    session.confirm();
}

int main() {
    auto world = black_pine::buildWorld();
    assert(world.validate().empty());
    assert(world.rooms.size() == 7);
    assert(world.items.size() == 6);
    assert(!world.presentation.title.artwork.empty());
    assert(world.soundEffects.size() >= 11);
    assert(!world.presentation.sounds.pickup.empty());
    assert(world.room("generator")->animations.size() == 2);
    assert(world.room("tower_base")->animations.size() == 1);

    e2d::AdventureSession session{world};

    // Trailhead: collect the required cable and the optional clue note.
    moveSession(session, "trailhead", {410, 228});
    session.performVerb(e2d::Verb::take);
    assert(session.hasItem("patch_cable"));
    assert(session.player().pose == e2d::PlayerPose::taking);
    const auto pickupSounds = session.takePendingSoundEffects();
    assert(std::ranges::find(pickupSounds, "pickup") != pickupSounds.end());
    session.tick(0.5F);
    assert(session.player().pose == e2d::PlayerPose::standing);
    dismiss(session);

    moveSession(session, "trailhead", {250, 228});
    session.performVerb(e2d::Verb::take);
    assert(session.hasItem("field_note"));
    dismiss(session);

    // Cabin: contextual talk, exploration reveals a previously hidden pickup.
    moveSession(session, "cabin", {294, 220});
    session.jumpOrContext();
    assert(session.messageAnchoredToTarget());
    session.advanceMessage();
    assert(!session.messageAnchoredToTarget());
    session.advanceMessage();
    assert(session.messageAnchoredToTarget());
    dismiss(session);
    assert(session.flag("met_mara"));

    moveSession(session, "cabin", {214, 220});
    session.performVerb(e2d::Verb::examine);
    assert(session.mode() == e2d::SessionMode::choice);
    session.confirm();
    dismiss(session);
    assert(session.flag("key_revealed"));

    // The key hotspot overlaps the desk; TAKE must still resolve to the actionable pickup.
    moveSession(session, "cabin", {225, 220});
    session.performVerb(e2d::Verb::take);
    dismiss(session);
    assert(session.hasItem("brass_key"));

    moveSession(session, "cabin", {88, 220});
    session.performVerb(e2d::Verb::take);
    dismiss(session);
    assert(session.hasItem("wrench"));

    // Yard: use an inventory item on a world mechanism.
    moveSession(session, "yard", {390, 220});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "brass_key");
    dismiss(session);
    assert(session.flag("gate_open"));

    // Generator: collect and consume one item, consume another, then use ENTER on the lever.
    moveSession(session, "generator", {375, 220});
    session.performVerb(e2d::Verb::take);
    dismiss(session);
    assert(session.hasItem("ceramic_fuse"));

    moveSession(session, "generator", {225, 198});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "ceramic_fuse");
    dismiss(session);
    assert(session.flag("fuse_installed"));
    assert(!session.hasItem("ceramic_fuse"));

    moveSession(session, "generator", {118, 220});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "patch_cable");
    dismiss(session);
    assert(session.flag("cable_installed"));
    assert(!session.hasItem("patch_cable"));

    moveSession(session, "generator", {250, 220});
    session.jumpOrContext();
    assert(session.flag("power_on"));
    assert(session.animationElapsed("generator_start").has_value());
    const auto powerSounds = session.takePendingSoundEffects();
    assert(std::ranges::find(powerSounds, "power") != powerSounds.end());
    dismiss(session);

    // Tower: context transition to another screen, then align the antenna and finish.
    moveSession(session, "tower_base", {395, 220});
    session.jumpOrContext();
    assert(session.currentRoomId() == "tower_top");
    dismiss(session);

    moveSession(session, "tower_top", {90, 220});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "wrench");
    assert(session.flag("antenna_aligned"));
    assert(session.animationElapsed("antenna_align").has_value());
    dismiss(session);

    moveSession(session, "tower_top", {350, 220});
    session.jumpOrContext();
    assert(session.mode() == e2d::SessionMode::won);
    assert(!session.terminalMessage().empty());
    const auto victorySounds = session.takePendingSoundEffects();
    assert(std::ranges::find(victorySounds, "victory") != victorySounds.end());

    // Hazard/death/restart path is independent from the winning route.
    e2d::AdventureSession danger{world};
    moveSession(danger, "ravine", {180, 232});
    danger.tick(1.0F / 60.0F);
    assert(danger.mode() == e2d::SessionMode::dead);
    assert(!danger.terminalMessage().empty());
    const auto deathSounds = danger.takePendingSoundEffects();
    assert(std::ranges::find(deathSounds, "death") != deathSounds.end());
    danger.jumpOrContext();
    assert(danger.mode() == e2d::SessionMode::world);
    assert(danger.currentRoomId() == "trailhead");

    std::cout << "Black Pine full scenario tests passed\n";
    return 0;
}
