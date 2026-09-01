// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "MainMenu.hpp"
#include <algorithm>
#include <filesystem>

namespace MeshWorld {

void MainMenu::go_new_world() {
    screen_ = Screen::NEW_WORLD;
    config_.action = MenuAction::NONE;
}

void MainMenu::go_load_world() {
    screen_ = Screen::LOAD_WORLD;
    config_.action = MenuAction::NONE;
}

void MainMenu::go_back() {
    screen_ = Screen::MAIN;
    config_.action = MenuAction::NONE;
}

MenuAction MainMenu::commit_new_world() {
    config_.action = MenuAction::NEW_WORLD;
    return config_.action;
}

MenuAction MainMenu::commit_load_world(const std::string& world_json_path) {
    config_.load_path = world_json_path;
    config_.action    = MenuAction::LOAD_WORLD;
    return config_.action;
}

MenuAction MainMenu::commit_quit() {
    config_.action = MenuAction::QUIT;
    return config_.action;
}

std::vector<std::string> MainMenu::scan_saves(const std::string& dir) {
    std::vector<std::string> result;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return result;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec) && !ec) {
            if (entry.path().filename() == "world.json") {
                result.push_back(entry.path().string());
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace MeshWorld
