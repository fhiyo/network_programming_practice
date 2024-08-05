#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define PORT "3080"
#define BACKLOG 10

void sigchld_handler(int _s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  // for ipv6
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void print_addrinfo(struct addrinfo* info) {
  printf("flags: %d, family: %d, socktype: %d, protocol: %d, addrlen: %u, canonname: %s, next: %p\n", info->ai_flags, info->ai_family, info->ai_socktype, info->ai_protocol, info->ai_addrlen, info->ai_canonname, info->ai_next);
}

int main(void) {
  struct addrinfo hints;
  struct addrinfo* servinfo;
  struct addrinfo* p;
  int get_addrinfo_result;
  int sock_fd;
  int new_process_fd;
  int yes = 1;
  struct sigaction sa;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((get_addrinfo_result = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(get_addrinfo_result));
    return 1;
  }
  for (p = servinfo; p != NULL; p = p->ai_next) {
    print_addrinfo(p);
    if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }
    if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sock_fd);
      perror("server: bind");
      continue;
    }
    break;
  }
  freeaddrinfo(servinfo);
  if (!p) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }
  if (listen(sock_fd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }
  printf("server: waiting for connections...\n");
  while(1) {
    sin_size = sizeof(their_addr);
    new_process_fd = accept(sock_fd, (struct sockaddr*)&their_addr, &sin_size);
    if (new_process_fd == -1) {
      perror("accept");
      continue;
    }
    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof(s));
    printf("server: got connection from %s\n", s);
    if (!fork()) {
      // child process
      close(sock_fd);
      char buffer[1024];
      sprintf(buffer, "hello!");
      // TODO: 指定した全データを送れなかった場合のハンドリングを行う
      if (send(new_process_fd, buffer, strlen(buffer), 0) == -1) {
        perror("send");
      }
      close(new_process_fd);
      exit(0);
    }
    close(new_process_fd);
  }

  printf("terminated!\n");
  return 0;
}
