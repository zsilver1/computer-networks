#include "graph.h"
#include <iostream>
#include <limits>
#include <set>
#include <string.h>

void Node::add_link(Node *other, std::string &cost) {
  int c = std::stoi(cost);
  this->outgoing.push_back(other);
  other->incoming.push_back(this);
  this->outgoing_distances.push_back(c);
}

std::ostream &operator<<(std::ostream &os, const Node &n) {
  os << "NODE: ID=" << n.id << ", " << n.incoming.size() << " incoming, "
     << n.outgoing.size() << " outgoing, " << n.type << ", " << n.ip_addr;
  return os;
}

void dijkstra(int start_index, std::vector<Node *> &graph) {
  int inf = std::numeric_limits<int>::infinity();
  std::set<Node *> set;
  for (Node *n : graph) {
    set.insert(n);
  }
  graph[start_index]->distance_to = 0;
  while (!set.empty()) {
    int min_dist = 10000000;
    int min_index;
    for (Node *n : set) {
      if (n->distance_to < min_dist) {
        min_dist = n->distance_to;
        min_index = n->id;
      }
    }
    set.erase(graph[min_index]);
    // std::cout << *graph[min_index] << "\n";
    for (int i = 0; i < graph[min_index]->outgoing.size(); i++) {
      int alt = min_dist + graph[min_index]->outgoing_distances[i];
      if (alt < graph[min_index]->outgoing[i]->distance_to) {
        graph[min_index]->outgoing[i]->distance_to = alt;
      }
    }
  }
}

std::string Node::get_closest_server(std::vector<Node *> &graph,
                                     char *source_ip, bool geo_based,
                                     std::vector<std::string> &ip_list) {
  int inf = std::numeric_limits<int>::infinity();
  if (geo_based) {
    for (int i = 0; i < graph.size(); i++) {
      if (strcmp(source_ip, graph[i]->ip_addr.c_str())) {
        dijkstra(i, graph);
        break;
      }
    }
    int min_dist = 10000000;
    Node *min_node;
    for (Node *n : graph) {
      if (n->type == SERVER && n->distance_to < min_dist) {
        min_dist = n->distance_to;
        min_node = n;
      }
    }
    std::cout << min_node->distance_to << "\n";
    return min_node->ip_addr;
  } else {
    // ROUND ROBIN
    std::string ip = ip_list.back();
    ip_list.pop_back();
    ip_list.insert(ip_list.begin(), ip);
    return ip;
  }
}