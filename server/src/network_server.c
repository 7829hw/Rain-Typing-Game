#include "network_server.h"  // 자신의 헤더 (handle_client 선언)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"  // 공통 프로토콜 헤더

// 개발자 B가 실제 구현할 함수들에 대한 헤더 포함
// 이 헤더들을 통해 함수 선언을 가져오고, 실제 구현은 auth.o, score_manager.o 등에서 링크됨
#include "auth.h"
#include "score_manager.h"

/*
// ===================================================================================
// 다음 함수들의 정의는 Makefile에 의해 생성되는 더미 auth.c, score_manager.c 또는
// 개발자 B가 제공할 실제 파일에 있어야 합니다.
// network_server.c 에는 이 함수들의 정의가 있으면 안 됩니다. (중복 정의 오류 발생)
// ===================================================================================

// auth.c 에 있어야 할 함수들 (여기서는 주석 처리 또는 삭제)
// int register_user_impl(const char* username, const char* password, char* response_msg) {
//     printf("[SERVER-AUTH-TEMP] Registering user: %s\n", username);
//     snprintf(response_msg, MAX_MSG_LEN, "User %s registered successfully (temp).", username);
//     return 1; // 성공
// }

// int login_user_impl(const char* username, const char* password, char* response_msg, char* logged_in_user) {
//     printf("[SERVER-AUTH-TEMP] Logging in user: %s\n", username);
//     if (strcmp(password, "pass") == 0) {
//         snprintf(response_msg, MAX_MSG_LEN, "User %s logged in successfully (temp).", username);
//         strncpy(logged_in_user, username, MAX_ID_LEN-1);
//         logged_in_user[MAX_ID_LEN-1] = '\0';
//         return 1; // 성공
//     } else {
//         snprintf(response_msg, MAX_MSG_LEN, "Invalid credentials for %s (temp).", username);
//         return 0; // 실패
//     }
// }

// score_manager.c 에 있어야 할 함수들 (여기서는 주석 처리 또는 삭제)
// int submit_score_impl(const char* username, int score, char* response_msg) {
//     printf("[SERVER-SCORE-TEMP] User %s submitted score: %d\n", username, score);
//     snprintf(response_msg, MAX_MSG_LEN, "Score %d submitted for %s (temp).", score, username);
//     return 1; // 성공
// }

// void get_leaderboard_impl(LeaderboardEntry* entries, int* count, int max_entries) {
//     printf("[SERVER-SCORE-TEMP] Getting leaderboard\n");
//     *count = 2;
//     if (*count > 0 && max_entries > 0) {
//         strncpy(entries[0].username, "Player1_temp", MAX_ID_LEN-1); entries[0].username[MAX_ID_LEN-1] = '\0';
//         entries[0].score = 1000;
//     }
//     if (*count > 1 && max_entries > 1) {
//         strncpy(entries[1].username, "Player2_temp", MAX_ID_LEN-1); entries[1].username[MAX_ID_LEN-1] = '\0';
//         entries[1].score = 900;
//     }
// }
*/

