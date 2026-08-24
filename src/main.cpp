#include "TamagotchiCna/Application/TamagotchiCnaGame.hpp"

#include <iostream>
#include <string_view>

int main(const int argc, char* argv[])
{
    bool smokeTest = false;
    TamagotchiCna::Display::LcdPalette lcdPalette =
        TamagotchiCna::Display::LcdPalette::ClassicOlive;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--smoke-test") {
            smokeTest = true;
            continue;
        }

        constexpr std::string_view palettePrefix = "--lcd-palette=";
        if (argument.starts_with(palettePrefix)) {
            const std::string_view value = argument.substr(palettePrefix.size());
            if (value == "olive") {
                lcdPalette = TamagotchiCna::Display::LcdPalette::ClassicOlive;
            } else if (value == "amber") {
                lcdPalette = TamagotchiCna::Display::LcdPalette::Amber;
            } else if (value == "ice") {
                lcdPalette = TamagotchiCna::Display::LcdPalette::IceBlue;
            } else if (value == "mono") {
                lcdPalette = TamagotchiCna::Display::LcdPalette::HighContrast;
            } else {
                std::cerr << "Unknown LCD palette: '" << value
                          << "'. Choose olive, amber, ice, or mono.\n";
                return 2;
            }
            continue;
        }

        std::cerr << "Usage: TamagotchiCna [--smoke-test] "
                     "[--lcd-palette=olive|amber|ice|mono]\n";
        return 2;
    }

#if defined(__EMSCRIPTEN__)
    // CNA hands control to Emscripten's browser-owned main loop.  Its callback
    // outlives this invocation of main(), so the Game instance must not live on
    // main's stack.
    static TamagotchiCna::Application::TamagotchiCnaGame game(smokeTest, lcdPalette);
#else
    TamagotchiCna::Application::TamagotchiCnaGame game(smokeTest, lcdPalette);
#endif
    game.Run();
    return 0;
}
