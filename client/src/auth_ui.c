// client/src/auth_ui.c

#include "auth_ui.h"

#include <ncursesw/curses.h>
#include <stdlib.h>
#include <string.h>

#include "network_client.h"
#include "protocol.h"

void draw_auth_menu() {
  clear();
  mvprintw(3, 10, "=== Welcome to Rain Typing Game ===");
  mvprintw(5, 10, "1. Login");
  mvprintw(6, 10, "2. Register");
  mvprintw(7, 10, "3. Exit");
  mvprintw(9, 10, "Select an option: ");
  refresh();
}

void input_plain(const char* label, char* buffer, int max_len) {
  // ID 입력은 11행을 사용한다고 가정
  int current_y = 11;
  int label_x = 10;

  // (선택적 개선) 입력을 시작하기 전에, 해당 라인을 깨끗하게 만들 수 있습니다.
  // move(current_y, label_x); // 레이블 시작 위치로 이동
  // clrtoeol();               // 거기서부터 라인 끝까지 지움

  echo();                                       // 일반 입력이므로 화면에 보이도록 설정
  mvprintw(current_y, label_x, "%s: ", label);  // 레이블 출력
  // getnstr은 현재 커서 위치에서 입력을 받습니다.
  // mvprintw 실행 후 커서는 레이블 문자열 바로 뒤에 위치합니다.
  refresh();  // 레이블을 즉시 화면에 표시

  getnstr(buffer, max_len - 1);
  buffer[max_len - 1] = '\0';  // Ensure null termination
  noecho();                    // 입력 완료 후 echo 끄기

  // 입력 후, 커서는 입력된 문자열 바로 뒤에 있습니다.
  // 그 위치부터 라인 끝까지 지웁니다. (ID 입력값 뒤의 불필요한 잔상 제거)
  // 이 clrtoeol()은 ID 입력값 자체를 지우는 것이 아니라, 그 '뒤'를 지웁니다.
  // 예를 들어 "ID: user123" 입력 후 이 clrtoeol()은 "user123" 뒤부터 지웁니다.
  clrtoeol();
  refresh();  // 지운 결과를 화면에 반영
}

void input_password(const char* label, char* buffer, int max_len) {
  // 비밀번호 입력도 ID와 동일한 11행을 사용한다고 가정
  int current_y = 11;
  int label_x = 10;
  // 실제 비밀번호 문자('*')가 입력될 시작 X 좌표
  int input_start_x = label_x + strlen(label) + 2;  // "PW: " 바로 다음 칸
  int ch, i = 0;

  // 1. "PW: " 레이블을 출력합니다.
  //    이때, 이전에 있던 "ID: " 레이블 위에 겹쳐 써집니다.
  mvprintw(current_y, label_x, "%s: ", label);

  // 2. (핵심 수정) 실제 비밀번호 문자 ('*')를 입력받기 시작할 위치(input_start_x)로 커서를 이동시키고,
  //    그 위치부터 라인의 끝까지를 깨끗하게 지웁니다.
  //    이렇게 하면 'input_plain'에서 ID를 입력했을 때 남아있을 수 있는 모든 잔상이 제거됩니다.
  move(current_y, input_start_x);
  clrtoeol();
  // clrtoeol() 후 커서는 input_start_x에 그대로 있으므로, 다시 move할 필요는 없습니다.

  noecho();  // 비밀번호 입력이므로 화면에 보이지 않도록 설정

  // 3. 비밀번호 입력 루프
  while ((ch = getch()) != '\n' && ch != KEY_ENTER && i < max_len - 1) {
    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {  // 백스페이스 처리
      if (i > 0) {
        i--;
        buffer[i] = '\0';
        // 현재 '*'가 찍힌 위치를 공백으로 덮고, 커서를 한 칸 뒤로 이동
        mvaddch(current_y, input_start_x + i, ' ');
        move(current_y, input_start_x + i);
      }
    } else if (ch >= 32 && ch <= 126) {  // 화면에 표시 가능한 문자일 경우
      buffer[i++] = ch;
      mvaddch(current_y, input_start_x + i - 1, '*');  // '*' 문자로 마스킹하여 출력
    }
    refresh();  // 각 문자 입력/삭제 시 화면 즉시 갱신
  }
  buffer[i] = '\0';  // 문자열 null 종단

  // (선택적) 입력 완료 후, 마지막 '*' 문자 뒤부터 라인 끝까지 정리할 수 있습니다.
  // 이렇게 하면 비밀번호 입력란 뒤에 혹시 다른 문자가 있었다면 지워집니다.
  move(current_y, input_start_x + i);
  clrtoeol();
  refresh();  // 정리 결과를 화면에 반영
}

int auth_ui_main(char* logged_in_user) {
  char id[MAX_ID_LEN], pw[MAX_PW_LEN];
  int choice_char;
  int choice;

  while (1) {
    draw_auth_menu();
    choice_char = getch();

    if (choice_char >= '1' && choice_char <= '3') {
      choice = choice_char - '0';
    } else {
      choice = -1;
    }

    if (choice == 3) return 0;         // Exit
    if (choice != 1 && choice != 2) {  // Invalid option
      mvprintw(13, 10, "Invalid option. Please try again.");
      clrtoeol();
      refresh();
      getch();
      continue;
    }

    clear();  // 메뉴 화면 지우고 입력 화면 준비
    mvprintw(3, 10, (choice == 1) ? "=== LOGIN ===" : "=== REGISTER ===");

    // ID 입력 (11행 사용)
    input_plain("ID", id, MAX_ID_LEN);

    // PW 입력 (11행 재사용)
    // input_password 함수 내부에서 이전 잔상을 정리하므로, 여기서는 특별한 조치 필요 없음
    input_password("PW", pw, MAX_PW_LEN);

    mvprintw(13, 10, "Processing...");  // 처리 중 메시지
    clrtoeol();                         // 메시지 뒤쪽 정리
    refresh();

    if (choice == 1) {  // Login
      LoginResponse res;
      int ret = send_login_request(id, pw, &res);
      if (ret == 0 && res.success) {
        strcpy(logged_in_user, id);
        mvprintw(13, 10, "Login Success! User: %s. Press any key...", res.message);
        clrtoeol();
        refresh();
        getch();
        return 1;  // 로그인 성공
      } else {
        mvprintw(13, 10, "Login Failed: %s (ret: %d)", (ret != 0 ? "Network/Comm error" : res.message), ret);
      }
    } else if (choice == 2) {  // Register
      RegisterResponse res;
      int ret = send_register_request(id, pw, &res);
      if (ret == 0 && res.success) {
        mvprintw(13, 10, "Register Success! %s Try logging in.", res.message);
      } else {
        mvprintw(13, 10, "Register Failed: %s (ret: %d)", (ret != 0 ? "Network/Comm error" : res.message), ret);
      }
    }

    // 결과 메시지 뒤쪽 정리 및 다음 입력 대기
    clrtoeol();
    refresh();
    mvprintw(14, 10, "Press any key to continue...");
    refresh();
    getch();
  }
}