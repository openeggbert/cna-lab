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

    // plan_19 IG-19-001: a named walking route an ambient pedestrian patrols. Splitting a pavement
    // at every crossing and doorway is what makes the graph connected, but it also means "the west
    // pavement" is no longer one walkway -- so which end-to-end walk a pedestrian does is an
    // authoring decision, and belongs in the data rather than as node ids compiled into the game.
    struct SidewalkRoute
    {
        std::string id;
        std::string fromNodeId;
        std::string toNodeId;
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
        // Loading also refuses a **disconnected** graph (plan_19 IG-19-004), naming an unreachable
        // node: a door or pavement nothing can walk to is a mistake that would otherwise surface
        // as pedestrians mysteriously absent from part of a district.
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
        [[nodiscard]] const std::vector<SidewalkRoute>& GetRoutes() const noexcept { return routes_; }
        [[nodiscard]] const std::vector<SidewalkEntrance>& GetEntrances() const noexcept
        {
            return entrances_;
        }

        [[nodiscard]] const SidewalkNode* FindNode(const std::string& id) const noexcept;

        // plan_19 IG-19-001/022: the shortest walk between two nodes, as node ids including both
        // ends. Empty when there is no route. Walkways and crossings are both edges and both
        // bidirectional -- a crossing is a way to get to the other pavement, which is the whole
        // reason pedestrian routing needs the graph rather than a single polyline.
        [[nodiscard]] std::vector<std::string> FindWalkingRoute(const std::string& fromNodeId,
                                                                const std::string& toNodeId) const;
        // The same route as a walkable path. @p loop closes it, which is what an ambient
        // pedestrian shuttling a pavement wants.
        [[nodiscard]] bool BuildRoutePath(const std::string& fromNodeId,
                                          const std::string& toNodeId,
                                          bool loop,
                                          WaypointPath& out) const;

        // plan_19 IG-19-004: every node not reachable from @p fromNodeId. A pavement nobody can
        // walk to is a content mistake that otherwise shows up as pedestrians who never appear
        // where an author expected them.
        [[nodiscard]] std::vector<std::string> FindUnreachableNodes(const std::string& fromNodeId) const;

        // One looping path per declared route -- what ambient pedestrians patrol. Falls back to one
        // path per walkway for a graph that declares no routes, so a pavement that needs no
        // splitting needs no routes either.
        [[nodiscard]] std::vector<WaypointPath> BuildWalkingPaths() const;

        // plan_20 IG-20-012: one shuttle path per crossing -- kerb to kerb and back. A pedestrian
        // on one of these actually uses the crossing, which is what makes the signal rule
        // observable rather than a rule about nothing.
        [[nodiscard]] std::vector<WaypointPath> BuildCrossingPaths() const;
        // The kerbs of every crossing, as the points a waiting pedestrian is held at.
        [[nodiscard]] std::vector<Vector3> GetCrossingKerbs() const;

    private:
        std::string id_;
        std::vector<SidewalkNode> nodes_;
        std::vector<SidewalkWalkway> walkways_;
        std::vector<SidewalkCrossing> crossings_;
        std::vector<SidewalkRoute> routes_;
        std::vector<SidewalkEntrance> entrances_;
    };
}
