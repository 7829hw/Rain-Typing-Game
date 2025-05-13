// client/src/main_client.c
/*
로그인 → 메인 메뉴
        └── 1. Start Game        → 게임 종료 후 → 메뉴 복귀
        └── 2. View Leaderboard → 리더보드 확인 후 → 메뉴 복귀
        └── 3. Exit              → 프로그램 종료 (확인 메시지 있음)
        └── 4. Logout            → 서버에 로그아웃 요청 후 로그인 화면 복귀
*/

#include <stdio.h>                     // 표준 입출력
#include <string.h>                   // 문자열 처리 함수
#include <ncursesw/curses.h>          // 터미널 UI 라이브러리
#include "network_client.h"           // 서버 연결 및 메시지 전송 함수
#include "auth_ui.h"                  // 로그인/회원가입 UI
#include "leaderboard_ui.h"          // 리더보드 UI
#include "../common/protocol.h"      // MAX_ID_LEN 상수 (ID 길이 제한)

// 외부 정의된 게임 실행 함수
extern int rain_typing_game_main(const char* user_id);

int main() {
    while (1) {
        // 현재 로그인한 사용자 ID 저장용
        char user_id[MAX_ID_LEN] = { 0 };

        // 서버 연결 시도
        if (!connect_to_server("127.0.0.1", 12345)) {
            fprintf(stderr, "서버 연결 실패\n");
            return 1;  // 실패 시 프로그램 종료(에러)
        }

        // 로그인/회원가입 UI 실행
        // 로그인 성공 시 user_id에 ID 저장됨
        if (!auth_ui_main(user_id)) {
            disconnect_from_server();  // 서버 연결 해제
            return 0;                  // 프로그램 종료
        }

        // 로그인 후 메인 메뉴 루프 진입
        while (1) {
            clear();  // 화면 초기화
            mvprintw(3, 10, "1. Start Game");
            mvprintw(4, 10, "2. View Leaderboard");
            mvprintw(5, 10, "3. Exit");
            mvprintw(6, 10, "4. Logout");
            mvprintw(8, 10, "Select an option: ");
            refresh();  // 화면 출력

            int ch = getch();  // 사용자 입력 받기

            if (ch == '1') {
                // 게임 실행 → 점수 반환 → 서버에 점수 전송
                int score = rain_typing_game_main(user_id);
                submit_score(user_id, score); // 개발자 c
            }
            else if (ch == '2') {
                // 리더보드 화면 출력
                show_leaderboard_ui();
            }
            else if (ch == '3') {
                // 프로그램 종료 전 사용자에게 확인 요청
                clear();
                mvprintw(10, 10, "Are you sure you want to exit? (y/n): ");
                refresh();
                int confirm = getch();
                if (confirm == 'y' || confirm == 'Y') {
                    disconnect_from_server();  // 서버 연결 종료
                    return 0;  // 프로그램 종료
                }
                // 아니면 메뉴로 돌아감
            }
            else if (ch == '4') {
                int result = logout_request(user_id); // 수정: 결과 확인
                if (!result) {
                    mvprintw(10, 10, "Logout failed. Press any key to continue...");
                    clrtoeol();
                    refresh();
                    getch();
                    // 실패 시 break 안 함 → 메뉴 유지
                }
                else {
                    break;  // 내부 메뉴 루프 종료 → 로그인 UI로 돌아감
                }
            }
            
        }

        // 로그아웃 후 또는 메뉴 탈출 시 연결 종료
        disconnect_from_server();
    }

    return 0;
}
