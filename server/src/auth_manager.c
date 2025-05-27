// server/src/auth_manager.c
#include "auth_manager.h"

#include <stdio.h>
#include <string.h>

#include "db_handler.h"
#include "hash_util.h" /* 암호화 유틸리티 추가 */

void init_auth_system() {
  /* 암호화 시스템 초기화 */
  if (!crypto_init()) {
    fprintf(stderr, "[AUTH_MANAGER] Failed to initialize crypto system\n");
    return;
  }
  printf("[AUTH_MANAGER] Auth system initialized with crypto support (using file DB).\n");
}

int register_user_impl(const char* username, const char* hashed_password, char* response_msg) {
  if (username == NULL || hashed_password == NULL || strlen(username) == 0 || strlen(hashed_password) == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "Username and password cannot be empty.");
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 0;
  }
  if (strlen(username) >= MAX_ID_LEN || strlen(hashed_password) >= MAX_PW_LEN) {
    snprintf(response_msg, MAX_MSG_LEN, "Username or password too long.");
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 0;
  }

  UserData existing_user;
  int find_res = find_user_in_file(username, &existing_user);

  if (find_res == 1) {
    snprintf(response_msg, MAX_MSG_LEN, "User '%s' already exists.", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 0;
  } else if (find_res == -1) {
    snprintf(response_msg, MAX_MSG_LEN, "Error checking user existence (DB).");
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 0;
  }

  UserData new_user;
  strncpy(new_user.username, username, MAX_ID_LEN - 1);
  new_user.username[MAX_ID_LEN - 1] = '\0';

  /* 클라이언트에서 이미 해시된 비밀번호를 받으므로 그대로 저장 */
  strncpy(new_user.password, hashed_password, MAX_PW_LEN - 1);
  new_user.password[MAX_PW_LEN - 1] = '\0';

  if (add_user_to_file(&new_user)) {
    snprintf(response_msg, MAX_MSG_LEN, "User '%s' registered successfully.", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 1;
  } else {
    snprintf(response_msg, MAX_MSG_LEN, "Failed to save new user '%s' (DB).", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 0;
  }
}

int login_user_impl(const char* username, const char* hashed_password, char* response_msg, char* logged_in_user) {
  if (username == NULL || hashed_password == NULL) {
    snprintf(response_msg, MAX_MSG_LEN, "Username or password cannot be null.");
    response_msg[MAX_MSG_LEN - 1] = '\0';
    logged_in_user[0] = '\0';
    return 0;
  }

  UserData user_from_db;
  int find_res = find_user_in_file(username, &user_from_db);

  if (find_res == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "User '%s' not found.", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    logged_in_user[0] = '\0';
    return 0;
  } else if (find_res == -1) {
    snprintf(response_msg, MAX_MSG_LEN, "Error finding user '%s' (DB).", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    logged_in_user[0] = '\0';
    return 0;
  }

  /* 해시된 비밀번호 직접 비교 */
  /* 클라이언트와 서버 모두 동일한 해싱 알고리즘을 사용하므로 문자열 비교로 충분 */
  if (strcmp(user_from_db.password, hashed_password) == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "Login successful for '%s'.", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    strncpy(logged_in_user, username, MAX_ID_LEN - 1);
    logged_in_user[MAX_ID_LEN - 1] = '\0';
    return 1;
  } else {
    snprintf(response_msg, MAX_MSG_LEN, "Invalid password for user '%s'.", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    logged_in_user[0] = '\0';
    return 0;
  }
}