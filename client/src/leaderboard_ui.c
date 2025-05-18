// client/src/leaderboard_ui.c
#include "leaderboard_ui.h"

#include <ncurses.h>

#include "client_globals.h"
#include "client_network.h"  // 변경: network_client.h -> client_network.h
#include "protocol.h"

// Forward declaration from client_main.c
void wait_for_key_or_signal(int y, int x, const char* prompt);

void show_leaderboard_ui() {
  LeaderboardResponse res;
  int ret = send_leaderboard_request(&res);

  clear();
  mvprintw(Y_TITLE, X_DEFAULT_POS, "===== LEADERBOARD =====");

  if (ret != 0 || res.count == -1) {
    mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "Failed to load leaderboard: %s", (ret != 0 ? "Network error" : res.message));
  } else if (res.count == 0) {
    mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "Leaderboard is empty or %s", res.message);
  } else {
    mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "Rank  ID              Score");
    mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "-------------------------------");
    for (int i = 0; i < res.count && i < MAX_LEADERBOARD_ENTRIES; i++) {
      if (Y_OPTIONS_START + 2 + i < Y_STATUS_MSG - 1) {
        mvprintw(Y_OPTIONS_START + 2 + i, X_DEFAULT_POS, "%2d    %-15s %5d", i + 1, res.entries[i].username, res.entries[i].score);
      } else {
        mvprintw(Y_OPTIONS_START + 2 + i, X_DEFAULT_POS, "--- more entries not shown ---");
        break;
      }
    }
  }

  wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key to return...");
}