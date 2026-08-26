#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/World/WaypointPath.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_14 IG-14-007/008: where people walk, as data.
    //
    // Deliberately **separate from the road graph**, and the plan entry says so for a reason: a
    // pavement is not a road with different numbers. It is bidirectional (a road segment is not),
    // it has no lanes in the traffic sense, it connects to building entrances, and it crosses
    // roads at marked places rather than merging with them. Modelling both with one type would
    // mean every field is meaningful for one of them and inert for the other.

    struct SidewalkNode
    {
        std::string id;
        Vector3 position{};
    };

    // A stretch of pavement between two nodes, walkable in both directions -- which is why this is
    // a "walkway" and not a directed segment.
    struct SidewalkWalkway
    {
        std::string id;
        std::string fromNodeId;
        std::string toNodeId;
        // How wide the pavement is, metres. Pedestrians offset themselves inside it so two walking
        // in opposite directions pass rather than collide (plan_20 IG-20-010).
        float widthMetres{3.0F};
    };

    // Where a pavement crosses a road. Carried so pedestrian crossing behaviour (plan_20
    // IG-20-012) has somewhere to read the crossing from, rather than each district hard-coding
    // its own idea of where it is safe to step off the kerb.
    struct SidewalkCrossing
    {
        std::string id;
        std::string fromNodeId;
        std::string toNodeId;
        // The road segment being crossed, if the district has a road graph naming one.
        std::string roadSegmentId;
        // True when a signal governs it -- an unsignalled crossing is a give-way, not a wait.
        bool signalControlled{false};
    };

    // A door: a pavement node paired with the thing it leads into.
    struct SidewalkEntrance
    {
        std::string id;
        std::string nodeId;
        // The world box this entrance belongs to ("hotel", "warehouse"), so an entrance cannot
        // name a building the district does not contain.
        std::string buildingId;
    };

    inline constexpr int kSidewalkGraphFileVersion = 1;

    class SidewalkGraph final
    {
    public:
        // Validation refuses an unsupported version, duplicate ids of any kind, a walkway,
        // crossing or entrance naming a node that does not exist, a zero-length walkway or
        // crossing, a non-positive width, and unknown fields.
        //
        // @p knownBuildingIds and @p knownRoadSegmentIds are checked when non-empty: an entrance
        // into a building the district does not have, or a crossing over a road that is not there,
        // is a layout that loads and then means nothing -- the same stale-reference class of bug
        // that dialogue ids and cutscene cues already refuse.
        [[nodiscard]] bool LoadFromFile(const std::string& path,
                                        const std::vector<std::string>& knownBuildingIds,
                                        const std::vector<std::string>& knownRoadSegmentIds,
                                        std::string& errorMessage);

        [[nodiscard]] bool IsEmpty() const noexcept { return walkways_.empty(); }
        [[nodiscard]] const std::string& GetId() const noexcept { return id_; }
        [[nodiscard]] const std::vector<SidewalkNode>& GetNodes() const noexcept { return nodes_; }
        [[nodiscard]] const std::vector<SidewalkWalkway>& GetWalkways() const noexcept { return walkways_; }
        [[nodiscard]] const std::vector<SidewalkCrossing>& GetCrossings() const noexcept
        {
            return crossings_;
        }
        [[nodiscard]] const std::vector<SidewalkEntrance>& GetEntrances() const noexcept
        {
            return entrances_;
        }

        [[nodiscard]] const SidewalkNode* FindNode(const std::string& id) const noexcept;

        // One back-and-forth path per walkway, which is what a pedestrian walks today. Returns an
        // empty vector for an empty graph.
        [[nodiscard]] std::vector<WaypointPath> BuildWalkingPaths() const;

    private:
        std::string id_;
        std::vector<SidewalkNode> nodes_;
        std::vector<SidewalkWalkway> walkways_;
        std::vector<SidewalkCrossing> crossings_;
        std::vector<SidewalkEntrance> entrances_;
    };
}
