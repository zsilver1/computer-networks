#include <arpa/inet.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#define BACKLOG 15
#define CHUNK_SIZE 3000

class Arguments {
public:
  char *logstr;
  float alpha;
  char *listen_port;
  char *www_ip;

  void handle_args(int argc, char **argv) {
    if (argc != 5) {
      std::cerr << "Error: invalid arguments\n";
      exit(1);
    }
    this->logstr = argv[1];
    this->alpha = atof(argv[2]);
    this->listen_port = argv[3];
    this->www_ip = argv[4];
  }
};

class Proxy {
public:
  Arguments *args;
  std::ofstream logfile;
  clock_t start;
  clock_t stop;
  double total_time;
  // all in Kbps
  double cur_throughput = 0;
  double avg_throughput = 0;
  std::string cur_filename;

  std::vector<int> bitrates;
  int cur_bitrate = 0;

  Proxy(Arguments *a) : args(a) { this->logfile.open(this->args->logstr); }
  ~Proxy() { this->logfile.close(); }

  void run() {
    int status;
    int browser_sock;
    int server_sock;
    int one = 1;
    struct addrinfo hints;
    struct addrinfo *browser_res;
    struct addrinfo *server_res;
    struct sockaddr_storage browser_addr;
    std::string cur_filename;
    bool is_xml = false;
    char data[CHUNK_SIZE];
    char data_copy[CHUNK_SIZE];
    std::string buffer;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((status = getaddrinfo(NULL, this->args->listen_port, &hints,
                              &browser_res)) != 0) {
      printf("getaddrinfo error: %s\n", gai_strerror(status));
      exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((status = getaddrinfo(this->args->www_ip, "80", &hints, &server_res)) !=
        0) {
      printf("getaddrinfo error: %s\n", gai_strerror(status));
      exit(1);
    }

    if ((browser_sock = socket(browser_res->ai_family, browser_res->ai_socktype,
                               browser_res->ai_protocol)) == -1) {
      perror("Socket");
      exit(1);
    }

    if ((server_sock = socket(server_res->ai_family, server_res->ai_socktype,
                              server_res->ai_protocol)) == -1) {
      perror("Socket");
      exit(1);
    }
    setsockopt(browser_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

    if (bind(browser_sock, browser_res->ai_addr, browser_res->ai_addrlen) ==
        -1) {
      perror("Bind browser");
      exit(1);
    }
    // Connect to server
    if (connect(server_sock, server_res->ai_addr, server_res->ai_addrlen) ==
        -1) {
      perror("Connect");
      exit(1);
    }

    if (listen(browser_sock, BACKLOG) == -1) {
      perror("Backlog");
      exit(1);
    }
    socklen_t addr_size = sizeof browser_addr;

    int count;
    int new_browser_sock =
        accept(browser_sock, (struct sockaddr *)&browser_addr, &addr_size);
    setsockopt(new_browser_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

    // read from the browser
    while (1) {
      memset(data, 0, CHUNK_SIZE);
      buffer.clear();
      count = read(new_browser_sock, data, CHUNK_SIZE);
      this->start = clock();
      if ((is_xml = check_request_for_xml(data))) {
        strncpy(data_copy, data, CHUNK_SIZE);
      }
      bool is_video = check_request_for_video(data);
      if (is_video) {
        count = modify_request_with_bitrate(data);
      }
      this->cur_filename = get_name_of_file(data);

      // write to server
      write(server_sock, data, count);
      memset(data, 0, CHUNK_SIZE);

      // read response from server
      count = read(server_sock, data, CHUNK_SIZE);
      copy_data_to_vec(data, buffer, count);
      int remaining_len =
          find_content_len(data) + find_header_len(data) - count;

      memset(data, 0, CHUNK_SIZE);
      while (remaining_len > 0) {
        count = read(server_sock, data,
                     (remaining_len < CHUNK_SIZE) ? remaining_len : CHUNK_SIZE);
        copy_data_to_vec(data, buffer, count);
        memset(data, 0, CHUNK_SIZE);
        remaining_len -= count;
      }
      this->stop = clock();
      this->total_time = (double)(this->stop - this->start) / CLOCKS_PER_SEC;

      // calculate throughput and bitrate
      calculate_throughput(8 * ((double)buffer.size() / 1000000));
      set_bitrate();
      log();

      // if the file is xml, we re-query the server for the nolist file
      if (is_xml) {
        this->bitrates = getBitrates(buffer);
        this->cur_bitrate = this->bitrates[0];

        buffer.clear();
        // this function modifies the request to add _nolist to end of URL
        // replace_with_nolist(data_copy);

        write(server_sock, data_copy, sizeof(data_copy));
        memset(data, 0, CHUNK_SIZE);

        // read response from server
        count = read(server_sock, data, CHUNK_SIZE);
        copy_data_to_vec(data, buffer, count);
        int remaining_len =
            find_content_len(data) + find_header_len(data) - count;

        memset(data, 0, CHUNK_SIZE);
        while (remaining_len > 0) {
          count =
              read(server_sock, data,
                   (remaining_len < CHUNK_SIZE) ? remaining_len : CHUNK_SIZE);
          copy_data_to_vec(data, buffer, count);
          memset(data, 0, CHUNK_SIZE);
          remaining_len -= count;
        }
        is_xml = false;
      }
      // send response to browser
      write(new_browser_sock, buffer.data(), buffer.size());
    }

    close(server_sock);
    close(browser_sock);
  }

  // takes size in Mb
  void calculate_throughput(double size) {
    if (this->cur_throughput == 0) {
      this->cur_throughput = (size / (this->total_time));
      this->avg_throughput = this->cur_throughput;
    } else {
      this->cur_throughput = (size / (this->total_time));
      this->avg_throughput = this->args->alpha * this->cur_throughput +
                             (1 - this->args->alpha) * (this->avg_throughput);
    }
  }

  std::string get_name_of_file(char *data) {
    std::string s(data);
    std::string result;
    size_t loc = s.find("vod/") + 4;
    if (loc == std::string::npos) {
      return "";
    }
    while (!isspace(s[loc])) {
      result += s[loc];
      loc++;
    }
    return result;
  }

  void set_bitrate() {
    double t = this->cur_throughput;
    for (int i : this->bitrates) {
      if (1000 * t >= 1.5 * i) {
        this->cur_bitrate = i;
      } else {
        break;
      }
    }
  }

  // takes a request and adds "_nolist" to the end of the url
  void replace_with_nolist(char *data) {
    std::string s(data);
    size_t loc = s.find(".f4m");
    s.insert(loc, "_nolist");
    strncpy(data, s.data(), s.size());
  }

  void copy_data_to_vec(char *data, std::string &vec, int len) {
    vec.append(data, len);
  }

  int find_content_len(char *data) {
    std::string s(data);
    int len = 16;
    size_t loc = s.find("Content-Length: ");
    if (loc == std::string::npos) {
      return 0;
    }
    loc += len;
    std::string num = "";
    while (isdigit(s[loc])) {
      num += s[loc];
      loc++;
    }
    return std::stoi(num);
  }

  bool check_request_for_xml(char *data) {
    std::string s(data);
    if (s.find(".f4m") != std::string::npos) {
      return true;
    }
    return false;
  }

  bool check_request_for_video(char *data) {
    std::string s(data);
    if (s.find("Seg") != std::string::npos) {
      return true;
    }
    return false;
  }

  size_t find_header_len(char *data) {
    std::string end_header = "\r\n\r\n";
    std::string s(data);
    return s.find(end_header) + 4;
  }

  int modify_request_with_bitrate(char *data) {
    std::string s(data);
    std::string bit = std::to_string(this->cur_bitrate);
    size_t loc = s.find("/vod/") + 5;
    int i = loc;
    while (isdigit(s[i])) {
      i++;
    }
    s.replace(loc, i - loc, bit);
    strncpy(data, s.data(), s.size());
    return s.size();
  }

  std::vector<int> getBitrates(const std::string &xml) {
    using std::string;
    string keyword = "bitrate=\"";
    std::vector<int> lst;
    for (size_t tagPos = xml.find("<media"); tagPos != string::npos;
         tagPos = xml.find("<media", tagPos + 1)) {
      size_t keyPos = xml.find(keyword, tagPos);
      if (keyPos == string::npos)
        continue;
      int bitrateLoc = keyPos + keyword.size();
      int len = xml.find('"', bitrateLoc) - bitrateLoc;
      int bitrate = std::stoi(xml.substr(bitrateLoc, len));
      lst.push_back(bitrate);
    }
    return lst;
  }

  void log() {
    this->logfile << std::fixed << std::setprecision(6) << this->total_time
                  << " " << std::setprecision(2) << this->cur_throughput << " "
                  << std::setprecision(2) << this->avg_throughput << " "
                  << this->cur_bitrate << " " << this->args->www_ip << " "
                  << this->cur_filename << "\n"
                  << std::flush;
  }

  void write_to_file(char *text) { this->logfile << text << std::flush; }
};

int main(int argc, char **argv) {
  Arguments args;
  args.handle_args(argc, argv);
  Proxy proxy(&args);
  proxy.run();
  return 0;
}