#include "graph.h"

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