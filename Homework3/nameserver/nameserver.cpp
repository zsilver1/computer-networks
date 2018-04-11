#include "nameserver.h"
#include "DNSHeader.h"
#include "DNSQuestion.h"
#include "DNSRecord.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void construct_dns_response(DNSHeader *dh, DNSRecord *dr, const char *addr);

void Nameserver::log(char *client_ip, char *query_name,
                     std::string response_ip) {
  this->logfile << client_ip << " " << query_name << " " << response_ip << "\n";
  this->logfile.flush();
}

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

void *get_in_addr(struct sockaddr *sa) {
  return sa->sa_family == AF_INET
             ? (void *)&(((struct sockaddr_in *)sa)->sin_addr)
             : (void *)&(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void Nameserver::run() {
  if (this->geo_based)
    this->parse_geo_servers();
  else
    this->parse_rr_servers();

  struct addrinfo hints;
  struct addrinfo *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  getaddrinfo(NULL, this->port, &hints, &res);

  /* Create a socket */
  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  ;
  if (sockfd < 0) {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    perror("socket");
    return;
  }

  /* Binding */
  int ret = ::bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (ret < 0) {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    perror("bind");
    close(sockfd);
    return;
  }
  freeaddrinfo(res);

  char buffer[10000];
  struct sockaddr_storage sender;
  socklen_t sendsize = sizeof(sender);
  bzero(&sender, sizeof(sender));

  /* Run server until terminated */

  while (true) {
    int n = recvfrom(sockfd, buffer, 10000, 0, (struct sockaddr *)&sender,
                     &sendsize);
    if (n == -1) {
      perror("recvfrom");
    }
    struct sockaddr_in *sin = (struct sockaddr_in *)&sender;
    char client_addr_str[INET6_ADDRSTRLEN];
    inet_ntop(sender.ss_family, get_in_addr((struct sockaddr *)&sender),
              client_addr_str, sizeof client_addr_str);
    DNSHeader dh;
    DNSQuestion dq;
    memcpy(&dh, buffer, sizeof(DNSHeader));
    memcpy(&dq, buffer + sizeof(DNSHeader), sizeof(DNSQuestion));

    strncpy(client_addr_str, dq.QNAME, INET6_ADDRSTRLEN - 1);
    strcat(client_addr_str, "\0");
    printf("client_addr_str: %s\n", client_addr_str);

    std::string response_ip =
        Node::get_closest_server(this->graph, client_addr_str);
    this->log(client_addr_str, dq.QNAME, response_ip);

    // SEND DNS RESPONSE WITH SERVER IP ADDRESS
    DNSHeader answer_h;
    DNSRecord answer_rec;
    const char *addr = response_ip.data();
    construct_dns_response(&answer_h, &answer_rec, addr);
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

void construct_dns_response(DNSHeader *dh, DNSRecord *dr, const char *addr) {
  dh->ID = (ushort)getpid();
  dh->QR = true;
  dh->OPCODE = 2;
  dh->AA = false;
  dh->TC = false;
  dh->RD = false;
  dh->RA = false;
  dh->Z = 0;
  dh->RCODE = 0;
  dh->QDCOUNT = 0;
  dh->ANCOUNT = 1;
  dh->NSCOUNT = 0;
  dh->ARCOUNT = 0;

  strcpy(dr->NAME, "video.cs.jhu.edu");
  dr->TYPE = 1;
  dr->CLASS = 1;
  dr->TTL = 0;
  dr->RDLENGTH = INET6_ADDRSTRLEN;
  strcpy(dr->RDATA, addr);
}