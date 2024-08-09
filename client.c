#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <SERVER_NAME> <PORT>\n", argv[0]);
    return 1;
  }

  char* server_name = argv[1];
  char* port = argv[2]; // TODO: 不正な文字列が渡されたときのエラーハンドリング
  struct addrinfo hints;
  struct addrinfo* server_info;
  int rv;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if ((rv = getaddrinfo(server_name, port, &hints, &server_info)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  int server_sock_fd;
  struct addrinfo* p;
  for (p = server_info; p != NULL; p = p->ai_next) {
    if ((server_sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("socket");
      continue;
    }
    if (connect(server_sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
      perror("connect");
      continue;
    }
    break; // connect is succeeded
  }
  if (!p) {
    fprintf(stderr, "client: failed to socket or connect");
    return 1;
  }
  freeaddrinfo(server_info);

  char buffer[BUFFER_SIZE];
  size_t total_received = 0;
  while (total_received < BUFFER_SIZE) {
    ssize_t received = recv(server_sock_fd, buffer + total_received, BUFFER_SIZE - total_received, 0);
    if (received == -1) {
      perror("recv");
      close(server_sock_fd); // TODO: エラーハンドリングを行うか決める
      return 1;
    }
    if (received == 0) {
      break;
    }
    total_received += received;
  }
  printf("received from server: %s\n", buffer);
  close(server_sock_fd);

  return 0;
}
