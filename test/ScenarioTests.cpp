#include "BlackPineWorld.hpp"

#include "explore2d/Session.hpp"

#include <cassert>
#include <iostream>
#include <ranges>
#include <string_view>
#include <variant>

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

static void assertHint(const e2d::AdventureSession& session, const std::string_view expected) {
    assert(session.currentHint() != nullptr);
    assert(session.localize(session.currentHint()->text) == expected);
}

static void assertCzech(const e2d::LocalizedText& text) {
    if (!text.empty()) assert(text.translations().contains("cs"));
}

static void assertVisualCzech(const e2d::Visual& visual) {
    if (const auto* text = std::get_if<e2d::TextVisual>(&visual); text != nullptr) {
        assertCzech(text->text);
    }
}

static void assertWorldCzechComplete(const e2d::WorldDefinition& world) {
    assertCzech(world.title);
    for (const auto& language : world.localization.languages) assertCzech(language.label);
    assertCzech(world.presentation.inventoryHeading);
    assertCzech(world.presentation.creditLine);
    assertCzech(world.presentation.title.subtitle);
    assertCzech(world.presentation.title.byline);
    assertCzech(world.presentation.title.startLabel);
    assertCzech(world.presentation.title.loadLabel);
    assertCzech(world.presentation.title.settingsLabel);
    assertCzech(world.presentation.title.quitLabel);
    for (const e2d::Visual& visual : world.presentation.title.artwork) assertVisualCzech(visual);

    const auto& ui = world.presentation.interfaceText;
    const e2d::LocalizedText* uiTexts[] = {
        &ui.inventoryEmpty, &ui.verbUse, &ui.verbExamine, &ui.verbTake,
        &ui.useWhat, &ui.confirmCancel, &ui.travelMap, &ui.travelHelp,
        &ui.messageAdvance, &ui.missionComplete, &ui.missionFailed,
        &ui.restartPrompt, &ui.paused, &ui.resume, &ui.settings,
        &ui.returnToTitle, &ui.language, &ui.back, &ui.settingsHelp,
        &ui.help, &ui.nextStep, &ui.closeHelp, &ui.noHint,
        &ui.nothingToUseOn, &ui.nothingUsable, &ui.nothingToExamine,
        &ui.nothingToTake, &ui.cannotTake, &ui.doesNotWork,
        &ui.noticeNothing, &ui.noTravelDestinations, &ui.gameSaved,
        &ui.saveFailed, &ui.loadFailed, &ui.loadWorldMismatch, &ui.gameLoaded,
        &ui.fellBeyondEdge,
    };
    for (const e2d::LocalizedText* text : uiTexts) assertCzech(*text);

    for (const auto& [id, item] : world.items) {
        static_cast<void>(id);
        assertCzech(item.label);
        assertCzech(item.description);
    }
    for (const auto& [id, room] : world.rooms) {
        static_cast<void>(id);
        assertCzech(room.label);
        assertCzech(room.travelLabel);
        for (const e2d::Visual& visual : room.decorations) assertVisualCzech(visual);
        for (const auto& hotspot : room.hotspots) {
            assertCzech(hotspot.label);
            for (const e2d::Visual& visual : hotspot.visuals) assertVisualCzech(visual);
        }
        for (const auto& animation : room.animations) {
            for (const auto& frame : animation.frames) {
                for (const e2d::Visual& visual : frame.visuals) assertVisualCzech(visual);
            }
        }
        for (const auto& hazard : room.hazards) assertCzech(hazard.deathMessage);
        for (const auto& roomExit : room.exits) assertCzech(roomExit.blockedMessage);
    }
    for (const auto& rule : world.interactions) {
        for (const e2d::Message& message : rule.messages) assertCzech(message.text);
        for (const e2d::Mutation& mutation : rule.mutations) assertCzech(mutation.text);
    }
    for (const e2d::HintDefinition& hint : world.hints) assertCzech(hint.text);
}

