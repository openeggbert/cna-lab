#include "BlackPineContent.hpp"
#include "BlackPineWorld.hpp"

#include "explore2d/Renderer.hpp"
#include "explore2d/Session.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <ranges>
#include <string_view>
#include <variant>

namespace e2d = explore2d;

static void dismiss(e2d::AdventureSession& session) {
    while (session.mode() == e2d::SessionMode::message) session.advanceMessage();
}

static const e2d::HotspotDefinition& hotspot(
    const e2d::WorldDefinition& world,
    const std::string_view targetId,
    std::string* roomId = nullptr)
{
    for (const auto& [candidateRoomId, room] : world.rooms) {
        const auto found = std::ranges::find_if(room.hotspots,
            [targetId](const e2d::HotspotDefinition& candidate) { return candidate.id == targetId; });
        if (found == room.hotspots.end()) continue;
        if (roomId != nullptr) *roomId = candidateRoomId;
        return *found;
    }
    assert(false && "scenario target does not exist");
    return world.rooms.begin()->second.hotspots.front();
}

static void moveToTarget(
    e2d::AdventureSession& session,
    const e2d::WorldDefinition& world,
    const std::string_view targetId)
{
    std::string roomId;
    const auto& target = hotspot(world, targetId, &roomId);
    auto snapshot = session.snapshot();
    snapshot.roomId = roomId;
    snapshot.player.position = {
        target.interactionArea.x + target.interactionArea.width * 0.5F - 7.0F,
        std::clamp(target.interactionArea.bottom() - 28.0F, target.interactionArea.y, 232.0F),
    };
    snapshot.player.verticalVelocity = 0.0F;
    snapshot.player.grounded = true;
    snapshot.visitedRooms.insert(roomId);
    assert(session.restore(snapshot));
}

static void chooseItem(e2d::AdventureSession& session, const std::string_view itemId) {
    assert(session.mode() == e2d::SessionMode::choice);
    const auto& choices = session.choices();
    const auto wanted = std::ranges::find_if(choices,
        [itemId](const e2d::ChoiceEntry& choice) {
            return choice.itemId.has_value() && *choice.itemId == itemId;
        });
    assert(wanted != choices.end());
    const std::size_t wantedIndex = static_cast<std::size_t>(std::distance(choices.begin(), wanted));
    while (session.selectionIndex() != wantedIndex) session.menuMove(1);
    session.confirm();
}

static void take(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view itemId)
{
    moveToTarget(session, world, targetId);
    session.performVerb(e2d::Verb::take);
    assert(session.hasItem(itemId));
    dismiss(session);
}

static void use(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view itemId, const std::string_view expectedFlag)
{
    moveToTarget(session, world, targetId);
    session.performVerb(e2d::Verb::use);
    chooseItem(session, itemId);
    assert(session.flag(expectedFlag));
    dismiss(session);
}

static void context(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view expectedFlag)
{
    moveToTarget(session, world, targetId);
    session.jumpOrContext();
    assert(session.flag(expectedFlag));
    dismiss(session);
}

static void examine(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view expectedFlag)
{
    moveToTarget(session, world, targetId);
    session.performVerb(e2d::Verb::examine);
    assert(session.mode() == e2d::SessionMode::choice);
    session.confirm();
    assert(session.flag(expectedFlag));
    dismiss(session);
}

static void assertCzech(const e2d::LocalizedText& text) {
    if (!text.empty()) assert(text.translations().contains("cs"));
}

static void assertVisualCzech(const e2d::Visual& visual) {
    if (const auto* text = std::get_if<e2d::TextVisual>(&visual); text != nullptr) assertCzech(text->text);
}

