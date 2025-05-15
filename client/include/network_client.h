#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include "protocol.h"  // MessageType 및 구조체 사용

// 서버 연결 및 해제
int connect_to_server(const char* ip, int port);
void disconnect_from_server();

// 요청 함수들 (개발자 A가 사용)
// 각 함수는 성공 시 0, 실패 시 -1 또는 오류 코드를 반환하고,
// 응답 구조체 포인터를 통해 결과를 전달합니다.
int send_register_request(const char* username, const char* password, RegisterResponse* response);
int send_login_request(const char* username, const char* password, LoginResponse* response);
int send_score_submit_request(int score, ScoreSubmitResponse* response);
int send_leaderboard_request(LeaderboardResponse* response);
int send_logout_request(LogoutResponse* response);  // 선택적

#endif  // NETWORK_CLIENT_H