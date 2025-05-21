// client/include/client_network.h
#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#include "protocol.h"

int connect_to_server(const char* ip, int port);
void disconnect_from_server();

int send_wordlist_request(WordListResponse* resp);
int send_register_request(const char* username, const char* password, RegisterResponse* response);
int send_login_request(const char* username, const char* password, LoginResponse* response);
int send_score_submit_request(int score, ScoreSubmitResponse* response);
int send_leaderboard_request(LeaderboardResponse* response);
int send_logout_request(LogoutResponse* response);

#endif  // CLIENT_NETWORK_H