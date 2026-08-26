#include "IronGang/World/RoadGraph.hpp"

#include "../Core/JsonDataFileInternal.hpp"
#include "../Core/JsonReadHelpers.hpp"

#include "System/Text/Json/JsonProperty.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    const RoadNode* RoadGraph::FindNode(const std::string& id) const noexcept
    {
        const auto found = std::find_if(nodes_.begin(), nodes_.end(),
                                        [&id](const RoadNode& node) { return node.id == id; });
        return found == nodes_.end() ? nullptr : &*found;
    }

    const RoadSegment* RoadGraph::FindSegment(const std::string& id) const noexcept
    {
        const auto found = std::find_if(segments_.begin(), segments_.end(),
                                        [&id](const RoadSegment& segment) { return segment.id == id; });
        return found == segments_.end() ? nullptr : &*found;
    }

    bool RoadGraph::GetLanePoint(const std::string& segmentId,
                                 int lane,
                                 float distanceMetres,
                                 Vector3& out) const noexcept
    {
        const RoadSegment* segment = FindSegment(segmentId);
        if (segment == nullptr || lane < 0 || lane >= segment->laneCount)
        {
            return false;
        }
        const RoadNode* from = FindNode(segment->fromNodeId);
        const RoadNode* to = FindNode(segment->toNodeId);
        if (from == nullptr || to == nullptr)
        {
            return false;
        }
        Vector3 along = to->position - from->position;
        const float length = along.Length();
        if (length <= 1e-4F)
        {
            return false;
        }
        along = along / length;
        // Right of travel, in the same convention ForwardFromYaw/RightFromYaw use.
        const Vector3 right(-along.Z, 0.0F, along.X);
        // Lane 0 sits half a width right of the paint, lane 1 a further width out, and so on.
        const float offset = segment->laneWidthMetres * (0.5F + static_cast<float>(lane));
        out = from->position + along * std::clamp(distanceMetres, 0.0F, length) + right * offset;
        return true;
    }

    bool RoadGraph::BuildLaneLoop(const std::vector<std::string>& segmentIds,
                                  int lane,
                                  WaypointPath& out) const
    {
        WaypointPath path;
        path.loop = true;
        for (const std::string& segmentId : segmentIds)
        {
            const RoadSegment* segment = FindSegment(segmentId);
            if (segment == nullptr)
            {
                return false;
            }
            const RoadNode* from = FindNode(segment->fromNodeId);
            const RoadNode* to = FindNode(segment->toNodeId);
            if (from == nullptr || to == nullptr)
            {
                return false;
            }
            const float length = (to->position - from->position).Length();
            Vector3 start;
            Vector3 end;
            if (!GetLanePoint(segmentId, lane, 0.0F, start) ||
                !GetLanePoint(segmentId, lane, length, end))
            {
                return false;
            }
            path.points.push_back(start);
            path.points.push_back(end);
        }
        out = std::move(path);
        return true;
    }

    std::vector<TrafficStopLine> RoadGraph::BuildTrafficStopLines() const
    {
        std::vector<TrafficStopLine> result;
        for (const RoadStopLine& line : stopLines_)
        {
            const RoadSegment* segment = FindSegment(line.segmentId);
            if (segment == nullptr)
            {
                continue;
            }
            const RoadNode* from = FindNode(segment->fromNodeId);
            const RoadNode* to = FindNode(segment->toNodeId);
            if (from == nullptr || to == nullptr)
            {
                continue;
            }
            Vector3 position;
            if (!GetLanePoint(line.segmentId, 0, line.distanceMetres, position))
            {
                continue;
            }
            const Vector3 along = to->position - from->position;
            // Derived from the segment, not authored: an approach yaw written by hand beside the
            // road it belongs to is a number that drifts the first time the road moves.
            const float yaw = std::atan2(along.X, -along.Z);
            result.push_back(TrafficStopLine{position, yaw, line.signalPosition, line.opposingPhase});
        }
        return result;
    }

    bool RoadGraph::LoadFromFile(const std::string& path, std::string& errorMessage)
    {
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }
        const JsonElement& root = file.root;

        std::string id;
        std::vector<RoadNode> nodes;
        std::vector<RoadSegment> segments;
        std::vector<RoadTurn> turns;
        std::vector<RoadStopLine> stopLines;

        try
        {
            if (!JsonRead::OnlyFields(root, {"id", "version", "nodes", "segments", "turns", "stopLines"},
                            "road graph", path, errorMessage))
            {
                return false;
            }

            double version = 0.0;
            if (!JsonRead::NumberField(root, "version", version) ||
                static_cast<int>(version) != kRoadGraphFileVersion)
            {
                errorMessage = "road graph version must be " + std::to_string(kRoadGraphFileVersion) +
                               ": " + path;
                return false;
            }
            if (!JsonRead::StringField(root, "id", id))
            {
                errorMessage = "road graph has no non-empty \"id\": " + path;
                return false;
            }

            JsonElement nodesElement;
            if (!root.TryGetProperty("nodes", nodesElement) ||
                nodesElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "road graph has no \"nodes\" array: " + path;
                return false;
            }
            for (const JsonElement& entry : nodesElement.EnumerateArray())
            {
                if (!JsonRead::OnlyFields(entry, {"id", "position"}, "a road node", path, errorMessage))
                {
                    return false;
                }
                RoadNode node;
                JsonElement positionElement;
                if (!JsonRead::StringField(entry, "id", node.id) ||
                    !entry.TryGetProperty("position", positionElement) ||
                    !JsonRead::Vector3Field(positionElement, node.position))
                {
                    errorMessage = "every road node needs an \"id\" and a three-number \"position\": " + path;
                    return false;
                }
                if (std::any_of(nodes.begin(), nodes.end(),
                                [&node](const RoadNode& existing) { return existing.id == node.id; }))
                {
                    errorMessage = "duplicate road node id \"" + node.id + "\": " + path;
                    return false;
                }
                nodes.push_back(std::move(node));
            }

            JsonElement segmentsElement;
            if (!root.TryGetProperty("segments", segmentsElement) ||
                segmentsElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "road graph has no \"segments\" array: " + path;
                return false;
            }
            for (const JsonElement& entry : segmentsElement.EnumerateArray())
            {
                if (!JsonRead::OnlyFields(entry, {"id", "from", "to", "laneCount", "laneWidth", "speedLimitKph"},
                                "a road segment", path, errorMessage))
                {
                    return false;
                }
                RoadSegment segment;
                double laneCount = 0.0;
                double laneWidth = 0.0;
                double speedLimit = 0.0;
                if (!JsonRead::StringField(entry, "id", segment.id) ||
                    !JsonRead::StringField(entry, "from", segment.fromNodeId) ||
                    !JsonRead::StringField(entry, "to", segment.toNodeId) ||
                    !JsonRead::NumberField(entry, "laneCount", laneCount) ||
                    !JsonRead::NumberField(entry, "laneWidth", laneWidth) ||
                    !JsonRead::NumberField(entry, "speedLimitKph", speedLimit))
                {
                    errorMessage = "every road segment needs id, from, to, laneCount, laneWidth and "
                                   "speedLimitKph: " + path;
                    return false;
                }
                segment.laneCount = static_cast<int>(laneCount);
                segment.laneWidthMetres = static_cast<float>(laneWidth);
                segment.speedLimitKph = static_cast<float>(speedLimit);

                if (std::any_of(segments.begin(), segments.end(),
                                [&segment](const RoadSegment& existing) {
                                    return existing.id == segment.id;
                                }))
                {
                    errorMessage = "duplicate road segment id \"" + segment.id + "\": " + path;
                    return false;
                }
                if (segment.laneCount <= 0 || segment.laneWidthMetres <= 0.0F ||
                    segment.speedLimitKph <= 0.0F)
                {
                    errorMessage = "road segment \"" + segment.id +
                                   "\" needs a positive lane count, lane width and speed limit: " + path;
                    return false;
                }

                const auto findNode = [&nodes](const std::string& nodeId) {
                    return std::find_if(nodes.begin(), nodes.end(), [&nodeId](const RoadNode& node) {
                        return node.id == nodeId;
                    });
                };
                const auto fromIt = findNode(segment.fromNodeId);
                const auto toIt = findNode(segment.toNodeId);
                if (fromIt == nodes.end() || toIt == nodes.end())
                {
                    errorMessage = "road segment \"" + segment.id + "\" names a node that does not exist: " +
                                   path;
                    return false;
                }
                if ((toIt->position - fromIt->position).Length() <= 1e-3F)
                {
                    errorMessage = "road segment \"" + segment.id +
                                   "\" has no length -- its endpoints coincide: " + path;
                    return false;
                }
                segments.push_back(std::move(segment));
            }
            if (segments.empty())
            {
                errorMessage = "road graph has no segments, so nothing can drive on it: " + path;
                return false;
            }

            const auto hasSegment = [&segments](const std::string& segmentId) {
                return std::any_of(segments.begin(), segments.end(),
                                   [&segmentId](const RoadSegment& segment) {
                                       return segment.id == segmentId;
                                   });
            };

            JsonElement turnsElement;
            if (root.TryGetProperty("turns", turnsElement))
            {
                if (turnsElement.getValueKindProperty() != JsonValueKind::Array)
                {
                    errorMessage = "road graph \"turns\" must be an array: " + path;
                    return false;
                }
                for (const JsonElement& entry : turnsElement.EnumerateArray())
                {
                    if (!JsonRead::OnlyFields(entry, {"from", "to"}, "a road turn", path, errorMessage))
                    {
                        return false;
                    }
                    RoadTurn turn;
                    if (!JsonRead::StringField(entry, "from", turn.fromSegmentId) ||
                        !JsonRead::StringField(entry, "to", turn.toSegmentId))
                    {
                        errorMessage = "every road turn needs \"from\" and \"to\" segment ids: " + path;
                        return false;
                    }
                    if (!hasSegment(turn.fromSegmentId) || !hasSegment(turn.toSegmentId))
                    {
                        errorMessage = "road turn names a segment that does not exist: " + path;
                        return false;
                    }
                    turns.push_back(std::move(turn));
                }
            }

            JsonElement stopLinesElement;
            if (root.TryGetProperty("stopLines", stopLinesElement))
            {
                if (stopLinesElement.getValueKindProperty() != JsonValueKind::Array)
                {
                    errorMessage = "road graph \"stopLines\" must be an array: " + path;
                    return false;
                }
                for (const JsonElement& entry : stopLinesElement.EnumerateArray())
                {
                    if (!JsonRead::OnlyFields(entry, {"segment", "distance", "signalPosition", "opposingPhase"},
                                    "a road stop line", path, errorMessage))
                    {
                        return false;
                    }
                    RoadStopLine line;
                    double distance = 0.0;
                    JsonElement signalElement;
                    if (!JsonRead::StringField(entry, "segment", line.segmentId) ||
                        !JsonRead::NumberField(entry, "distance", distance) ||
                        !entry.TryGetProperty("signalPosition", signalElement) ||
                        !JsonRead::Vector3Field(signalElement, line.signalPosition))
                    {
                        errorMessage = "every stop line needs a segment, a distance and a "
                                       "three-number signalPosition: " + path;
                        return false;
                    }
                    line.distanceMetres = static_cast<float>(distance);
                    JsonElement opposingElement;
                    if (entry.TryGetProperty("opposingPhase", opposingElement))
                    {
                        const JsonValueKind kind = opposingElement.getValueKindProperty();
                        if (kind != JsonValueKind::True && kind != JsonValueKind::False)
                        {
                            errorMessage = "stop line \"opposingPhase\" must be a boolean: " + path;
                            return false;
                        }
                        line.opposingPhase = kind == JsonValueKind::True;
                    }
                    const auto segmentIt =
                        std::find_if(segments.begin(), segments.end(),
                                     [&line](const RoadSegment& segment) {
                                         return segment.id == line.segmentId;
                                     });
                    if (segmentIt == segments.end())
                    {
                        errorMessage = "stop line names segment \"" + line.segmentId +
                                       "\", which does not exist: " + path;
                        return false;
                    }
                    const auto findNode = [&nodes](const std::string& nodeId) {
                        return std::find_if(nodes.begin(), nodes.end(), [&nodeId](const RoadNode& node) {
                            return node.id == nodeId;
                        });
                    };
                    const float length = (findNode(segmentIt->toNodeId)->position -
                                          findNode(segmentIt->fromNodeId)->position)
                                             .Length();
                    if (line.distanceMetres < 0.0F || line.distanceMetres > length)
                    {
                        errorMessage = "stop line on \"" + line.segmentId + "\" is at " +
                                       std::to_string(line.distanceMetres) + " m, outside the segment's " +
                                       std::to_string(length) + " m: " + path;
                        return false;
                    }
                    stopLines.push_back(std::move(line));
                }
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string("failed to read road graph ") + path + ": " + exception.what();
            return false;
        }

        id_ = std::move(id);
        nodes_ = std::move(nodes);
        segments_ = std::move(segments);
        turns_ = std::move(turns);
        stopLines_ = std::move(stopLines);
        return true;
    }
}
