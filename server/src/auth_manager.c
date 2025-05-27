

#include "auth_manager.h"
#include "db_handler.h"
#include "hash_util.h"  // �ؽ� �Լ� �߰�

#include <stdio.h>
#include <string.h>

// ���� �ý��� �ʱ�ȭ
void init_auth_system() {
    printf("[AUTH_MANAGER] Auth system initialized (with SHA-256).\n");
}

// ȸ������ ó��
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
    strncpy(new_user.password, password, MAX_PW_LEN);  // �� �Է� ���� (�ؽ� ���뵵 ���)

    // add_user_to_file ���ο��� �ؽ� ó����
    if (add_user_to_file(&new_user)) {
        snprintf(response_msg, MAX_MSG_LEN, "User '%s' registered successfully.", username);
        return 1;
    }
    else {
        snprintf(response_msg, MAX_MSG_LEN, "Failed to register user '%s'.", username);
        return 0;
    }
}

// �α��� ó��
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

    // �Է� ��й�ȣ �ؽ�
    char hashed_input[65];
    compute_sha256(password, hashed_input);

    // �ؽð� ��
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