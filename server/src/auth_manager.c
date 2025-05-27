

#include "auth_manager.h"
#include "db_handler.h"
#include "hash_util.h"  // 해시 함수 추가

#include <stdio.h>
#include <string.h>

// 인증 시스템 초기화
void init_auth_system() {
    printf("[AUTH_MANAGER] Auth system initialized (with SHA-256).\n");
}

// 회원가입 처리
int register_user_impl(const char* username, const char* password, char* response_msg) {
    if (!username || !password || strlen(username) == 0 || strlen(password) == 0) {
        snprintf(response_msg, MAX_MSG_LEN, "Username and password cannot be empty.");
        return 0;
    }

    UserData existing;
    if (find_user_in_file(username, &existing) == 1) {
        snprintf(response_msg, MAX_MSG_LEN, "User '%s' already exists.", username);
        return 0;
    }

    UserData new_user;
    strncpy(new_user.username, username, MAX_ID_LEN);
    strncpy(new_user.password, password, MAX_PW_LEN);  // 평문 입력 저장 (해시 전용도 사용)

    // add_user_to_file 내부에서 해싱 처리함
    if (add_user_to_file(&new_user)) {
        snprintf(response_msg, MAX_MSG_LEN, "User '%s' registered successfully.", username);
        return 1;
    }
    else {
        snprintf(response_msg, MAX_MSG_LEN, "Failed to register user '%s'.", username);
        return 0;
    }
}

// 로그인 처리
int login_user_impl(const char* username, const char* password, char* response_msg, char* logged_in_user) {
    if (!username || !password) {
        snprintf(response_msg, MAX_MSG_LEN, "Username or password cannot be null.");
        return 0;
    }

    UserData stored_user;
    if (find_user_in_file(username, &stored_user) != 1) {
        snprintf(response_msg, MAX_MSG_LEN, "User '%s' not found.", username);
        return 0;
    }

    // 입력 비밀번호 해싱
    char hashed_input[65];
    compute_sha256(password, hashed_input);

    // 해시값 비교
    if (strcmp(stored_user.password, hashed_input) == 0) {
        snprintf(response_msg, MAX_MSG_LEN, "Login successful for '%s'.", username);
        strncpy(logged_in_user, username, MAX_ID_LEN);
        return 1;
    }
    else {
        snprintf(response_msg, MAX_MSG_LEN, "Invalid password.");
        return 0;
    }
}