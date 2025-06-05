// server/src/server_network.c
#include "server_network.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "auth_manager.h"
#include "protocol.h"
#include "score_manager.h"
#include "word_manager.h"

// 로그인된 사용자 관리 구조체
typedef struct {
  char username[MAX_ID_LEN];
  int client_sock;
  bool is_active;
} LoggedInUser;

#define MAX_LOGGED_IN_USERS 100
static LoggedInUser logged_in_users[MAX_LOGGED_IN_USERS];
static pthread_mutex_t logged_in_users_mutex = PTHREAD_MUTEX_INITIALIZER;

// 정확한 바이트 수만큼 송신하는 함수
static int send_all(int sock, const void* buf, size_t len) {
  size_t total_sent = 0;
  const char* ptr = (const char*)buf;

  while (total_sent < len) {
    ssize_t sent = send(sock, ptr + total_sent, len - total_sent, 0);
    if (sent == -1) {
      if (errno == EINTR) continue;  // 시그널에 의한 중단은 재시도
      return -1;
    }
    if (sent == 0) {
      return -1;  // 연결 종료
    }
    total_sent += sent;
  }
  return 0;
}

// 정확한 바이트 수만큼 수신하는 함수
static int recv_all(int sock, void* buf, size_t len) {
  size_t total_received = 0;
  char* ptr = (char*)buf;

  while (total_received < len) {
    ssize_t received = recv(sock, ptr + total_received, len - total_received, 0);
    if (received == -1) {
      if (errno == EINTR) continue;  // 시그널에 의한 중단은 재시도
      return -1;
    }
    if (received == 0) {
      return -1;  // 연결 종료
    }
    total_received += received;
  }
  return 0;
}

// 응답 전송 함수
static int send_response(int client_sock, MessageType msg_type, const void* response_data, size_t data_len) {
  MessageHeader header;
  header.type = msg_type;
  header.length = data_len;

  // 헤더 전송
  if (send_all(client_sock, &header, sizeof(MessageHeader)) != 0) {
    return -1;
  }

  // 데이터 전송 (있는 경우)
  if (response_data && data_len > 0) {
    if (send_all(client_sock, response_data, data_len) != 0) {
      return -1;
    }
  }

  return 0;
}

// 사용자가 이미 로그인되어 있는지 확인하는 함수
static int is_user_already_logged_in(const char* username) {
  int result = -1;
  pthread_mutex_lock(&logged_in_users_mutex);
  for (int i = 0; i < MAX_LOGGED_IN_USERS; i++) {
    if (logged_in_users[i].is_active && strcmp(logged_in_users[i].username, username) == 0) {
      result = i;
      break;
    }
  }
  pthread_mutex_unlock(&logged_in_users_mutex);
  return result;
}

// 사용자를 로그인 목록에 추가하는 함수
static int add_logged_in_user(const char* username, int client_sock) {
  int result = 0;
  pthread_mutex_lock(&logged_in_users_mutex);
  for (int i = 0; i < MAX_LOGGED_IN_USERS; i++) {
    if (!logged_in_users[i].is_active) {
      snprintf(logged_in_users[i].username, MAX_ID_LEN, "%s", username);
      logged_in_users[i].username[MAX_ID_LEN - 1] = '\0';
      logged_in_users[i].client_sock = client_sock;
      logged_in_users[i].is_active = true;
      result = 1;
      break;
    }
  }
  pthread_mutex_unlock(&logged_in_users_mutex);
  return result;
}

// 사용자를 로그인 목록에서 제거하는 함수
static void remove_logged_in_user(const char* username) {
  pthread_mutex_lock(&logged_in_users_mutex);
  for (int i = 0; i < MAX_LOGGED_IN_USERS; i++) {
    if (logged_in_users[i].is_active && strcmp(logged_in_users[i].username, username) == 0) {
      logged_in_users[i].is_active = false;
      memset(logged_in_users[i].username, 0, sizeof(logged_in_users[i].username));
      break;
    }
  }
  pthread_mutex_unlock(&logged_in_users_mutex);
}

// 서버 시작 시 로그인 사용자 목록 초기화
void init_logged_in_users() {
  pthread_mutex_lock(&logged_in_users_mutex);
  for (int i = 0; i < MAX_LOGGED_IN_USERS; i++) {
    logged_in_users[i].is_active = false;
    memset(logged_in_users[i].username, 0, sizeof(logged_in_users[i].username));
  }
  pthread_mutex_unlock(&logged_in_users_mutex);
}

