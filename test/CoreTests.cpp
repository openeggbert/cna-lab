#include "explore2d/Persistence.hpp"
#include "explore2d/Session.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace e2d = explore2d;

static e2d::WorldDefinition makeWorld() {
    e2d::WorldDefinition world;
    world.title = "Test";
    world.startRoom = "a";
    world.addItem({"key", "KEY", "A small test key.", true});

    e2d::RoomDefinition a;
    a.id = "a";
    a.label = "ROOM A";
    a.defaultSpawn = {10, 232};
    a.travelAnchor = true;
    a.solids.push_back({0, 260, 512, 28});
    a.hotspots.push_back({"box", "BOX", {0, 200, 80, 60}, e2d::HotspotKind::item, {}, {}});
    a.exits.push_back({e2d::Direction::right, "b", {5, 232}, {}, {}});
    world.addRoom(std::move(a));

    e2d::RoomDefinition b;
    b.id = "b";
    b.label = "ROOM B";
    b.defaultSpawn = {20, 232};
    b.travelAnchor = true;
    b.solids.push_back({0, 260, 512, 28});
    b.exits.push_back({e2d::Direction::left, "a", {480, 232}, {}, {}});
    world.addRoom(std::move(b));

    world.addInteraction({
        e2d::Verb::take, "box", std::nullopt, {},
        {{"You take the key.", e2d::MessageStyle::inspect}},
        {e2d::Mutation::addItem("key"), e2d::Mutation::setFlag("box_taken")},
        0, "taken_once"});
    return world;
}

int main() {
    auto world = makeWorld();
    assert(world.validate().empty());
    e2d::AdventureSession session{world};
    assert(session.currentRoomId() == "a");
    assert(session.unlockedTravel().contains("a"));

    session.performVerb(e2d::Verb::take);
    assert(session.hasItem("key"));
    assert(session.mode() == e2d::SessionMode::message);
    session.advanceMessage();
    assert(session.mode() == e2d::SessionMode::world);

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

    std::cout << "Explore2D core tests passed\n";
    return 0;
}
