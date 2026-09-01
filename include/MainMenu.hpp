// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MeshWorld {

enum class MenuAction { NONE, NEW_WORLD, LOAD_WORLD, QUIT };

// Parameters for the chosen action, set by MainMenu before returning.
struct MenuConfig {
    // NEW_WORLD fields
    uint64_t    seed{0};
    std::string style{"central_europe_small_city"};
    // LOAD_WORLD fields
    std::string load_path;
    // The resolved action
    MenuAction  action{MenuAction::NONE};
};

// State machine for the main menu.  Display-agnostic: the caller (WorldApp)
// drives the actual ImGui or text rendering; this class manages state and
// produces a MenuConfig when the user commits a choice.
class MainMenu {
public:
    enum class Screen { MAIN, NEW_WORLD, LOAD_WORLD };

    MainMenu() = default;

    // ---- State queries ----
    Screen      current_screen() const { return screen_; }
    const MenuConfig& config()   const { return config_; }

    // ---- Transitions (called by the UI layer or tests) ----
    void go_new_world();
    void go_load_world();
    void go_back();         // return to MAIN screen

    // NEW_WORLD configuration
    void set_seed(uint64_t s)            { config_.seed  = s; }
    void set_style(const std::string& s) { config_.style = s; }

    // Commit the current screen's choice — returns the resulting action.
    MenuAction commit_new_world();
    MenuAction commit_load_world(const std::string& world_json_path);
    MenuAction commit_quit();

    // Scan a directory for saved world.json files.
    // Returns sorted list of absolute paths found.
    static std::vector<std::string> scan_saves(const std::string& dir);

private:
    Screen    screen_{Screen::MAIN};
    MenuConfig config_;
};

} // namespace MeshWorld
