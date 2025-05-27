// client/src/auth_ui.c
#include "auth_ui.h"

#include <ctype.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#include "client_globals.h"
#include "client_network.h"
#include "hash_util.h" /* 암호화 유틸리티 추가 */
#include "protocol.h"

/* client_main.c에서 선언된 함수 */
void wait_for_key_or_signal(int y, int x, const char* prompt);

void draw_auth_menu() {
  clear();
  mvprintw(Y_TITLE, X_DEFAULT_POS, "=== Welcome to Rain Typing Game ===");
  mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "1. Login");
  mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "2. Register");
  mvprintw(Y_OPTIONS_START + 2, X_DEFAULT_POS, "3. Exit");
  mvprintw(Y_OPTIONS_START + 4, X_DEFAULT_POS, "Select an option: ");
  refresh();
}

/* 사용자 입력을 위한 커스텀 함수 (SIGINT 및 ncurses timeout 처리) */
static int custom_get_string(const char* label, char* buffer, int max_len, bool is_password) {
  int current_y = Y_INPUT_FIELD;
  int label_x = X_DEFAULT_POS;
  int input_start_x = label_x + strlen(label) + 2; /* "ID: " 또는 "PW: " */
  int ch, i = 0;

  buffer[0] = '\0'; /* 버퍼 초기화 */

  if (!is_password) {
    echo(); /* 일반 입력에서는 echo 활성화 */
  } /* 비밀번호에서는 noecho() 기본값 유지 */

  /* 라벨 출력 및 입력 라인 정리 */
  mvprintw(current_y, label_x, "%s: ", label);
  move(current_y, input_start_x);
  clrtoeol();
  refresh();

  while (i < max_len - 1) {
    if (sigint_received) {
      if (!is_password) noecho(); /* echo가 활성화되었다면 복원 */
      return -1;                  /* 인터럽트됨 */
    }

    ch = getch(); /* 전역 timeout(100) 활용 */

    if (ch == ERR) { /* 타임아웃 또는 입력 없음 */
      continue;
    }

    if (ch == '\n' || ch == KEY_ENTER) {
      break; /* 입력 종료 */
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (i > 0) {
        i--;
        /* 커서 이동, 공백 출력, 커서 재이동 */
        mvaddch(current_y, input_start_x + i, ' ');
        move(current_y, input_start_x + i);
      }
    } else if (isprint(ch)) { /* 출력 가능한 문자 처리 */
      buffer[i++] = ch;
      if (is_password) {
        mvaddch(current_y, input_start_x + i - 1, '*');
      } else { /* 일반 echo는 ncurses echo()에서 처리 */
        mvaddch(current_y, input_start_x + i - 1, ch);
      }
    }
    refresh(); /* 각 문자 후 새로고침 */
  }
  buffer[i] = '\0'; /* null 종료 */

  if (!is_password) {
    noecho(); /* 일반 입력 후 noecho 복원 */
  }

  /* 입력 끝에서 라인 끝까지 정리 */
  move(current_y, input_start_x + i);
  clrtoeol();
  refresh();

  return 0; /* 성공 */
}

void input_plain(const char* label, char* buffer, int max_len) {
  if (custom_get_string(label, buffer, max_len, false) == -1) {
    /* SIGINT 수신됨, auth_ui_main에서 전역 플래그 확인할 것 */
  }
}

void input_password(const char* label, char* buffer, int max_len) {
  if (custom_get_string(label, buffer, max_len, true) == -1) {
    /* SIGINT 수신됨, auth_ui_main에서 전역 플래그 확인할 것 */
  }
}

/*
 * 비밀번호를 해싱하는 보안 함수
 * 입력: 평문 비밀번호
 * 출력: 해시된 비밀번호 (MAX_PW_LEN 크기 버퍼에 저장)
 */
