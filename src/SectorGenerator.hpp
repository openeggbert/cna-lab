#pragma once

#include <cstdint>
#include <string>

namespace WolfCna
{
    // A sector built at run time for the procedural mode. The same run seed and depth
    // always produce the same sector, so a run can be reproduced from two integers and a
    // save only has to store those rather than a whole grid.
    struct GeneratedSector final
    {
        std::string grid;
        std::uint32_t seed = 0;
        int depth = 0;
        int walkable = 0;
        int doors = 0;
        int objectiveRoute = 0;
        int attempts = 0;
        bool valid = false;
    };

    // Depth is zero-based. Deeper sectors carry more enemies and less ammunition, which is
    // the difficulty curve M10 asks for; the generator rejects any layout that would break
    // the same invariants the authored sectors are audited against.
    [[nodiscard]] GeneratedSector GenerateSector(std::uint32_t runSeed, int depth);

    [[nodiscard]] int ProceduralTargetSeconds(int depth);

    // Exposed for tests: the audit a generated grid has to pass before it is accepted.
    [[nodiscard]] bool IsAcceptableSector(const std::string& grid, std::string& reason);
}
