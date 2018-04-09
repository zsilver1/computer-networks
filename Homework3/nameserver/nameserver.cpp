#include "nameserver.h"
#include "DNSHeader.h"
#include "DNSQuestion.h"
#include "DNSRecord.h"
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void Nameserver::parse_rr_servers() {
  std::string line;
  while (std::getline(this->servers, line)) {
    this->server_ip_list.push_back(line);
  }
}

int parser_helper(std::string &line) {
  std::stringstream lineStream(line);
  std::string token;
  std::string ignored;
  lineStream >> ignored >> token;
  return std::stoi(token);
}

void Nameserver::parse_geo_servers() {
  int num_nodes;
  int num_links;
  std::string line;
  std::getline(this->servers, line);
  num_nodes = parser_helper(line);
  this->graph.reserve(num_nodes);

  std::string id;
  std::string type_str;
  std::string ip;
  for (int i = 0; i < num_nodes; i++) {
    std::getline(this->servers, line);
    std::stringstream stream(line);
    stream >> id >> type_str >> ip;
    Node *n = new Node(id, type_str, ip);
    this->graph.push_back(n);
  }
  std::getline(this->servers, line);
  num_links = parser_helper(line);
  std::string id1;
  std::string id2;
  int i1;
  int i2;
  std::string cost;
  for (int i = 0; i < num_links; i++) {
    std::getline(this->servers, line);
    std::stringstream stream(line);
    stream >> id1 >> id2 >> cost;
    i1 = std::stoi(id1);
    i2 = std::stoi(id2);
    this->graph[i1]->add_link(this->graph[i2], cost);
  }
}

void Nameserver::run() {
  if (this->geo_based)
    this->parse_geo_servers();
  else
    this->parse_rr_servers();

  /* Create a socket */
  int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0) {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    perror("socket");
    return;
  }

  /* Create sockaddr */
  struct sockaddr_in proxy_addr;
  proxy_addr.sin_family = AF_INET;
  proxy_addr.sin_addr.s_addr = INADDR_ANY;
  proxy_addr.sin_port = std::stoi(this->port);
  /* Binding */
  int ret =
      ::bind(sockfd, (const struct sockaddr *)&proxy_addr, sizeof(proxy_addr));
  if (ret < 0) {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    perror("bind");
    close(sockfd);
    return;
  }
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