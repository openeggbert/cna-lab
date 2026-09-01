#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Gameplay/TrafficSignal.hpp"
#include "IronGang/World/WaypointPath.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_14 IG-14-001/002: the road network as data.
    //
    // The warehouse block's traffic loop, its lane offsets, its speed limit and its two signalled
    // stop lines were C++ literals inside `PrototypeWorld::BuildWarehouseBlock()`. That is fine for
    // exactly one district and stops being fine at two: every new road means recompiling, and
    // nothing can validate a layout that only exists as statements.
    //
    // The schema is deliberately a **graph**, not a list of polylines: `nodes` are junctions and
    // road ends, `segments` are directed edges between them, and a lane is an offset from the
    // segment's centreline. That is what lets traffic ask "where does this road go" rather than
    // "what is the next point on my loop".

    struct RoadNode
    {
        std::string id;
        Vector3 position{};
    };

    struct RoadSegment
    {
        std::string id;
        std::string fromNodeId;
        std::string toNodeId;
        // How many lanes travel in this segment's direction. Lane 0 is nearest the centreline.
        int laneCount{1};
        // Distance between lane centres, metres.
        float laneWidthMetres{3.0F};
        float speedLimitKph{50.0F};
    };

    // plan_14 IG-14-001 names turn links in the schema. They are defined and validated here, and
    // the shipped district has none: its two segments are a straight there-and-back, and inventing
    // turns for it would be schema no content exercises.
    struct RoadTurn
    {
        std::string fromSegmentId;
        std::string toSegmentId;
    };

    // Where traffic must stop for a signal. `opposingPhase` picks the second of the two phases one
    // signal drives, so two directions can never show green together.
    struct RoadStopLine
    {
        std::string segmentId;
        // Distance along the segment from its `from` node, metres.
        float distanceMetres{0.0F};
        Vector3 signalPosition{};
        bool opposingPhase{false};
    };

    inline constexpr int kRoadGraphFileVersion = 1;

    class RoadGraph final
    {
    public:
        // Validation refuses an unsupported version, duplicate node or segment ids, a segment
        // naming a node that does not exist, a segment whose endpoints coincide, a non-positive
        // lane count, width or speed limit, a turn or stop line naming a segment that does not
        // exist, a stop line beyond the end of its own segment, and unknown fields. Every one of
        // those is a road layout that would load and then behave as something nobody authored.
        [[nodiscard]] bool LoadFromFile(const std::string& path, std::string& errorMessage);

        [[nodiscard]] bool IsEmpty() const noexcept { return segments_.empty(); }
        [[nodiscard]] const std::string& GetId() const noexcept { return id_; }
        [[nodiscard]] const std::vector<RoadNode>& GetNodes() const noexcept { return nodes_; }
        [[nodiscard]] const std::vector<RoadSegment>& GetSegments() const noexcept { return segments_; }
        [[nodiscard]] const std::vector<RoadTurn>& GetTurns() const noexcept { return turns_; }
        [[nodiscard]] const std::vector<RoadStopLine>& GetStopLines() const noexcept { return stopLines_; }

        [[nodiscard]] const RoadNode* FindNode(const std::string& id) const noexcept;
        [[nodiscard]] const RoadSegment* FindSegment(const std::string& id) const noexcept;

        // The centre of @p lane on @p segment, at @p distanceMetres from its `from` node. Lane 0
        // sits half a lane width right of the centreline, so a two-lane road has its lanes either
        // side of the paint rather than both on top of it.
        [[nodiscard]] bool GetLanePoint(const std::string& segmentId,
                                        int lane,
                                        float distanceMetres,
                                        Vector3& out) const noexcept;

        // A closed circuit through the given segments' lane 0, in order -- what a traffic vehicle
        // drives. Returns false if any segment id is unknown.
        [[nodiscard]] bool BuildLaneLoop(const std::vector<std::string>& segmentIds,
                                         int lane,
                                         WaypointPath& out) const;

        // The stop lines as the traffic simulation wants them, with each approach yaw derived from
        // its own segment's direction rather than authored separately and left to drift.
        [[nodiscard]] std::vector<TrafficStopLine> BuildTrafficStopLines() const;

    private:
        std::string id_;
        std::vector<RoadNode> nodes_;
        std::vector<RoadSegment> segments_;
        std::vector<RoadTurn> turns_;
        std::vector<RoadStopLine> stopLines_;
    };
}
