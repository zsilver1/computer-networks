#ifndef NAMESERVER_H
#define NAMESERVER_H
#include "graph.h"
#include <fstream>
#include <string>
#include <vector>

class Nameserver {
private:
  bool geo_based;
  char *port;
  std::ofstream logfile;
  std::ifstream servers;
  std::vector<std::string> server_ip_list;
  std::vector<Node *> graph;
  void parse_rr_servers();
  void parse_geo_servers();
  void log(char *client_ip, char *query_name, std::string response_ip);

public:
  Nameserver(char *port, bool geo_based, char *servers, char *logfile)
      : port(port), geo_based(geo_based) {
    this->logfile.open(logfile);
    this->servers.open(servers);
  }
  ~Nameserver() {
    this->logfile.close();
    this->servers.close();
    for (Node *n : graph) {
      delete n;
    }
  }
  void run();
};
#endif