static int hash_user_password(const char* plain_password, char* hashed_password) {
  if (!plain_password || !hashed_password) {
    return 0;
  }

  /* 암호화 시스템 초기화 확인 */
  if (!crypto_init()) {
    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Crypto system initialization failed.");
    return 0;
  }

  /* SHA-256 해싱 수행 */
  if (!hash_password_sha256(plain_password, hashed_password)) {
    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Password hashing failed.");
    return 0;
  }

  return 1;
}

int auth_ui_main(char* logged_in_user) {
  char id[MAX_ID_LEN], pw_plain[64], pw_hashed[MAX_PW_LEN];
  int choice_char;
  int choice;

  logged_in_user[0] = '\0'; /* logged_in_user 초기화 */

  /* 암호화 시스템 초기화 */
  if (!crypto_init()) {
    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Failed to initialize crypto system.");
    clrtoeol();
    wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "Press any key to exit...");
    return AUTH_UI_EXIT_SIGNAL;
  }

  while (1) {
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL;

    draw_auth_menu();
    choice_char = getch();

    if (choice_char == ERR) { /* 타임아웃, 입력 없음 */
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
      continue;
    }

    if (choice_char >= '1' && choice_char <= '3') {
      choice = choice_char - '0';
    } else {
      choice = -1; /* 무효한 입력 */
    }

    if (choice == 3) return AUTH_UI_EXIT_NORMAL; /* 사용자가 '종료' 선택 */
    if (choice != 1 && choice != 2) {
      mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Invalid option. Please try again.");
      clrtoeol();
      wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, ""); /* 대기만 */
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
      continue;
    }

    clear();
    mvprintw(Y_TITLE, X_DEFAULT_POS, (choice == 1) ? "=== LOGIN ===" : "=== REGISTER ===");

    input_plain("ID", id, MAX_ID_LEN);
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL; /* input_plain 후 확인 */

    input_password("PW", pw_plain, sizeof(pw_plain));
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL; /* input_password 후 확인 */

    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Processing...");
    clrtoeol();
    refresh();

    /* 입력된 평문 비밀번호를 해싱 */
    if (!hash_user_password(pw_plain, pw_hashed)) {
      mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Password encryption failed.");
      clrtoeol();
      wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "Press any key to continue...");
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
      continue;
    }

    /* 평문 비밀번호 메모리에서 제거 (보안) */
    memset(pw_plain, 0, sizeof(pw_plain));

    char response_message_buffer[MAX_MSG_LEN * 2]; /* 조합된 메시지용 */

    if (choice == 1) { /* 로그인 */
      LoginResponse res;
      int ret = send_login_request(id, pw_hashed, &res);
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;

      if (ret == 0 && res.success) {
        strncpy(logged_in_user, id, MAX_ID_LEN - 1);
        logged_in_user[MAX_ID_LEN - 1] = '\0';
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Login Success! %s", res.message);
        mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "%s", response_message_buffer);
        clrtoeol();
        wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "Press any key...");
        if (sigint_received) return AUTH_UI_EXIT_SIGNAL;

        /* 해시된 비밀번호 메모리에서 제거 */
        memset(pw_hashed, 0, sizeof(pw_hashed));
        return AUTH_UI_LOGIN_SUCCESS;
      } else {
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Login Failed: %s (ret: %d)",
                 (ret != 0 ? "Network/Comm error" : res.message), ret);
      }
    } else if (choice == 2) { /* 회원가입 */
      RegisterResponse res;
      int ret = send_register_request(id, pw_hashed, &res);
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;

      if (ret == 0 && res.success) {
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Register Success! %s Try logging in.", res.message);
      } else {
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Register Failed: %s (ret: %d)",
                 (ret != 0 ? "Network/Comm error" : res.message), ret);
      }
    }

    /* 해시된 비밀번호 메모리에서 제거 (보안) */
    memset(pw_hashed, 0, sizeof(pw_hashed));

    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "%s", response_message_buffer);
    clrtoeol();
    wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "Press any key to continue...");
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
  }
}