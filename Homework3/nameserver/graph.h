#ifndef GRAPH_H
#define GRAPH_H

#include <iostream>
#include <string>
#include <vector>

enum NodeType { CLIENT, SERVER, SWITCH };

class Node {
public:
  Node(std::string id, std::string type_str, std::string ip)
      : ip_addr(ip), id(std::stoi(id)) {
    if (type_str == "CLIENT") {
      type = CLIENT;
    } else if (type_str == "SERVER") {
      type = SERVER;
    } else {
      type = SWITCH;
    }
  }
  std::vector<Node *> incoming;
  std::vector<Node *> outgoing;
  std::vector<int> outgoing_distances;
  std::string ip_addr;
  NodeType type;
  int id;
  int distance_to = -1;

  void add_link(Node *other, std::string &cost);
  friend std::ostream &operator<<(std::ostream &os, const Node &n);
  static std::string get_closest_server(std::vector<Node *> &graph,
                                        char *source_ip);
};

#endif