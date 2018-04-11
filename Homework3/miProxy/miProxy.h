#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#define BACKLOG 10

using namespace std;

bool check_video_data(string str);
string get_server_ip(int query_type, char *www_ip, char *dns_ip,
                     char *dns_port);
string get_value(string str, string key);
string recv_response(int server_sd);
string get_chunkname(string request);
void convert_to_dns_name(char *dns, char *host);
