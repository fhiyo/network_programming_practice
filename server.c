#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "utils.h"

#define BUFFER_SIZE 1024
#define BACKLOG 10

pthread_mutex_t send_number_counter_mutex;
pthread_mutex_t output_mutex;
pthread_mutex_t client_counter_mutex;
int count = 0;
int sock_fd;
volatile sig_atomic_t working = 1;
int active_clients = 0;

void sigaction_handler(int signum) {
  working = 0;
}

void* client_handler(void* arg) {
  if (pthread_detach(pthread_self()) != 0) {
    perror("pthread_detach");
    exit(1);
  }

  int client_sock_fd = *(int*)arg;
  free(arg);
  pthread_mutex_lock(&send_number_counter_mutex);
  int send_number = ++count;
  pthread_mutex_unlock(&send_number_counter_mutex);
  pthread_mutex_lock(&client_counter_mutex);
  active_clients++;
  pthread_mutex_unlock(&client_counter_mutex);

  if (send_all(client_sock_fd, (char*)&send_number, sizeof(int)) == -1) {
    fprintf(stderr, "send_all failed\n");
    goto cleanup;
  }
  size_t numbers_array_size = sizeof(int) * send_number;
  int* received_numbers = malloc(numbers_array_size);
  if (!received_numbers) {
    perror("malloc");
    goto cleanup;
  }
  if (recv_all(client_sock_fd, (char*)received_numbers, numbers_array_size) == -1) {
    fprintf(stderr, "recv_all failed\n");
    goto cleanup_received_numbers;
  }
  int* sorted_numbers = malloc(numbers_array_size);
  if (!sorted_numbers) {
    perror("malloc");
    goto cleanup_received_numbers;
  }
  memcpy(sorted_numbers, received_numbers, numbers_array_size);
  qsort(sorted_numbers, send_number, sizeof(int), compare_ints);
  if (send_all(client_sock_fd, (char*)sorted_numbers, numbers_array_size) == -1) {
    fprintf(stderr, "send_all failed\n");
    goto cleanup_sorted_numbers;
  }

  pthread_mutex_lock(&output_mutex);
  printf("sending data: %d\n", send_number);
  print_array((const void*)received_numbers, sizeof(int), (size_t)send_number, "received_numbers: ", "", print_int);
  print_array((const void*)sorted_numbers, sizeof(int), (size_t)send_number, "sorted_numbers: ", "", print_int);
  printf("--------\n");
  pthread_mutex_unlock(&output_mutex);

cleanup_sorted_numbers:
  free(sorted_numbers);
cleanup_received_numbers:
  free(received_numbers);
cleanup:
  close(client_sock_fd);
  pthread_mutex_lock(&client_counter_mutex);
  active_clients--;
  pthread_mutex_unlock(&client_counter_mutex);

  return NULL;
}

int parse_port(char* port_str) {
  int port = (int)strtol(port_str, NULL, 0);
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

  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
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

  struct sigaction act = { 0 };
  act.sa_handler = &sigaction_handler;
  if (sigaction(SIGINT, &act, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  pthread_mutex_init(&send_number_counter_mutex, NULL);
  pthread_mutex_init(&output_mutex, NULL);
  pthread_mutex_init(&client_counter_mutex, NULL);
  int address_length = sizeof(address);
  while (working) {
    int* client_sock_fd = malloc(sizeof(int));
    if (!client_sock_fd) {
      perror("malloc");
      exit(1);
    }
    *client_sock_fd = accept(sock_fd, (struct sockaddr*)&address, (socklen_t*)&address_length);
    if (*client_sock_fd == -1) {
      perror("accept");
      exit(1);
    }
    pthread_t thread_id;
    int rv = pthread_create(&thread_id, NULL, client_handler, (void*)client_sock_fd);
    if (rv != 0) {
      errno = rv;
      perror("pthread_create");
      close(*client_sock_fd);
      free(client_sock_fd);
    }
  }

  while (active_clients) {}

  close(sock_fd);
  pthread_mutex_destroy(&send_number_counter_mutex);
  pthread_mutex_destroy(&output_mutex);
  pthread_mutex_destroy(&client_counter_mutex);

  return 0;
}
