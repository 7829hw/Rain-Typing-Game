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
  if (ret < 0) {
    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS,
             "Failed to get leaderboard (error code: %d)", ret);
    mvprintw(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS,
             "Press any key to return...");
    wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2,
                           X_DEFAULT_POS, "");
    return;
  }
   // ret == 0 : 성공했지만 저장된 점수가 없음
  if ( res.count == 0) {
    mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "No scores to display.");
    refresh();
    wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2,
                           X_DEFAULT_POS, "Press any key to return...");
    return;
  }
  if (res.count == 0) {
    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "No scores to display.");
  } else {
    mvprintw(Y_OPTIONS_START,     X_DEFAULT_POS, "Rank  ID              Score");
    mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "--------------------------------");
    for (int i = 0; i < res.count && i < MAX_LEADERBOARD_ENTRIES; i++) {
      if (Y_OPTIONS_START + 2 + i < Y_STATUS_MSG - 1) {
        mvprintw(Y_OPTIONS_START + 2 + i, X_DEFAULT_POS,
                 "%2d   %15s %5d",    
                 i + 1,
                 res.entries[i].username,
                 res.entries[i].score);
      } else {
        mvprintw(Y_OPTIONS_START + 2 + i, X_DEFAULT_POS,
                 "--- more entries not shown ---");
        break;
      }
    }
  }

  wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2,
                         X_DEFAULT_POS, "Press any key to return...");
}
