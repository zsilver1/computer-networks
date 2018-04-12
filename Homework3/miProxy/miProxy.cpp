#include "miProxy.h"
#include "../nameserver/DNSHeader.h"
#include "../nameserver/DNSQuestion.h"
#include "../nameserver/DNSRecord.h"
#include <arpa/inet.h>

int main(int argc, char const *argv[]) {
  int one = 1;
  if (argc < 5) {
    perror("Usage: ./miProxy <log> <alpha> <listen-port> <dns-ip> <dns-port> "
           "[<www-ip>]");
    return -1;
  }

  /* Parse cmd arguments */
  char *log_path = (char *)argv[1];
  float alpha = atof(argv[2]);
  int listen_port = atoi(argv[3]);
  char *dns_ip = (char *)argv[4];
  char *dns_port = (char *)argv[5];
  bool need_dns = false;
  char *www_ip;
  if (argc == 7) {
    www_ip = (char *)argv[6];
  } else {
    www_ip = (char *)"video.cs.jhu.edu";
    need_dns = true;
  }

  /* Create a socket */
  int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
  if (sockfd < 0) {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    perror("socket");
    return -1;
  }

  /* Create sockaddr */
  struct sockaddr_in proxy_addr;
  proxy_addr.sin_family = AF_INET;
  proxy_addr.sin_addr.s_addr = INADDR_ANY;
  proxy_addr.sin_port = htons(listen_port);

  /* Binding */
  int ret =
      ::bind(sockfd, (const struct sockaddr *)&proxy_addr, sizeof(proxy_addr));
  if (ret < 0) {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    perror("bind");
    close(sockfd);
    return -1;
  }

  /* Listening */
  ret = listen(sockfd, BACKLOG);
  if (ret < 0) {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    perror("listen");
    close(sockfd);
    return -1;
  }

  // Set of file descriptors to listen to
  fd_set readSet;
  // Keep track of each file descriptor accepted
  vector<int> fds;
  // Data structure for storing bitrates
  vector<int> bitrate_vector;
  // Data structure for mapping with fd and throughput
  unordered_map<int, double> throughput_map;
  // ofstream to write log info into log_path file
  ofstream log_out(log_path, ofstream::out);

  while (true) {
    struct sockaddr_storage client_addr;
    socklen_t len = sizeof(client_addr);
    char ipstr[INET6_ADDRSTRLEN];
    // Set up the readSet
    FD_ZERO(&readSet);
    FD_SET(sockfd, &readSet);
    for (int i = 0; i < (int)fds.size(); ++i) {
      FD_SET(fds[i], &readSet);
    }

    int maxfd = 0;
    if (fds.size() > 0) {
      maxfd = *max_element(fds.begin(), fds.end());
    }
    maxfd = max(maxfd, sockfd);

    // maxfd + 1 is important
    int err = select(maxfd + 1, &readSet, NULL, NULL, NULL);
    assert(err != -1);

    if (FD_ISSET(sockfd, &readSet)) {
      int clientsd = accept(sockfd, (struct sockaddr *)&client_addr, &len);

      if (clientsd == -1) {
        printf("%s %d\n", __FUNCTION__, __LINE__);
        cout << "Error on accept" << endl;
      } else {
        // get ip address of client
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

        fds.push_back(clientsd);
        // printf("client: %d\n", clientsd);
      }
    }

    for (int i = 0; i < (int)fds.size(); ++i) {
      if (FD_ISSET(fds[i], &readSet)) {
        char buf;
        string request;
        bool closed = false;
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        int cur_fd = fds[i];

        /* Complete receiving the request from clients */
        while (1) {
          int bytesRecvd = recv(cur_fd, &buf, 1, 0);

          if (bytesRecvd < 0) {
            printf("%s %d\n", __FUNCTION__, __LINE__);
            cout << "Error recving bytes" << endl;
            cout << strerror(errno) << endl;
            exit(1);
          } else if (bytesRecvd == 0) // end of recv
          {
            printf("client: %d, fds i = %d\n", cur_fd, i);
            perror("Connection closed");
            if (fds.size() != 0)
              fds.erase(fds.begin() + i);
            if (throughput_map.find(cur_fd) != throughput_map.end())
              throughput_map.erase(cur_fd);
            closed = true;
            break;
          }

          request += buf;
          if (request.size() >= 4) {
            if (request.substr(request.size() - 4) == "\r\n\r\n")
              break;
          }
        }

        /* Client Keep alive */
        if (closed == false) {
          bool is_video_data = check_video_data(request);
          int cur_bitrate;
          string chunk_name;

          /* Client is requesting video data,
             modify the request for bitrate adaption */
          if (is_video_data) {
            chunk_name = get_chunkname(request);
            int seg_index = chunk_name.find("Seg");
            int bitrate = atoi(chunk_name.substr(0, seg_index).c_str());
            cur_bitrate = bitrate;

            if (throughput_map.find(cur_fd) != throughput_map.end()) {
              int chosen_bitrate;
              int last_bitrate = 1;
              if (bitrate_vector.size() != 0) {
                last_bitrate = bitrate_vector.front();
              }

              for (int b : bitrate_vector) {
                if (b <= (throughput_map[cur_fd] / 1.5))
                  chosen_bitrate = b;
                else {
                  chosen_bitrate = last_bitrate;
                  break;
                }
                last_bitrate = b;
              }
              cur_bitrate = chosen_bitrate;
              if (cur_bitrate != bitrate) {
                string new_name =
                    to_string(cur_bitrate) + chunk_name.substr(seg_index);
                int get_index = request.find("GET");
                int end_index = request.find(' ', get_index + 4);
                int vod_index = request.rfind("vod", end_index);
                request.replace(vod_index + 4, end_index - vod_index - 4,
                                new_name);
                chunk_name = new_name;
              }
            }
          }
          // CONNECT TO DNS
          const char *server_ip;
          if (need_dns) {
            server_ip = get_server_ip(1, ipstr, dns_ip, dns_port).data();
          } else {
            server_ip = www_ip;
          }

          /* Start send request to the server */
          /* Create a socket */
          struct addrinfo hints, *res;
          memset(&hints, 0, sizeof hints);
          hints.ai_family = AF_UNSPEC;
          hints.ai_socktype = SOCK_STREAM;
          printf("SERVER: %s\n", server_ip);
          getaddrinfo(server_ip, "80", &hints, &res);

          int server_sd =
              socket(res->ai_family, res->ai_socktype, res->ai_protocol);
          if (server_sd < 0) {
            printf("%s %d\n", __FUNCTION__, __LINE__);
            perror("socket");
            close(server_sd);
            return -1;
          }
          setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

          /* Connect */
          // struct hostent *host = gethostbyname(www_ip);

          int ret = connect(server_sd, res->ai_addr, res->ai_addrlen);
          freeaddrinfo(res);
          if (ret < 0) {
            printf("%s %d\n", __FUNCTION__, __LINE__);
            perror("connect");
            close(sockfd);
            close(server_sd);
            return -1;
          }

          /* Send */
          printf("%s\n", request.data());
          if (send(server_sd, request.c_str(), request.size(), 0) < 0) {
            printf("%s %d\n", __FUNCTION__, __LINE__);
            perror("send");
            close(sockfd);
            close(server_sd);
            return -1;
          }

          /* Receive from server */
          string response = recv_response(server_sd);
          string content_type = get_value(response, "Content-Type");
          int content_len = atoi(get_value(response, "Content-Length").c_str());
          for (int i = 0; i < content_len; i++) {
            if (recv(server_sd, &buf, 1, 0) < 0) {
              printf("%s %d\n", __FUNCTION__, __LINE__);
              perror("recv");
              close(sockfd);
              close(server_sd);
              return -1;
            }
            response += buf;
          }

          /* Getting video xml data and parse available bitrates */
          if (content_type.find("text/xml") != -1) {
            int index = 0;
            int cur = response.find("bitrate", index);
            while (cur != -1) {
              int fst_index = response.find('\"', cur);
              int snd_index = response.find('\"', fst_index);
              int br =
                  atoi(response.substr(fst_index + 1, snd_index - fst_index - 1)
                           .c_str());
              bool new_br = true;
              for (int b : bitrate_vector) {
                if (br == b) {
                  new_br = false;
                  break;
                }
                new_br = true;
              }

              if (new_br)
                bitrate_vector.push_back(br);
              sort(bitrate_vector.begin(), bitrate_vector.end());
              index = snd_index;
              cur = response.find("bitrate", index);
            }

            /* Request nolist.f4m*/
            string request_cp = request;
            request_cp.replace(request.find(".f4m"), 4, "_nolist.f4m");

            if (send(server_sd, request_cp.c_str(), request_cp.size(), 0) < 0) {
              printf("%s %d\n", __FUNCTION__, __LINE__);
              perror("send");
              close(sockfd);
              close(server_sd);
              return -1;
            }

            response = recv_response(server_sd);
            content_len = atoi(get_value(response, "Content-Length").c_str());
            for (int i = 0; i < content_len; i++) {
              if (recv(server_sd, &buf, 1, 0) < 0) {
                printf("%s %d\n", __FUNCTION__, __LINE__);
                perror("recv");
                close(sockfd);
                close(server_sd);
                return -1;
              }
              response += buf;
            }
          }
          /* Getting video chunk, calculate the throughput and log out info*/
          else if (content_type.find("video/f4f") != -1) {
            gettimeofday(&t2, NULL);
            double duration =
                t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) / 1000000.0;
            double t_new = (content_len / 1000) / duration * 8;
            double t_cur = 1;
            if (bitrate_vector.size() != 0) {
              if (throughput_map.find(cur_fd) == throughput_map.end())
                t_cur = bitrate_vector.front();
              else
                t_cur = throughput_map[cur_fd];
            }

            t_cur = alpha * t_new + (1 - alpha) * t_cur;
            throughput_map[cur_fd] = t_cur;

            log_out << duration << " " << t_new << " " << t_cur << " "
                    << cur_bitrate << " " << string(www_ip) << " " << chunk_name
                    << endl;
          }

          /* Send response to client */
          if (send(cur_fd, response.c_str(), response.size(), 0) < 0) {
            printf("%s %d\n", __FUNCTION__, __LINE__);
            perror("send");
            close(sockfd);
            close(server_sd);
            return -1;
          }
        }
      }
    }
  }
  close(sockfd);
  return 0;
}

