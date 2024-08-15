#ifndef UTILS_H
#define UTILS_H

void recv_all(int sock_fd, char* buffer, size_t length);

void send_all(int sock_fd, char* buffer, size_t length);

int compare_ints(const void* a, const void* b);

/* array is assumed to be non-empty */
void print_array(const void* array, size_t element_size, size_t element_count, const char* header, const char* trailer, void (*print_element)(const void*));

void print_int(const void* element);

#endif // UTILS_H
