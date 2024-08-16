#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "utils.h"

ssize_t recv_all(int sock_fd, char* buffer, size_t length) {
  size_t total_received = 0;
  while (total_received < length) {
    ssize_t received = recv(sock_fd, buffer + total_received, length - total_received, 0);
    if (received == -1) {
      perror("recv");
      return -1;
    }
    if (!received) {
      break;
    }
    total_received += received;
  }
  return total_received;
}

ssize_t send_all(int sock_fd, char* buffer, size_t length) {
  size_t total_sent = 0;
  while (total_sent < length) {
    ssize_t sent = send(sock_fd, buffer + total_sent, length - total_sent, 0);
    if (sent == -1) {
      perror("send");
      return -1;
    }
    total_sent += sent;
  }
  return total_sent;
}

int compare_ints(const void* a, const void* b) {
  int arg1 = *(const int*)a;
  int arg2 = *(const int*)b;
  return (arg1 > arg2) - (arg1 < arg2);
}

void print_array(const void* array, size_t element_size, size_t element_count, const char* header, const char* trailer, void (*print_element)(const void*)) {
  printf("%s", header);
  for (size_t i = 0; i < element_count; i++) {
    print_element((const void*)(array + i * element_size));
  }
  printf("%s\n", trailer);
}

void print_int(const void* element) {
  printf("%d ", *(int*)element);
}
