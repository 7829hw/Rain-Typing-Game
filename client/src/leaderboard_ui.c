// client/src/leaderboard_ui.c
#include "leaderboard_ui.h"

#include <ncurses.h>

#include "client_globals.h"
#include "client_network.h"
#include "protocol.h"

// client_main.c 에서 선언된 함수
void wait_for_key_or_signal(int y, int x, const char* prompt);

void show_leaderboard_ui() {
  LeaderboardResponse res;
  int ret = send_leaderboard_request(&res);

  clear();

  // 네트워크 오류 처리
  if (ret < 0) {
    const char* error_msg;
    switch (ret) {
      case -1:
        error_msg = "Not connected to server";
        break;
      case -2:
        error_msg = "Failed to send request";
        break;
      case -3:
        error_msg = "Failed to receive response";
        break;
      case -4:
        error_msg = "Server returned error";
        break;
      case -5:
        error_msg = "Unexpected response type from server";
        break;
      case -6:
        error_msg = "Response data too large";
        break;
      case -10:
        error_msg = "Interrupted by user";
        break;
      default:
        error_msg = "Unknown network error";
        break;
    }

    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Failed to get leaderboard: %s (code: %d)", error_msg, ret);
    mvprintw(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "Please check your connection and try again.");
    wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key to return...");
    return;
  }

  // 성공했지만 점수가 없는 경우
  if (res.count == 0) {
    mvprintw(Y_TITLE, X_DEFAULT_POS, "=== LEADERBOARD ===");
    mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "No scores available yet.");
    mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "Be the first to play and set a record!");
    refresh();
    wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key to return...");
    return;
  }

  // 리더보드 표시
  mvprintw(Y_TITLE, X_DEFAULT_POS, "=== LEADERBOARD ===");
  mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "Rank  Username         Score");
  mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "----  --------         -----");

  int display_count = (res.count < MAX_LEADERBOARD_ENTRIES) ? res.count : MAX_LEADERBOARD_ENTRIES;
  int max_display_lines = Y_STATUS_MSG - Y_OPTIONS_START - 3;  // 화면 하단 여백 고려

  if (display_count > max_display_lines) {
    display_count = max_display_lines;
  }

  for (int i = 0; i < display_count; i++) {
    int display_line = Y_OPTIONS_START + 2 + i;

    // 화면 경계 확인
    if (display_line >= Y_STATUS_MSG - 1) {
      mvprintw(display_line, X_DEFAULT_POS, "... and %d more entries", res.count - i);
      break;
    }

    // 사용자명 길이 제한 (15자)
    char username_display[16];
    if (strlen(res.entries[i].username) > 15) {
      strncpy(username_display, res.entries[i].username, 12);
      username_display[12] = '.';
      username_display[13] = '.';
      username_display[14] = '.';
      username_display[15] = '\0';
    } else {
      strcpy(username_display, res.entries[i].username);
    }

    mvprintw(display_line, X_DEFAULT_POS, "%2d    %-15s  %5d", i + 1, username_display, res.entries[i].score);
  }

  // 통계 정보 표시
  if (res.count > 0) {
    mvprintw(Y_STATUS_MSG - 1, X_DEFAULT_POS, "Total players: %d", res.count);
  }

  refresh();
  wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key to return...");
}