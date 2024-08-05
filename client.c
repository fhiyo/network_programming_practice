#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT "3080"
#define MAXDATASIZE 100

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./%s <server_name>\n", argv[0]);
    return 1;
  }

  int sock_fd;
  int rv;
  struct addrinfo hints;
  struct addrinfo *server_info;
  struct addrinfo *p;
  char* server_name;
  char s[INET6_ADDRSTRLEN];
  int num_bytes;
  char buffer[MAXDATASIZE];

  server_name = argv[1];
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if ((rv = getaddrinfo(server_name, PORT, &hints, &server_info)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  for (p = server_info; p != NULL; p = p->ai_next) {
    if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }
    if (connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sock_fd);
      perror("client: connect");
      continue;
    }
    break;
  }
  if (!p) {
    fprintf(stderr, "client: failed to connect\n");
    return 1;
  }
  inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s, sizeof(s));
  printf("client: connecting to %s\n", s);
  freeaddrinfo(server_info);
  if ((num_bytes = recv(sock_fd, buffer, MAXDATASIZE - 1, 0)) == -1) {
    perror("recv");
    return 1;
  }
  printf("length of received bytes: %d\n", num_bytes);
  buffer[num_bytes] = '\0';
  printf("client: received '%s'\n", buffer);
  close(sock_fd);

  printf("terminated\n");
  return 0;
}