void* handle_client(void* arg) {
  int client_sock = *((int*)arg);
  free(arg);

  char current_user[MAX_ID_LEN] = {0};
  MessageHeader header;

  printf("[SERVER_NETWORK] Client connected on socket %d\n", client_sock);

  while (1) {
    // 헤더 수신
    if (recv_all(client_sock, &header, sizeof(MessageHeader)) != 0) {
      if (errno == EINTR) {
        printf("[SERVER_NETWORK] recv header on socket %d interrupted. Assuming shutdown.\n", client_sock);
      } else {
        printf("[SERVER_NETWORK] Failed to receive header from socket %d (user: %s), errno: %d\n", client_sock,
               strlen(current_user) > 0 ? current_user : "N/A", errno);
      }
      break;
    }

    // 메시지 바디 버퍼 할당
    void* message_body = NULL;
    if (header.length > 0) {
      if (header.length > 10240) {  // 10KB 제한
        printf("[SERVER_NETWORK] Message too large from socket %d: %d bytes\n", client_sock, header.length);
        break;
      }

      message_body = malloc(header.length);
      if (!message_body) {
        printf("[SERVER_NETWORK] Memory allocation failed for socket %d\n", client_sock);
        break;
      }

      if (recv_all(client_sock, message_body, header.length) != 0) {
        printf("[SERVER_NETWORK] Failed to receive body from socket %d\n", client_sock);
        free(message_body);
        break;
      }
    }

    // 메시지 처리
    bool should_disconnect = false;

    switch (header.type) {
      case MSG_TYPE_REGISTER_REQ: {
        RegisterRequest* req = (RegisterRequest*)message_body;
        RegisterResponse resp_data;
        resp_data.success = register_user_impl(req->username, req->password, resp_data.message);

        if (send_response(client_sock, MSG_TYPE_REGISTER_RESP, &resp_data, sizeof(RegisterResponse)) != 0) {
          should_disconnect = true;
        }
        break;
      }

      case MSG_TYPE_LOGIN_REQ: {
        LoginRequest* req = (LoginRequest*)message_body;
        LoginResponse resp_data;

        // 이미 로그인된 사용자인지 확인
        if (is_user_already_logged_in(req->username) != -1) {
          resp_data.success = 0;
          strncpy(resp_data.message, "이 ID는 이미 다른 세션에서 로그인 중입니다.", MAX_MSG_LEN - 1);
          resp_data.message[MAX_MSG_LEN - 1] = '\0';
        } else {
          resp_data.success = login_user_impl(req->username, req->password, resp_data.message, current_user);
          if (resp_data.success) {
            // 로그인 성공 시 목록에 추가
            if (!add_logged_in_user(current_user, client_sock)) {
              // 로그인 목록에 추가 실패 (목록이 꽉 참)
              resp_data.success = 0;
              strncpy(current_user, "", MAX_ID_LEN);
              strncpy(resp_data.message, "서버 로그인 제한에 도달했습니다. 나중에 다시 시도하세요.", MAX_MSG_LEN - 1);
              resp_data.message[MAX_MSG_LEN - 1] = '\0';
            } else {
              printf("[SERVER_NETWORK] User '%s' logged in on socket %d.\n", current_user, client_sock);
            }
          } else {
            current_user[0] = '\0';
          }
        }

        if (send_response(client_sock, MSG_TYPE_LOGIN_RESP, &resp_data, sizeof(LoginResponse)) != 0) {
          should_disconnect = true;
        }
        break;
      }

      case MSG_TYPE_SCORE_SUBMIT_REQ: {
        ScoreSubmitRequest* req = (ScoreSubmitRequest*)message_body;
        ScoreSubmitResponse resp_data;

        if (strlen(current_user) == 0) {
          resp_data.success = 0;
          strncpy(resp_data.message, "Not logged in. Cannot submit score.", MAX_MSG_LEN - 1);
          resp_data.message[MAX_MSG_LEN - 1] = '\0';
        } else {
          resp_data.success = submit_score_impl(current_user, req->score, resp_data.message);
        }

        if (send_response(client_sock, MSG_TYPE_SCORE_SUBMIT_RESP, &resp_data, sizeof(ScoreSubmitResponse)) != 0) {
          should_disconnect = true;
        }
        break;
      }

      case MSG_TYPE_LEADERBOARD_REQ: {
        LeaderboardResponse resp_data;
        get_leaderboard_impl(resp_data.entries, &resp_data.count, MAX_LEADERBOARD_ENTRIES);

        if (send_response(client_sock, MSG_TYPE_LEADERBOARD_RESP, &resp_data, sizeof(LeaderboardResponse)) != 0) {
          should_disconnect = true;
        }
        break;
      }

      case MSG_TYPE_WORDLIST_REQ: {
        if (send_response(client_sock, MSG_TYPE_WORDLIST_RESP, &g_wordlist, sizeof(WordListResponse)) != 0) {
          should_disconnect = true;
        }
        break;
      }

      case MSG_TYPE_LOGOUT_REQ: {
        LogoutResponse resp_data;
        if (strlen(current_user) > 0) {
          printf("[SERVER_NETWORK] User %s logged out from socket %d.\n", current_user, client_sock);
          remove_logged_in_user(current_user);
          memset(current_user, 0, sizeof(current_user));
          resp_data.success = 1;
          strncpy(resp_data.message, "Logged out successfully.", MAX_MSG_LEN - 1);
        } else {
          resp_data.success = 0;
          strncpy(resp_data.message, "Not logged in, cannot log out.", MAX_MSG_LEN - 1);
        }
        resp_data.message[MAX_MSG_LEN - 1] = '\0';

        if (send_response(client_sock, MSG_TYPE_LOGOUT_RESP, &resp_data, sizeof(LogoutResponse)) != 0) {
          should_disconnect = true;
        }
        break;
      }

      default: {
        ErrorResponse err_resp;
        snprintf(err_resp.message, MAX_MSG_LEN, "Unknown or unsupported message type: %d", header.type);
        printf("[SERVER_NETWORK] Error on socket %d: %s\n", client_sock, err_resp.message);

        if (send_response(client_sock, MSG_TYPE_ERROR, &err_resp, sizeof(ErrorResponse)) != 0) {
          should_disconnect = true;
        }
        break;
      }
    }

    if (message_body) {
      free(message_body);
    }

    if (should_disconnect) {
      break;
    }
  }

  // 연결 종료 처리
  if (strlen(current_user) > 0) {
    printf("[SERVER_NETWORK] Cleaning up session for user %s on socket %d due to disconnect/error.\n", current_user, client_sock);
    remove_logged_in_user(current_user);
  }

  printf("[SERVER_NETWORK] Client disconnected from socket %d\n", client_sock);
  close(client_sock);
  return NULL;
}