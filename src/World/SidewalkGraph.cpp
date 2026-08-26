#include "IronGang/World/SidewalkGraph.hpp"

#include "../Core/JsonDataFileInternal.hpp"
#include "../Core/JsonReadHelpers.hpp"

#include <algorithm>
#include <limits>

namespace IronGang
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    namespace
    {
        template <typename T>
        [[nodiscard]] bool HasId(const std::vector<T>& items, const std::string& id)
        {
            return std::any_of(items.begin(), items.end(),
                               [&id](const T& item) { return item.id == id; });
        }

        [[nodiscard]] bool Contains(const std::vector<std::string>& values, const std::string& value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }
    }

    const SidewalkNode* SidewalkGraph::FindNode(const std::string& id) const noexcept
    {
        const auto found = std::find_if(nodes_.begin(), nodes_.end(),
                                        [&id](const SidewalkNode& node) { return node.id == id; });
        return found == nodes_.end() ? nullptr : &*found;
    }

    std::vector<WaypointPath> SidewalkGraph::BuildWalkingPaths() const
    {
        std::vector<WaypointPath> paths;
        if (!routes_.empty())
        {
            paths.reserve(routes_.size());
            for (const SidewalkRoute& route : routes_)
            {
                WaypointPath path;
                if (BuildRoutePath(route.fromNodeId, route.toNodeId, true, path))
                {
                    paths.push_back(std::move(path));
                }
            }
            return paths;
        }
        paths.reserve(walkways_.size());
        for (const SidewalkWalkway& walkway : walkways_)
        {
            const SidewalkNode* from = FindNode(walkway.fromNodeId);
            const SidewalkNode* to = FindNode(walkway.toNodeId);
            if (from == nullptr || to == nullptr)
            {
                continue;
            }
            // A two-point looping path: AdvanceAlongPath() walks it there and back, which is what
            // a pedestrian on a stretch of pavement does.
            paths.push_back(WaypointPath{{from->position, to->position}, true});
        }
        return paths;
    }

    namespace
    {
        struct Edge
        {
            std::size_t to;
            float cost;
        };
    }

    std::vector<std::string> SidewalkGraph::FindWalkingRoute(const std::string& fromNodeId,
                                                             const std::string& toNodeId) const
    {
        const auto indexOf = [this](const std::string& id) -> std::size_t {
            for (std::size_t index = 0; index < nodes_.size(); ++index)
            {
                if (nodes_[index].id == id)
                {
                    return index;
                }
            }
            return nodes_.size();
        };

        const std::size_t start = indexOf(fromNodeId);
        const std::size_t goal = indexOf(toNodeId);
        if (start == nodes_.size() || goal == nodes_.size())
        {
            return {};
        }
        if (start == goal)
        {
            return {fromNodeId};
        }

        // Walkways and crossings are the same kind of edge here: both are ways to walk from one
        // node to another, both bidirectional. What distinguishes a crossing is that a pedestrian
        // has to wait at it, which is Pedestrian's problem, not the router's.
        std::vector<std::vector<Edge>> adjacency(nodes_.size());
        const auto addEdge = [&](const std::string& a, const std::string& b) {
            const std::size_t from = indexOf(a);
            const std::size_t to = indexOf(b);
            if (from == nodes_.size() || to == nodes_.size())
            {
                return;
            }
            const float cost = (nodes_[to].position - nodes_[from].position).Length();
            adjacency[from].push_back(Edge{to, cost});
            adjacency[to].push_back(Edge{from, cost});
        };
        for (const SidewalkWalkway& walkway : walkways_)
        {
            addEdge(walkway.fromNodeId, walkway.toNodeId);
        }
        for (const SidewalkCrossing& crossing : crossings_)
        {
            addEdge(crossing.fromNodeId, crossing.toNodeId);
        }

        // Dijkstra: a handful of nodes per district, so the simplest correct thing is also the
        // right thing. A* would need a heuristic and buy nothing at this size.
        constexpr float kInfinity = std::numeric_limits<float>::max();
        std::vector<float> distance(nodes_.size(), kInfinity);
        std::vector<std::size_t> previous(nodes_.size(), nodes_.size());
        std::vector<bool> settled(nodes_.size(), false);
        distance[start] = 0.0F;

        for (std::size_t step = 0; step < nodes_.size(); ++step)
        {
            std::size_t current = nodes_.size();
            float best = kInfinity;
            for (std::size_t index = 0; index < nodes_.size(); ++index)
            {
                if (!settled[index] && distance[index] < best)
                {
                    best = distance[index];
                    current = index;
                }
            }
            if (current == nodes_.size())
            {
                break; // nothing reachable is left
            }
            if (current == goal)
            {
                break;
            }
            settled[current] = true;
            for (const Edge& edge : adjacency[current])
            {
                const float candidate = distance[current] + edge.cost;
                if (candidate < distance[edge.to])
                {
                    distance[edge.to] = candidate;
                    previous[edge.to] = current;
                }
            }
        }

        if (distance[goal] == kInfinity)
        {
            return {};
        }
        std::vector<std::string> route;
        for (std::size_t index = goal; index != nodes_.size(); index = previous[index])
        {
            route.push_back(nodes_[index].id);
            if (index == start)
            {
                break;
            }
        }
        std::reverse(route.begin(), route.end());
        return route;
    }

    bool SidewalkGraph::BuildRoutePath(const std::string& fromNodeId,
                                       const std::string& toNodeId,
                                       bool loop,
                                       WaypointPath& out) const
    {
        const std::vector<std::string> route = FindWalkingRoute(fromNodeId, toNodeId);
        if (route.size() < 2)
        {
            return false;
        }
        WaypointPath path;
        path.loop = loop;
        for (const std::string& nodeId : route)
        {
            const SidewalkNode* node = FindNode(nodeId);
            if (node == nullptr)
            {
                return false;
            }
            path.points.push_back(node->position);
        }
        out = std::move(path);
        return true;
    }

    std::vector<std::string> SidewalkGraph::FindUnreachableNodes(const std::string& fromNodeId) const
    {
        std::vector<std::string> unreachable;
        if (FindNode(fromNodeId) == nullptr)
        {
            return unreachable;
        }
        for (const SidewalkNode& node : nodes_)
        {
            if (node.id != fromNodeId && FindWalkingRoute(fromNodeId, node.id).empty())
            {
                unreachable.push_back(node.id);
            }
        }
        return unreachable;
    }

    std::vector<WaypointPath> SidewalkGraph::BuildCrossingPaths() const
    {
        std::vector<WaypointPath> paths;
        paths.reserve(crossings_.size());
        for (const SidewalkCrossing& crossing : crossings_)
        {
            const SidewalkNode* from = FindNode(crossing.fromNodeId);
            const SidewalkNode* to = FindNode(crossing.toNodeId);
            if (from != nullptr && to != nullptr)
            {
                paths.push_back(WaypointPath{{from->position, to->position}, true});
            }
        }
        return paths;
    }

    std::vector<Vector3> SidewalkGraph::GetCrossingKerbs() const
    {
        std::vector<Vector3> kerbs;
        for (const SidewalkCrossing& crossing : crossings_)
        {
            const SidewalkNode* from = FindNode(crossing.fromNodeId);
            const SidewalkNode* to = FindNode(crossing.toNodeId);
            if (from != nullptr)
            {
                kerbs.push_back(from->position);
            }
            if (to != nullptr)
            {
                kerbs.push_back(to->position);
            }
        }
        return kerbs;
    }

    bool SidewalkGraph::LoadFromFile(const std::string& path,
                                     const std::vector<std::string>& knownBuildingIds,
                                     const std::vector<std::string>& knownRoadSegmentIds,
                                     std::string& errorMessage)
    {
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }
        const JsonElement& root = file.root;

        std::string id;
        std::vector<SidewalkNode> nodes;
        std::vector<SidewalkWalkway> walkways;
        std::vector<SidewalkCrossing> crossings;
        std::vector<SidewalkRoute> routes;
        std::vector<SidewalkEntrance> entrances;

        try
        {
            if (!JsonRead::OnlyFields(root, {"id", "version", "nodes", "walkways", "crossings", "routes", "entrances"},
                                      "sidewalk graph", path, errorMessage))
            {
                return false;
            }

            double version = 0.0;
            if (!JsonRead::NumberField(root, "version", version) ||
                static_cast<int>(version) != kSidewalkGraphFileVersion)
            {
                errorMessage = "sidewalk graph version must be " +
                               std::to_string(kSidewalkGraphFileVersion) + ": " + path;
                return false;
            }
            if (!JsonRead::StringField(root, "id", id))
            {
                errorMessage = "sidewalk graph has no non-empty \"id\": " + path;
                return false;
            }

            JsonElement nodesElement;
            if (!root.TryGetProperty("nodes", nodesElement) ||
                nodesElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "sidewalk graph has no \"nodes\" array: " + path;
                return false;
            }
            for (const JsonElement& entry : nodesElement.EnumerateArray())
            {
                if (!JsonRead::OnlyFields(entry, {"id", "position"}, "a sidewalk node", path, errorMessage))
                {
                    return false;
                }
                SidewalkNode node;
                JsonElement positionElement;
                if (!JsonRead::StringField(entry, "id", node.id) ||
                    !entry.TryGetProperty("position", positionElement) ||
                    !JsonRead::Vector3Field(positionElement, node.position))
                {
                    errorMessage = "every sidewalk node needs an \"id\" and a three-number "
                                   "\"position\": " + path;
                    return false;
                }
                if (HasId(nodes, node.id))
                {
                    errorMessage = "duplicate sidewalk node id \"" + node.id + "\": " + path;
                    return false;
                }
                nodes.push_back(std::move(node));
            }

            const auto findNode = [&nodes](const std::string& nodeId) {
                return std::find_if(nodes.begin(), nodes.end(), [&nodeId](const SidewalkNode& node) {
                    return node.id == nodeId;
                });
            };
            // Shared by walkways and crossings: both join two distinct nodes that must exist and
            // must not be in the same place.
            const auto readEnds = [&](const JsonElement& entry, const char* what, std::string& fromId,
                                      std::string& toId) {
                if (!JsonRead::StringField(entry, "from", fromId) ||
                    !JsonRead::StringField(entry, "to", toId))
                {
                    errorMessage = std::string("every ") + what + " needs \"from\" and \"to\" node ids: " +
                                   path;
                    return false;
                }
                const auto fromIt = findNode(fromId);
                const auto toIt = findNode(toId);
                if (fromIt == nodes.end() || toIt == nodes.end())
                {
                    errorMessage = std::string("a ") + what + " names a node that does not exist: " + path;
                    return false;
                }
                if ((toIt->position - fromIt->position).Length() <= 1e-3F)
                {
                    errorMessage = std::string("a ") + what +
                                   " has no length -- its endpoints coincide: " + path;
                    return false;
                }
                return true;
            };

            JsonElement walkwaysElement;
            if (!root.TryGetProperty("walkways", walkwaysElement) ||
                walkwaysElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "sidewalk graph has no \"walkways\" array: " + path;
                return false;
            }
            for (const JsonElement& entry : walkwaysElement.EnumerateArray())
            {
                if (!JsonRead::OnlyFields(entry, {"id", "from", "to", "width"}, "a walkway", path,
                                          errorMessage))
                {
                    return false;
                }
                SidewalkWalkway walkway;
                double width = 0.0;
                if (!JsonRead::StringField(entry, "id", walkway.id) ||
                    !JsonRead::NumberField(entry, "width", width))
                {
                    errorMessage = "every walkway needs an \"id\" and a \"width\": " + path;
                    return false;
                }
                if (!readEnds(entry, "walkway", walkway.fromNodeId, walkway.toNodeId))
                {
                    return false;
                }
                walkway.widthMetres = static_cast<float>(width);
                if (walkway.widthMetres <= 0.0F)
                {
                    errorMessage = "walkway \"" + walkway.id + "\" needs a positive width: " + path;
                    return false;
                }
                if (HasId(walkways, walkway.id))
                {
                    errorMessage = "duplicate walkway id \"" + walkway.id + "\": " + path;
                    return false;
                }
                walkways.push_back(std::move(walkway));
            }
            if (walkways.empty())
            {
                errorMessage = "sidewalk graph has no walkways, so nobody can walk on it: " + path;
                return false;
            }

            JsonElement crossingsElement;
            if (root.TryGetProperty("crossings", crossingsElement))
            {
                if (crossingsElement.getValueKindProperty() != JsonValueKind::Array)
                {
                    errorMessage = "sidewalk graph \"crossings\" must be an array: " + path;
                    return false;
                }
                for (const JsonElement& entry : crossingsElement.EnumerateArray())
                {
                    if (!JsonRead::OnlyFields(entry, {"id", "from", "to", "roadSegment", "signalControlled"},
                                              "a crossing", path, errorMessage))
                    {
                        return false;
                    }
                    SidewalkCrossing crossing;
                    if (!JsonRead::StringField(entry, "id", crossing.id))
                    {
                        errorMessage = "every crossing needs an \"id\": " + path;
                        return false;
                    }
                    if (!readEnds(entry, "crossing", crossing.fromNodeId, crossing.toNodeId))
                    {
                        return false;
                    }
                    if (HasId(crossings, crossing.id))
                    {
                        errorMessage = "duplicate crossing id \"" + crossing.id + "\": " + path;
                        return false;
                    }
                    // Optional: a crossing over painted tarmac need not name a road segment.
                    (void)JsonRead::StringField(entry, "roadSegment", crossing.roadSegmentId);
                    if (!crossing.roadSegmentId.empty() && !knownRoadSegmentIds.empty() &&
                        !Contains(knownRoadSegmentIds, crossing.roadSegmentId))
                    {
                        errorMessage = "crossing \"" + crossing.id + "\" crosses road segment \"" +
                                       crossing.roadSegmentId + "\", which this district does not have: " +
                                       path;
                        return false;
                    }
                    bool malformed = false;
                    (void)JsonRead::OptionalBoolField(entry, "signalControlled", crossing.signalControlled,
                                                      malformed);
                    if (malformed)
                    {
                        errorMessage = "crossing \"" + crossing.id +
                                       "\" has a non-boolean \"signalControlled\": " + path;
                        return false;
                    }
                    crossings.push_back(std::move(crossing));
                }
            }

            JsonElement routesElement;
            if (root.TryGetProperty("routes", routesElement))
            {
                if (routesElement.getValueKindProperty() != JsonValueKind::Array)
                {
                    errorMessage = "sidewalk graph \"routes\" must be an array: " + path;
                    return false;
                }
                for (const JsonElement& entry : routesElement.EnumerateArray())
                {
                    if (!JsonRead::OnlyFields(entry, {"id", "from", "to"}, "a route", path, errorMessage))
                    {
                        return false;
                    }
                    SidewalkRoute route;
                    if (!JsonRead::StringField(entry, "id", route.id) ||
                        !JsonRead::StringField(entry, "from", route.fromNodeId) ||
                        !JsonRead::StringField(entry, "to", route.toNodeId))
                    {
                        errorMessage = "every route needs an \"id\", a \"from\" and a \"to\": " + path;
                        return false;
                    }
                    if (HasId(routes, route.id))
                    {
                        errorMessage = "duplicate route id \"" + route.id + "\": " + path;
                        return false;
                    }
                    if (findNode(route.fromNodeId) == nodes.end() ||
                        findNode(route.toNodeId) == nodes.end())
                    {
                        errorMessage = "route \"" + route.id + "\" names a node that does not exist: " + path;
                        return false;
                    }
                    if (route.fromNodeId == route.toNodeId)
                    {
                        errorMessage = "route \"" + route.id + "\" starts and ends at the same node: " + path;
                        return false;
                    }
                    routes.push_back(std::move(route));
                }
            }

            JsonElement entrancesElement;
            if (root.TryGetProperty("entrances", entrancesElement))
            {
                if (entrancesElement.getValueKindProperty() != JsonValueKind::Array)
                {
                    errorMessage = "sidewalk graph \"entrances\" must be an array: " + path;
                    return false;
                }
                for (const JsonElement& entry : entrancesElement.EnumerateArray())
                {
                    if (!JsonRead::OnlyFields(entry, {"id", "node", "building"}, "an entrance", path,
                                              errorMessage))
                    {
                        return false;
                    }
                    SidewalkEntrance entrance;
                    if (!JsonRead::StringField(entry, "id", entrance.id) ||
                        !JsonRead::StringField(entry, "node", entrance.nodeId) ||
                        !JsonRead::StringField(entry, "building", entrance.buildingId))
                    {
                        errorMessage = "every entrance needs an \"id\", a \"node\" and a \"building\": " +
                                       path;
                        return false;
                    }
                    if (HasId(entrances, entrance.id))
                    {
                        errorMessage = "duplicate entrance id \"" + entrance.id + "\": " + path;
                        return false;
                    }
                    if (findNode(entrance.nodeId) == nodes.end())
                    {
                        errorMessage = "entrance \"" + entrance.id + "\" names node \"" + entrance.nodeId +
                                       "\", which does not exist: " + path;
                        return false;
                    }
                    if (!knownBuildingIds.empty() && !Contains(knownBuildingIds, entrance.buildingId))
                    {
                        // The same stale-reference rule dialogue ids and cutscene cues already use:
                        // a door into a building the district does not contain leads nowhere.
                        errorMessage = "entrance \"" + entrance.id + "\" leads into \"" +
                                       entrance.buildingId + "\", which this district does not contain: " +
                                       path;
                        return false;
                    }
                    entrances.push_back(std::move(entrance));
                }
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string("failed to read sidewalk graph ") + path + ": " + exception.what();
            return false;
        }

        // plan_19 IG-19-004: connectivity, checked on the candidate data before it is committed,
        // so a rejected graph leaves the previous one intact.
        SidewalkGraph candidate;
        candidate.id_ = id;
        candidate.nodes_ = nodes;
        candidate.walkways_ = walkways;
        candidate.crossings_ = crossings;
        candidate.routes_ = routes;
        candidate.entrances_ = entrances;
        const std::vector<std::string> unreachable =
            candidate.FindUnreachableNodes(candidate.nodes_.front().id);
        if (!unreachable.empty())
        {
            errorMessage = "sidewalk graph is disconnected: \"" + unreachable.front() +
                           "\" cannot be walked to from \"" + candidate.nodes_.front().id + "\" (" +
                           std::to_string(unreachable.size()) + " unreachable node(s)): " + path;
            return false;
        }

        id_ = std::move(id);
        nodes_ = std::move(nodes);
        walkways_ = std::move(walkways);
        crossings_ = std::move(crossings);
        routes_ = std::move(routes);
        entrances_ = std::move(entrances);
        return true;
    }
}
