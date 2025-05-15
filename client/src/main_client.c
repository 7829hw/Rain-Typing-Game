#include <locale.h>           // For setlocale
#include <ncursesw/curses.h>  // 기본 ncurses 헤더, UTF-8 지원 포함
#include <stdio.h>
#include <stdlib.h>  // For exit, EXIT_FAILURE
#include <string.h>

#include "auth_ui.h"
#include "game_logic.h"  // 수정된 게임 로직 헤더
#include "leaderboard_ui.h"
#include "network_client.h"
#include "protocol.h"  // 공통 프로토콜

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080  // 서버 포트와 일치해야 함

// Ncurses 초기화 함수
void init_ncurses_settings() {
  setlocale(LC_ALL, "");  // 로케일 설정을 먼저 해야 유니코드 문자가 제대로 처리됨
  initscr();              // ncurses 모드 시작
  cbreak();               // 라인 버퍼링 끄기 (Enter 없이 바로 입력)
  noecho();               // 입력 문자 화면에 표시 안 함
  keypad(stdscr, TRUE);   // 특수 키(화살표 등) 사용 가능하도록 설정
  curs_set(0);            // 커서 숨기기
  timeout(-1);            // getch()를 블로킹 모드로 기본 설정 (필요에 따라 변경)
                          // 게임 루프에서는 nodelay(stdscr, TRUE) 사용

  if (has_colors()) {
    start_color();
    // 필요한 색상 쌍 정의 (예시)
    // init_pair(1, COLOR_RED, COLOR_BLACK);
    // init_pair(2, COLOR_GREEN, COLOR_BLACK);
    // init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    // init_pair(4, COLOR_BLUE, COLOR_BLACK);
    // init_pair(5, COLOR_CYAN, COLOR_BLACK);
  } else {
    // 색상 지원 안 할 경우 처리 (선택적)
    // mvprintw(LINES -1, 0, "Warning: Terminal does not support colors.");
    // getch();
  }
}

// Ncurses 종료 함수
void end_ncurses_settings() {
  endwin();  // ncurses 모드 종료
}

