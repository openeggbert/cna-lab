#include "BlackPineWorld.hpp"

#include "explore2d/Renderer.hpp"
#include "explore2d/Session.hpp"

#include <array>
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

static void place(e2d::AdventureSession& session, const std::string& roomId, const e2d::Vec2 position) {
    auto snapshot = session.snapshot();
    snapshot.roomId = roomId;
    snapshot.player.position = position;
    snapshot.player.grounded = true;
    snapshot.player.verticalVelocity = 0.0F;
    snapshot.visitedRooms.insert(roomId);
    if (!session.restore(snapshot)) throw std::runtime_error{"cannot preview room " + roomId};
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
        place(session, roomId, room.defaultSpawn);
        session.cancel();
        renderer.render(session);
        writePpm(renderer.canvas(), outputDirectory / (roomId + ".ppm"));
    }

    auto repaired = session.snapshot();
    for (const auto* flag : {"vehicle_gate_open", "cable_patched", "fuse_installed",
             "battery_linked", "fuel_valve_open", "feeder_isolated", "workshop_open",
             "nightjar_signal_found", "power_on", "mast_calibrated", "act1_complete",
             "bandage_cache_found", "fir_cut", "theo_freed", "theo_rescued", "theo_briefed",
             "echo_route_solved", "quarry_trace_found", "bear_gone", "weather_data_read",
             "lookout_briefed", "hook_fixed", "ravine_rope_fixed", "ravine_descended",
             "culvert_lit", "sluice_closed", "quarry_gate_open", "owen_freed",
             "horn_sounded", "brant_secured", "quarry_tunnel_lit", "hoist_signal_fixed",
             "pulley_repaired", "hoist_running", "act2_complete"}) {
        repaired.flags[flag] = true;
    }
    if (!session.restore(repaired)) throw std::runtime_error{"cannot create repaired relay preview"};
    constexpr std::array repairedRooms{
        "old_service_road_fork", "vehicle_gate", "cable_trench", "generator_shed",
        "battery_room", "fuel_pump_alcove", "transformer_pad", "relay_workshop",
        "lower_relay_hall", "local_control_room", "fallen_fir", "mossy_hollow",
        "echo_grove", "buried_cable_ridge", "bear_meadow", "automatic_weather_station",
        "north_fire_lookout", "ravine_west_lip", "broken_service_bridge",
        "ravine_floor_west", "culvert_mouth", "waterfall_shelf", "ravine_floor_east",
        "quarry_gate", "quarry_office", "crusher_deck", "quarry_magazine",
        "quarry_tunnel", "east_hoist_landing",
    };
    for (const auto* roomId : repairedRooms) {
        const auto* repairedRoom = world.room(roomId);
        place(session, roomId, repairedRoom->defaultSpawn);
        renderer.render(session);
        writePpm(renderer.canvas(), outputDirectory / (std::string{roomId} + "_complete.ppm"));
    }
    std::cout << outputDirectory.string() << '\n';
}
