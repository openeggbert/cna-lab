#include "BlackPineWorld.hpp"

#include "explore2d/Renderer.hpp"
#include "explore2d/Session.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace e2d = explore2d;

static void writePpm(const e2d::Canvas& canvas, const std::filesystem::path& path) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) throw std::runtime_error{"cannot create " + path.string()};
    output << "P6\n" << canvas.width() << ' ' << canvas.height() << "\n255\n";
    const auto bytes = canvas.bytes();
    for (std::size_t index = 0; index < bytes.size(); index += 4U) {
        output.put(static_cast<char>(bytes[index]));
        output.put(static_cast<char>(bytes[index + 1U]));
        output.put(static_cast<char>(bytes[index + 2U]));
    }
}

int main(const int argc, const char* const argv[]) {
    const std::filesystem::path outputDirectory = argc > 1 ? argv[1] : "/tmp/black-pine-preview";
    std::filesystem::create_directories(outputDirectory);

    const auto world = black_pine::buildWorld();
    e2d::AdventureSession session{world};
    e2d::AdventureRenderer renderer{world, {}, black_pine::buildTheme()};

    renderer.renderTitle();
    writePpm(renderer.canvas(), outputDirectory / "title.ppm");
    renderer.render(session);
    writePpm(renderer.canvas(), outputDirectory / "trailhead.ppm");
    session.showSystemMessage("The storm has passed, but the relay is silent. Find Mara in the caretaker cabin.");
    renderer.render(session);
    writePpm(renderer.canvas(), outputDirectory / "message.ppm");
    for (const auto& [roomId, room] : world.rooms) {
        auto snapshot = session.snapshot();
        snapshot.roomId = roomId;
        snapshot.player.position = room.defaultSpawn;
        snapshot.player.grounded = true;
        snapshot.player.verticalVelocity = 0.0F;
        snapshot.visitedRooms.insert(roomId);
        if (!session.restore(snapshot)) throw std::runtime_error{"cannot preview room " + roomId};
        session.cancel();
        renderer.render(session);
        writePpm(renderer.canvas(), outputDirectory / (roomId + ".ppm"));
    }
    std::cout << outputDirectory.string() << '\n';
}