static void assertLocalizationComplete(const e2d::WorldDefinition& world) {
    assertCzech(world.title);
    for (const auto& language : world.localization.languages) assertCzech(language.label);
    for (const auto& [id, item] : world.items) {
        static_cast<void>(id);
        assertCzech(item.label);
        assertCzech(item.description);
    }
    for (const auto& [id, room] : world.rooms) {
        static_cast<void>(id);
        assertCzech(room.label);
        assertCzech(room.travelLabel);
        for (const auto& visual : room.decorations) assertVisualCzech(visual);
        for (const auto& hotspot : room.hotspots) {
            assertCzech(hotspot.label);
            for (const auto& visual : hotspot.visuals) assertVisualCzech(visual);
        }
        for (const auto& animation : room.animations) {
            for (const auto& frame : animation.frames) {
                for (const auto& visual : frame.visuals) assertVisualCzech(visual);
            }
        }
        for (const auto& hazard : room.hazards) assertCzech(hazard.deathMessage);
        for (const auto& roomExit : room.exits) assertCzech(roomExit.blockedMessage);
    }
    for (const auto& rule : world.interactions) {
        for (const auto& message : rule.messages) assertCzech(message.text);
        for (const auto& mutation : rule.mutations) assertCzech(mutation.text);
    }
    for (const auto& hint : world.hints) assertCzech(hint.text);
}

