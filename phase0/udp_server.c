#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8888
#define BUFFER_SIZE 1024

// Structure to pass data to thread
typedef struct {
  struct sockaddr_in client_addr;
  char buffer[BUFFER_SIZE];
  int sockfd;
} client_data;

// Function to reverse a string
void reverse_string(char *str) {
  int len = strlen(str);
  for (int i = 0; i < len / 2; i++) {
    char temp = str[i];
    str[i] = str[len - i - 1];
    str[len - i - 1] = temp;
  }
}

// Thread function to handle each client request
void *handle_client(void *arg) {
  client_data *data = (client_data *)arg;
  struct sockaddr_in client_addr = data->client_addr;
  char *buffer = data->buffer;
  int sockfd = data->sockfd;

  // Print received message
  printf("Thread %lu: Received from %s:%d: %s\n", pthread_self(),
         inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

  // Reverse the string
  reverse_string(buffer);

  // Send reversed string back to client
  sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&client_addr,
         sizeof(client_addr));

  printf("Thread %lu: Sent back: %s\n", pthread_self(), buffer);

  free(data);
  return NULL;
}

int main() {
  int sockfd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);

  // Create UDP socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Configure server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  // Bind socket to address
  if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("UDP Reverse Server listening on port %d...\n", PORT);

  while (1) {
    // Allocate memory for thread data
    client_data *data = malloc(sizeof(client_data));
    if (data == NULL) {
      perror("Failed to allocate memory");
      continue;
    }

    data->sockfd = sockfd;

    // Receive message from client
    int n = recvfrom(sockfd, data->buffer, BUFFER_SIZE - 1, 0,
                     (struct sockaddr *)&client_addr, &client_len);

    if (n < 0) {
      perror("Receive failed");
      free(data);
      continue;
    }

    data->buffer[n] = '\0'; // Null-terminate the string
    data->client_addr = client_addr;

    // Create thread to handle the request
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, (void *)data) != 0) {
      perror("Failed to create thread");
      free(data);
      continue;
    }

    // Detach thread so it cleans up automatically
    pthread_detach(thread_id);
  }

  close(sockfd);
  return 0;
}