int main() {
  init_ncurses_settings();  // Ncurses 초기화

  char user_id[MAX_ID_LEN] = {0};  // 현재 로그인된 사용자 ID 저장

  // 메인 애플리케이션 루프 (로그인 화면으로 계속 돌아올 수 있도록)
  while (1) {
    // 서버 연결 시도
    if (connect_to_server(SERVER_IP, SERVER_PORT) != 0) {
      clear();
      mvprintw(LINES / 2, (COLS - strlen("Failed to connect to server. Press any key to exit.")) / 2,
               "Failed to connect to server. Press any key to exit.");
      refresh();
      timeout(-1);  // getch 대기를 위해 블로킹 모드
      getch();
      end_ncurses_settings();
      fprintf(stderr, "Server connection failed\n");
      return EXIT_FAILURE;
    }
    // printf("Debug: Connected to server.\n"); // 콘솔 디버그 메시지

    // 인증 UI 실행 (로그인 또는 회원가입)
    // auth_ui_main은 성공 시 1, 종료(Exit) 선택 시 0 반환
    int auth_result = auth_ui_main(user_id);

    if (auth_result == 0) {  // 사용자가 명시적으로 'Exit' 선택
      disconnect_from_server();
      end_ncurses_settings();
      printf("Exiting game.\n");
      return EXIT_SUCCESS;
    }

    // auth_result가 1이면 로그인 성공, user_id에 사용자 ID가 채워짐
    if (strlen(user_id) > 0) {
      // 로그인 성공 후 메인 메뉴 루프
      bool stay_in_menu = true;
      while (stay_in_menu) {
        clear();
        mvprintw(2, 10, "Logged in as: %s", user_id);
        mvprintw(4, 10, "1. Start Game");
        mvprintw(5, 10, "2. View Leaderboard");
        mvprintw(6, 10, "3. Logout");
        mvprintw(7, 10, "4. Exit Game");  // 전체 종료 옵션 추가
        mvprintw(9, 10, "Select an option: ");
        refresh();
        timeout(-1);  // 메뉴 선택은 블로킹으로 입력 대기
        int choice = getch();

        switch (choice) {
          case '1': {  // Start Game
            clear();   // 이전 메뉴 화면 지우기
            refresh();

            // 게임 실행 및 점수 받아오기
            // run_rain_typing_game 내부에서 nodelay(stdscr, TRUE) 사용 후
            // 종료 시 nodelay(stdscr, FALSE)로 복원해야 함.
            int final_score = run_rain_typing_game(user_id);

            // 게임 종료 후, 점수 표시 및 서버 전송 화면으로 전환
            clear();  // 게임 화면 지우기
            refresh();

            if (final_score < 0) {  // 게임 실행 중 오류 (예: 화면 너무 작음)
              mvprintw(10, 10, "Game could not start or was aborted. (Error: %d)", final_score);
            } else {
              mvprintw(8, 10, "Game Over! Your final score: %d", final_score);
              ScoreSubmitResponse score_res;
              int ret = send_score_submit_request(final_score, &score_res);

              if (ret == 0 && score_res.success) {
                mvprintw(10, 10, "Score submitted successfully! Server: %s", score_res.message);
              } else {
                mvprintw(10, 10, "Failed to submit score. Server: %s (ret: %d)", (ret != 0 ? "Network/Communication error" : score_res.message), ret);
              }
            }
            mvprintw(12, 10, "Press any key to return to the menu...");
            refresh();
            timeout(-1);  // 키 입력 대기
            getch();
            break;
          }
          case '2':                 // View Leaderboard
            show_leaderboard_ui();  // 이 함수 내부에서 clear, refresh, getch 처리
            break;
          case '3': {  // Logout
            LogoutResponse logout_res;
            int ret = send_logout_request(&logout_res);
            clear();
            if (ret == 0 && logout_res.success) {
              mvprintw(10, 10, "Logout successful. %s", logout_res.message);
              user_id[0] = '\0';     // 사용자 ID 초기화
              stay_in_menu = false;  // 메뉴 루프 탈출 -> 바깥 while 루프로 가서 다시 로그인 시도
            } else {
              mvprintw(10, 10, "Logout failed. Server: %s (ret: %d)", (ret != 0 ? "Network/Communication error" : logout_res.message), ret);
            }
            mvprintw(12, 10, "Press any key to continue...");
            refresh();
            timeout(-1);
            getch();
            break;
          }
          case '4': {  // Exit Game
            clear();
            mvprintw(LINES / 2 - 1, (COLS - strlen("Are you sure you want to exit? (y/n)")) / 2, "Are you sure you want to exit? (y/n)");
            refresh();
            timeout(-1);
            int confirm = getch();
            if (confirm == 'y' || confirm == 'Y') {
              disconnect_from_server();
              end_ncurses_settings();
              printf("Exiting game by user's choice.\n");
              return EXIT_SUCCESS;
            }
            // 'n' 또는 다른 키 입력 시 메뉴로 돌아감
            break;
          }
          default:
            mvprintw(11, 10, "Invalid option. Press any key to try again.");
            clrtoeol();
            refresh();
            timeout(-1);
            getch();
            break;
        }  // end switch
      }  // end while(stay_in_menu)
    } else {
      // auth_ui_main에서 로그인 실패 후 (명시적 Exit가 아닌 경우) 여기로 올 수 있음.
      // 또는 auth_ui_main 내부에서 루프를 돌며 재시도하므로, 여기까지 오지 않을 수도 있음.
      // 만약 온다면, 서버 연결을 끊고 다시 외부 루프에서 연결 시도.
    }
    disconnect_from_server();  // 로그아웃 했거나, 인증 실패 후 다시 시작하기 전에 연결 끊기
                               // printf("Debug: Disconnected from server. Loop back to connect/auth.\n");
  }  // end while(1)

  // 정상적으로는 while(1) 루프를 빠져나오지 않음 (Exit Game으로 종료)
  end_ncurses_settings();
  return EXIT_SUCCESS;
}