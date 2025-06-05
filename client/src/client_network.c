// client/src/client_network.c
#include "client_network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_globals.h"
#include "protocol.h"

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

// 정확한 바이트 수만큼 송신하는 함수
static int send_all(int sock, const void* buf, size_t len) {
  size_t total_sent = 0;
  const char* ptr = (const char*)buf;

  while (total_sent < len) {
    if (sigint_received) return -10;

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
    if (sigint_received) return -10;

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

static int send_request_and_receive_response(MessageType type, const void* request_body, int request_body_len, MessageType expected_resp_type,
                                             void* response_body, int response_body_max_len) {
  if (client_sock == -1) {
    return -1;
  }
  if (sigint_received) return -10;

  // 요청 헤더 준비
  MessageHeader req_header;
  req_header.type = type;
  req_header.length = request_body_len;

  // 헤더 전송
  if (send_all(client_sock, &req_header, sizeof(MessageHeader)) != 0) {
    disconnect_from_server();
    return -2;
  }
  if (sigint_received) return -10;

  // 바디 전송 (있는 경우)
  if (request_body && request_body_len > 0) {
    if (send_all(client_sock, request_body, request_body_len) != 0) {
      disconnect_from_server();
      return -2;
    }
  }
  if (sigint_received) return -10;

  // 응답 헤더 수신
  MessageHeader resp_header;
  if (recv_all(client_sock, &resp_header, sizeof(MessageHeader)) != 0) {
    disconnect_from_server();
    return -3;
  }
  if (sigint_received) return -10;

  // 에러 응답 처리
  if (resp_header.type == MSG_TYPE_ERROR) {
    if (resp_header.length > 0 && resp_header.length <= sizeof(ErrorResponse)) {
      ErrorResponse err_resp;
      if (recv_all(client_sock, &err_resp, resp_header.length) == 0) {
        if (response_body && (size_t)response_body_max_len >= sizeof(ErrorResponse)) {
          snprintf(((struct {
                     int success;
                     char message[MAX_MSG_LEN];
                   }*)response_body)
                       ->message,
                   MAX_MSG_LEN, "%s", err_resp.message);
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
      }
    }
    return -4;
  }

  // 예상한 응답 타입인지 확인
  if (resp_header.type != expected_resp_type) {
    printf("[CLIENT_NETWORK] Expected response type %d, but got %d\n", expected_resp_type, resp_header.type);
    // 응답 바디가 있다면 버퍼를 비워줘야 함
    if (resp_header.length > 0) {
      char discard_buffer[1024];
      size_t remaining = resp_header.length;
      while (remaining > 0) {
        size_t to_read = (remaining > sizeof(discard_buffer)) ? sizeof(discard_buffer) : remaining;
        if (recv_all(client_sock, discard_buffer, to_read) != 0) {
          disconnect_from_server();
          break;
        }
        remaining -= to_read;
      }
    }
    return -5;
  }

  // 응답 바디 크기 확인
  if (resp_header.length > response_body_max_len) {
    printf("[CLIENT_NETWORK] Response body too large: %d > %d\n", resp_header.length, response_body_max_len);
    return -6;
  }

  // 응답 바디 수신
  if (resp_header.length > 0) {
    if (recv_all(client_sock, response_body, resp_header.length) != 0) {
      disconnect_from_server();
      return -3;
    }
  }

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

int send_wordlist_request(WordListResponse* resp) {
  return send_request_and_receive_response(MSG_TYPE_WORDLIST_REQ, NULL, 0, MSG_TYPE_WORDLIST_RESP, resp, sizeof(WordListResponse));
}