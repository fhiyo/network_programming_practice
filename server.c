#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define BACKLOG 10

int cnt = 0;
pthread_mutex_t mutex;

void* client_handler(void* arg) {
  int client_sock_fd = *(int*)arg;
  free(arg);
  if (pthread_detach(pthread_self()) != 0) {
    perror("pthread_detach");
    exit(1);
  }
  pthread_mutex_lock(&mutex);
  int send_number = cnt++;
  pthread_mutex_unlock(&mutex);
  char buffer[BUFFER_SIZE];
  sprintf(buffer, "%d", send_number);
  size_t total_sent = 0;
  size_t to_send = strlen(buffer) + 1;
  while (total_sent < to_send) {
    ssize_t sent = send(client_sock_fd, buffer + total_sent, to_send - total_sent, 0);
    if (sent == -1) {
      perror("send");
      close(client_sock_fd); // TODO: closeのエラーハンドリングを行うか決めてやるなら実装する
      return NULL;
    }
    total_sent += sent;
  }
  close(client_sock_fd);
  return NULL;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./%s <port>\n", argv[0]);
    return 1;
  }
  int port;
  if ((port = atoi(argv[1])) == 0) {
    perror("failed to parse port string");
    return 1;
  }

  int server_fd;
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    return 1;
  }

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(server_fd, BACKLOG) == -1) {
    perror("listen");
    close(server_fd);
    return 1;
  }

  pthread_mutex_init(&mutex, NULL);

  int addr_len = sizeof(address);
  while (1) {
    int* new_sock_fd = malloc(sizeof(int));
    if (!new_sock_fd) {
      perror("malloc");
      continue;
    }
    if ((*new_sock_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addr_len)) == -1) {
      perror("accept");
      continue;
    }
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, client_handler, (void*)new_sock_fd) != 0) {
      perror("pthread_create");
      close(*new_sock_fd);
      free(new_sock_fd);
    }
  }

  pthread_mutex_destroy(&mutex);
  close(server_fd);

  return 0;
}
