#include "People/PeopleGame.hpp"

#include <charconv>
#include <iostream>
#include <string_view>

namespace
{
    [[nodiscard]] int ParsePositiveInt(const std::string_view text)
    {
        int value = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value <= 0)
            return 0;
        return value;
    }
}

int main(int argc, char* argv[])
{
    int smokeFrames = 0;
    bool smokeRotations = false;
    bool smokeWalk = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--smoke-test")
        {
            smokeFrames = 4;
            smokeRotations = true;
        }
        else if (argument == "--smoke-walk")
        {
            smokeWalk = true;
        }
        else if (argument == "--smoke-frames" && index + 1 < argc)
        {
            smokeFrames = ParsePositiveInt(argv[++index]);
            if (smokeFrames == 0)
            {
                std::cerr << "People: --smoke-frames requires a positive integer\n";
                return 2;
            }
        }
        else
        {
            std::cerr << "People: unknown argument '" << argument << "'\n";
            return 2;
        }
    }

    PeopleGame game(smokeFrames, smokeRotations, smokeWalk);
    game.Run();
    return 0;
}
