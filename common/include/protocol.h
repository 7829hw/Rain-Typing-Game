#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_ID_LEN 20
#define MAX_PW_LEN 20
#define MAX_MSG_LEN 100
#define MAX_LEADERBOARD_ENTRIES 10

// 메시지 타입 정의
typedef enum {
  // 클라이언트 -> 서버 요청
  MSG_TYPE_REGISTER_REQ,
  MSG_TYPE_LOGIN_REQ,
  MSG_TYPE_SCORE_SUBMIT_REQ,
  MSG_TYPE_LEADERBOARD_REQ,
  MSG_TYPE_LOGOUT_REQ,

  // 서버 -> 클라이언트 응답
  MSG_TYPE_REGISTER_RESP,
  MSG_TYPE_LOGIN_RESP,
  MSG_TYPE_SCORE_SUBMIT_RESP,
  MSG_TYPE_LEADERBOARD_RESP,
  MSG_TYPE_LOGOUT_RESP,

  MSG_TYPE_ERROR
} MessageType;

// 공통 헤더
typedef struct {
  MessageType type;
  int length;
} MessageHeader;

// --- 요청 구조체 ---
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

// --- 응답 구조체 ---
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
  int count;  // -1 on error fetching, 0 if empty, >0 for data
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