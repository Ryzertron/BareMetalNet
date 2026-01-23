#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5

// Function to reverse a string in-place
void strrev(char *str) {
  for (int start = 0, end = strlen(str) - 2; start < end; start++, end--) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
  }
}

int main() {
  // Create listening socket
  int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock_fd < 0) {
    perror("socket creation failed");
    exit(1);
  }

  // Set the socket as reusable address
  int enable = 1;
  setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

  // Structure for storing server address
  struct sockaddr_in server_addr;

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  // Bind the socket to the address-port combination
  if (bind(listen_sock_fd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("bind failed");
    close(listen_sock_fd);
    exit(1);
  }

  if (listen(listen_sock_fd, MAX_ACCEPT_BACKLOG) < 0) {
    perror("listen failed");
    close(listen_sock_fd);
    exit(1);
  }

  printf("[INFO] Server listening on port %d\n", PORT);

  // Main server loop to handle multiple clients sequentially
  while (1) {
    // Structure for storing client address
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);
    char client_ip[INET_ADDRSTRLEN];

    printf("[INFO] Waiting for new client connection...\n");

    // Accept client request
    int conn_sock_fd = accept(listen_sock_fd, (struct sockaddr *)&client_addr,
                              &client_addr_len);
    if (conn_sock_fd < 0) {
      perror("accept failed");
      continue; // Continue to accept next client
    }

    // Convert client IP to string
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("[INFO] Client connected from %s:%d\n", client_ip,
           ntohs(client_addr.sin_port));

    // Handle this client until they disconnect
    while (1) {
      // Buffer to store client messages
      char buff[BUFF_SIZE];
      memset(buff, 0, BUFF_SIZE);

      // Read the message from client to Buffer
      ssize_t read_n = recv(conn_sock_fd, buff, sizeof(buff), 0);

      // client closed or error
      if (read_n < 0) {
        perror("[ERROR] recv failed");
        break;
      } else if (read_n == 0) {
        printf("[INFO] Client %s:%d disconnected\n", client_ip,
               ntohs(client_addr.sin_port));
        break;
      }

      printf("[CLIENT %s:%d] %s", client_ip, ntohs(client_addr.sin_port), buff);

      // Reverse the string if it's not empty
      if (read_n > 1) {
        strrev(buff);
      }

      // Send the reversed message back
      if (send(conn_sock_fd, buff, read_n, 0) < 0) {
        perror("[ERROR] send failed");
        break;
      }
    }

    // Close the connection socket for this client
    close(conn_sock_fd);
    printf("[INFO] Connection closed for client %s:%d\n", client_ip,
           ntohs(client_addr.sin_port));
  }

  // Close the listening socket (though we never reach here in this infinite
  // loop)
  close(listen_sock_fd);
  return 0;
}
