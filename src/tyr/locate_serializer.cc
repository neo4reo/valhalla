#include <cstdint>
#include "baldr/json.h"
#include "tyr/serializers.h"

using namespace valhalla;
using namespace valhalla::baldr;

namespace {
  json::ArrayPtr serialize_edges(const PathLocation& location, GraphReader& reader, bool verbose) {
    auto array = json::array({});
    for(const auto& edge : location.edges) {
      try {
        //get the osm way id
        auto tile = reader.GetGraphTile(edge.id);
        auto* directed_edge = tile->directededge(edge.id);
        auto edge_info = tile->edgeinfo(directed_edge->edgeinfo_offset());
        //they want MOAR!
        if(verbose) {
          auto segments = tile->GetTrafficSegments(edge.id);
          auto segments_array = json::array({});
          for(const auto& segment : segments)
            segments_array->emplace_back(segment.json());
          array->emplace_back(json::map({
            {"correlated_lat", json::fp_t{edge.projected.lat(), 6}},
            {"correlated_lon", json::fp_t{edge.projected.lng(), 6}},
            {"side_of_street",
              edge.sos == PathLocation::LEFT ? std::string("left") :
                (edge.sos == PathLocation::RIGHT ? std::string("right") : std::string("neither"))
            },
            {"percent_along", json::fp_t{edge.percent_along, 5} },
            {"distance", json::fp_t{edge.distance, 1}},
            {"minimum_reachability", static_cast<int64_t>(edge.minimum_reachability)},
            {"edge_id", edge.id.json()},
            {"edge", directed_edge->json()},
            {"edge_info", edge_info.json()},
            {"traffic_segments", segments_array},
          }));
        }//they want it lean and mean
        else {
          array->emplace_back(
            json::map({
              {"way_id", edge_info.wayid()},
              {"correlated_lat", json::fp_t{edge.projected.lat(), 6}},
              {"correlated_lon", json::fp_t{edge.projected.lng(), 6}},
              {"side_of_street",
                edge.sos == PathLocation::LEFT ? std::string("left") :
                  (edge.sos == PathLocation::RIGHT ? std::string("right") : std::string("neither"))
              },
              {"percent_along", json::fp_t{edge.percent_along, 5} },
            })
          );
        }
      }
      catch(...) {
        //this really shouldnt ever get hit
        LOG_WARN("Expected edge not found in graph but found by loki::search!");
      }
    }
    return array;
  }

  json::ArrayPtr serialize_nodes(const PathLocation& location, GraphReader& reader, bool verbose) {
    //get the nodes we need
    std::unordered_set<uint64_t> nodes;
    for(const auto& e : location.edges)
      if(e.end_node())
        nodes.emplace(reader.GetGraphTile(e.id)->directededge(e.id)->endnode());
    //ad them into an array of json
    auto array = json::array({});
    for(auto node_id : nodes) {
      GraphId n(node_id);
      const GraphTile* tile = reader.GetGraphTile(n);
      auto* node_info = tile->node(n);
      json::MapPtr node;
      if(verbose) {
        node = node_info->json(tile);
        node->emplace("node_id", n.json());
      }
      else {
        node = json::map({
          {"lon", json::fp_t{node_info->latlng().first, 6}},
          {"lat", json::fp_t{node_info->latlng().second, 6}},
          //TODO: osm_id
        });
      }
      array->emplace_back(node);
    }
    //give them back
    return array;
  }

  json::MapPtr serialize(const PathLocation& location, GraphReader& reader, bool verbose) {
    //serialze all the edges
    auto m = json::map
    ({
      {"edges", serialize_edges(location, reader, verbose)},
      {"nodes", serialize_nodes(location, reader, verbose)},
      {"input_lat", json::fp_t{location.latlng_.lat(), 6}},
      {"input_lon", json::fp_t{location.latlng_.lng(), 6}},
    });
    return m;
  }

  json::MapPtr serialize(const PointLL& ll, const std::string& reason, bool verbose) {
    auto m = json::map({
      {"edges", static_cast<std::nullptr_t>(nullptr)},
      {"nodes", static_cast<std::nullptr_t>(nullptr)},
      {"input_lat", json::fp_t{ll.lat(), 6}},
      {"input_lon", json::fp_t{ll.lng(), 6}},
    });
    if(verbose)
      m->emplace("reason", reason);

    return m;
  }
}

namespace valhalla {
  namespace tyr {

    std::string serializeLocate(const valhalla_request_t& request, const std::vector<Location>& locations,
        const std::unordered_map<Location, PathLocation>& projections, GraphReader& reader) {
      auto json = json::array({});
      for(const auto& location : locations) {
        try {
          json->emplace_back(serialize(projections.at(location), reader, request.options.verbose()));
        }
        catch(const std::exception& e) {
          json->emplace_back(serialize(location.latlng_, "No data found for location", request.options.verbose()));
        }
      }

      std::stringstream ss;
      ss << *json;
      return ss.str();
    }

  }
}
