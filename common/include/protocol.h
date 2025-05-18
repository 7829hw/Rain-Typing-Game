// common/include/protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_ID_LEN 20
#define MAX_PW_LEN 20
#define MAX_MSG_LEN 100
#define MAX_LEADERBOARD_ENTRIES 10

typedef enum {
  MSG_TYPE_REGISTER_REQ,
  MSG_TYPE_LOGIN_REQ,
  MSG_TYPE_SCORE_SUBMIT_REQ,
  MSG_TYPE_LEADERBOARD_REQ,
  MSG_TYPE_LOGOUT_REQ,

  MSG_TYPE_REGISTER_RESP,
  MSG_TYPE_LOGIN_RESP,
  MSG_TYPE_SCORE_SUBMIT_RESP,
  MSG_TYPE_LEADERBOARD_RESP,
  MSG_TYPE_LOGOUT_RESP,

  MSG_TYPE_ERROR
} MessageType;

typedef struct {
  MessageType type;
  int length;
} MessageHeader;

typedef struct {
  char username[MAX_ID_LEN];
  char password[MAX_PW_LEN];
} RegisterRequest;

typedef struct {
  char username[MAX_ID_LEN];
  char password[MAX_PW_LEN];
} LoginRequest;

typedef struct {
  int score;
} ScoreSubmitRequest;

typedef struct {
  int success;
  char message[MAX_MSG_LEN];
} RegisterResponse;

typedef struct {
  int success;
  char message[MAX_MSG_LEN];
} LoginResponse;

typedef struct {
  int success;
  char message[MAX_MSG_LEN];
} ScoreSubmitResponse;

typedef struct {
  char username[MAX_ID_LEN];
  int score;
} LeaderboardEntry;

typedef struct {
  int count;
  LeaderboardEntry entries[MAX_LEADERBOARD_ENTRIES];
  char message[MAX_MSG_LEN];
} LeaderboardResponse;

typedef struct {
  int success;
  char message[MAX_MSG_LEN];
} LogoutResponse;

typedef struct {
  char message[MAX_MSG_LEN];
} ErrorResponse;

#endif  // PROTOCOL_H