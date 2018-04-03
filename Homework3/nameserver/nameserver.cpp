#include "nameserver.h"
#include "DNSHeader.h"
#include "DNSQuestion.h"
#include "DNSRecord.h"

void Nameserver::parse_rr_servers() {
  std::string line;
  while (std::getline(this->servers, line)) {
    this->server_ip_list.push_back(line);
    printf("%s\n", line.data());
  }
}

void Nameserver::run() {
  if (this->geo_based)
    this->parse_geo_servers();
  else
    this->parse_rr_servers();
}

int main(int argc, char **argv) {
  char *logfile = argv[1];
  char *port = argv[2];
  char geo = argv[3][0];
  char *servers = argv[4];
  bool geob;
  if (geo == '0')
    geob = false;
  else
    geob = true;
  Nameserver nameserver(port, geob, servers, logfile);
  nameserver.run();
}