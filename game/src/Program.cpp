#include "CopperBoots/CopperBootsGame.hpp"
#include "CopperBoots/CnaProgressStore.hpp"
#include "CopperBoots/CnaSettingsStore.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

int main(const int argc, char* argv[])
{
    bool smokeTest = false;
    bool audioEnabled = true;
    bool settingsEnabled = true;
    bool storageSmokeTest = false;
    bool displaySmokeTest = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--smoke-test")
            smokeTest = true;
        else if (argument == "--no-audio")
            audioEnabled = false;
        else if (argument == "--no-settings")
            settingsEnabled = false;
        else if (argument == "--storage-smoke-test")
            storageSmokeTest = true;
        else if (argument == "--display-smoke-test")
            displaySmokeTest = true;
        else {
            std::cerr << "Unknown argument: " << argument << '\n'
                      << "Usage: " << argv[0]
                      << " [--smoke-test] [--no-audio] [--no-settings]"
                         " [--storage-smoke-test] [--display-smoke-test]\n";
            return 2;
        }
    }

    if (storageSmokeTest) {
        try {
            CopperBoots::GameSettings settings;
            settings.MasterVolume = 0.625F;
            settings.EffectsVolume = 0.375F;
            CopperBoots::CnaSettingsStore::Save(settings);
            const CopperBoots::SettingsLoadResult loadedSettings =
                CopperBoots::CnaSettingsStore::Load();

            CopperBoots::ProgressData progress;
            progress.HighestUnlockedStage = 2;
            progress.BestScore = 4'200;
            progress.BestCompletionTicks = 3'600;
            (void)CopperBoots::CnaProgressStore::Save(progress);
            const CopperBoots::ProgressLoadResult loadedProgress =
                CopperBoots::CnaProgressStore::Load();
            if (loadedSettings.Status !=
                    CopperBoots::SettingsLoadStatus::Loaded ||
                loadedSettings.Settings != settings ||
                loadedProgress.Source == CopperBoots::ProgressSlot::None ||
                loadedProgress.Data != progress) {
                std::cerr << "Copper Boots: storage smoke mismatch\n";
                return 1;
            }
            std::cout << "Copper Boots: CNA storage smoke test completed\n";
            return 0;
        }
        catch (const std::exception& error) {
            std::cerr << "Copper Boots: storage smoke failed: "
                      << error.what() << '\n';
            return 1;
        }
    }

    CopperBoots::CopperBootsGame game(
        smokeTest, audioEnabled, settingsEnabled, displaySmokeTest);
    game.Run();

    if (smokeTest)
        std::cout << "Copper Boots: smoke test completed\n";
    if (displaySmokeTest)
        std::cout << "Copper Boots: display lifecycle smoke test completed\n";
    return 0;
}
