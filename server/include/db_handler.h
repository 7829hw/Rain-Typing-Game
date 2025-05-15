#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include "protocol.h"  // For MAX_ID_LEN, MAX_PW_LEN

#define MAX_USERS 100         // 파일에서 한 번에 읽어올 최대 사용자 수 (예시)
#define MAX_TOTAL_SCORES 500  // 파일에서 한 번에 읽어올 최대 점수 수 (예시)

// DB 내부에서 사용할 사용자 데이터 구조체
typedef struct {
  char username[MAX_ID_LEN];
  char password[MAX_PW_LEN];
} UserData;

// DB 내부에서 사용할 점수 기록 구조체
typedef struct {
  char username[MAX_ID_LEN];
  int score;
} ScoreRecord;

// DB 파일 초기화 (서버 시작 시 호출)
void init_db_files();

// 사용자 관련 함수
int find_user_in_file(const char* username, UserData* found_user);
int add_user_to_file(const UserData* user);

// 점수 관련 함수
int add_score_to_file(const char* username, int score);
int load_all_scores_from_file(ScoreRecord scores[], int max_records);

#endif  // DB_HANDLER_H