int main() {
    const auto world = black_pine::buildWorld();
    assert(world.validate().empty());
    assertLocalizationComplete(world);
    assert(world.rooms.size() == 124);
    assert(world.hints.size() >= 40);
    assert(world.items.size() >= 50);
    assert(world.localization.supports("en"));
    assert(world.localization.supports("cs"));

    std::size_t anchors = 0;
    for (std::size_t i = 0; i < black_pine::content::screens.size(); ++i) {
        const auto& spec = black_pine::content::screens[i];
        const auto* current = world.room(spec.id);
        assert(current != nullptr);
        assert(current->travelAnchor == spec.travelAnchor);
        assert(current->animations.size() == 1);
        assert(!current->decorations.empty());
        anchors += current->travelAnchor ? 1U : 0U;
        if (i > 0) {
            assert(std::ranges::any_of(current->exits, [i](const e2d::ExitDefinition& exit) {
                return exit.direction == e2d::Direction::left
                    && exit.destinationRoom == black_pine::content::screens[i - 1].id;
            }));
        }
        if (i + 1 < black_pine::content::screens.size()) {
            assert(std::ranges::any_of(current->exits, [i](const e2d::ExitDefinition& exit) {
                return exit.direction == e2d::Direction::right
                    && exit.destinationRoom == black_pine::content::screens[i + 1].id;
            }));
        }
    }
    assert(anchors == 17);

    e2d::AdventureSession session{world};
    assert(session.currentRoomId() == "storm_gate_trailhead");
    assert(session.currentHint() != nullptr);

    // Act I — restore the relay chain, return to the generator, and trace Nightjar.
    take(session, world, "s001_take_patch_cable", "patch_cable");
    take(session, world, "s001_take_field_note", "field_note");
    context(session, world, "s007_mara", "met_mara");
    examine(session, world, "s007_mara_desk", "key_revealed");
    take(session, world, "s007_take_brass_key", "brass_key");
    take(session, world, "s008_take_site_map", "site_map");
    take(session, world, "s009_take_wrench", "wrench");
    take(session, world, "s009_take_lineman_gloves", "lineman_gloves");
    take(session, world, "s009_take_pruning_saw", "pruning_saw");
    take(session, world, "s010_take_ceramic_fuse", "ceramic_fuse");
    take(session, world, "s010_take_hand_crank_torch", "hand_crank_torch");
    use(session, world, "s014_vehicle_gate", "brass_key", "vehicle_gate_open");
    use(session, world, "s017_blue_terminals", "patch_cable", "cable_patched");
    use(session, world, "s018_main_fuse_holder", "ceramic_fuse", "fuse_installed");
    use(session, world, "s019_battery_bus", "wrench", "battery_linked");
    use(session, world, "s020_fuel_valve", "wrench", "fuel_valve_open");
    take(session, world, "s020_take_siphon_hose", "siphon_hose");
    use(session, world, "s021_fallen_feeder", "lineman_gloves", "feeder_isolated");
    use(session, world, "s022_locked_cabinet", "brass_key", "workshop_open");
    assert(session.hasItem("multimeter"));
    use(session, world, "s023_nightjar_trunk", "multimeter", "nightjar_signal_found");
    context(session, world, "s018_main_lever", "power_on");
    use(session, world, "s011_weather_mast", "multimeter", "mast_calibrated");
    context(session, world, "s024_direction_console", "act1_complete");

    // Act II — rescue Theo, cross the forest and take the stolen phase coil.
    take(session, world, "s026_take_bandage_roll", "bandage_roll");
    use(session, world, "s027_fallen_fir", "pruning_saw", "fir_cut");
    take(session, world, "s029_take_signal_flare", "signal_flare");
    use(session, world, "s030_theo_branch", "pruning_saw", "theo_freed");
    use(session, world, "s030_theo_wound", "bandage_roll", "theo_rescued");
    context(session, world, "s030_theo", "theo_briefed");
    take(session, world, "s031_take_climbing_rope", "climbing_rope");
    take(session, world, "s031_take_iron_hook", "iron_hook");
    take(session, world, "s031_take_mine_lamp", "mine_lamp");
    take(session, world, "s031_take_compass", "compass");
    take(session, world, "s032_take_charcoal", "charcoal");
    use(session, world, "s033_bearing_route", "compass", "echo_route_solved");
    use(session, world, "s034_cable_posts", "multimeter", "quarry_trace_found");
    use(session, world, "s035_bear_wind", "signal_flare", "bear_gone");
    context(session, world, "s038_nell", "lookout_briefed");
    use(session, world, "s039_anchor_eye", "iron_hook", "hook_fixed");
    use(session, world, "s039_fixed_hook", "climbing_rope", "ravine_rope_fixed");
    use(session, world, "s043_sluice", "wrench", "sluice_closed");
    take(session, world, "s043_take_quarry_office_key", "quarry_office_key");
    use(session, world, "s045_quarry_gate", "quarry_office_key", "quarry_gate_open");
    context(session, world, "s046_owen", "owen_freed");
    context(session, world, "s047_crusher_horn", "horn_sounded");
    use(session, world, "s047_inspection_cage", "brass_key", "brant_secured");
    take(session, world, "s048_take_red_phase_coil", "red_phase_coil");
    take(session, world, "s048_take_survey_notebook", "survey_notebook");
    use(session, world, "s049_hoist_signal", "multimeter", "hoist_signal_fixed");
    use(session, world, "s050_hoist_pulley", "pulley_pin", "pulley_repaired");
    context(session, world, "s050_hoist_controls", "hoist_running");
    assert(session.flag("act2_complete"));

    // Act III — logging railway, dam, mine and freight lift.
    context(session, world, "s052_lila", "met_lila");
    use(session, world, "s053_planer_tension", "wrench", "belt_released");
    take(session, world, "s053_take_drive_belt", "drive_belt");
    take(session, world, "s054_take_oil_can", "oil_can");
    take(session, world, "s054_take_hand_mirror", "hand_mirror");
    use(session, world, "s055_reserve_tank", "siphon_hose", "engine_fueled");
    context(session, world, "s056_log_pike", "spark_retrieved");
    take(session, world, "s057_take_rail_switch_key", "rail_switch_key");
    context(session, world, "s058_june", "met_june");
    use(session, world, "s059_carbon_impression", "hand_mirror", "lift_time_known");
    use(session, world, "s060_rail_points", "rail_switch_key", "rail_points_aligned");
    use(session, world, "s061_engine_belt", "drive_belt", "engine_belt_installed");
    use(session, world, "s061_engine_ignition", "spark_plug", "engine_plug_installed");
    use(session, world, "s061_engine_bearings", "oil_can", "engine_oiled");
    context(session, world, "s061_engine_start", "logging_engine_running");
    context(session, world, "s062_mill_whistle", "trestle_guard_diverted");
    use(session, world, "s062_brake_linkage", "wrench", "trestle_brake_fixed");
    context(session, world, "s063_portable_radio", "elias_contacted");
    take(session, world, "s065_take_insulated_boots", "insulated_boots");
    take(session, world, "s065_take_turbine_badge", "turbine_badge");
    use(session, world, "s066_spray_shield", "wrench", "spray_shield_fixed");
    use(session, world, "s067_gatehouse_reader", "turbine_badge", "gatehouse_open");
    context(session, world, "s067_spillway_crank", "spillway_closed");
    context(session, world, "s067_jonah", "jonah_briefed");
    context(session, world, "s068_power_diagram", "dam_diagram_read");
    take(session, world, "s069_take_pump_gasket", "pump_gasket");
    context(session, world, "s069_bay_breakers", "bay_isolated");
    take(session, world, "s070_take_dry_cell", "dry_cell");
    use(session, world, "s070_pump_flange", "pump_gasket", "pump_gasket_installed");
    use(session, world, "s070_pump_starter", "dry_cell", "pump_battery_installed");
    take(session, world, "s072_take_valve_wheel", "valve_wheel");
    use(session, world, "s074_intake_valve", "valve_wheel", "pump_intake_open");
    context(session, world, "s070_pump_controls", "pump_running");
    take(session, world, "s071_take_magnet_cord", "magnet_cord");
    context(session, world, "s075_shaft_grille", "mine_access_open");
    take(session, world, "s076_take_respirator", "respirator");
    use(session, world, "s077_timber_brace", "wrench", "drift_braced");
    use(session, world, "s079_respirator_filter", "charcoal", "respirator_fitted");
    use(session, world, "s079_fan_starter", "multimeter", "ventilation_running");
    take(session, world, "s080_take_copper_bus_bar", "copper_bus_bar");
    context(session, world, "s081_mine_pump", "mine_drained");
    use(session, world, "s082_submerged_grate", "magnet_cord", "lift_fuse_retrieved");
    assert(session.hasItem("lift_fuse"));
    take(session, world, "s083_take_mine_map", "mine_map");
    take(session, world, "s083_take_research_badge", "research_badge");
    take(session, world, "s083_take_punched_card", "punched_card");
    use(session, world, "s084_lift_fuse_box", "lift_fuse", "lift_fuse_installed");
    context(session, world, "s087_isolation_order", "substation_isolated");
    use(session, world, "s088_quiet_field_feed", "wrench", "quiet_feed_cut");
    use(session, world, "s088_lift_bus", "copper_bus_bar", "lift_powered");
    use(session, world, "s089_research_reader", "research_badge", "research_badge_presented");
    use(session, world, "s089_code_reader", "punched_card", "research_door_open");
    context(session, world, "s090_ridge_lift", "act3_complete");

    // Act IV — observatory infiltration and Nightjar shutdown.
    use(session, world, "s091_tracking_camera", "hand_mirror", "camera_blinded");
    use(session, world, "s094_kitchen_bait", "sealed_ration", "guard_bait_placed");
    context(session, world, "s094_kitchen_timer", "courtyard_patrol_diverted");
    take(session, world, "s095_take_first_aid_kit", "first_aid_kit");
    context(session, world, "s095_kline_recording", "calder_warning_known");
    context(session, world, "s096_project_portraits", "archive_dates_known");
    context(session, world, "s097_archive_drawers", "archive_open");
    assert(session.hasItem("cipher_lens") && session.hasItem("archive_reel"));
    take(session, world, "s098_take_phase_prism", "phase_prism");
    context(session, world, "s098_ventilation_duct", "kline_located");
    use(session, world, "s102_security_keypad", "hand_mirror", "security_office_open");
    take(session, world, "s102_take_dome_key", "dome_key");
    use(session, world, "s099_dome_lock", "dome_key", "dome_open");
    context(session, world, "s099_dome_drive", "dome_aligned");
    assert(session.hasItem("calibration_fork"));
    context(session, world, "s100_telescope", "tower_alignment_known");
    context(session, world, "s101_sable", "sable_persuaded");
    use(session, world, "s103_ante_badge", "research_badge", "ante_badge_open");
    use(session, world, "s103_ante_phrase", "cipher_lens", "ante_phrase_open");
    use(session, world, "s103_ante_tone", "calibration_fork", "bunker_door_open");
    use(session, world, "s104_decon_reader", "research_badge", "decon_authorized");
    context(session, world, "s104_decon_cycle", "decon_complete");
    use(session, world, "s105_guard_intercom", "archive_reel", "bunker_guards_sealed");
    use(session, world, "s106_diagnostic_coil", "red_phase_coil", "diagnostic_coil_ready");
    use(session, world, "s106_diagnostic_prism", "phase_prism", "inversion_calculated");
    context(session, world, "s107_fork_sequence", "protected_sequence_known");
    use(session, world, "s108_test_cell_player", "archive_reel", "calder_testimony_heard");
    use(session, world, "s109_seized_rack", "wrench", "machine_rack_open");
    take(session, world, "s109_take_coolant_hose", "coolant_hose");
    take(session, world, "s109_take_grounding_clamp", "grounding_clamp");
    use(session, world, "s110_capacitor_banks", "grounding_clamp", "capacitors_grounded");
    use(session, world, "s111_split_coolant_line", "coolant_hose", "cooling_diverted");
    use(session, world, "s112_archive_deck", "archive_reel", "command_archive_loaded");
    use(session, world, "s112_archive_decoder", "cipher_lens", "evidence_copied");
    assert(session.hasItem("evidence_spool"));
    context(session, world, "s113_miriam", "kline_freed");
    use(session, world, "s114_dark_stair", "hand_crank_torch", "emergency_stair_lit");
    use(session, world, "s115_summit_override", "override_key", "summit_override_open");
    context(session, world, "s115_summit_sequence", "act4_complete");

    // Act V — ground the tower, invert the field and broadcast the evidence.
    context(session, world, "s116_broken_ground", "summit_ground_fault_found");
    context(session, world, "s117_windbreak", "ledge_crossed");
    use(session, world, "s118_broken_ground_strap", "grounding_clamp", "summit_ground_clamped");
    use(session, world, "s118_ground_clamp_bolt", "wrench", "summit_grounded");
    use(session, world, "s119_tower_feed", "red_phase_coil", "tower_coil_installed");
    context(session, world, "s119_sable_summit", "transmitter_key_received");
    assert(session.hasItem("transmitter_key"));
    use(session, world, "s120_sheltered_ladder", "compass", "mid_tower_crossed");
    use(session, world, "s121_waveguide_prism", "phase_prism", "tower_prism_installed");
    use(session, world, "s121_waveguide_tuning", "calibration_fork", "waveguide_tuned");
    context(session, world, "s122_beacon_crystal", "beacon_reference_ready");
    use(session, world, "s123_azimuth_mount", "wrench", "antenna_aligned");
    use(session, world, "s123_local_motor_lock", "override_key", "antenna_control_locked");
    context(session, world, "s124_voss", "voss_confronted");
    use(session, world, "s124_transmitter_lock", "transmitter_key", "transmitter_unlocked");
    use(session, world, "s124_evidence_loader", "evidence_spool", "evidence_loaded");
    moveToTarget(session, world, "s124_protected_carrier_console");
    session.jumpOrContext();
    assert(session.mode() == e2d::SessionMode::won);
    assert(session.terminalMessage().find("OPEN CHANNEL") != std::string_view::npos);

    e2d::AdventureRenderer renderer{world};
    for (const int number : {1, 14, 25, 39, 52, 64, 76, 91, 104, 124}) {
        auto snapshot = session.snapshot();
        snapshot.roomId = std::string{black_pine::content::screens[static_cast<std::size_t>(number - 1)].id};
        snapshot.player.position = {24, 232};
        assert(session.restore(snapshot));
        renderer.render(session);
        assert(renderer.canvas().bytes().size()
            == static_cast<std::size_t>(e2d::ScreenMetrics::width * e2d::ScreenMetrics::height * 4));
    }

    std::cout << "Black Pine 124-screen full scenario passed\n";
    return 0;
}
