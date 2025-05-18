// client/src/client_network.c
#include "client_network.h"  // 변경된 헤더 파일 이름

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_globals.h"  // For sigint_received
#include "protocol.h"        // protocol.h는 그대로 유지

static int client_sock = -1;

int connect_to_server(const char* ip, int port) {
  if (client_sock != -1) {
    return 0;
  }

  client_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (client_sock == -1) {
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);

  if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    close(client_sock);
    client_sock = -1;
    return -1;
  }
  return 0;
}

void disconnect_from_server() {
  if (client_sock != -1) {
    close(client_sock);
    client_sock = -1;
  }
}

static int send_request_and_receive_response(MessageType type, const void* request_body, int request_body_len, MessageType expected_resp_type,
                                             void* response_body, int response_body_max_len) {
  if (client_sock == -1) {
    return -1;
  }
  if (sigint_received) return -10;

  char send_buffer[1024];
  char recv_buffer[1024];

  MessageHeader req_header;
  req_header.type = type;
  req_header.length = request_body_len;

  memcpy(send_buffer, &req_header, sizeof(MessageHeader));
  if (request_body && request_body_len > 0) {
    memcpy(send_buffer + sizeof(MessageHeader), request_body, request_body_len);
  }

  if (send(client_sock, send_buffer, sizeof(MessageHeader) + request_body_len, 0) == -1) {
    disconnect_from_server();
    return -2;
  }
  if (sigint_received) return -10;

  ssize_t received_bytes = recv(client_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
  if (sigint_received) return -10;

  if (received_bytes <= 0) {
    disconnect_from_server();
    return -3;
  }
  recv_buffer[received_bytes] = '\0';

  MessageHeader* resp_header = (MessageHeader*)recv_buffer;
  if (resp_header->type == MSG_TYPE_ERROR) {
    ErrorResponse* err_resp = (ErrorResponse*)(recv_buffer + sizeof(MessageHeader));
    if (response_body && (size_t)response_body_max_len >= sizeof(ErrorResponse)) {
      strncpy(((struct {
                int success;
                char message[MAX_MSG_LEN];
              }*)response_body)
                  ->message,
              err_resp->message, MAX_MSG_LEN - 1);
      ((struct {
        int success;
        char message[MAX_MSG_LEN];
      }*)response_body)
          ->message[MAX_MSG_LEN - 1] = '\0';
      ((struct {
        int success;
        char message[MAX_MSG_LEN];
      }*)response_body)
          ->success = 0;
    }
    return -4;
  }

  if (resp_header->type != expected_resp_type) {
    return -5;
  }

  if (resp_header->length > response_body_max_len) {
    return -6;
  }

  memcpy(response_body, recv_buffer + sizeof(MessageHeader), resp_header->length);
  return 0;
}

int send_register_request(const char* username, const char* password, RegisterResponse* response) {
  RegisterRequest req_data;
  strncpy(req_data.username, username, MAX_ID_LEN - 1);
  req_data.username[MAX_ID_LEN - 1] = '\0';
  strncpy(req_data.password, password, MAX_PW_LEN - 1);
  req_data.password[MAX_PW_LEN - 1] = '\0';

  return send_request_and_receive_response(MSG_TYPE_REGISTER_REQ, &req_data, sizeof(RegisterRequest), MSG_TYPE_REGISTER_RESP, response,
                                           sizeof(RegisterResponse));
}

int send_login_request(const char* username, const char* password, LoginResponse* response) {
  LoginRequest req_data;
  strncpy(req_data.username, username, MAX_ID_LEN - 1);
  req_data.username[MAX_ID_LEN - 1] = '\0';
  strncpy(req_data.password, password, MAX_PW_LEN - 1);
  req_data.password[MAX_PW_LEN - 1] = '\0';

  return send_request_and_receive_response(MSG_TYPE_LOGIN_REQ, &req_data, sizeof(LoginRequest), MSG_TYPE_LOGIN_RESP, response, sizeof(LoginResponse));
}

int send_score_submit_request(int score, ScoreSubmitResponse* response) {
  ScoreSubmitRequest req_data;
  req_data.score = score;
  return send_request_and_receive_response(MSG_TYPE_SCORE_SUBMIT_REQ, &req_data, sizeof(ScoreSubmitRequest), MSG_TYPE_SCORE_SUBMIT_RESP, response,
                                           sizeof(ScoreSubmitResponse));
}

int send_leaderboard_request(LeaderboardResponse* response) {
  return send_request_and_receive_response(MSG_TYPE_LEADERBOARD_REQ, NULL, 0, MSG_TYPE_LEADERBOARD_RESP, response, sizeof(LeaderboardResponse));
}

int send_logout_request(LogoutResponse* response) {
  return send_request_and_receive_response(MSG_TYPE_LOGOUT_REQ, NULL, 0, MSG_TYPE_LOGOUT_RESP, response, sizeof(LogoutResponse));
}