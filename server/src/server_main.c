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
#include "hash_util.h"
#include "score_manager.h"
#include "server_network.h"
#include "word_manager.h"

#define PORT 8080
#define MAX_CLIENTS 10

volatile sig_atomic_t server_shutdown_requested = 0;
int server_sock_fd = -1;

/* 안전한 서버 종료를 위한 정리 함수 */
static void cleanup_server_resources(void) {
  printf("[SERVER_MAIN] Starting cleanup process...\n");

  /* 소켓 정리 */
  if (server_sock_fd != -1) {
    close(server_sock_fd);
    server_sock_fd = -1;
    printf("[SERVER_MAIN] Server socket closed\n");
  }

  /* 모든 mutex 정리 */
  cleanup_db_mutexes();
  cleanup_word_manager_mutexes();
  cleanup_logged_in_users_mutex();

  /* 암호화 시스템 정리 */
  crypto_cleanup();

  printf("[SERVER_MAIN] All resources cleaned up\n");
}

void handle_server_sigint(int sig) {
  (void)sig;
  server_shutdown_requested = 1;
  if (server_sock_fd != -1) {
    printf("\n[SERVER_MAIN] SIGINT received. Initiating graceful shutdown...\n");
    close(server_sock_fd);
    server_sock_fd = -1;
  }
}

int main() {
  int client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_size;
  pthread_t tid;

  /* 시그널 핸들러 등록 */
  signal(SIGINT, handle_server_sigint);
  signal(SIGTERM, handle_server_sigint);

  printf("Rain Typing Game Server (Thread-Safe Version) starting...\n");

  /* 소켓 생성 */
  server_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (server_sock_fd == -1) {
    perror("socket() error");
    exit(EXIT_FAILURE);
  }

  /* 소켓 옵션 설정 */
  int opt = 1;
  setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  /* 주소 설정 */
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  /* 바인드 */
  if (bind(server_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    perror("bind() error");
    cleanup_server_resources();
    exit(EXIT_FAILURE);
  }

  /* 리슨 */
  if (listen(server_sock_fd, MAX_CLIENTS) == -1) {
    perror("listen() error");
    cleanup_server_resources();
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d with thread-safe file access...\n", PORT);
  printf("Press Ctrl+C to shut down the server gracefully.\n");

  /* 시스템 초기화 (순서 중요!) */
  printf("[SERVER_MAIN] Initializing thread-safe systems...\n");

  /* 1. DB 파일 초기화 (mutex가 자동으로 초기화됨) */
  init_db_files();

  /* 2. 단어 리스트 로드 (thread-safe) */
  if (load_wordlist_from_file("data/words.txt") <= 0) {
    fprintf(stderr, "[SERVER] data/words.txt load failed\n");
    cleanup_server_resources();
    exit(EXIT_FAILURE);
  }

  /* 3. 로그인 사용자 관리 초기화 */
  init_logged_in_users();

  /* 4. 인증 시스템 초기화 (암호화 포함) */
  init_auth_system();

  /* 5. 점수 시스템 초기화 */
  init_score_system();

  printf("[SERVER_MAIN] All thread-safe systems initialized successfully\n");

  /* 메인 서버 루프 */
  while (!server_shutdown_requested) {
    client_addr_size = sizeof(client_addr);
    client_sock = accept(server_sock_fd, (struct sockaddr *)&client_addr, &client_addr_size);

    /* 종료 신호 확인 */
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

    /* 클라이언트 소켓을 동적 할당하여 스레드에 전달 */
    int *p_client_sock = malloc(sizeof(int));
    if (!p_client_sock) {
      perror("malloc for client_sock failed");
      close(client_sock);
      continue;
    }
    *p_client_sock = client_sock;

    /* 클라이언트 처리 스레드 생성 */
    if (pthread_create(&tid, NULL, handle_client, (void *)p_client_sock) != 0) {
      perror("pthread_create() error");
      free(p_client_sock);
      close(client_sock);
    } else {
      /* 스레드 분리 (자동 정리) */
      pthread_detach(tid);
    }
  }

  printf("[SERVER_MAIN] Shutdown sequence initiated.\n");

  /* 잠시 대기하여 활성 클라이언트들이 정리될 시간 제공 */
  printf("[SERVER_MAIN] Waiting for active connections to close...\n");
  sleep(2);

  /* 서버 리소스 정리 */
  cleanup_server_resources();

  printf("[SERVER_MAIN] Thread-safe server has shut down gracefully.\n");
  return 0;
}