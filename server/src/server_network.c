// server/src/server_network.c
#include "server_network.h"  // 변경된 헤더 파일 이름

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
  }
  pthread_mutex_unlock(&logged_in_users_mutex);
}

void* handle_client(void* arg) {
  int client_sock = *((int*)arg);
  free(arg);

  char buffer[1024];
  ssize_t received_bytes;
  char current_user[MAX_ID_LEN] = {0};

  while (1) {
    received_bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (received_bytes <= 0) {
      if (received_bytes == 0) {
        printf("[SERVER_NETWORK] Client disconnected gracefully: socket %d (user: %s)\n", client_sock,
               strlen(current_user) > 0 ? current_user : "N/A");
      } else {
        if (errno == EINTR) {
          printf("[SERVER_NETWORK] recv on socket %d interrupted. Assuming shutdown.\n", client_sock);
          break;
        }
        printf("[SERVER_NETWORK] recv() error from client on socket %d (user: %s), errno: %d\n", client_sock,
               strlen(current_user) > 0 ? current_user : "N/A", errno);
      }
      break;
    }
    buffer[received_bytes] = '\0';

    MessageHeader* header = (MessageHeader*)buffer;

    char response_buffer[1024];
    MessageHeader resp_header;
    int response_data_len = 0;
    void* resp_data_ptr = response_buffer + sizeof(MessageHeader);

    bool send_response = true;

    switch (header->type) {
      case MSG_TYPE_REGISTER_REQ: {
        RegisterRequest* req = (RegisterRequest*)(buffer + sizeof(MessageHeader));
        RegisterResponse resp_data;
        resp_data.success = register_user_impl(req->username, req->password, resp_data.message);
        resp_header.type = MSG_TYPE_REGISTER_RESP;
        response_data_len = sizeof(RegisterResponse);
        memcpy(resp_data_ptr, &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_LOGIN_REQ: {
        LoginRequest* req = (LoginRequest*)(buffer + sizeof(MessageHeader));
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
        resp_header.type = MSG_TYPE_LOGIN_RESP;
        response_data_len = sizeof(LoginResponse);
        memcpy(resp_data_ptr, &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_SCORE_SUBMIT_REQ: {
        ScoreSubmitRequest* req = (ScoreSubmitRequest*)(buffer + sizeof(MessageHeader));
        ScoreSubmitResponse resp_data;
        if (strlen(current_user) == 0) {
          resp_data.success = 0;
          strncpy(resp_data.message, "Not logged in. Cannot submit score.", MAX_MSG_LEN - 1);
          resp_data.message[MAX_MSG_LEN - 1] = '\0';
        } else {
          resp_data.success = submit_score_impl(current_user, req->score, resp_data.message);
        }
        resp_header.type = MSG_TYPE_SCORE_SUBMIT_RESP;
        response_data_len = sizeof(ScoreSubmitResponse);
        memcpy(resp_data_ptr, &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_LEADERBOARD_REQ: {
        LeaderboardResponse resp_data;
        // 서버 사이드에서 점수 파일을 읽어와 응답 데이터 채우기
        get_leaderboard_impl(resp_data.entries, &resp_data.count,
                             MAX_LEADERBOARD_ENTRIES /* :contentReference[oaicite:2]{index=2}:contentReference[oaicite:3]{index=3} */
        );
        resp_header.type = MSG_TYPE_LEADERBOARD_RESP; /* :contentReference[oaicite:4]{index=4}:contentReference[oaicite:5]{index=5} */
        response_data_len = sizeof(LeaderboardResponse);
        memcpy(resp_data_ptr, &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_WORDLIST_REQ: {
        /* 한 덩어리 버퍼에 헤더와 본문을 붙여 전송 */
        char buf[sizeof(MessageHeader) + sizeof(WordListResponse)];
        MessageHeader hdr = {MSG_TYPE_WORDLIST_RESP, sizeof(WordListResponse)};

        memcpy(buf, &hdr, sizeof(hdr));
        memcpy(buf + sizeof(hdr), &g_wordlist, sizeof(WordListResponse));

        send(client_sock, buf, sizeof(buf), 0); /* 단일 send */
        send_response = false;                  /* 공통 루틴 skip */
        break;
      }
      case MSG_TYPE_LOGOUT_REQ: {
        LogoutResponse resp_data;
        if (strlen(current_user) > 0) {
          printf("[SERVER_NETWORK] User %s logged out from socket %d.\n", current_user, client_sock);
          // 로그인 목록에서 제거
          remove_logged_in_user(current_user);
          memset(current_user, 0, sizeof(current_user));
          resp_data.success = 1;
          strncpy(resp_data.message, "Logged out successfully.", MAX_MSG_LEN - 1);
        } else {
          resp_data.success = 0;
          strncpy(resp_data.message, "Not logged in, cannot log out.", MAX_MSG_LEN - 1);
        }
        resp_data.message[MAX_MSG_LEN - 1] = '\0';
        resp_header.type = MSG_TYPE_LOGOUT_RESP;
        response_data_len = sizeof(LogoutResponse);
        memcpy(resp_data_ptr, &resp_data, response_data_len);
        break;
      }

      default: {
        ErrorResponse err_resp;
        snprintf(err_resp.message, MAX_MSG_LEN, "Unknown or unsupported message type: %d", header->type);
        printf("[SERVER_NETWORK] Error on socket %d: %s\n", client_sock, err_resp.message);
        resp_header.type = MSG_TYPE_ERROR;
        response_data_len = sizeof(ErrorResponse);
        memcpy(resp_data_ptr, &err_resp, response_data_len);
        break;
      }
    }

    if (send_response) {
      resp_header.length = response_data_len;
      memcpy(response_buffer, &resp_header, sizeof(MessageHeader));
      if (send(client_sock, response_buffer, sizeof(MessageHeader) + response_data_len, 0) == -1) {
        if (errno == EINTR) {
          printf("[SERVER_NETWORK] send on socket %d interrupted. Assuming shutdown.\n", client_sock);
        } else {
          printf("[SERVER_NETWORK] send() error on socket %d (user: %s), errno: %d\n", client_sock, strlen(current_user) > 0 ? current_user : "N/A",
                 errno);
        }
        break;
      }
    }
  }

  // 연결 종료 처리 부분(함수 끝부분)
  if (strlen(current_user) > 0) {
    printf("[SERVER_NETWORK] Cleaning up session for user %s on socket %d due to disconnect/error.\n", current_user, client_sock);
    // 로그인 목록에서 제거
    remove_logged_in_user(current_user);
  }
  close(client_sock);
  return NULL;
}