#include "explore2d/Drawing.hpp"
#include "explore2d/Persistence.hpp"
#include "explore2d/QBasicSound.hpp"
#include "explore2d/Renderer.hpp"
#include "explore2d/Session.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace e2d = explore2d;

static e2d::LocalizedText testText(std::string english, std::string czech) {
    e2d::LocalizedText result{std::move(english)};
    result.addTranslation("cs", std::move(czech));
    return result;
}

static e2d::WorldDefinition makeWorld() {
    e2d::WorldDefinition world;
    world.localization.languages = {
        {"en", testText("English", "Angličtina")},
        {"cs", testText("Czech", "Čeština")},
    };
    world.title = "Test";
    world.startRoom = "a";
    world.addItem({"key", testText("KEY", "KLÍČ"),
        testText("A small test key.", "Malý zkušební klíč."), true});

    e2d::RoomDefinition a;
    a.id = "a";
    a.label = "ROOM A";
    a.defaultSpawn = {60, 232};
    a.travelAnchor = true;
    a.solids.push_back({0, 260, 492, 28});
    a.hotspots.push_back({"box", "BOX", {0, 200, 80, 60}, e2d::HotspotKind::item, {}, {}});
    a.animations.push_back({"box_flash", false, false, {}, {
        {1, {e2d::CircleVisual{{40, 205}, 5, e2d::PaletteColor::brightYellow, false}}},
        {2, {e2d::CircleVisual{{40, 205}, 8, e2d::PaletteColor::brightCyan, false}}},
    }});
    a.animations.push_back({"room_blink", true, true, {}, {
        {1, {e2d::PixelVisual{{300, 100}, e2d::PaletteColor::brightRed}}},
        {1, {e2d::PixelVisual{{300, 100}, e2d::PaletteColor::brightCyan}}},
    }});
    a.exits.push_back({e2d::Direction::right, "b", {5, 232}, {}, {}});
    world.addRoom(std::move(a));

    e2d::RoomDefinition b;
    b.id = "b";
    b.label = "ROOM B";
    b.defaultSpawn = {20, 232};
    b.travelAnchor = true;
    b.solids.push_back({0, 260, 492, 28});
    b.exits.push_back({e2d::Direction::left, "a", {460, 232}, {}, {}});
    world.addRoom(std::move(b));

    world.addInteraction({
        e2d::Verb::take, "box", std::nullopt, {},
        {{testText("You take the key.", "Vezmeš klíč."), e2d::MessageStyle::inspect}},
        {e2d::Mutation::addItem("key"), e2d::Mutation::setFlag("box_taken"),
            e2d::Mutation::playAnimation("box_flash")},
        0, "taken_once"});
    world.addInteraction({
        e2d::Verb::context, "box", std::nullopt, {},
        {{"I am standing beside the box.", e2d::MessageStyle::speech, e2d::MessageSpeaker::player},
            {"The box cannot answer.", e2d::MessageStyle::speech, e2d::MessageSpeaker::target}},
        {}, 0, ""});
    world.addSoundEffect({"pickup", {{440, 1}, {660, 1}}, 0.2F});
    world.addSoundEffect({"talk", {{330, 1}}, 0.15F});
    world.presentation.sounds.pickup = "pickup";
    world.presentation.sounds.interaction = "talk";
    return world;
}

