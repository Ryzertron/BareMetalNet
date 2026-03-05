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
#define UPSTREAM_IP "127.0.0.1"
#define UPSTREAM_PORT 3000
#define MAX_SOCKS 10

int listen_sock_fd, epoll_fd;
struct epoll_event event, events[MAX_EPOLL_EVENTS];
int route_table[MAX_SOCKS][2];
int route_table_size = 0;

int create_loop() { return epoll_create1(0); }

void strrev(char *str) {
  for (int start = 0, end = strlen(str) - 2; start < end; start++, end--) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
  }
}

void loop_attach(int fd, int events) {
  struct epoll_event ev;

  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    perror("[ERROR] epoll_ctl: add FAILED\n");
    exit(EXIT_FAILURE);
  }
}

int create_server() {
  int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (listen_sock_fd < 0) {
    perror("[ERROR] socket creation failiure\n");
    exit(EXIT_FAILURE);
  }

  int enable = 1;
  if (setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                 sizeof(enable)) < 0) {
    perror("[ERROR] setsockopt FAILED\n");
    close(listen_sock_fd);
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);

  if (bind(listen_sock_fd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("[ERROR] bind FAILED\n");
    close(listen_sock_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(listen_sock_fd, MAX_ACCEPT_BACKLOG) < 0) {
    perror("[ERROR] listen FAILED\n");
    close(listen_sock_fd);
    exit(EXIT_FAILURE);
  }

  printf("[INFO] Server listenting on port %d\n", PORT);

  return listen_sock_fd;
}

int connect_upstream() {
  int upstream_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (upstream_sock_fd < 0) {
    perror("socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in upstream_addr;

  memset(&upstream_addr, 0, sizeof(upstream_addr));
  upstream_addr.sin_family = AF_INET;
  upstream_addr.sin_port = htons(UPSTREAM_PORT); // upstream server port
  upstream_addr.sin_addr.s_addr = inet_addr(UPSTREAM_IP);

  if (connect(upstream_sock_fd, (struct sockaddr *)&upstream_addr,
              sizeof(upstream_addr)) < 0) {
    perror("connect failed");
    close(upstream_sock_fd);
    exit(EXIT_FAILURE);
  }

  return upstream_sock_fd;
}

void accept_connection() {
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  char client_ip[INET_ADDRSTRLEN];

  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

  int conn_sock_fd =
      accept(listen_sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);

  if (conn_sock_fd < 0) {
    perror("[ERROR] accept FAILED\n");
    return;
  }
  printf("[INFO] Client connected from %s:%d\n", client_ip,
         ntohs(client_addr.sin_port));

  loop_attach(conn_sock_fd, EPOLLIN);

  int upstream_sock_fd = connect_upstream();

  loop_attach(upstream_sock_fd, EPOLLIN);

  route_table[route_table_size][0] = conn_sock_fd;
  route_table[route_table_size][1] = upstream_sock_fd;
  route_table_size += 1;
}

int find_upstream_socket(int conn_sock_fd) {
  for (int i = 0; i < route_table_size; i++) {
    if (route_table[i][0] == conn_sock_fd) {
      return route_table[i][1];
    }
  }
  return -1; // not found
}

int find_client_socket(int upstream_sock_fd) {
  for (int i = 0; i < route_table_size; i++) {
    if (route_table[i][1] == upstream_sock_fd) {
      return route_table[i][0];
    }
  }
  return -1; // not found
}

void handle_client(int conn_sock_fd) {

  char buff[BUFF_SIZE];

  // read message from client
  int read_n = recv(conn_sock_fd, buff, BUFF_SIZE, 0);

  // client closed connection or error occurred
  if (read_n <= 0) {
    close(conn_sock_fd);
    return;
  }

  // print client message
  printf("Client message: %.*s\n", read_n, buff);

  // find the right upstream socket from the route table
  int upstream_sock_fd = find_upstream_socket(conn_sock_fd);
  // (Assume this function exists in your routing logic)

  if (upstream_sock_fd < 0) {
    printf("No upstream route found\n");
    close(conn_sock_fd);
    return;
  }

  // sending client message to upstream
  int bytes_written = 0;
  int message_len = read_n;

  while (bytes_written < message_len) {
    int n = send(upstream_sock_fd, buff + bytes_written,
                 message_len - bytes_written, 0);

    if (n <= 0) {
      perror("send failed");
      break;
    }

    bytes_written += n;
  }
}

void handle_upstream(int upstream_sock_fd) {

  char buff[BUFF_SIZE];

  // read message from upstream
  int read_n = recv(upstream_sock_fd, buff, BUFF_SIZE, 0);

  // Upstream closed connection or error occurred
  if (read_n <= 0) {
    close(upstream_sock_fd);
    return;
  }

  // find the right client socket from the route table
  int client_sock_fd = find_client_socket(upstream_sock_fd);

  if (client_sock_fd < 0) {
    printf("Client socket not found\n");
    return;
  }

  // send upstream message to client
  int bytes_written = 0;
  int message_len = read_n;

  while (bytes_written < message_len) {
    int n = send(client_sock_fd, buff + bytes_written,
                 message_len - bytes_written, 0);

    if (n <= 0) {
      perror("send failed");
      break;
    }

    bytes_written += n;
  }
}

int loop_run() {
  while (1) {
    int n_ready_fds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);

    for (int i = 0; i < n_ready_fds; i++) {
      int fd = events[i].data.fd;
      if (fd == listen_sock_fd) {
        accept_connection();
      }

      for (int j = 0; j < route_table_size; j++) {

        if (fd == route_table[j][0]) {
          handle_client(fd);
          break;
        }

        else if (fd == route_table[j][1]) {
          handle_upstream(fd);
          break;
        }
      }
    }
  }
}

int main() {
  listen_sock_fd = create_server();
  epoll_fd = create_loop();
  loop_attach(listen_sock_fd, EPOLLIN);
  loop_run();
  return 0;
}
