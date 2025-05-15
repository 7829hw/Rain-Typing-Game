#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "auth.h"
#include "db_handler.h"  // db_handler.h 추가
#include "network_server.h"
#include "score_manager.h"

#define PORT 8080
#define MAX_CLIENTS 10

int main() {
  int server_sock, client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_size;
  pthread_t tid;

  server_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (server_sock == -1) {
    perror("socket() error");
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  int opt = 1;
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    perror("setsockopt(SO_REUSEADDR) failed");
  }

  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    perror("bind() error");
    close(server_sock);
    exit(EXIT_FAILURE);
  }

  if (listen(server_sock, MAX_CLIENTS) == -1) {
    perror("listen() error");
    close(server_sock);
    exit(EXIT_FAILURE);
  }

  printf("Rain Typing Game Server started on port %d...\n", PORT);

  // Initialize DB files first, then other systems
  init_db_files();      // DB 파일 시스템 초기화
  init_auth_system();   // 인증 시스템 초기화
  init_score_system();  // 점수 관리 시스템 초기화

  while (1) {
    client_addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size);
    if (client_sock == -1) {
      perror("accept() error");
      continue;
    }

    printf("Client connected: %s:%d (socket: %d)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_sock);

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

  close(server_sock);
  return 0;
}