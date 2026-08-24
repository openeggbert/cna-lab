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

static std::size_t catalogueIndex(const std::string_view roomId) {
    const auto found = std::ranges::find_if(black_pine::content::screens,
        [roomId](const black_pine::content::Screen& candidate) { return candidate.id == roomId; });
    assert(found != black_pine::content::screens.end());
    return static_cast<std::size_t>(std::distance(black_pine::content::screens.begin(), found));
}

static bool intersectsTarget(
    const e2d::AdventureSession& session,
    const e2d::HotspotDefinition& target)
{
    const e2d::Rect player{session.player().position.x, session.player().position.y, 14.0F, 28.0F};
    return player.intersects(target.interactionArea);
}

static void walkToTarget(
    e2d::AdventureSession& session,
    const e2d::WorldDefinition& world,
    const std::string_view targetId)
{
    std::string roomId;
    const auto& target = hotspot(world, targetId, &roomId);
    std::size_t travelSteps = 0;
    while (session.currentRoomId() != roomId && travelSteps++ < 100000) {
        const std::size_t current = catalogueIndex(session.currentRoomId());
        const std::size_t destination = catalogueIndex(roomId);
        session.walk(destination < current ? e2d::Direction::left : e2d::Direction::right);
        assert(session.mode() == e2d::SessionMode::world);
    }
    assert(session.currentRoomId() == roomId);

    std::size_t approachSteps = 0;
    while (!intersectsTarget(session, target) && approachSteps++ < 1000) {
        const float playerCenter = session.player().position.x + 7.0F;
        const float targetCenter = target.interactionArea.x + target.interactionArea.width * 0.5F;
        session.walk(playerCenter < targetCenter ? e2d::Direction::right : e2d::Direction::left);
        assert(session.mode() == e2d::SessionMode::world);
        assert(session.currentRoomId() == roomId);
    }
    assert(intersectsTarget(session, target));
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
    walkToTarget(session, world, targetId);
    session.performVerb(e2d::Verb::take);
    assert(session.hasItem(itemId));
    dismiss(session);
}

static void use(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view itemId, const std::string_view expectedFlag)
{
    walkToTarget(session, world, targetId);
    session.performVerb(e2d::Verb::use);
    chooseItem(session, itemId);
    assert(session.flag(expectedFlag));
    dismiss(session);
}

static void context(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view expectedFlag)
{
    walkToTarget(session, world, targetId);
    session.jumpOrContext();
    assert(session.flag(expectedFlag));
    dismiss(session);
}

static void portal(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view destinationRoom)
{
    walkToTarget(session, world, targetId);
    session.jumpOrContext();
    dismiss(session);
    if (session.currentRoomId() != destinationRoom) {
        std::cerr << "Portal " << targetId << " stayed in " << session.currentRoomId()
                  << " instead of entering " << destinationRoom << '\n';
    }
    assert(session.currentRoomId() == destinationRoom);
}

