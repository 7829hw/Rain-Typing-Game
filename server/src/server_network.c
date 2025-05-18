// server/src/server_network.c
#include "server_network.h"  // 변경된 헤더 파일 이름

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "auth_manager.h"
#include "protocol.h"
#include "score_manager.h"

// extern volatile sig_atomic_t server_shutdown_requested; // from server_main.c

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
        resp_data.success = login_user_impl(req->username, req->password, resp_data.message, current_user);
        if (resp_data.success) {
          printf("[SERVER_NETWORK] User '%s' logged in on socket %d.\n", current_user, client_sock);
        } else {
          current_user[0] = '\0';
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
        get_leaderboard_impl(resp_data.entries, &resp_data.count, MAX_LEADERBOARD_ENTRIES);
        if (resp_data.count <= 0) {
          strncpy(resp_data.message, "Leaderboard is empty or unavailable.", MAX_MSG_LEN - 1);
        } else {
          strncpy(resp_data.message, "Leaderboard data retrieved.", MAX_MSG_LEN - 1);
        }
        resp_data.message[MAX_MSG_LEN - 1] = '\0';
        resp_header.type = MSG_TYPE_LEADERBOARD_RESP;
        response_data_len = sizeof(LeaderboardResponse);
        memcpy(resp_data_ptr, &resp_data, response_data_len);
        break;
      }
      case MSG_TYPE_LOGOUT_REQ: {
        LogoutResponse resp_data;
        if (strlen(current_user) > 0) {
          printf("[SERVER_NETWORK] User %s logged out from socket %d.\n", current_user, client_sock);
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

  if (strlen(current_user) > 0) {
    printf("[SERVER_NETWORK] Cleaning up session for user %s on socket %d due to disconnect/error.\n", current_user, client_sock);
  }
  close(client_sock);
  return NULL;
}