string get_server_ip(int query_type, char *www_ip, char *dns_ip,
                     char *dns_port) {
  int one = 1;
  struct addrinfo hints;
  struct addrinfo *res, *p;
  int sock;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  getaddrinfo(dns_ip, dns_port, &hints, &res);
  // loop through all the results and make a socket
  for (p = res; p != NULL; p = p->ai_next) {
    if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("talker: socket");
      continue;
    }
    break;
  }
  if (p == NULL) {
    fprintf(stderr, "talker: failed to create socket\n");
  }

  // int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
  // bind(sock, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  DNSHeader qh;
  DNSQuestion question;
  DNSRecord qr;
  char buffer[10000];
  qh.ID = (ushort)getpid();
  qh.QR = false;
  qh.OPCODE = 0;
  qh.AA = false;
  qh.TC = false;
  qh.RD = false;
  qh.RA = false;
  qh.Z = 0;
  qh.RCODE = 0;
  qh.QDCOUNT = 1;
  qh.ANCOUNT = 0;
  qh.NSCOUNT = 0;
  qh.ARCOUNT = 0;

  char *dns_name;
  // convert_to_dns_name(dns_name, www_ip);
  dns_name = (char *)"5video2cs3jhu3edu";
  strcpy(question.QNAME, dns_name);
  question.QCLASS = 1;
  question.QTYPE = query_type;

  memcpy(buffer, (void *)&qh, sizeof(DNSHeader));
  memcpy(buffer + sizeof(DNSHeader), (void *)&question, sizeof(DNSQuestion));

  if (sendto(sock, buffer, sizeof(DNSHeader) + sizeof(DNSQuestion), 0,
             p->ai_addr, p->ai_addrlen) == -1) {
    perror("sendto failure");
  }

  memset(buffer, 0, 10000);
  recvfrom(sock, buffer, 10000, 0, p->ai_addr, &(p->ai_addrlen));
  DNSHeader dh;
  DNSRecord dr;
  memcpy(&dh, buffer, sizeof(DNSHeader));
  memcpy(&dr, buffer + sizeof(DNSHeader), sizeof(DNSRecord));
  return std::string(dr.RDATA);
}

