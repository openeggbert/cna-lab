// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/MapPayloadCodec.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace MeshWorld::Map {
namespace {

using json = nlohmann::json;

json field_to_json(const FieldGrid& g) {
    return json{{"w", g.w}, {"h", g.h}, {"data", g.data}};
}

FieldGrid field_from_json(const json& j) {
    FieldGrid g;
    g.w    = j.at("w").get<int>();
    g.h    = j.at("h").get<int>();
    g.data = j.at("data").get<std::vector<float>>();
    return g;
}

json biome_to_json(const BiomeGrid& g) {
    return json{{"w", g.w}, {"h", g.h}, {"data", g.data}};
}

BiomeGrid biome_from_json(const json& j) {
    BiomeGrid g;
    g.w    = j.at("w").get<int>();
    g.h    = j.at("h").get<int>();
    g.data = j.at("data").get<std::vector<std::uint8_t>>();
    return g;
}

json feature_to_json(const MapFeature& f) {
    json pts = json::array();
    for (const auto& pt : f.points) pts.push_back(json::array({pt[0], pt[1]}));
    return json{
        {"type",       static_cast<int>(f.type)},
        {"name",       f.name},
        {"points",     pts},
        {"attributes", f.attributes},
    };
}

MapFeature feature_from_json(const json& j) {
    MapFeature f;
    f.type = static_cast<FeatureType>(j.at("type").get<int>());
    f.name = j.at("name").get<std::string>();
    for (const auto& pt : j.at("points"))
        f.points.push_back({pt.at(0).get<double>(), pt.at(1).get<double>()});
    f.attributes = j.at("attributes").get<std::map<std::string, double>>();
    return f;
}

json label_to_json(const PlaceLabel& l) {
    return json{
        {"name", l.name},
        {"pos",  json::array({l.pos[0], l.pos[1]})},
        {"kind", l.kind},
    };
}

PlaceLabel label_from_json(const json& j) {
    PlaceLabel l;
    l.name = j.at("name").get<std::string>();
    l.pos  = {j.at("pos").at(0).get<double>(), j.at("pos").at(1).get<double>()};
    l.kind = j.at("kind").get<std::string>();
    return l;
}

json edge_to_json(const TileEdge& e) {
    json crossings = json::array();
    for (const auto& c : e.crossings)
        crossings.push_back(json{{"type", static_cast<int>(c.type)}, {"position", c.position}});
    return json{{"elevation", e.elevation}, {"biome", e.biome}, {"crossings", crossings}};
}

TileEdge edge_from_json(const json& j) {
    TileEdge e;
    e.elevation = j.at("elevation").get<std::vector<float>>();
    // M107 additions: unlike the rest of this codec, missing keys default to
    // empty rather than throwing, so tiles persisted before M107 still decode.
    e.biome = j.value("biome", std::vector<std::uint8_t>{});
    for (const auto& c : j.value("crossings", json::array()))
        e.crossings.push_back(EdgeCrossing{static_cast<EdgeCrossingType>(c.at("type").get<int>()),
                                            c.at("position").get<float>()});
    return e;
}

} // namespace

std::string MapPayloadCodec::encode(const MapTilePayload& p) {
    json j;
    j["tile"]        = json{{"level", p.tile.level}, {"x", p.tile.x}, {"y", p.tile.y}};
    j["entropy"]     = p.entropy;
    j["culture"]     = p.culture;
    j["generator"]   = p.generator;
    j["elevation"]   = field_to_json(p.elevation);
    j["temperature"] = field_to_json(p.temperature);
    j["moisture"]    = field_to_json(p.moisture);
    j["biome"]       = biome_to_json(p.biome);
    j["zone_candidates"] = biome_to_json(p.zone_candidates);

    json feats = json::array();
    for (const auto& f : p.features) feats.push_back(feature_to_json(f));
    j["features"] = feats;

    json labels = json::array();
    for (const auto& l : p.labels) labels.push_back(label_to_json(l));
    j["labels"] = labels;

    json edges = json::array();
    for (const auto& e : p.edges) edges.push_back(edge_to_json(e));
    j["edges"] = edges;

    return j.dump();
}

MapTilePayload MapPayloadCodec::decode(const std::string& text) {
    const json j = json::parse(text);

    MapTilePayload p;
    const json& t = j.at("tile");
    p.tile = TileCoord{t.at("level").get<int>(),
                       t.at("x").get<std::int64_t>(),
                       t.at("y").get<std::int64_t>()};
    p.entropy     = j.at("entropy").get<std::uint64_t>();
    p.culture     = j.at("culture").get<std::string>();
    p.generator   = j.at("generator").get<std::string>();
    p.elevation   = field_from_json(j.at("elevation"));
    p.temperature = field_from_json(j.at("temperature"));
    p.moisture    = field_from_json(j.at("moisture"));
    p.biome       = biome_from_json(j.at("biome"));
    // M156 addition: unlike the rest of this codec, a missing key defaults
    // to an empty grid rather than throwing, so tiles persisted before
    // M156 still decode (same backward-compat rule M107's edges.biome/
    // crossings additions used).
    if (j.contains("zone_candidates")) p.zone_candidates = biome_from_json(j.at("zone_candidates"));

    for (const auto& f : j.at("features")) p.features.push_back(feature_from_json(f));
    for (const auto& l : j.at("labels"))   p.labels.push_back(label_from_json(l));

    const json& edges = j.at("edges");
    for (std::size_t i = 0; i < p.edges.size() && i < edges.size(); ++i)
        p.edges[i] = edge_from_json(edges[i]);

    return p;
}

} // namespace MeshWorld::Map
