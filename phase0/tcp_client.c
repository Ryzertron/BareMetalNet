#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 8080
#define BUFF_SIZE 10000

int main() {
  // Create listening socket
  int client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

  // To store server address
  struct sockaddr_in server_addr;

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(SERVER_PORT);

  // Connect to server
  if (connect(client_sock_fd, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) != 0) {
    printf("[ERROR] Failed to connect to tcp server\n");
    exit(1);
  } else {
    printf("[INFO] Connected to tcp server\n");
  }
}
