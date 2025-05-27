#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* ─────────────────────────────────────────────
 *  공통 한계치
 * ────────────────────────────────────────────*/
#define MAX_ID_LEN                 32
#define MAX_PW_LEN                 65
#define MAX_MSG_LEN                128
#define MAX_LEADERBOARD_ENTRIES    10

/* ─────────────────────────────────────────────
 *  메시지 타입 열거
 * ────────────────────────────────────────────*/
typedef enum {
    /* 클라이언트 ↔ 서버 기본 기능 */
    MSG_TYPE_ERROR              = 0x00,

    MSG_TYPE_REGISTER_REQ       = 0x01,
    MSG_TYPE_REGISTER_RESP      = 0x02,

    MSG_TYPE_LOGIN_REQ          = 0x03,
    MSG_TYPE_LOGIN_RESP         = 0x04,

    MSG_TYPE_SCORE_SUBMIT_REQ   = 0x05,
    MSG_TYPE_SCORE_SUBMIT_RESP  = 0x06,

    MSG_TYPE_LEADERBOARD_REQ    = 0x07,
    MSG_TYPE_LEADERBOARD_RESP   = 0x08,

    MSG_TYPE_LOGOUT_REQ         = 0x09,
    MSG_TYPE_LOGOUT_RESP        = 0x0A,

    /* ────── ① 추가: 단어 리스트 송수신 ────── */
    MSG_TYPE_WORDLIST_REQ       = 0x20,
    MSG_TYPE_WORDLIST_RESP      = 0x21
} MessageType;

/* ─────────────────────────────────────────────
 *  모든 패킷 공통 헤더
 * ────────────────────────────────────────────*/
typedef struct {
    MessageType type;
    uint16_t    length;   /* 헤더 뒤 바디 길이 (byte) */
} __attribute__((packed)) MessageHeader;

/* ────────── 기본 구조체들 (변경 없음) ─────────*/
typedef struct {
    char username[MAX_ID_LEN];
    char password[MAX_PW_LEN];
} RegisterRequest;

typedef struct {
    int  success;
    char message[MAX_MSG_LEN];
} RegisterResponse;

typedef RegisterRequest  LoginRequest;
typedef RegisterResponse LoginResponse;

typedef struct {
    int score;
} ScoreSubmitRequest;

typedef RegisterResponse ScoreSubmitResponse;

typedef struct {
    char username[MAX_ID_LEN];
    int  score;
} LeaderboardEntry;

typedef struct {
    int  count;                              /* <= MAX_LEADERBOARD_ENTRIES */
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

/* ────── ② 추가: WordListResponse ──────*/
#define MAX_WORDLIST_WORDS  256
#define MAX_WORD_STR_LEN    32

typedef struct {
    int  count;                                          /* 실제 단어 수 */
    char words[MAX_WORDLIST_WORDS][MAX_WORD_STR_LEN];    /* 단어 배열 */
} WordListResponse;

#endif /* PROTOCOL_H */