/* Extract the video chunk name from request */
string get_chunkname(string request) {
  int get_index = request.find("GET");
  int end_index = request.find(' ', get_index + 4);
  int vod_index = request.rfind("vod", end_index);
  string fn = request.substr(vod_index + 4, end_index - vod_index - 4);
  // printf("%s\n", fn.c_str());
  return fn;
}

/* Check if the str is the video data format */
bool check_video_data(string str) {
  if (str.find("Seg") == -1)
    return false;
  if (str.find("Frag") == -1)
    return false;
  if (str.find("Frag") > str.find("Seg"))
    return true;
  return false;
}

/* Get the value from str based on the key */
string get_value(string str, string key) {
  int key_index = str.find(key);
  int space_index = str.find(' ', key_index);
  int end = str.find('\n', key_index);
  return str.substr(space_index + 1, end - space_index - 1);
}

/* Recerive the response from the server */
string recv_response(int server_sd) {
  string data = "";
  char buf;
  while (1) {
    int bytesRecvd = recv(server_sd, &buf, 1, 0);

    if (bytesRecvd < 0) {
      printf("%s %d\n", __FUNCTION__, __LINE__);
      perror("recv");
      exit(-1);
    }

    data += buf;
    if (data.size() >= 4) {
      if (data.substr(data.size() - 4) == "\r\n\r\n")
        break;
    }
  }
  return data;
}

// // mostly taken from reference version on github
// void convert_to_dns_name(char *dns, char *host) {
//   int n = 0, i;
//   strcat((char *)host, ".");

//   for (i = 0; i < strlen((char *)host); i++) {
//     if (host[i] == '.') {
//       *dns++ = i - n;
//       for (; n < i; n++) {
//         *dns++ = host[n];
//       }
//       n++;
//     }
//   }
//   *dns++ = '\0';
//   dns = "5video2cs3jhu3edu";
// }