static void examine(e2d::AdventureSession& session, const e2d::WorldDefinition& world,
    const std::string_view targetId, const std::string_view expectedFlag)
{
    walkToTarget(session, world, targetId);
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
    assert(world.items.size() == 64);
    assert(world.localization.supports("en"));
    assert(world.localization.supports("cs"));
    const auto& ringingPhone = hotspot(world, "s001_emergency_phone");
    const auto& answeredPhone = hotspot(world, "s001_emergency_phone_complete");
    const auto& markedDeerPath = hotspot(world, "s003_deer_path");
    const auto& returningDeerPath = hotspot(world, "s005_deer_path_return");
    const auto& cabinDoor = hotspot(world, "s006_cabin_door");
    const auto& radioDoor = hotspot(world, "s007_radio_door");
    const auto& cellarHatch = hotspot(world, "s007_cellar_hatch");
    const auto& serviceHatch = hotspot(world, "s010_service_hatch");
    const auto& forestRoute = hotspot(world, "s012_forest_route");
    const auto& trenchLadder = hotspot(world, "s015_trench_ladder");
    const auto& generatorPath = hotspot(world, "s016_generator_path");
    const auto& controlStairs = hotspot(world, "s023_control_stairs");
    const auto& kilnPath = hotspot(world, "s026_kiln_path");
    const auto& blindPath = hotspot(world, "s028_blind_path");
    const auto& cachePath = hotspot(world, "s030_cache_path");
    const auto& ravinePath = hotspot(world, "s036_ravine_path");
    assert(ringingPhone.visuals.size() >= 10);
    assert(answeredPhone.visuals.size() >= 10);
    assert(markedDeerPath.visuals.size() >= 7);
    assert(returningDeerPath.visuals.size() >= 5);
    assert(cabinDoor.visuals.size() >= 4);
    assert(radioDoor.visuals.size() >= 4);
    assert(cellarHatch.visuals.size() >= 4);
    assert(serviceHatch.visuals.size() >= 1);
    assert(forestRoute.visuals.size() >= 7);
    assert(trenchLadder.visuals.size() >= 7);
    assert(generatorPath.visuals.size() >= 2);
    assert(controlStairs.visuals.size() >= 6);
    assert(kilnPath.visuals.size() >= 2);
    assert(blindPath.visuals.size() >= 2);
    assert(cachePath.visuals.size() >= 2);
    assert(ravinePath.visuals.size() >= 2);
    assert(world.room("caretaker_cabin_main")->decorations.size() >= 30);
    assert(world.room("cabin_radio_nook")->decorations.size() >= 20);
    assert(world.room("caretaker_tool_shed")->decorations.size() >= 20);
    assert(world.room("cabin_root_cellar")->decorations.size() >= 25);

    std::size_t anchors = 0;
    std::size_t animatedRooms = 0;
    std::size_t visiblePickups = 0;
    for (std::size_t i = 0; i < black_pine::content::screens.size(); ++i) {
        const auto& spec = black_pine::content::screens[i];
        const auto* current = world.room(spec.id);
        assert(current != nullptr);
        assert(current->travelAnchor == spec.travelAnchor);
        assert(!current->decorations.empty());
        assert(current->label.resolve("en").find("SCREEN") == std::string_view::npos);
        assert(current->label.resolve("cs").find("OBRAZOVKA") == std::string_view::npos);
        animatedRooms += current->animations.empty() ? 0U : 1U;
        const std::string observationFlag = "once_observed_" + std::string{spec.id};
        assert(std::ranges::any_of(world.interactions,
            [&observationFlag](const e2d::InteractionRule& rule) { return rule.onceFlag == observationFlag; }));
        for (const auto& visual : current->decorations) {
            if (const auto* text = std::get_if<e2d::TextVisual>(&visual); text != nullptr) {
                assert(text->text.resolve("en").find("SCREEN ") == std::string_view::npos);
                assert(text->text.resolve("cs").find("OBRAZOVKA ") == std::string_view::npos);
            }
        }
        for (const auto& candidate : current->hotspots) {
            if (candidate.kind != e2d::HotspotKind::item) continue;
            ++visiblePickups;
            assert(candidate.visuals.size() >= 5);
            assert(candidate.interactionArea.bottom() == 260.0F);
        }
        anchors += current->travelAnchor ? 1U : 0U;
        const bool authoredHub = spec.number >= 6 && spec.number <= 39;
        if (i > 0 && !authoredHub) {
            assert(std::ranges::any_of(current->exits, [i](const e2d::ExitDefinition& exit) {
                return exit.direction == e2d::Direction::left
                    && exit.destinationRoom == black_pine::content::screens[i - 1].id;
            }));
        }
        if (i + 1 < black_pine::content::screens.size() && !authoredHub) {
            assert(std::ranges::any_of(current->exits, [i](const e2d::ExitDefinition& exit) {
                return exit.direction == e2d::Direction::right
                    && exit.destinationRoom == black_pine::content::screens[i + 1].id;
            }));
        }
    }
    const auto hasExit = [&world](const std::string_view roomId, const e2d::Direction direction,
                             const std::string_view destination) {
        return std::ranges::any_of(world.room(roomId)->exits,
            [direction, destination](const e2d::ExitDefinition& exit) {
                return exit.direction == direction && exit.destinationRoom == destination;
            });
    };
    assert(hasExit("caretaker_cabin_exterior", e2d::Direction::left, "pine_hollow_footbridge"));
    assert(hasExit("caretaker_cabin_exterior", e2d::Direction::right, "caretaker_tool_shed"));
    assert(world.room("caretaker_cabin_main")->exits.empty());
    assert(hasExit("cabin_radio_nook", e2d::Direction::left, "caretaker_cabin_main"));
    assert(hasExit("caretaker_tool_shed", e2d::Direction::left, "caretaker_cabin_exterior"));
    assert(hasExit("caretaker_tool_shed", e2d::Direction::right, "weather_mast_clearing"));
    assert(world.room("cabin_root_cellar")->exits.empty());
    assert(hasExit("weather_mast_clearing", e2d::Direction::left, "caretaker_tool_shed"));
    assert(hasExit("weather_mast_clearing", e2d::Direction::right, "old_service_road_fork"));
    assert(hasExit("old_service_road_fork", e2d::Direction::left, "weather_mast_clearing"));
    assert(hasExit("old_service_road_fork", e2d::Direction::right, "relay_perimeter"));
    assert(hasExit("relay_perimeter", e2d::Direction::left, "old_service_road_fork"));
    assert(hasExit("relay_perimeter", e2d::Direction::right, "vehicle_gate"));
    assert(hasExit("vehicle_gate", e2d::Direction::left, "relay_perimeter"));
    assert(hasExit("vehicle_gate", e2d::Direction::right, "relay_yard_west"));
    assert(hasExit("relay_yard_west", e2d::Direction::left, "vehicle_gate"));
    assert(hasExit("relay_yard_west", e2d::Direction::right, "relay_yard_east"));
    assert(hasExit("relay_yard_east", e2d::Direction::left, "relay_yard_west"));
    assert(world.room("relay_yard_east")->exits.size() == 1);
    for (const int branch : {17, 18, 19, 20, 21, 22, 23, 24}) {
        assert(world.room(black_pine::content::screens[static_cast<std::size_t>(branch - 1)].id)->exits.empty());
    }
    assert(hasExit("north_service_road", e2d::Direction::left, "old_service_road_fork"));
    assert(hasExit("north_service_road", e2d::Direction::right, "burned_pine_stand"));
    assert(hasExit("burned_pine_stand", e2d::Direction::left, "north_service_road"));
    assert(hasExit("burned_pine_stand", e2d::Direction::right, "fallen_fir"));
    assert(hasExit("fallen_fir", e2d::Direction::left, "burned_pine_stand"));
    assert(hasExit("fallen_fir", e2d::Direction::right, "cold_creek_crossing"));
    assert(hasExit("cold_creek_crossing", e2d::Direction::left, "fallen_fir"));
    assert(world.room("cold_creek_crossing")->exits.size() == 1);
    for (const int branch : {29, 30, 31, 32, 33}) {
        assert(world.room(black_pine::content::screens[static_cast<std::size_t>(branch - 1)].id)->exits.empty());
    }
    assert(hasExit("buried_cable_ridge", e2d::Direction::left, "echo_grove"));
    assert(hasExit("buried_cable_ridge", e2d::Direction::right, "bear_meadow"));
    assert(hasExit("bear_meadow", e2d::Direction::left, "buried_cable_ridge"));
    assert(hasExit("bear_meadow", e2d::Direction::right, "firebreak_junction"));
    assert(hasExit("firebreak_junction", e2d::Direction::left, "bear_meadow"));
    assert(world.room("firebreak_junction")->exits.size() == 1);
    assert(world.room("automatic_weather_station")->exits.empty());
    assert(world.room("north_fire_lookout")->exits.empty());
    assert(hasExit("ravine_west_lip", e2d::Direction::left, "firebreak_junction"));
    assert(hasExit("ravine_west_lip", e2d::Direction::right, "broken_service_bridge"));
    assert(anchors == 17);
    assert(visiblePickups >= 45);
    assert(animatedRooms > 80 && animatedRooms < world.rooms.size());
    for (const auto& hint : world.hints) {
        assert(hint.text.resolve("en").find("screen ") == std::string_view::npos);
        assert(hint.text.resolve("cs").find("obrazov") == std::string_view::npos);
    }

    e2d::AdventureSession session{world};
    assert(session.currentRoomId() == "storm_gate_trailhead");
    assert(session.currentHint() != nullptr);

    // Act I — restore the relay chain, return to the generator, and trace Nightjar.
    context(session, world, "s001_emergency_phone", "mission_started");
    take(session, world, "s001_take_patch_cable", "patch_cable");
    take(session, world, "s001_take_field_note", "field_note");
    context(session, world, "s003_deer_path", "deer_path_taken");
    context(session, world, "s006_cabin_door", "cabin_entered");
    context(session, world, "s007_mara", "met_mara");
    examine(session, world, "s007_mara_desk", "key_revealed");
    take(session, world, "s007_take_brass_key", "brass_key");
    assert(session.currentHint()->text.resolve("en").find("RADIO") != std::string_view::npos);
    portal(session, world, "s007_radio_door", "cabin_radio_nook");
    take(session, world, "s008_take_site_map", "site_map");
    assert(session.currentHint()->text.resolve("en").find("CELLAR") != std::string_view::npos);
    portal(session, world, "s008_cabin_return", "caretaker_cabin_main");
    portal(session, world, "s007_cellar_hatch", "cabin_root_cellar");
    take(session, world, "s010_take_ceramic_fuse", "ceramic_fuse");
    take(session, world, "s010_take_hand_crank_torch", "hand_crank_torch");
    assert(session.currentHint()->text.resolve("en").find("SHED") != std::string_view::npos);
    portal(session, world, "s010_cellar_stairs", "caretaker_cabin_main");
    portal(session, world, "s007_cabin_exit", "caretaker_cabin_exterior");
    portal(session, world, "s006_shed_path", "caretaker_tool_shed");
    take(session, world, "s009_take_wrench", "wrench");
    take(session, world, "s009_take_lineman_gloves", "lineman_gloves");
    take(session, world, "s009_take_pruning_saw", "pruning_saw");
    assert(session.currentHint()->text.resolve("en").find("Vehicle Gate") != std::string_view::npos);
    portal(session, world, "s009_mast_path", "weather_mast_clearing");
    walkToTarget(session, world, "s012_forest_route");
    session.jumpOrContext();
    assert(session.currentRoomId() == "old_service_road_fork");
    assert(session.mode() == e2d::SessionMode::message);
    dismiss(session);
    use(session, world, "s014_vehicle_gate", "brass_key", "vehicle_gate_open");
    portal(session, world, "s015_trench_ladder", "cable_trench");
    use(session, world, "s017_blue_terminals", "patch_cable", "cable_patched");
    portal(session, world, "s017_yard_ladder", "relay_yard_west");
    portal(session, world, "s016_generator_path", "generator_shed");
    use(session, world, "s018_main_fuse_holder", "ceramic_fuse", "fuse_installed");
    portal(session, world, "s018_battery_door", "battery_room");
    use(session, world, "s019_battery_bus", "wrench", "battery_linked");
    portal(session, world, "s019_generator_door", "generator_shed");
    portal(session, world, "s018_yard_door", "relay_yard_east");
    portal(session, world, "s016_fuel_path", "fuel_pump_alcove");
    use(session, world, "s020_fuel_valve", "wrench", "fuel_valve_open");
    take(session, world, "s020_take_siphon_hose", "siphon_hose");
    portal(session, world, "s020_yard_path", "relay_yard_east");
    portal(session, world, "s016_transformer_path", "transformer_pad");
    use(session, world, "s021_fallen_feeder", "lineman_gloves", "feeder_isolated");
    portal(session, world, "s021_switchback_path", "upper_switchback");
    examine(session, world, "s004_story", "observed_upper_switchback");
    portal(session, world, "s016_generator_path", "generator_shed");
    portal(session, world, "s018_workshop_door", "relay_workshop");
    use(session, world, "s022_locked_cabinet", "brass_key", "workshop_open");
    assert(session.hasItem("multimeter"));
    portal(session, world, "s022_generator_door", "generator_shed");
    portal(session, world, "s018_yard_door", "relay_yard_east");
    portal(session, world, "s015_hall_door", "lower_relay_hall");
    use(session, world, "s023_nightjar_trunk", "multimeter", "nightjar_signal_found");
    portal(session, world, "s023_yard_door", "relay_yard_west");
    portal(session, world, "s016_generator_path", "generator_shed");
    context(session, world, "s018_main_lever", "power_on");
    portal(session, world, "s018_yard_door", "relay_yard_east");
    use(session, world, "s011_weather_mast", "multimeter", "mast_calibrated");
    portal(session, world, "s015_hall_door", "lower_relay_hall");
    portal(session, world, "s023_control_stairs", "local_control_room");
    context(session, world, "s024_direction_console", "act1_complete");
    portal(session, world, "s024_hall_stairs", "lower_relay_hall");
    portal(session, world, "s023_yard_door", "relay_yard_west");
    portal(session, world, "s012_forest_route", "north_service_road");
    assert(session.flag("forest_route_entered"));

    // Act II — rescue Theo, cross the forest and take the stolen phase coil.
    examine(session, world, "s025_survey_ribbon", "survey_ribbon_recorded");
    examine(session, world, "s026_boot_cache", "bandage_cache_found");
    take(session, world, "s026_take_bandage_roll", "bandage_roll");
    portal(session, world, "s026_kiln_path", "charcoal_kiln_ruin");
    take(session, world, "s032_take_charcoal", "charcoal");
    portal(session, world, "s032_pine_path", "burned_pine_stand");
    use(session, world, "s027_fallen_fir", "pruning_saw", "fir_cut");
    portal(session, world, "s028_blind_path", "hunters_blind");
    take(session, world, "s029_take_signal_flare", "signal_flare");
    portal(session, world, "s029_hollow_path", "mossy_hollow");
    use(session, world, "s030_theo_branch", "pruning_saw", "theo_freed");
    use(session, world, "s030_theo_wound", "bandage_roll", "theo_rescued");
    context(session, world, "s030_theo", "theo_briefed");
    portal(session, world, "s030_cache_path", "ranger_cache");
    take(session, world, "s031_take_climbing_rope", "climbing_rope");
    take(session, world, "s031_take_iron_hook", "iron_hook");
    take(session, world, "s031_take_mine_lamp", "mine_lamp");
    take(session, world, "s031_take_compass", "compass");
    take(session, world, "s031_take_ranger_patch", "ranger_patch");
    portal(session, world, "s031_hollow_path", "mossy_hollow");
    portal(session, world, "s030_blind_path", "hunters_blind");
    portal(session, world, "s029_creek_path", "cold_creek_crossing");
    portal(session, world, "s028_grove_path", "echo_grove");
    use(session, world, "s033_bearing_route", "compass", "echo_route_solved");
    portal(session, world, "s033_ridge_path", "buried_cable_ridge");
    use(session, world, "s034_cable_posts", "multimeter", "quarry_trace_found");
    use(session, world, "s035_bear_wind", "signal_flare", "bear_gone");
    portal(session, world, "s036_weather_path", "automatic_weather_station");
    use(session, world, "s037_weather_recorder", "hand_crank_torch", "weather_data_read");
    portal(session, world, "s037_junction_path", "firebreak_junction");
    portal(session, world, "s036_lookout_path", "north_fire_lookout");
    context(session, world, "s038_nell", "lookout_briefed");
    portal(session, world, "s038_junction_path", "firebreak_junction");
    portal(session, world, "s036_ravine_path", "ravine_west_lip");
    use(session, world, "s039_anchor_eye", "iron_hook", "hook_fixed");
    use(session, world, "s039_fixed_hook", "climbing_rope", "ravine_rope_fixed");
    context(session, world, "s039_rope_descent", "ravine_descended");
    take(session, world, "s041_take_old_relay_badge", "old_relay_badge");
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
    examine(session, world, "s040_story", "observed_broken_service_bridge");

    // Act III — logging railway, dam, mine and freight lift.
    context(session, world, "s052_lila", "met_lila");
    use(session, world, "s053_planer_tension", "wrench", "belt_released");
    take(session, world, "s053_take_drive_belt", "drive_belt");
    take(session, world, "s054_take_oil_can", "oil_can");
    take(session, world, "s054_take_hand_mirror", "hand_mirror");
    use(session, world, "s055_reserve_tank", "siphon_hose", "fuel_can_filled");
    assert(session.hasItem("filled_fuel_can"));
    context(session, world, "s056_log_pike", "spark_retrieved");
    take(session, world, "s057_take_rail_switch_key", "rail_switch_key");
    context(session, world, "s058_june", "met_june");
    use(session, world, "s059_carbon_impression", "hand_mirror", "lift_time_known");
    use(session, world, "s060_rail_points", "rail_switch_key", "rail_points_aligned");
    use(session, world, "s061_engine_belt", "drive_belt", "engine_belt_installed");
    use(session, world, "s061_engine_ignition", "spark_plug", "engine_plug_installed");
    use(session, world, "s061_engine_bearings", "oil_can", "engine_oiled");
    use(session, world, "s061_engine_fuel_tank", "filled_fuel_can", "engine_fueled");
    context(session, world, "s061_engine_start", "logging_engine_running");
    context(session, world, "s062_mill_whistle", "trestle_guard_diverted");
    use(session, world, "s062_brake_linkage", "wrench", "trestle_brake_fixed");
    context(session, world, "s063_portable_radio", "elias_contacted");
    take(session, world, "s065_take_insulated_boots", "insulated_boots");
    take(session, world, "s065_take_turbine_badge", "turbine_badge");
    use(session, world, "s066_spray_shield", "wrench", "spray_shield_fixed");
    use(session, world, "s067_gatehouse_reader", "turbine_badge", "gatehouse_open");
    take(session, world, "s067_take_spillway_crank", "spillway_crank");
    use(session, world, "s067_spillway_crank_socket", "spillway_crank", "spillway_closed");
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
    take(session, world, "s079_take_filter_housing", "filter_housing");
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
    context(session, world, "s091_staff_passage", "staff_passage_taken");
    use(session, world, "s094_kitchen_bait", "sealed_ration", "guard_bait_placed");
    context(session, world, "s094_kitchen_timer", "courtyard_patrol_diverted");
    examine(session, world, "s092_story", "observed_ridge_courtyard");
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
    take(session, world, "s109_take_calder_photo", "calder_photo");
    use(session, world, "s110_capacitor_banks", "grounding_clamp", "capacitors_grounded");
    use(session, world, "s111_split_coolant_line", "coolant_hose", "cooling_diverted");
    use(session, world, "s112_archive_deck", "archive_reel", "command_archive_loaded");
    use(session, world, "s112_archive_decoder", "cipher_lens", "evidence_copied");
    assert(session.hasItem("evidence_spool"));
    context(session, world, "s113_miriam", "kline_freed");
    use(session, world, "s113_miriam", "first_aid_kit", "kline_treated");
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
    context(session, world, "s122_beacon_housing", "beacon_crystal_removed");
    use(session, world, "s122_beacon_cleaning", "first_aid_kit", "beacon_crystal_cleaned");
    use(session, world, "s122_beacon_socket", "beacon_crystal", "beacon_reference_ready");
    use(session, world, "s123_azimuth_mount", "wrench", "antenna_aligned");
    use(session, world, "s123_local_motor_lock", "override_key", "antenna_control_locked");
    context(session, world, "s124_voss", "voss_confronted");
    use(session, world, "s124_transmitter_lock", "transmitter_key", "transmitter_unlocked");
    const auto beforeEvidence = session.snapshot();

    e2d::AdventureSession carrierSession{world};
    assert(carrierSession.restore(beforeEvidence));
    walkToTarget(carrierSession, world, "s124_protected_carrier_console");
    carrierSession.jumpOrContext();
    assert(carrierSession.mode() == e2d::SessionMode::won);
    assert(carrierSession.terminalMessage().find("CARRIER RESTORED") != std::string_view::npos);

    use(session, world, "s124_evidence_loader", "evidence_spool", "evidence_loaded");
    assert(session.visitedRooms().size() == 124);
    const auto beforeOpenChannel = session.snapshot();
    walkToTarget(session, world, "s124_protected_carrier_console");
    session.jumpOrContext();
    assert(session.mode() == e2d::SessionMode::won);
    assert(session.terminalMessage().find("OPEN CHANNEL") != std::string_view::npos);

    auto keeperSnapshot = beforeOpenChannel;
    for (const std::string_view item : {"pine_bird", "relay_badge", "ranger_patch", "old_relay_badge",
            "quartz_sample", "logger_token", "nightjar_patch", "calder_photo"}) {
        keeperSnapshot.inventory.insert(std::string{item});
    }
    for (const std::string_view flag : {"theo_followup", "nell_followup", "owen_followup", "lila_followup",
            "june_history_heard", "jonah_followup", "sable_persuaded", "kline_treated"}) {
        keeperSnapshot.flags[std::string{flag}] = true;
    }
    e2d::AdventureSession keeperSession{world};
    assert(keeperSession.restore(keeperSnapshot));
    walkToTarget(keeperSession, world, "s124_protected_carrier_console");
    keeperSession.jumpOrContext();
    assert(keeperSession.mode() == e2d::SessionMode::won);
    assert(keeperSession.terminalMessage().find("KEEPER OF BLACK PINE") != std::string_view::npos);

    e2d::AdventureSession darkHatchSession{world};
    auto darkHatchSnapshot = darkHatchSession.snapshot();
    darkHatchSnapshot.roomId = "cabin_root_cellar";
    darkHatchSnapshot.player.position = {340, 232};
    assert(darkHatchSession.restore(darkHatchSnapshot));
    walkToTarget(darkHatchSession, world, "s010_service_hatch");
    darkHatchSession.jumpOrContext();
    assert(darkHatchSession.mode() == e2d::SessionMode::message);
    assert(darkHatchSession.currentRoomId() == "cabin_root_cellar");
    dismiss(darkHatchSession);
    darkHatchSnapshot.inventory.insert("hand_crank_torch");
    assert(darkHatchSession.restore(darkHatchSnapshot));
    portal(darkHatchSession, world, "s010_service_hatch", "weather_mast_clearing");

    e2d::AdventureSession safeRouteSession{world};
    context(safeRouteSession, world, "s001_emergency_phone", "mission_started");
    context(safeRouteSession, world, "s003_deer_path", "deer_path_taken");
    assert(safeRouteSession.currentRoomId() == "pine_hollow_footbridge");
    walkToTarget(safeRouteSession, world, "s005_deer_path_return");
    safeRouteSession.jumpOrContext();
    dismiss(safeRouteSession);
    assert(safeRouteSession.currentRoomId() == "lower_switchback");
    std::size_t blockedApproach = 0;
    while (safeRouteSession.mode() == e2d::SessionMode::world && blockedApproach++ < 1000) {
        safeRouteSession.walk(e2d::Direction::right);
    }
    assert(safeRouteSession.mode() == e2d::SessionMode::message);
    assert(safeRouteSession.currentRoomId() == "lower_switchback");

    e2d::AdventureSession hazardSession{world};
    context(hazardSession, world, "s001_emergency_phone", "mission_started");
    auto hazardSnapshot = hazardSession.snapshot();
    hazardSnapshot.roomId = "upper_switchback";
    hazardSnapshot.player.position = {330, 232};
    assert(hazardSession.restore(hazardSnapshot));
    std::size_t hazardApproach = 0;
    while (hazardSession.mode() == e2d::SessionMode::world && hazardApproach++ < 6000) {
        hazardSession.walk(e2d::Direction::right);
    }
    assert(hazardSession.mode() == e2d::SessionMode::dead);
    hazardSession.jumpOrContext();
    assert(hazardSession.mode() == e2d::SessionMode::world);
    assert(hazardSession.currentRoomId() == "upper_switchback");
    assert(hazardSession.flag("mission_started"));

    e2d::AdventureRenderer renderer{world};
    for (const int number : {1, 6, 7, 14, 25, 39, 52, 64, 76, 91, 104, 124}) {
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