void* handle_client(void* arg) {
  int client_sock = *((int*)arg);
  free(arg);

  char buffer[1024];
  ssize_t received_bytes;
  char current_user[MAX_ID_LEN] = {0};

  // 초기화 함수 호출 (개발자 B의 모듈이 실제 파일로 대체되면 이 호출이 의미있어짐)
  // init_auth_system(); // 이 함수들은 main_server.c 에서 한번만 호출하는 것이 적절할 수 있음
  // init_score_system();

  while ((received_bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
    buffer[received_bytes] = '\0';

    MessageHeader* header = (MessageHeader*)buffer;
    printf("[SERVER] Received msg type: %d from socket %d\n", header->type, client_sock);

    char response_buffer[1024];
    MessageHeader resp_header;
    int response_data_len = 0;

    switch (header->type) {
      case MSG_TYPE_REGISTER_REQ: {
        RegisterRequest* req = (RegisterRequest*)(buffer + sizeof(MessageHeader));
        RegisterResponse resp_data;
        // 이제 register_user_impl은 auth.o (더미 또는 실제)에서 링크됨
        resp_data.success = register_user_impl(req->username, req->password, resp_data.message);

        resp_header.type = MSG_TYPE_REGISTER_RESP;
        response_data_len = sizeof(RegisterResponse);
        memcpy(response_buffer + sizeof(MessageHeader), &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_LOGIN_REQ: {
        LoginRequest* req = (LoginRequest*)(buffer + sizeof(MessageHeader));
        LoginResponse resp_data;
        // 이제 login_user_impl은 auth.o (더미 또는 실제)에서 링크됨
        resp_data.success = login_user_impl(req->username, req->password, resp_data.message, current_user);

        resp_header.type = MSG_TYPE_LOGIN_RESP;
        response_data_len = sizeof(LoginResponse);
        memcpy(response_buffer + sizeof(MessageHeader), &resp_data, response_data_len);
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
          // 이제 submit_score_impl은 score_manager.o (더미 또는 실제)에서 링크됨
          resp_data.success = submit_score_impl(current_user, req->score, resp_data.message);
        }

        resp_header.type = MSG_TYPE_SCORE_SUBMIT_RESP;
        response_data_len = sizeof(ScoreSubmitResponse);
        memcpy(response_buffer + sizeof(MessageHeader), &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_LEADERBOARD_REQ: {
        LeaderboardResponse resp_data;
        // 이제 get_leaderboard_impl은 score_manager.o (더미 또는 실제)에서 링크됨
        get_leaderboard_impl(resp_data.entries, &resp_data.count, MAX_LEADERBOARD_ENTRIES);
        if (resp_data.count == 0) {
          strncpy(resp_data.message, "Leaderboard is empty.", MAX_MSG_LEN - 1);
        } else {
          strncpy(resp_data.message, "Leaderboard data.", MAX_MSG_LEN - 1);
        }
        resp_data.message[MAX_MSG_LEN - 1] = '\0';

        resp_header.type = MSG_TYPE_LEADERBOARD_RESP;
        response_data_len = sizeof(LeaderboardResponse);
        memcpy(response_buffer + sizeof(MessageHeader), &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_LOGOUT_REQ: {
        LogoutResponse resp_data;
        printf("[SERVER] User %s logged out.\n", current_user);
        memset(current_user, 0, sizeof(current_user));
        resp_data.success = 1;
        strncpy(resp_data.message, "Logged out successfully.", MAX_MSG_LEN - 1);
        resp_data.message[MAX_MSG_LEN - 1] = '\0';

        resp_header.type = MSG_TYPE_LOGOUT_RESP;
        response_data_len = sizeof(LogoutResponse);
        memcpy(response_buffer + sizeof(MessageHeader), &resp_data, response_data_len);
        break;
      }
      default: {
        ErrorResponse err_resp;
        snprintf(err_resp.message, MAX_MSG_LEN, "Unknown message type: %d", header->type);
        printf("[SERVER] Error: %s\n", err_resp.message);

        resp_header.type = MSG_TYPE_ERROR;
        response_data_len = sizeof(ErrorResponse);
        memcpy(response_buffer + sizeof(MessageHeader), &err_resp, response_data_len);
        break;
      }
    }

    resp_header.length = response_data_len;
    memcpy(response_buffer, &resp_header, sizeof(MessageHeader));
    if (send(client_sock, response_buffer, sizeof(MessageHeader) + response_data_len, 0) == -1) {
      perror("send() error in handle_client");
      break;  // 클라이언트와 통신 불가, 루프 종료
    }
  }

  if (received_bytes == 0) {
    printf("Client disconnected: socket %d (user: %s)\n", client_sock, strlen(current_user) > 0 ? current_user : "N/A");
  } else if (received_bytes == -1) {
    perror("recv() error from client in handle_client");
  }

  close(client_sock);
  printf("Thread for socket %d finished.\n", client_sock);
  return NULL;
}