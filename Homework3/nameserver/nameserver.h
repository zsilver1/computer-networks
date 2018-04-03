#ifndef NAMESERVER_H
#define NAMESERVER_H
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
  void parse_rr_servers();
  void parse_geo_servers();

public:
  Nameserver(char *port, bool geo_based, char *servers, char *logfile)
      : port(port), geo_based(geo_based) {
    this->logfile.open(logfile);
    this->servers.open(servers);
  }
  ~Nameserver() {
    this->logfile.close();
    this->servers.close();
  }
  void run();
};
#endif