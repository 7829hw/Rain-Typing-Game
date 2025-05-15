#include "auth.h"

#include <stdio.h>
#include <string.h>

#include "db_handler.h"  // DB 핸들러 사용

// init_auth_system은 main_server.c에서 init_db_files()가 먼저 호출된 후 호출됨
void init_auth_system() {
  printf("[AUTH] Auth system initialized (using file DB).\n");
  // 필요한 경우, 여기서 사용자 목록을 메모리로 로드할 수 있으나,
  // 여기서는 각 요청 시 파일 직접 접근.
}

int register_user_impl(const char* username, const char* password, char* response_msg) {
  if (username == NULL || password == NULL || strlen(username) == 0 || strlen(password) == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "Username and password cannot be empty.");
    return 0;  // 실패
  }
  if (strlen(username) >= MAX_ID_LEN || strlen(password) >= MAX_PW_LEN) {
    snprintf(response_msg, MAX_MSG_LEN, "Username or password too long.");
    return 0;  // 실패
  }

  UserData existing_user;
  int find_res = find_user_in_file(username, &existing_user);

  if (find_res == 1) {
    snprintf(response_msg, MAX_MSG_LEN, "User '%s' already exists.", username);
    return 0;  // 실패 - 사용자 이미 존재
  } else if (find_res == -1) {
    snprintf(response_msg, MAX_MSG_LEN, "Error checking user existence.");
    return 0;  // 실패 - DB 오류
  }

  UserData new_user;
  strncpy(new_user.username, username, MAX_ID_LEN - 1);
  new_user.username[MAX_ID_LEN - 1] = '\0';
  strncpy(new_user.password, password, MAX_PW_LEN - 1);  // 실제로는 해싱 필요
  new_user.password[MAX_PW_LEN - 1] = '\0';

  if (add_user_to_file(&new_user)) {
    snprintf(response_msg, MAX_MSG_LEN, "User '%s' registered successfully.", username);
    return 1;  // 성공
  } else {
    snprintf(response_msg, MAX_MSG_LEN, "Failed to save new user '%s'.", username);
    return 0;  // 실패 - 저장 실패
  }
}

int login_user_impl(const char* username, const char* password, char* response_msg, char* logged_in_user) {
  if (username == NULL || password == NULL) {
    snprintf(response_msg, MAX_MSG_LEN, "Username or password cannot be null.");
    return 0;
  }

  UserData user_from_db;
  int find_res = find_user_in_file(username, &user_from_db);

  if (find_res == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "User '%s' not found.", username);
    return 0;  // 실패 - 사용자 없음
  } else if (find_res == -1) {
    snprintf(response_msg, MAX_MSG_LEN, "Error finding user '%s'.", username);
    return 0;  // 실패 - DB 오류
  }

  // 비밀번호 비교 (평문 비교, 실제로는 해시 비교)
  if (strcmp(user_from_db.password, password) == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "Login successful for '%s'.", username);
    strncpy(logged_in_user, username, MAX_ID_LEN - 1);
    logged_in_user[MAX_ID_LEN - 1] = '\0';
    return 1;  // 성공
  } else {
    snprintf(response_msg, MAX_MSG_LEN, "Invalid password for user '%s'.", username);
    return 0;  // 실패 - 비밀번호 불일치
  }
}