// server/src/server_main.c
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "auth_manager.h"
#include "db_handler.h"
#include "score_manager.h"
#include "server_network.h"  // 변경: network_server.h -> server_network.h
#include "word_manager.h"

#define PORT 8080
#define MAX_CLIENTS 10

volatile sig_atomic_t server_shutdown_requested = 0;
int server_sock_fd = -1;

void handle_server_sigint(int sig) {
  (void)sig;  // Unused parameter
  server_shutdown_requested = 1;
  if (server_sock_fd != -1) {
    printf("\n[SERVER_MAIN] SIGINT received. Closing server socket to stop accept() loop.\n");
    close(server_sock_fd);
    server_sock_fd = -1;
  }
}

int main() {
  int client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_size;
  pthread_t tid;

  signal(SIGINT, handle_server_sigint);
  signal(SIGTERM, handle_server_sigint);

  server_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (server_sock_fd == -1) {
    perror("socket() error");
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  int opt = 1;
  if (setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    perror("setsockopt(SO_REUSEADDR) failed");
  }

  if (bind(server_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    perror("bind() error");
    if (server_sock_fd != -1) close(server_sock_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(server_sock_fd, MAX_CLIENTS) == -1) {
    perror("listen() error");
    if (server_sock_fd != -1) close(server_sock_fd);
    exit(EXIT_FAILURE);
  }

  printf("Rain Typing Game Server started on port %d...\n", PORT);
  printf("Press Ctrl+C to shut down the server.\n");

  /* ① users.txt·scores.txt를 만들면서 data/ 폴더가 존재하도록 만듦 */
  init_db_files();               /* DATA_DIR_PATH == "data" */  

  /* ② 이제 data/words.txt 를 읽어 들임 */
  if (load_wordlist_from_file("data/words.txt") <= 0) {
      fprintf(stderr, "[SERVER] data/words.txt load failed\n");
      exit(EXIT_FAILURE);
  }
  extern void init_logged_in_users();
  init_logged_in_users();
  init_auth_system();
  init_score_system();

  while (!server_shutdown_requested) {
    client_addr_size = sizeof(client_addr);
    client_sock = accept(server_sock_fd, (struct sockaddr *)&client_addr, &client_addr_size);

    if (server_shutdown_requested) {
      if (client_sock != -1) close(client_sock);
      break;
    }

    if (client_sock == -1) {
      if (server_shutdown_requested) {
        printf("[SERVER_MAIN] accept() interrupted by shutdown signal. Exiting loop.\n");
      } else {
        perror("accept() error");
      }
      continue;
    }

    printf("[SERVER_MAIN] Client connected: %s:%d (socket: %d)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_sock);

    int *p_client_sock = malloc(sizeof(int));
    if (!p_client_sock) {
      perror("malloc for client_sock failed");
      close(client_sock);
      continue;
    }
    *p_client_sock = client_sock;

    if (pthread_create(&tid, NULL, handle_client, (void *)p_client_sock) != 0) {
      perror("pthread_create() error");
      free(p_client_sock);
      close(client_sock);
    }
    pthread_detach(tid);
  }

  printf("[SERVER_MAIN] Shutdown sequence initiated.\n");

  if (server_sock_fd != -1) {
    close(server_sock_fd);
    server_sock_fd = -1;
  }
  printf("[SERVER_MAIN] Server has shut down.\n");
  return 0;
}