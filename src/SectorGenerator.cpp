#include "SectorGenerator.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace WolfCna
{
    namespace
    {
        constexpr int Size = 64;
        constexpr char Wall = '#';
        constexpr char Floor = '.';

        // The same layout rules the authoring tool uses, ported so a run can build sectors
        // without touching the filesystem. Keeping one set of rules matters: the audits
        // below are the ones the shipped sectors already satisfy.
        struct Rng final
        {
            std::uint32_t state;

            std::uint32_t Next()
            {
                state = state * 1664525u + 1013904223u;
                return state >> 8;
            }

            int Range(int low, int high)
            {
                if (high <= low)
                    return low;
                return low + static_cast<int>(
                    Next() % static_cast<std::uint32_t>(high - low + 1));
            }

            float Unit() { return static_cast<float>(Next() % 10000u) / 10000.0f; }
        };

        struct Room final
        {
            int x0 = 0;
            int z0 = 0;
            int x1 = 0;
            int z1 = 0;

            [[nodiscard]] int Width() const { return x1 - x0; }
            [[nodiscard]] int Height() const { return z1 - z0; }
        };

        using Grid = std::vector<std::string>;

        [[nodiscard]] bool Touches(const Room& a, const Room& b)
        {
            if (a.x1 + 1 == b.x0 || b.x1 + 1 == a.x0)
                return std::min(a.z1, b.z1) > std::max(a.z0, b.z0);
            if (a.z1 + 1 == b.z0 || b.z1 + 1 == a.z0)
                return std::min(a.x1, b.x1) > std::max(a.x0, b.x0);
            return false;
        }

        [[nodiscard]] bool DoorBetween(const Room& a, const Room& b, Rng& rng, int& outX, int& outZ)
        {
            if (a.x1 + 1 == b.x0 || b.x1 + 1 == a.x0)
            {
                const int wallX = a.x1 + 1 == b.x0 ? a.x1 : b.x1;
                const int low = std::max(a.z0, b.z0);
                const int high = std::min(a.z1, b.z1) - 1;
                if (high < low)
                    return false;
                outX = wallX;
                outZ = rng.Range(low, high);
                return true;
            }
            const int wallZ = a.z1 + 1 == b.z0 ? a.z1 : b.z1;
            const int low = std::max(a.x0, b.x0);
            const int high = std::min(a.x1, b.x1) - 1;
            if (high < low)
                return false;
            outX = rng.Range(low, high);
            outZ = wallZ;
            return true;
        }

        void Split(std::vector<Room>& rooms, Rng& rng, int minLeaf)
        {
            std::vector<Room> out;
            out.reserve(rooms.size() * 2);
            for (const Room& room : rooms)
            {
                const bool canVertical = room.Width() >= minLeaf * 2 + 1;
                const bool canHorizontal = room.Height() >= minLeaf * 2 + 1;
                if (!canVertical && !canHorizontal)
                {
                    out.push_back(room);
                    continue;
                }

                // Always cut the longer side. Preferring width shrinks rooms into narrow
                // slots, because the height never gets a turn inside the round budget.
                bool vertical = canVertical;
                if (canVertical && canHorizontal)
                {
                    if (room.Width() > room.Height() * 6 / 5)
                        vertical = true;
                    else if (room.Height() > room.Width() * 6 / 5)
                        vertical = false;
                    else
                        vertical = rng.Unit() < 0.5f;
                }

                if (vertical)
                {
                    const int cut = rng.Range(room.x0 + minLeaf, room.x1 - minLeaf - 1);
                    out.push_back({room.x0, room.z0, cut, room.z1});
                    out.push_back({cut + 1, room.z0, room.x1, room.z1});
                }
                else
                {
                    const int cut = rng.Range(room.z0 + minLeaf, room.z1 - minLeaf - 1);
                    out.push_back({room.x0, room.z0, room.x1, cut});
                    out.push_back({room.x0, cut + 1, room.x1, room.z1});
                }
            }
            rooms.swap(out);
        }

        [[nodiscard]] std::vector<std::vector<int>> Distances(const Grid& grid, int startX, int startZ)
        {
            std::vector<std::vector<int>> dist(
                Size, std::vector<int>(static_cast<std::size_t>(Size), -1));
            std::deque<std::pair<int, int>> queue;
            dist[static_cast<std::size_t>(startZ)][static_cast<std::size_t>(startX)] = 0;
            queue.emplace_back(startX, startZ);
            constexpr std::array<std::pair<int, int>, 4> steps{
                std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};
            while (!queue.empty())
            {
                const auto [x, z] = queue.front();
                queue.pop_front();
                for (const auto [dx, dz] : steps)
                {
                    const int nx = x + dx;
                    const int nz = z + dz;
                    if (nx < 0 || nz < 0 || nx >= Size || nz >= Size)
                        continue;
                    if (dist[static_cast<std::size_t>(nz)][static_cast<std::size_t>(nx)] >= 0)
                        continue;
                    const char cell = grid[static_cast<std::size_t>(nz)][static_cast<std::size_t>(nx)];
                    if (cell == Wall || cell == 'Y')
                        continue;
                    dist[static_cast<std::size_t>(nz)][static_cast<std::size_t>(nx)] =
                        dist[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] + 1;
                    queue.emplace_back(nx, nz);
                }
            }
            return dist;
        }

        [[nodiscard]] bool OpenArea(const Grid& grid, int x, int z)
        {
            if (x < 1 || z < 1 || x >= Size - 1 || z >= Size - 1)
                return false;
            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (grid[static_cast<std::size_t>(z + dz)][static_cast<std::size_t>(x + dx)] != Floor)
                        return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool WallAdjacent(const Grid& grid, int x, int z)
        {
            if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != Floor)
                return false;
            constexpr std::array<std::pair<int, int>, 4> steps{
                std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};
            for (const auto [dx, dz] : steps)
            {
                if (grid[static_cast<std::size_t>(z + dz)][static_cast<std::size_t>(x + dx)] == Wall)
                    return true;
            }
            return false;
        }

        enum class Placement { Anywhere, OpenFloor, AgainstWall };

        int Scatter(
            Grid& grid,
            Rng& rng,
            char symbol,
            int count,
            Placement placement)
        {
            std::vector<std::pair<int, int>> pool;
            for (int z = 1; z < Size - 1; ++z)
            {
                for (int x = 1; x < Size - 1; ++x)
                {
                    if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != Floor)
                        continue;
                    if (placement == Placement::OpenFloor && !OpenArea(grid, x, z))
                        continue;
                    if (placement == Placement::AgainstWall && !WallAdjacent(grid, x, z))
                        continue;
                    pool.emplace_back(x, z);
                }
            }
            for (std::size_t index = pool.size(); index > 1; --index)
                std::swap(pool[index - 1], pool[static_cast<std::size_t>(rng.Range(0, static_cast<int>(index) - 1))]);

            std::vector<std::pair<int, int>> placed;
            for (const auto [x, z] : pool)
            {
                if (static_cast<int>(placed.size()) >= count)
                    break;
                const bool tooClose = std::any_of(
                    placed.begin(),
                    placed.end(),
                    [x, z](const std::pair<int, int>& other)
                    {
                        return std::abs(x - other.first) + std::abs(z - other.second) < 3;
                    });
                if (tooClose)
                    continue;
                grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] = symbol;
                placed.emplace_back(x, z);
            }
            return static_cast<int>(placed.size());
        }

        [[nodiscard]] bool PlacePatrol(Grid& grid, Rng& rng, char symbol)
        {
            constexpr std::array<std::pair<std::pair<int, int>, char>, 4> arrows{
                std::pair{std::pair{1, 0}, '>'},
                std::pair{std::pair{-1, 0}, '<'},
                std::pair{std::pair{0, 1}, 'v'},
                std::pair{std::pair{0, -1}, '^'}};
            for (int attempt = 0; attempt < 400; ++attempt)
            {
                const int x = rng.Range(2, Size - 3);
                const int z = rng.Range(2, Size - 3);
                if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != Floor)
                    continue;
                for (const auto& [step, arrow] : arrows)
                {
                    const int ax = x + step.first;
                    const int az = z + step.second;
                    if (ax < 1 || az < 1 || ax >= Size - 1 || az >= Size - 1)
                        continue;
                    if (grid[static_cast<std::size_t>(az)][static_cast<std::size_t>(ax)] != Floor)
                        continue;
                    bool clear = true;
                    for (int k = 1; k <= 3 && clear; ++k)
                    {
                        const int rx = ax + step.first * k;
                        const int rz = az + step.second * k;
                        clear = rx >= 0 && rz >= 0 && rx < Size && rz < Size &&
                            grid[static_cast<std::size_t>(rz)][static_cast<std::size_t>(rx)] == Floor;
                    }
                    if (!clear)
                        continue;
                    grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] = symbol;
                    grid[static_cast<std::size_t>(az)][static_cast<std::size_t>(ax)] = arrow;
                    return true;
                }
            }
            return false;
        }
    }

    bool IsAcceptableSector(const std::string& grid, std::string& reason)
    {
        Grid rows;
        std::size_t begin = 0;
        while (begin < grid.size())
        {
            const std::size_t end = grid.find('\n', begin);
            if (end == std::string::npos)
                break;
            rows.push_back(grid.substr(begin, end - begin));
            begin = end + 1;
        }
        if (rows.size() != static_cast<std::size_t>(Size))
        {
            reason = "grid is not 64 rows";
            return false;
        }

        int startX = -1;
        int startZ = -1;
        int exits = 0;
        int relays = 0;
        int terminals = 0;
        int walkable = 0;
        int health = 0;
        int patrols = 0;
        int ambushes = 0;
        int props = 0;
        for (int z = 0; z < Size; ++z)
        {
            if (rows[static_cast<std::size_t>(z)].size() != static_cast<std::size_t>(Size))
            {
                reason = "grid is not 64 columns";
                return false;
            }
            for (int x = 0; x < Size; ++x)
            {
                const char cell = rows[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
                if (cell != Wall && cell != 'Y')
                    ++walkable;
                switch (cell)
                {
                case 'P': startX = x; startZ = z; break;
                case 'E': ++exits; break;
                case 'O': ++relays; break;
                case 'M': ++terminals; break;
                case 'H': case 'h': ++health; break;
                case '^': case '>': case 'v': case '<': ++patrols; break;
                case 'g': case 'k': case 'f': case 'u': ++ambushes; break;
                default: break;
                }
                if ((cell >= '0' && cell <= '9') || cell == 's')
                {
                    ++props;
                    // Only the ring: the centre holds the prop itself, so requiring it to
                    // be plain floor could never pass.
                    if (x < 1 || z < 1 || x >= Size - 1 || z >= Size - 1)
                    {
                        reason = "a prop sits on the border";
                        return false;
                    }
                    for (int dz = -1; dz <= 1; ++dz)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (dx == 0 && dz == 0)
                                continue;
                            if (rows[static_cast<std::size_t>(z + dz)]
                                    [static_cast<std::size_t>(x + dx)] != Floor)
                            {
                                reason = "a prop stands outside open floor";
                                return false;
                            }
                        }
                    }
                }
            }
        }

        if (startX < 0) { reason = "no player spawn"; return false; }
        if (exits != 1) { reason = "not exactly one exit"; return false; }
        if (relays != 1 || terminals != 1) { reason = "objectives are not paired"; return false; }
        if (health < 2) { reason = "fewer than two health pickups"; return false; }
        if (patrols < 1 || ambushes < 1) { reason = "missing patrol or ambush"; return false; }
        if (props < 4) { reason = "too few props"; return false; }
        if (walkable < 1500) { reason = "footprint is too small"; return false; }

        const auto dist = Distances(rows, startX, startZ);
        int reachable = 0;
        for (int z = 0; z < Size; ++z)
        {
            for (int x = 0; x < Size; ++x)
            {
                if (dist[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] >= 0)
                    ++reachable;
            }
        }
        if (reachable != walkable) { reason = "level has disconnected rooms"; return false; }
        return true;
    }

    int ProceduralTargetSeconds(int depth)
    {
        return 200 + std::min(depth, 12) * 12;
    }

    GeneratedSector GenerateSector(std::uint32_t runSeed, int depth)
    {
        GeneratedSector result;
        result.seed = runSeed;
        result.depth = depth;

        for (int attempt = 0; attempt < 600; ++attempt)
        {
            result.attempts = attempt + 1;
            Rng rng{runSeed + static_cast<std::uint32_t>(depth) * 7919u +
                static_cast<std::uint32_t>(attempt) * 104729u};

            std::vector<Room> rooms{{1, 1, Size - 1, Size - 1}};
            for (int round = 0; round < 8; ++round)
                Split(rooms, rng, 5);

            Grid grid(static_cast<std::size_t>(Size), std::string(static_cast<std::size_t>(Size), Wall));
            std::vector<Room> kept;
            for (const Room& room : rooms)
            {
                if (room.Width() < 3 || room.Height() < 3)
                    continue;
                if (rng.Unit() < 0.18f && room.Width() * room.Height() <= 42)
                    continue;
                const int inset = room.Width() >= 7 && room.Height() >= 7 ? 1 : 0;
                Room carved{room.x0 + inset, room.z0 + inset, room.x1 - inset, room.z1 - inset};
                if (carved.Width() < 2 || carved.Height() < 2)
                    carved = room;

                // Corridors as their own element rather than just small rooms: an
                // elongated leaf is sometimes cut down to a one or two cell strip along
                // its long axis. It stays a rectangle, so door placement is unaffected.
                const bool elongated = carved.Width() >= 6 || carved.Height() >= 6;
                if (elongated && rng.Unit() < 0.42f)
                {
                    const int width = rng.Unit() < 0.6f ? 1 : 2;
                    if (carved.Width() >= carved.Height())
                    {
                        const int mid = (carved.z0 + carved.z1) / 2;
                        carved = {carved.x0, mid,
                            carved.x1, std::min(carved.z1, mid + width)};
                    }
                    else
                    {
                        const int mid = (carved.x0 + carved.x1) / 2;
                        carved = {mid, carved.z0,
                            std::min(carved.x1, mid + width), carved.z1};
                    }
                }

                for (int z = carved.z0; z < carved.z1; ++z)
                    for (int x = carved.x0; x < carved.x1; ++x)
                        grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] = Floor;

                // A pillar breaks up a large room without risking connectivity: it sits
                // strictly inside, so no doorway is blocked and there is always a way
                // round it.
                if (carved.Width() >= 7 && carved.Height() >= 7 && rng.Unit() < 0.45f)
                {
                    const int px = rng.Range(carved.x0 + 2, carved.x1 - 3);
                    const int pz = rng.Range(carved.z0 + 2, carved.z1 - 3);
                    const int size = rng.Unit() < 0.5f ? 1 : 2;
                    for (int z = pz; z < std::min(pz + size, carved.z1 - 1); ++z)
                        for (int x = px; x < std::min(px + size, carved.x1 - 1); ++x)
                            grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] = Wall;
                }

                kept.push_back(carved);
            }
            if (kept.size() < 12) { continue; }

            // Spanning tree of doors first, then extras so the level has loops.
            std::vector<std::pair<int, int>> edges;
            for (std::size_t i = 0; i < kept.size(); ++i)
                for (std::size_t j = i + 1; j < kept.size(); ++j)
                    if (Touches(kept[i], kept[j]))
                        edges.emplace_back(static_cast<int>(i), static_cast<int>(j));
            for (std::size_t index = edges.size(); index > 1; --index)
                std::swap(edges[index - 1], edges[static_cast<std::size_t>(rng.Range(0, static_cast<int>(index) - 1))]);

            std::vector<int> parent(kept.size());
            for (std::size_t i = 0; i < parent.size(); ++i)
                parent[i] = static_cast<int>(i);
            const std::function<int(int)> find = [&parent](int i)
            {
                while (parent[static_cast<std::size_t>(i)] != i)
                {
                    parent[static_cast<std::size_t>(i)] =
                        parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(i)])];
                    i = parent[static_cast<std::size_t>(i)];
                }
                return i;
            };

            std::vector<std::pair<int, int>> tree;
            std::vector<std::pair<int, int>> spare;
            for (const auto& [i, j] : edges)
            {
                const int ri = find(i);
                const int rj = find(j);
                if (ri != rj)
                {
                    parent[static_cast<std::size_t>(ri)] = rj;
                    tree.emplace_back(i, j);
                }
                else
                {
                    spare.emplace_back(i, j);
                }
            }
            const std::size_t extras = std::min(spare.size(), tree.size() * 35 / 100);
            tree.insert(tree.end(), spare.begin(), spare.begin() + static_cast<std::ptrdiff_t>(extras));

            int doorCount = 0;
            for (const auto& [i, j] : tree)
            {
                int dx = 0;
                int dz = 0;
                if (!DoorBetween(kept[static_cast<std::size_t>(i)], kept[static_cast<std::size_t>(j)], rng, dx, dz))
                    continue;
                if (dx <= 0 || dz <= 0 || dx >= Size - 1 || dz >= Size - 1)
                    continue;
                grid[static_cast<std::size_t>(dz)][static_cast<std::size_t>(dx)] = 'D';
                ++doorCount;
            }
            if (doorCount < 24) { continue; }

            const Room& startRoom = kept[static_cast<std::size_t>(rng.Range(0, static_cast<int>(kept.size()) - 1))];
            const int startX = (startRoom.x0 + startRoom.x1) / 2;
            const int startZ = (startRoom.z0 + startRoom.z1) / 2;
            if (grid[static_cast<std::size_t>(startZ)][static_cast<std::size_t>(startX)] != Floor) { continue; }

            // Wall off anything the player cannot stand in, so reachable == walkable holds.
            auto reach = Distances(grid, startX, startZ);
            for (int z = 0; z < Size; ++z)
                for (int x = 0; x < Size; ++x)
                    if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != Wall &&
                        reach[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] < 0)
                        grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] = Wall;

            int walkable = 0;
            for (int z = 0; z < Size; ++z)
                for (int x = 0; x < Size; ++x)
                    if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != Wall)
                        ++walkable;
            if (walkable < 1505 || walkable > 1980) { continue; }

            reach = Distances(grid, startX, startZ);

            // The elevator cabin: a wall cell with exactly one open side, as far as possible.
            int exitX = -1;
            int exitZ = -1;
            int bestDistance = -1;
            for (int z = 1; z < Size - 1; ++z)
            {
                for (int x = 1; x < Size - 1; ++x)
                {
                    if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != Wall)
                        continue;
                    int open = 0;
                    int openX = 0;
                    int openZ = 0;
                    constexpr std::array<std::pair<int, int>, 4> steps{
                        std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};
                    for (const auto [dx, dz] : steps)
                    {
                        if (grid[static_cast<std::size_t>(z + dz)][static_cast<std::size_t>(x + dx)] == Floor)
                        {
                            ++open;
                            openX = x + dx;
                            openZ = z + dz;
                        }
                    }
                    if (open != 1)
                        continue;
                    // The approach must itself sit in a room or corridor, not in a dead
                    // pocket: an elevator reachable only through a blind alley is one the
                    // player walks past without ever seeing its open face.
                    int approachNeighbours = 0;
                    for (const auto [ax, az] : steps)
                    {
                        if (grid[static_cast<std::size_t>(openZ + az)]
                                [static_cast<std::size_t>(openX + ax)] != Wall)
                            ++approachNeighbours;
                    }
                    if (approachNeighbours < 3)
                        continue;
                    const int distance = reach[static_cast<std::size_t>(openZ)][static_cast<std::size_t>(openX)];
                    if (distance > bestDistance)
                    {
                        bestDistance = distance;
                        exitX = x;
                        exitZ = z;
                    }
                }
            }
            if (exitX < 0) { continue; }
            grid[static_cast<std::size_t>(exitZ)][static_cast<std::size_t>(exitX)] = 'E';

            // Objectives, chosen so the shortest route through both lands in range.
            const auto exitDistance = Distances(grid, exitX, exitZ);
            std::vector<std::pair<int, int>> candidates;
            for (int z = 1; z < Size - 1; ++z)
                for (int x = 1; x < Size - 1; ++x)
                    if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] == Floor &&
                        reach[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] > 10)
                        candidates.emplace_back(x, z);
            if (candidates.size() < 32) { continue; }

            int relayX = -1;
            int relayZ = -1;
            int terminalX = -1;
            int terminalZ = -1;
            int route = 0;
            for (int probe = 0; probe < 240 && relayX < 0; ++probe)
            {
                const auto& a = candidates[static_cast<std::size_t>(rng.Range(0, static_cast<int>(candidates.size()) - 1))];
                const auto& b = candidates[static_cast<std::size_t>(rng.Range(0, static_cast<int>(candidates.size()) - 1))];
                if (a == b)
                    continue;
                const auto fromA = Distances(grid, a.first, a.second);
                const int pa = reach[static_cast<std::size_t>(a.second)][static_cast<std::size_t>(a.first)];
                const int pb = reach[static_cast<std::size_t>(b.second)][static_cast<std::size_t>(b.first)];
                const int ab = fromA[static_cast<std::size_t>(b.second)][static_cast<std::size_t>(b.first)];
                const int ae = exitDistance[static_cast<std::size_t>(a.second)][static_cast<std::size_t>(a.first)];
                const int be = exitDistance[static_cast<std::size_t>(b.second)][static_cast<std::size_t>(b.first)];
                if (pa < 0 || pb < 0 || ab < 0 || ae < 0 || be < 0)
                    continue;
                const int shortest = std::min(pa + ab + be, pb + ab + ae);
                if (shortest >= 90 && shortest <= 130)
                {
                    relayX = a.first;
                    relayZ = a.second;
                    terminalX = b.first;
                    terminalZ = b.second;
                    route = shortest;
                }
            }
            if (relayX < 0) { continue; }
            grid[static_cast<std::size_t>(relayZ)][static_cast<std::size_t>(relayX)] = 'O';
            grid[static_cast<std::size_t>(terminalZ)][static_cast<std::size_t>(terminalX)] = 'M';
            grid[static_cast<std::size_t>(startZ)][static_cast<std::size_t>(startX)] = 'P';

            // Recovery early and late on the route, which the pacing audit demands.
            std::vector<std::pair<int, int>> near;
            std::vector<std::pair<int, int>> far;
            for (int z = 1; z < Size - 1; ++z)
            {
                for (int x = 1; x < Size - 1; ++x)
                {
                    if (grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != Floor)
                        continue;
                    const int d = reach[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
                    if (d >= 0 && d <= 14)
                        near.emplace_back(x, z);
                    else if (d >= 52)
                        far.emplace_back(x, z);
                }
            }
            if (near.empty() || far.empty()) { continue; }
            {
                const auto& n = near[static_cast<std::size_t>(rng.Range(0, static_cast<int>(near.size()) - 1))];
                grid[static_cast<std::size_t>(n.second)][static_cast<std::size_t>(n.first)] = 'H';
                const auto& f = far[static_cast<std::size_t>(rng.Range(0, static_cast<int>(far.size()) - 1))];
                grid[static_cast<std::size_t>(f.second)][static_cast<std::size_t>(f.first)] = 'H';
                const auto& n2 = near[static_cast<std::size_t>(rng.Range(0, static_cast<int>(near.size()) - 1))];
                if (grid[static_cast<std::size_t>(n2.second)][static_cast<std::size_t>(n2.first)] == Floor)
                    grid[static_cast<std::size_t>(n2.second)][static_cast<std::size_t>(n2.first)] = 'h';
            }

            if (!PlacePatrol(grid, rng, 'G')) { continue; }

            // The curve: deeper runs face more of everything and are given less back.
            const int tier = std::min(depth, 10);
            const int theme = depth % 5;
            const bool bossFloor = depth > 0 && depth % 5 == 4;
            const bool secretFloor = depth > 0 && depth % 3 == 2 && !bossFloor;

            // Each theme leans on a different archetype, so consecutive sectors do not
            // simply repeat with larger numbers.
            int guards = 4 + tier / 2;
            int hounds = 2 + tier / 3;
            int troopers = 2 + tier / 3;
            int heavies = tier / 2;
            switch (theme)
            {
            case 0: guards += 2; break;                 // storage: rank and file
            case 1: hounds += 2; break;                 // kennels
            case 2: troopers += 2; break;               // laboratories
            case 3: heavies += 1; troopers += 1; break; // archive garrison
            default: guards += 1; heavies += 1; break;  // core
            }
            if (bossFloor)
            {
                // A Warden every fifth floor, with the garrison thinned so the fight is
                // the encounter rather than an extra one on top of a full sector.
                guards = std::max(2, guards - 2);
                hounds = std::max(1, hounds - 1);
            }
            const int ammoLarge = std::max(3, 6 - tier / 4) + (bossFloor ? 2 : 0);
            const int ammoSmall = std::max(1, 3 - tier / 5);

            struct Entry { char symbol; int count; Placement placement; };
            const std::array<Entry, 22> entries{{
                {'Z', bossFloor ? 1 : 0, Placement::OpenFloor},
                {'X', secretFloor ? 1 : 0, Placement::AgainstWall},
                {'S', 1, Placement::OpenFloor},
                {'Y', 2, Placement::OpenFloor},
                {'I', 3, Placement::Anywhere},
                {'G', guards, Placement::Anywhere},
                {'K', hounds, Placement::Anywhere},
                {'F', troopers, Placement::Anywhere},
                {'U', heavies, Placement::Anywhere},
                {'u', 1, Placement::Anywhere},
                {'A', ammoLarge, Placement::Anywhere},
                {'a', ammoSmall, Placement::Anywhere},
                {'W', 1, Placement::Anywhere},
                {'T', 1, Placement::Anywhere},
                {'J', 1, Placement::Anywhere},
                {'p', 1, Placement::Anywhere},
                {'R', 3, Placement::AgainstWall},
                {'B', 3, Placement::AgainstWall},
                {'L', 4, Placement::Anywhere},
                {'0', 1, Placement::OpenFloor},
                {'7', 1, Placement::OpenFloor},
                {'s', 2, Placement::OpenFloor}}};

            bool complete = true;
            for (const Entry& entry : entries)
            {
                if (entry.count <= 0)
                    continue;
                if (Scatter(grid, rng, entry.symbol, entry.count, entry.placement) < entry.count)
                {
                    complete = false;
                    break;
                }
            }
            if (!complete) { continue; }

            std::string text;
            text.reserve(static_cast<std::size_t>(Size) * (static_cast<std::size_t>(Size) + 1));
            for (const std::string& row : grid)
            {
                text += row;
                text += '\n';
            }

            std::string reason;
            if (!IsAcceptableSector(text, reason)) { continue; }

            result.grid = std::move(text);
            result.walkable = walkable;
            result.doors = doorCount;
            result.objectiveRoute = route;
            result.theme = theme;
            result.bossFloor = bossFloor;
            result.secretFloor = secretFloor;
            result.valid = true;
            return result;
        }
        return result;
    }
}
