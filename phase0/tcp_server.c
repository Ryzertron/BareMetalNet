#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define MAX_EPOLL_EVENTS 10
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

  int epoll_fd = epoll_create1(0);
  struct epoll_event event, events[MAX_EPOLL_EVENTS];

  event.events = EPOLLIN;
  event.data.fd = listen_sock_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock_fd, &event);

  // Main server loop to handle multiple clients sequentially
  while (1) {
    int n_ready_fds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);

    for (int i = 0; i < n_ready_fds; i++) {

      if (events[i].data.fd == listen_sock_fd) {

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        char client_ip[INET_ADDRSTRLEN];

        int conn_sock_fd = accept(
            listen_sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (conn_sock_fd < 0) {
          perror("[ERROR] Accept Failed\n");
          continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        printf("[INFO] Client connected from %s:%d\n", client_ip,
               ntohs(client_addr.sin_port));
        event.events = EPOLLIN;
        event.data.fd = conn_sock_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock_fd, &event);
      }

      else {

        int client_fd = events[i].data.fd;
        char buf[BUFF_SIZE];

        memset(buf, 0, BUFF_SIZE);

        ssize_t read_n = recv(client_fd, buf, sizeof(buf), 0);

        if (read_n <= 0) {
          close(client_fd);
          printf("[INFO] Client Disconnected\n");
          continue;
        }
        printf("[CLIENT] %s", buf);

        if (read_n > 1)
          strrev(buf);

        send(client_fd, buf, read_n, 0);
      }
    }
  }

  close(listen_sock_fd);
  return 0;
}
