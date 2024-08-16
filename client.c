#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "utils.h"

#define BUFFER_SIZE 1024
#define MAX_RAND_NUM 10000

pthread_mutex_t mutex;

struct thread_arg {
  int request_number;
  struct addrinfo server_info;
};

int get_random_number(void) {
  /*
    [ref]
    - https://stackoverflow.com/a/49880109/7503647
    - https://ericlippert.com/2013/12/16/how-much-bias-is-introduced-by-the-remainder-technique/
  */
  const int threshold = RAND_MAX - RAND_MAX % MAX_RAND_NUM;
  int val = rand();
  while (val >= threshold) {
    val = rand();
  }
  return val % MAX_RAND_NUM;
}

void* request_handler(void* arg) {
  struct thread_arg targ = *(struct thread_arg*)arg;
  for (int i = 0; i < targ.request_number; i++) {
    struct addrinfo* p;
    int sock_fd;
    for (p = &targ.server_info; p != NULL; p = p->ai_next) {
      sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sock_fd == -1) {
        perror("socket");
        continue;
      }
      if (connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
        perror("connect");
        continue;
      }
      break;
    }
    if (!p) {
      fprintf(stderr, "failed to get addrinfo\n");
      exit(1);
    }

    int number;
    if (recv_all(sock_fd, (char*)&number, sizeof(number)) == -1) {
      fprintf(stderr, "recv_all failed\n");
      exit(1);
    }
    size_t numbers_array_size = sizeof(int) * number;
    int* random_numbers = malloc(numbers_array_size);
    if (!random_numbers) {
      perror("malloc");
      exit(1);
    }
    for (int i = 0; i < number; i++) {
      random_numbers[i] = get_random_number();
    }
    if (send_all(sock_fd, (char*)random_numbers, numbers_array_size) == -1) {
      fprintf(stderr, "send_all failed\n");
      exit(1);
    }
    int* sorted_numbers = malloc(numbers_array_size);
    if (!sorted_numbers) {
      perror("malloc");
      exit(1);
    }
    if (recv_all(sock_fd, (char*)sorted_numbers, numbers_array_size) == -1) {
      fprintf(stderr, "recv_all failed\n");
      exit(1);
    }

    pthread_mutex_lock(&mutex);
    printf("[thread_id: %d] received data: %d\n", gettid(), number);
    print_array((const void*)random_numbers, sizeof(int), (size_t)number, "random_numbers: ", "", print_int);
    print_array((const void*)sorted_numbers, sizeof(int), (size_t)number, "sorted_numbers: ", "", print_int);
    printf("--------\n");
    pthread_mutex_unlock(&mutex);

    free(random_numbers);
    close(sock_fd);
  }

  return NULL;
}

int main(int argc, char** argv) {
  if (argc != 5) {
    fprintf(stderr, "Usage: %s <SERVER_NAME> <SERVER_PORT> <REQUEST_NUMBER> <CLIENT_NUMBER>\n", argv[0]);
    return 1;
  }
  char* server_name = argv[1];
  char* server_port = argv[2];
  /* TODO: handling invalid commandline arguments */
  int request_number = atoi(argv[3]);
  int client_number = atoi(argv[4]);

  srand(time(NULL));
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* server_info;
  int rv = getaddrinfo(server_name, server_port, &hints, &server_info);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  pthread_mutex_init(&mutex, NULL);
  int rest_request_number = request_number;
  int per_thread_request_number = request_number / client_number;
  struct thread_arg* thread_args = (struct thread_arg*)malloc(sizeof(struct thread_arg) * client_number);
  if (!thread_args) {
    perror("malloc");
    exit(1);
  }
  pthread_t thread_ids[client_number];
  for (int i = 0; i < client_number; i++) {
    if (i < client_number - 1) {
      thread_args[i].request_number = per_thread_request_number;
      rest_request_number -= per_thread_request_number;
    } else {
      thread_args[i].request_number = rest_request_number;
    }
    thread_args[i].server_info = *server_info;

    int rv = pthread_create(&thread_ids[i], NULL, request_handler, (void*)&thread_args[i]);
    if (rv != 0) {
      errno = rv;
      perror("pthread_create");
      return 1;
    }
  }

  for (int i = 0; i < client_number; i++) {
    pthread_join(thread_ids[i], NULL);
  }
  freeaddrinfo(server_info);
  free(thread_args);
  pthread_mutex_destroy(&mutex);

  return 0;
}
