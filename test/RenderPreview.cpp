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
             "pulley_repaired", "hoist_running", "act2_complete", "met_lila",
             "belt_released", "taken_drive_belt", "taken_oil_can", "taken_hand_mirror",
             "fuel_can_filled", "spark_retrieved", "met_june", "lift_time_known",
             "taken_rail_switch_key", "taken_logger_token", "rail_points_aligned",
             "engine_belt_installed", "engine_plug_installed", "engine_oiled",
             "engine_fueled", "logging_engine_running", "trestle_guard_diverted",
             "trestle_brake_fixed", "elias_contacted", "railway_complete",
             "taken_insulated_boots", "taken_turbine_badge", "spray_shield_fixed",
             "gatehouse_open", "taken_spillway_crank", "spillway_closed", "jonah_briefed",
             "dam_diagram_read", "taken_pump_gasket", "bay_isolated", "taken_dry_cell",
             "pump_gasket_installed", "pump_battery_installed", "intake_tunnel_lit",
             "taken_valve_wheel", "pump_intake_open", "pump_running",
             "taken_magnet_cord", "mine_access_open", "reservoir_complete",
             "taken_respirator", "drift_braced", "taken_filter_housing",
             "respirator_fitted", "ventilation_running", "taken_copper_bus_bar",
             "mine_drained", "lift_fuse_retrieved", "taken_mine_map",
             "taken_research_badge", "taken_punched_card", "mine_cart_ready",
             "lift_fuse_installed", "substation_isolated", "quiet_feed_cut",
             "lift_powered", "flood_order_heard", "research_badge_presented",
             "research_door_open", "act3_complete"}) {
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
        "quarry_tunnel", "east_hoist_landing", "logging_road", "sawmill_yard",
        "sawmill_floor", "saw_filing_room", "boiler_house", "log_pond",
        "workers_bunkhouse", "camp_mess_hall", "camp_office", "rail_spur_west",
        "derelict_logging_engine", "trestle_approach", "east_rail_cut",
        "dam_overlook", "west_abutment", "spillway_walk", "gatehouse",
        "turbine_hall_upper", "turbine_hall_lower", "pump_gallery",
        "flooded_maintenance_bay", "intake_tunnel", "reservoir_shore",
        "valve_garden", "east_access_shaft", "ore_cart_chamber",
        "timber_gallery", "collapsed_drift", "ventilation_room", "copper_vein",
        "mine_pump_station", "flooded_drift", "survey_chamber",
        "freight_lift_bottom", "freight_lift_top", "underground_substation",
        "switchgear_aisle", "cable_vault", "sealed_research_door",
        "ridge_freight_lift",
    };
    for (const auto* roomId : repairedRooms) {
        const auto* repairedRoom = world.room(roomId);
        place(session, roomId, repairedRoom->defaultSpawn);
        renderer.render(session);
        writePpm(renderer.canvas(), outputDirectory / (std::string{roomId} + "_complete.ppm"));
    }
    std::cout << outputDirectory.string() << '\n';
}
