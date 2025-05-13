// client/src/auth_ui.c
#include <ncursesw/curses.h>     // 텍스트 기반 UI를 그리는 ncurses 라이브러리
#include <string.h>              // 문자열 함수 사용 (strcpy, strlen 등)
#include <stdlib.h>              // 표준 라이브러리
#include "network_client.h"      // 서버와 통신하는 클라이언트용 네트워크 API 헤더
#include "../common/protocol.h"  // 로그인/회원가입 관련 구조체 정의 포함

// 로그인/회원가입 메뉴를 출력하는 함수
void draw_auth_menu() {
    clear();  // 화면 초기화
    mvprintw(3, 10, "=== Welcome to Rain Typing Game ===");
    mvprintw(5, 10, "1. Login");
    mvprintw(6, 10, "2. Register");
    mvprintw(7, 10, "3. Exit");
    mvprintw(9, 10, "Select an option: ");
    refresh();  // 화면 업데이트
}

// 일반 입력 (ID 입력 보이게)
void input_plain(const char* label, char* buffer, int max_len) {
    echo();  // 입력이 보이도록 설정 (비밀번호 입력이 아니기 때문에 허용)
    mvprintw(11, 10, "%s: ", label);
    getnstr(buffer, max_len - 1); // 사용자 입력
    noecho();
}

// 비밀번호 입력 (마스킹 *)
void input_password(const char* label, char* buffer, int max_len) {
    int x = 11, y = 10 + strlen(label) + 2;  // 시작 위치
    int ch, i = 0;

    mvprintw(11, 10, "%s: ", label);
    move(x, y);
    noecho();

    while ((ch = getch()) != '\n' && i < max_len - 1) {
        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                mvaddch(x, y + i, ' '); // 지운 것처럼 보이게
                move(x, y + i);
            }
        }
        else if (ch >= 32 && ch <= 126) {
            buffer[i++] = ch;
            mvaddch(x, y + i - 1, '*');  // 마스킹 문자
        }
        refresh();
    }
    buffer[i] = '\0';
}
// 로그인/회원가입 UI 루프를 실행하고 로그인 성공 시 user ID를 저장하여 반환
// return 값: 1 = 로그인 성공, 0 = 프로그램 종료 선택
int auth_ui_main(char* logged_in_user) {
    char id[MAX_ID_LEN], pw[MAX_PW_LEN];  // 사용자 입력 받을 버퍼
    int choice;

    while (1) {
        draw_auth_menu();    // 메뉴 표시
        scanw("%d", &choice);  // 사용자로부터 선택 입력 받기(숫자가 아닌경우 위험할수 있음)

        if (choice == 3) return 0;        // 3번: 종료
        if (choice != 1 && choice != 2) continue;  // 1, 2번 외에는 무시하고 다시 루프

        // ID/PW 입력 받기
        input_plain("ID", id, MAX_ID_LEN);
        input_password("PW", pw, MAX_PW_LEN);

        if (choice == 1) {  // 로그인 처리
            LoginResponse res = login_request(id, pw);  // 서버에 로그인 요청
            if (res.success) {
                // 로그인 성공 시 ID 저장 및 게임 시작 알림
                strcpy(logged_in_user, id); // main_client.c에서 아이디 전달 / 아이디 저장
                mvprintw(13, 10, "Login Success! Press any key to start the game.");
                clrtoeol();  // 기존 줄 정리
                refresh();
                getch();
                return 1;
            }
            else {
                // 실패 메시지 출력
                mvprintw(13, 10, "Login Failed: %s", res.message);
            }
        }
        else if (choice == 2) {  // 회원가입 처리
            RegisterResponse res = register_request(id, pw);  // 서버에 회원가입 요청
            if (res.success) { // 로그인과 같은 구조체
                mvprintw(13, 10, "Register Success! Try logging in.");
            }
            else {
                mvprintw(13, 10, "Register Failed: %s", res.message);
            }
        }

        refresh();
        getch();  // 결과 메시지 확인을 위해 사용자 입력 대기
    }
}
