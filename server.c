#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define BACKLOG 10

pthread_mutex_t mutex;
int count = 0;

void* client_handler(void* arg) {
  if (pthread_detach(pthread_self()) != 0) {
    perror("pthread_detach");
    exit(1);
  }

  int client_sock_fd = *(int*)arg;
  free(arg);
  pthread_mutex_lock(&mutex);
  int send_number = ++count;
  pthread_mutex_unlock(&mutex);

  char buffer[BUFFER_SIZE];
  snprintf(buffer, BUFFER_SIZE, "%d", send_number);
  size_t length = strlen(buffer) + 1;
  size_t total_sent = 0;
  while (total_sent < length) {
    ssize_t sent = send(client_sock_fd, buffer + total_sent, length - total_sent, 0);
    if (sent == -1) {
      perror("send");
      exit(1);
    }
    total_sent += sent;
  }
  printf("sending data: %s\n", buffer);

  close(client_sock_fd);
  return NULL;
}

int parse_port(char* port_str) {
  int port = atoi(port_str);
  char s[6]; /* maximum port number is 65535, so number of digit is less than or equal to 5 (+ null character) */
  int n = snprintf(s, strlen(port_str) + 1, "%d", port);
  if (n < 0) {
    perror("snprintf");
    exit(1);
  }
  if ((strcmp(s, port_str) != 0) || port < 0 || port > 65535) {
    fprintf(stderr, "Invalid argument. port: %s\n", port_str);
    return -1;
  }
  return port;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
    return 1;
  }
  int port = parse_port(argv[1]);
  if (port < 0) {
    fprintf(stderr, "Invalid port. port: %s\n", argv[1]);
    return 1;
  }

  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1) {
    perror("socket");
    return 1;
  }
  /*
    avoid "Address already in use" when the socket's state is TIME_WAIT
    ref: https://qiita.com/bamchoh/items/1dd44ba1fbef43b5284b
   */
  int yes = 1;
  int sockopt_ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
  if (sockopt_ret) {
    perror("setsockopt");
    return 1;
  }

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(sock_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
    perror("bind");
    return 1;
  }
  if (listen(sock_fd, BACKLOG) == -1) {
    perror("listen");
    return 1;
  }
  printf("listening...\n");

  pthread_mutex_init(&mutex, NULL);
  int address_length = sizeof(address);
  while (1) {
    int* client_sock_fd = malloc(sizeof(int));
    *client_sock_fd = accept(sock_fd, (struct sockaddr*)&address, (socklen_t*)&address_length);
    pthread_t thread_id;
    int rv = pthread_create(&thread_id, NULL, client_handler, (void*)client_sock_fd);
    if (rv != 0) {
      errno = rv;
      perror("pthread_create");
      close(*client_sock_fd);
      free(client_sock_fd);
    }
  }

  close(sock_fd);
  pthread_mutex_destroy(&mutex);

  return 0;
}
