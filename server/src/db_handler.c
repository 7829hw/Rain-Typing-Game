// server/src/db_handler.c (일부, 디렉토리 생성 관련)

#include "db_handler.h"

#include <errno.h>  // For errno to check mkdir errors
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>   // For stat, mkdir
#include <sys/types.h>  // For mkdir (on some systems)

// 데이터 디렉토리 및 파일 경로 정의
#define DATA_DIR_PATH "data"  // 프로젝트 루트 바로 아래 "data"
#define USERS_FILE_PATH DATA_DIR_PATH "/users.txt"
#define SCORES_FILE_PATH DATA_DIR_PATH "/scores.txt"

// 디렉토리 생성 함수
static int create_directory_if_not_exists(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == 0) {   // 경로 존재
    if (S_ISDIR(st.st_mode)) {  // 그리고 디렉토리임
      return 0;                 // 성공 (이미 존재)
    } else {
      fprintf(stderr, "[DB] Error: Path exists but is not a directory: %s\n", path);
      return -1;  // 실패
    }
  } else {                         // 경로 존재하지 않음
    if (mkdir(path, 0755) == 0) {  // 생성 시도
      printf("[DB] Directory created: %s\n", path);
      return 0;               // 성공 (생성됨)
    } else {                  // 생성 실패
      if (errno == EEXIST) {  // 혹시 다른 프로세스가 동시에 만들었거나 stat이 잘못된 경우
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
          return 0;  // 사실은 이미 존재했던 것
        }
      }
      perror("[DB] mkdir failed");
      fprintf(stderr, "[DB] Failed to create directory: %s (errno: %d)\n", path, errno);
      return -1;  // 실패
    }
  }
}

void init_db_files() {
  // 1. "data" 디렉토리 존재 확인 및 생성 시도
  if (create_directory_if_not_exists(DATA_DIR_PATH) != 0) {
    fprintf(stderr, "[DB] Critical: Could not ensure data directory %s exists. File operations may fail.\n", DATA_DIR_PATH);
    // 필요시 프로그램 종료 처리: exit(EXIT_FAILURE);
  }

  // 2. users.txt 파일 확인/생성 (이하 동일)
  FILE *fp_users = fopen(USERS_FILE_PATH, "a");
  if (fp_users == NULL) {
    perror("[DB] Failed to open/create users.txt");
  } else {
    fclose(fp_users);
  }

  FILE *fp_scores = fopen(SCORES_FILE_PATH, "a");
  if (fp_scores == NULL) {
    perror("[DB] Failed to open/create scores.txt");
  } else {
    fclose(fp_scores);
  }
  printf("[DB] Checked/Initialized data files: %s, %s\n", USERS_FILE_PATH, SCORES_FILE_PATH);
}

// 파일에서 특정 사용자 찾기
// 성공 시 1, 사용자 없음 0, 오류 시 -1 반환 (또는 파일 없음 시 0 반환)
int find_user_in_file(const char *username, UserData *found_user) {
  FILE *fp = fopen(USERS_FILE_PATH, "r");
  if (fp == NULL) {
    // 파일이 없는 것은 정상이므로 perror는 주석 처리하거나, DEBUG 레벨 로그로 변경
    // perror("find_user_in_file: fopen users.txt");
    return 0;  // 사용자 없음 (파일이 없거나, 있어도 해당 사용자 없음)
  }

  UserData current_user;
  int found = 0;
  // 파일 형식: username:password\n
  while (fscanf(fp, "%19[^:]:%19[^\n]\n", current_user.username, current_user.password) == 2) {
    if (strcmp(current_user.username, username) == 0) {
      if (found_user != NULL) {
        strncpy(found_user->username, current_user.username, MAX_ID_LEN - 1);
        found_user->username[MAX_ID_LEN - 1] = '\0';
        strncpy(found_user->password, current_user.password, MAX_PW_LEN - 1);
        found_user->password[MAX_PW_LEN - 1] = '\0';
      }
      found = 1;
      break;
    }
  }
  fclose(fp);
  return found;
}

// 파일에 새 사용자 추가 (append)
// 성공 시 1, 실패 시 0 반환
int add_user_to_file(const UserData *user) {
  // init_db_files()에서 디렉토리 생성을 시도했으므로,
  // 여기서는 파일 열기만 시도합니다.
  FILE *fp = fopen(USERS_FILE_PATH, "a");
  if (fp == NULL) {
    perror("[DB] add_user_to_file: fopen users.txt");
    return 0;  // 실패
  }
  fprintf(fp, "%s:%s\n", user->username, user->password);
  fclose(fp);
  return 1;  // 성공
}

// 파일에 새 점수 추가 (append)
// 성공 시 1, 실패 시 0 반환
int add_score_to_file(const char *username, int score) {
  FILE *fp = fopen(SCORES_FILE_PATH, "a");
  if (fp == NULL) {
    perror("[DB] add_score_to_file: fopen scores.txt");
    return 0;
  }
  fprintf(fp, "%s:%d\n", username, score);
  fclose(fp);
  return 1;
}

// 파일에서 모든 점수 읽기
// 읽은 점수 개수 반환, 오류 시 0 반환 (파일 없음 포함)
int load_all_scores_from_file(ScoreRecord scores[], int max_records) {
  FILE *fp = fopen(SCORES_FILE_PATH, "r");
  if (fp == NULL) {
    // perror("[DB] load_all_scores_from_file: fopen scores.txt");
    return 0;  // 점수 파일 없음, 0개
  }

  int count = 0;
  while (count < max_records && fscanf(fp, "%19[^:]:%d\n", scores[count].username, &scores[count].score) == 2) {
    scores[count].username[MAX_ID_LEN - 1] = '\0';  // 안전장치
    count++;
  }
  fclose(fp);
  return count;
}