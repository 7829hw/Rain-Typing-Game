#include "network_client.h"  // 자신이 제공할 API 헤더

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int client_sock = -1;  // 클라이언트 소켓 (모듈 전역)

int connect_to_server(const char* ip, int port) {
  if (client_sock != -1) {
    fprintf(stderr, "Already connected.\n");
    return 0;  // 이미 연결됨
  }

  client_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (client_sock == -1) {
    perror("socket() error");
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);

  if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    perror("connect() error");
    close(client_sock);
    client_sock = -1;
    return -1;
  }
  printf("Connected to server %s:%d\n", ip, port);
  return 0;  // 성공
}

void disconnect_from_server() {
  if (client_sock != -1) {
    // (선택적) 로그아웃 메시지 전송
    // LogoutResponse logout_resp;
    // send_logout_request(&logout_resp);

    close(client_sock);
    client_sock = -1;
    printf("Disconnected from server.\n");
  }
}

// 메시지 송수신을 위한 내부 헬퍼 함수
// request_body: MessageHeader를 제외한 실제 데이터
// response_body: MessageHeader를 제외한 실제 데이터가 채워질 버퍼
static int send_request_and_receive_response(MessageType type, const void* request_body, int request_body_len, MessageType expected_resp_type,
                                             void* response_body, int response_body_max_len) {
  if (client_sock == -1) {
    fprintf(stderr, "Not connected to server.\n");
    return -1;
  }

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
    perror("send() error");
    return -2;
  }

  ssize_t received_bytes = recv(client_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
  if (received_bytes <= 0) {
    perror("recv() error or server disconnected");
    disconnect_from_server();  // 연결 끊김 처리
    return -3;
  }
  recv_buffer[received_bytes] = '\0';

  MessageHeader* resp_header = (MessageHeader*)recv_buffer;
  if (resp_header->type == MSG_TYPE_ERROR) {
    ErrorResponse* err_resp = (ErrorResponse*)(recv_buffer + sizeof(MessageHeader));
    fprintf(stderr, "Server Error: %s\n", err_resp->message);
    // 오류 응답도 response_body에 복사할지 결정 필요 (예: LoginResponse의 message 필드에)
    // 여기서는 간단히 오류 코드 반환
    return -4;  // 서버에서 명시적 오류 응답
  }

  if (resp_header->type != expected_resp_type) {
    fprintf(stderr, "Unexpected response type. Expected %d, Got %d\n", expected_resp_type, resp_header->type);
    return -5;  // 예기치 않은 응답
  }

  if (resp_header->length > response_body_max_len) {
    fprintf(stderr, "Response body too large. Expected max %d, Got %d\n", response_body_max_len, resp_header->length);
    return -6;
  }

  memcpy(response_body, recv_buffer + sizeof(MessageHeader), resp_header->length);
  return 0;  // 성공
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
  // LeaderboardRequest는 body가 없음
  return send_request_and_receive_response(MSG_TYPE_LEADERBOARD_REQ, NULL, 0, MSG_TYPE_LEADERBOARD_RESP, response, sizeof(LeaderboardResponse));
}

int send_logout_request(LogoutResponse* response) {  // 선택적
  return send_request_and_receive_response(MSG_TYPE_LOGOUT_REQ, NULL, 0, MSG_TYPE_LOGOUT_RESP, response, sizeof(LogoutResponse));
}