int main() {
    e2d::LocalizedText fallback{"Fallback"};
    fallback.addTranslation("cs", "Náhradní text");
    assert(fallback.resolve("en") == "Fallback");
    assert(fallback.resolve("cs") == "Náhradní text");

    e2d::Canvas primitiveCanvas{32, 32};
    primitiveCanvas.clear(e2d::PaletteColor::black);
    primitiveCanvas.strokeRect({3, 3, 20, 20}, e2d::PaletteColor::brightYellow);
    primitiveCanvas.paint({8, 8}, e2d::PaletteColor::blue, e2d::PaletteColor::brightYellow);
    primitiveCanvas.circle({12, 12}, 4, e2d::PaletteColor::brightCyan, false);
    assert(primitiveCanvas.colorAt(3, 3) == e2d::PaletteColor::brightYellow);
    assert(primitiveCanvas.colorAt(8, 8) == e2d::PaletteColor::blue);
    primitiveCanvas.pixel(1, 1, e2d::PaletteColor::brightCyan);
    const std::size_t cyanPixel = static_cast<std::size_t>((1 * primitiveCanvas.width() + 1) * 4);
    assert(primitiveCanvas.bytes()[cyanPixel] == 85);
    assert(primitiveCanvas.bytes()[cyanPixel + 1] == 255);
    assert(primitiveCanvas.bytes()[cyanPixel + 2] == 255);
    primitiveCanvas.polygon({{{2, 24}, {10, 16}, {18, 24}}}, e2d::PaletteColor::brightGreen, true);
    assert(primitiveCanvas.colorAt(10, 20) == e2d::PaletteColor::brightGreen);
    const e2d::IndexedImage captured = primitiveCanvas.capture({2, 16, 17, 9});
    primitiveCanvas.blit(captured, {2, 2}, e2d::RasterOperation::copy);
    assert(primitiveCanvas.colorAt(10, 6) == e2d::PaletteColor::brightGreen);
    primitiveCanvas.blit(captured, {2, 2}, e2d::RasterOperation::bitXor);
    assert(primitiveCanvas.colorAt(10, 6) == e2d::PaletteColor::black);
    primitiveCanvas.clear(e2d::PaletteColor::black);
    primitiveCanvas.text(0, 0, "ČEŠTINA", e2d::PaletteColor::white);
    assert(primitiveCanvas.textWidth("ČEŠTINA") == 7 * 6);
    assert(primitiveCanvas.textWidth("ÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ") == 15 * 6);
    assert(primitiveCanvas.colorAt(1, 0) == e2d::PaletteColor::white);
    assert(primitiveCanvas.colorAt(2, 0) == e2d::PaletteColor::black);

    e2d::Drawing drawing;
    drawing.origin({10, 12})
        .line({0, 0}, {5, 5}, e2d::PaletteColor::white)
        .arc({8, 8}, {5, 3}, 0.0F, 3.14F, e2d::PaletteColor::brightCyan)
        .polygon({{0, 8}, {4, 2}, {8, 8}}, e2d::PaletteColor::brightYellow);
    assert(drawing.visuals().size() == 3);
    assert(std::get<e2d::LineVisual>(drawing.visuals().front()).from == e2d::Vec2(10, 12));

    const e2d::ToneEffectDefinition tones{"test", {{440, 1}, {0, 1}}, 0.25F};
    const auto samples = e2d::synthesizeToneEffect(tones, 18207);
    assert(samples.size() >= 1999 && samples.size() <= 2001);
    assert(samples.front() > 0);
    assert(samples.back() == 0);

    auto world = makeWorld();
    assert(world.validate().empty());
    e2d::AdventureSession session{world};
    assert(session.language() == "en");
    assert(!session.setLanguage("missing"));
    assert(session.setLanguage("cs"));
    assert(session.localize(world.item("key")->label) == "KLÍČ");
    assert(session.currentRoomId() == "a");
    assert(session.unlockedTravel().contains("a"));

    e2d::AdventureRenderer animationRenderer{world};
    animationRenderer.render(session);
    assert(animationRenderer.canvas().colorAt(308, 108) == e2d::PaletteColor::brightRed);
    session.tick(0.06F);
    animationRenderer.render(session);
    assert(animationRenderer.canvas().colorAt(308, 108) == e2d::PaletteColor::brightCyan);

    assert(session.player().facing == e2d::Facing::right);
    session.performVerb(e2d::Verb::take);
    assert(session.hasItem("key"));
    assert(session.player().pose == e2d::PlayerPose::taking);
    assert(session.player().facing == e2d::Facing::left);
    assert(session.animationElapsed("box_flash").has_value());
    const auto pickupSounds = session.takePendingSoundEffects();
    assert(pickupSounds.size() == 1 && pickupSounds.front() == "pickup");
    assert(session.mode() == e2d::SessionMode::message);
    assert(session.localize(session.activeMessage()->text) == "Vezmeš klíč.");
    assert(session.setLanguage("en"));
    assert(session.localize(session.activeMessage()->text) == "You take the key.");
    assert(session.setLanguage("cs"));
    session.tick(0.5F);
    assert(session.player().pose == e2d::PlayerPose::standing);
    session.advanceMessage();
    assert(session.mode() == e2d::SessionMode::world);

    session.performVerb(e2d::Verb::context);
    assert(session.mode() == e2d::SessionMode::message);
    assert(!session.messageAnchoredToTarget());
    session.advanceMessage();
    assert(session.messageAnchoredToTarget());
    assert(session.messageAnchor() == e2d::Vec2(40, 200));
    session.advanceMessage();

    const auto snapshot = session.snapshot();
    const auto path = std::filesystem::temp_directory_path() / "explore2d-test.e2dsave";
    std::string error;
    assert(e2d::saveSnapshot(snapshot, path, &error));
    auto loaded = e2d::loadSnapshot(path);
    assert(loaded);
    e2d::AdventureSession restored{world};
    assert(restored.restore(*loaded.snapshot));
    assert(restored.hasItem("key"));
    assert(restored.currentRoomId() == "a");
    std::filesystem::remove(path);

    session.openMap();
    assert(session.mode() == e2d::SessionMode::map);
    session.cancel();

    e2d::AdventureRenderer renderer{world};
    renderer.renderTitle(0, "cs");
    assert(renderer.canvas().width() == e2d::ScreenMetrics::width);
    assert(renderer.canvas().height() == e2d::ScreenMetrics::height);
    assert(renderer.canvas().bytes().size() ==
        static_cast<std::size_t>(e2d::ScreenMetrics::width * e2d::ScreenMetrics::height * 4));
    renderer.render(session);
    renderer.renderPause(session, 1);
    renderer.renderSettings(0, "cs", &session);

    std::cout << "Explore2D core tests passed\n";
    return 0;
}
