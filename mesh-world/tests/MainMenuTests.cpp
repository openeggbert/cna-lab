// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "MainMenu.hpp"
#include <filesystem>
#include <fstream>

using namespace MeshWorld;

// T275: MainMenu constructed without display — no crash.
TEST(MainMenuTests, T275_ConstructHeadless) {
    MainMenu menu;
    EXPECT_EQ(menu.current_screen(), MainMenu::Screen::MAIN);
    EXPECT_EQ(menu.config().action, MenuAction::NONE);
}

TEST(MainMenuTests, GoNewWorldChangesScreen) {
    MainMenu menu;
    menu.go_new_world();
    EXPECT_EQ(menu.current_screen(), MainMenu::Screen::NEW_WORLD);
}

TEST(MainMenuTests, GoBackReturnsToMain) {
    MainMenu menu;
    menu.go_new_world();
    menu.go_back();
    EXPECT_EQ(menu.current_screen(), MainMenu::Screen::MAIN);
}

TEST(MainMenuTests, CommitNewWorldReturnsCorrectAction) {
    MainMenu menu;
    menu.go_new_world();
    menu.set_seed(12345);
    menu.set_style("nordic_town");
    MenuAction a = menu.commit_new_world();
    EXPECT_EQ(a, MenuAction::NEW_WORLD);
    EXPECT_EQ(menu.config().seed, 12345u);
    EXPECT_EQ(menu.config().style, "nordic_town");
    EXPECT_EQ(menu.config().action, MenuAction::NEW_WORLD);
}

TEST(MainMenuTests, CommitLoadWorldSetsPath) {
    MainMenu menu;
    menu.go_load_world();
    MenuAction a = menu.commit_load_world("/saves/world1/world.json");
    EXPECT_EQ(a, MenuAction::LOAD_WORLD);
    EXPECT_EQ(menu.config().load_path, "/saves/world1/world.json");
}

TEST(MainMenuTests, CommitQuitReturnsQuit) {
    MainMenu menu;
    MenuAction a = menu.commit_quit();
    EXPECT_EQ(a, MenuAction::QUIT);
}

TEST(MainMenuTests, ScanSavesEmptyDirReturnsEmpty) {
    auto saves = MainMenu::scan_saves("/nonexistent_dir_xyz_12345");
    EXPECT_TRUE(saves.empty());
}

TEST(MainMenuTests, ScanSavesFindsWorldJson) {
    // Create a temp dir with a world.json
    const std::string tmp = "/tmp/mw_test_saves";
    std::filesystem::create_directories(tmp + "/world1");
    {
        std::ofstream f(tmp + "/world1/world.json");
        f << "{}";
    }
    auto saves = MainMenu::scan_saves(tmp);
    EXPECT_EQ(saves.size(), 1u);
    EXPECT_TRUE(saves[0].find("world.json") != std::string::npos);
    std::filesystem::remove_all(tmp);
}