int main() {
    auto world = black_pine::buildWorld();
    assert(world.validate().empty());
    assertWorldCzechComplete(world);
    assert(world.rooms.size() == 7);
    assert(world.items.size() == 6);
    assert(world.hints.size() == 13);
    assert(world.localization.languages.size() == 2);
    assert(world.localization.supports("en"));
    assert(world.localization.supports("cs"));
    assert(world.item("wrench")->label.resolve("cs") == "MONTÁŽNÍ KLÍČ");
    assert(world.room("generator")->label.resolve("cs") == "GENERÁTOROVNA");
    assert(!world.presentation.title.artwork.empty());
    assert(world.soundEffects.size() >= 11);
    assert(!world.presentation.sounds.pickup.empty());
    assert(world.room("generator")->animations.size() == 2);
    assert(world.room("tower_base")->animations.size() == 1);

    e2d::AdventureSession czechSession{world};
    assert(czechSession.setLanguage("cs"));
    assert(czechSession.currentHint() != nullptr);
    assert(czechSession.localize(czechSession.currentHint()->text) ==
        "Na výchozišti SEBER propojovací kabel ze skříňky. Bude potřeba u generátoru.");
    moveSession(czechSession, "trailhead", {410, 228});
    czechSession.performVerb(e2d::Verb::take);
    assert(czechSession.mode() == e2d::SessionMode::message);
    assert(czechSession.localize(czechSession.activeMessage()->text) ==
        "Smotáš propojovací kabel a uložíš ho do batohu.");
    assert(czechSession.localize(czechSession.currentHint()->text) ==
        "Jdi doprava do správcovské chaty a promluv s Marou klávesou ENTER.");
    e2d::AdventureRenderer czechRenderer{world};
    czechRenderer.renderSettings(0, "cs", &czechSession);
    czechRenderer.renderHelp(czechSession);
    assert(czechRenderer.canvas().bytes().size() ==
        static_cast<std::size_t>(e2d::ScreenMetrics::width * e2d::ScreenMetrics::height * 4));

    e2d::AdventureSession session{world};
    assertHint(session,
        "At the trailhead, TAKE the patch cable from the toolbox. You will need it for the generator.");

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
    assertHint(session, "Go right to the caretaker cabin and speak to Mara with ENTER.");

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
    assertHint(session, "EXAMINE Mara's desk in the cabin. Her logbook hides the yard key.");

    moveSession(session, "cabin", {214, 220});
    session.performVerb(e2d::Verb::examine);
    assert(session.mode() == e2d::SessionMode::choice);
    session.confirm();
    dismiss(session);
    assert(session.flag("key_revealed"));
    assertHint(session, "TAKE the brass key revealed on the cabin desk.");

    // The key hotspot overlaps the desk; TAKE must still resolve to the actionable pickup.
    moveSession(session, "cabin", {225, 220});
    session.performVerb(e2d::Verb::take);
    dismiss(session);
    assert(session.hasItem("brass_key"));
    assertHint(session, "TAKE the wrench leaning inside the caretaker cabin.");

    moveSession(session, "cabin", {88, 220});
    session.performVerb(e2d::Verb::take);
    dismiss(session);
    assert(session.hasItem("wrench"));
    assertHint(session, "Go right to the relay yard and USE the brass key on the locked gate.");

    // Yard: use an inventory item on a world mechanism.
    moveSession(session, "yard", {390, 220});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "brass_key");
    dismiss(session);
    assert(session.flag("gate_open"));
    assertHint(session,
        "Enter the generator shed beyond the gate and TAKE the spare ceramic fuse.");

    // Generator: collect and consume one item, consume another, then use ENTER on the lever.
    moveSession(session, "generator", {375, 220});
    session.performVerb(e2d::Verb::take);
    dismiss(session);
    assert(session.hasItem("ceramic_fuse"));
    assertHint(session,
        "USE the ceramic fuse on the empty MAIN FUSE holder in the generator.");

    moveSession(session, "generator", {225, 198});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "ceramic_fuse");
    dismiss(session);
    assert(session.flag("fuse_installed"));
    assert(!session.hasItem("ceramic_fuse"));
    assertHint(session,
        "USE the patch cable on the two blue terminals in the generator shed.");

    moveSession(session, "generator", {118, 220});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "patch_cable");
    dismiss(session);
    assert(session.flag("cable_installed"));
    assert(!session.hasItem("patch_cable"));
    assertHint(session,
        "With the fuse and cable installed, operate the generator's main lever with ENTER.");

    moveSession(session, "generator", {250, 220});
    session.jumpOrContext();
    assert(session.flag("power_on"));
    assert(session.animationElapsed("generator_start").has_value());
    const auto powerSounds = session.takePendingSoundEffects();
    assert(std::ranges::find(powerSounds, "power") != powerSounds.end());
    dismiss(session);
    assertHint(session, "Go right to the tower base and climb the ladder with ENTER.");

    // Tower: context transition to another screen, then align the antenna and finish.
    moveSession(session, "tower_base", {395, 220});
    session.jumpOrContext();
    assert(session.currentRoomId() == "tower_top");
    dismiss(session);
    assertHint(session,
        "On the tower platform, USE the wrench on the twisted antenna mount.");

    moveSession(session, "tower_top", {90, 220});
    session.performVerb(e2d::Verb::use);
    chooseItem(session, "wrench");
    assert(session.flag("antenna_aligned"));
    assert(session.animationElapsed("antenna_align").has_value());
    dismiss(session);
    assertHint(session,
        "Use ENTER at the powered relay console to put Black Pine back on the air.");

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
