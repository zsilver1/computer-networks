#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PACKET_LEN 1000
#define BACKLOG 15

char data[PACKET_LEN];
char fin[PACKET_LEN];
char ack[PACKET_LEN];

struct arguments {
  char *server_hostname;
  char *server_port;
  int duration;
  char *listen_port;
};

void handle_server_args(char *argv[], struct arguments *args);
void handle_client_args(char *argv[], struct arguments *args);
void run_client(char *host, char *port, int duration);
void run_server(char *port);

int main(int argc, char *argv[]) {
  struct arguments args;
  memset(&data, 0, PACKET_LEN);
  memset(&fin, 1, PACKET_LEN);
  memset(&ack, 2, PACKET_LEN);

  if (argc == 4) {
    handle_server_args(argv, &args);
    run_server(args.listen_port);
  } else if (argc == 8) {
    handle_client_args(argv, &args);
    run_client(args.server_hostname, args.server_port, args.duration);
  } else {
    printf("Error: missing or additional arguments\n");
    exit(1);
  }
  return 0;
}

void run_client(char *host, char *port, int duration) {
  int status;
  int sock;
  struct addrinfo hints;
  struct addrinfo *res;
  unsigned int kb_sent = 0;
  time_t measured_time;
  time_t cur_time;
  time_t start_time;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
    printf("getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    perror("Socket:");
    exit(1);
  }

  // connect to server
  if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
    perror("Connection");
    exit(1);
  }

  start_time = time(NULL);
  cur_time = time(NULL);
  while (cur_time - start_time < duration) {
    send(sock, data, PACKET_LEN, 0);
    kb_sent++;
    cur_time = time(NULL);
  }
  send(sock, fin, PACKET_LEN, 0);
  recv(sock, data, PACKET_LEN, 0);
  measured_time = time(NULL) - start_time;
  float mbps = (kb_sent / measured_time) / 1000.0 * 8;
  printf("sent=%d KB rate=%.3f Mbps\n", kb_sent, mbps);
  freeaddrinfo(res);
  close(sock);
}

void run_server(char *port) {
  int status;
  int sock;
  struct addrinfo hints;
  struct addrinfo *res;
  struct sockaddr_storage their_addr;
  unsigned int kb_rec = 0;
  time_t end_time;
  time_t start_time;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    printf("getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    perror("Socket");
    exit(1);
  }

  if (bind(sock, res->ai_addr, res->ai_addrlen) == -1) {
    perror("Bind");
    exit(1);
  }

  if (listen(sock, BACKLOG) == -1) {
    perror("Backlog");
    exit(1);
  }

  socklen_t addr_size = sizeof their_addr;
  int new_fd = accept(sock, (struct sockaddr *)&their_addr, &addr_size);

  start_time = time(NULL);
  do {
    recv(new_fd, (void *)data, PACKET_LEN, 0);
    kb_rec++;
  } while (data[0] != 1);

  send(new_fd, ack, PACKET_LEN, 0);
  end_time = time(NULL);
  // remove last kb because it was fin packet
  kb_rec--;

  float mbps = (kb_rec / (end_time - start_time)) / 1000.0 * 8;
  printf("recieved=%d KB rate=%.3f Mbps\n", kb_rec, mbps);
  freeaddrinfo(res);
  close(sock);
}

void handle_server_args(char *argv[], struct arguments *args) {
  if (strcmp(argv[1], "-s") != 0) {
    printf("Error: missing or additional arguments\n");
    exit(1);
  }
  if (strcmp(argv[2], "-p") != 0) {
    printf("Error: missing or additional arguments\n");
    exit(1);
  }
  int listen_port = atoi(argv[3]);
  if (listen_port < 1024 || listen_port > 65535) {
    printf("Error: port number must be in the range 1024 to 65535\n");
    exit(1);
  }
  args->listen_port = argv[3];
}

void handle_client_args(char *argv[], struct arguments *args) {
  if (strcmp(argv[1], "-c") != 0) {
    printf("Error: missing or additional arguments\n");
    exit(1);
  }
  if (strcmp(argv[2], "-h") != 0) {
    printf("Error: missing or additional arguments\n");
    exit(1);
  }
  if (strcmp(argv[4], "-p") != 0) {
    printf("Error: missing or additional arguments\n");
    exit(1);
  }
  if (strcmp(argv[6], "-t") != 0) {
    printf("Error: missing or additional arguments\n");
    exit(1);
  }
  args->server_hostname = argv[3];
  int server_port = atoi(argv[5]);
  if (server_port < 1024 || server_port > 65535) {
    printf("Error: port number must be in the range 1024 to 65535\n");
    exit(1);
  }
  args->server_port = argv[5];
  args->duration = atoi(argv[7]);
}