#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#define PACKET_LEN 1000

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
  } else if (argc == 8) {
    handle_client_args(argv, &args);
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

  if ((sock =
           socket(res->ai_family, res->ai_socktype, res->ai_protocol) == -1)) {
    printf("Socket Error\n");
    exit(1);
  }

  start_time = time(NULL);
  cur_time = time(NULL);
  while (cur_time - start_time < duration) {
    // TODO
    kb_sent++;
    cur_time = time(NULL);
  }
  // SEND FIN
  // GET ACK
  measured_time = time(NULL) - start_time;
  freeaddrinfo(res);
}

void run_server(char *port) {}

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
  args->listen_port = listen_port;
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