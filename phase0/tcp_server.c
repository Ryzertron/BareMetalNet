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

void strrev(char *str) {
  for (int start = 0, end = strlen(str); start < end; start++, end--) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
  }
}

int main() {
  // Create listening socket
  int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

  // Set the socket as reusable address
  int enable = 1;
  setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

  // Structure for storing server address
  struct sockaddr_in server_addr;

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  // Bind the socket to the address-port combination
  bind(listen_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

  listen(listen_sock_fd, MAX_ACCEPT_BACKLOG);
  printf("[INFO] Server listening on port %d\n", PORT);
  /***********************************************************************************************************************/

  // Structure for storing client address
  struct sockaddr_in client_addr;
  socklen_t client_addr_len;

  // Accept client reqs
  int conn_sock_fd =
      accept(listen_sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);
  printf("[INFO] Client connected to the server \n");

  while (1) {
    // Buffer to store client messages
    char buff[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);

    // Read the message from client to Buffer
    ssize_t read_n = recv(conn_sock_fd, buff, sizeof(buff), 0);

    // client closed or error
    if (read_n < 0) {
      printf("[INFO] Error occured. Closing server\n");
      close(conn_sock_fd);
      exit(1);
    } else if (read_n == 0) {
      printf("[INFO] Client Disconnected. Closing server\n");
      close(conn_sock_fd);
      exit(1);
    }

    printf("[CLIENT MSG] %s", buff);

    strrev(buff);

    send(conn_sock_fd, buff, read_n, 0);
  }
}
