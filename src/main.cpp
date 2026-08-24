#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"

#include <iostream>
#include <string_view>

int main(const int argc, char* argv[])
{
    bool smokeTest = false;
    CnaTamagotchi::Display::LcdPalette lcdPalette =
        CnaTamagotchi::Display::LcdPalette::ClassicOlive;

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
                lcdPalette = CnaTamagotchi::Display::LcdPalette::ClassicOlive;
            } else if (value == "amber") {
                lcdPalette = CnaTamagotchi::Display::LcdPalette::Amber;
            } else if (value == "ice") {
                lcdPalette = CnaTamagotchi::Display::LcdPalette::IceBlue;
            } else if (value == "mono") {
                lcdPalette = CnaTamagotchi::Display::LcdPalette::HighContrast;
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

    CnaTamagotchi::Application::CnaTamagotchiGame game(smokeTest, lcdPalette);
    game.Run();
    return 0;
}
