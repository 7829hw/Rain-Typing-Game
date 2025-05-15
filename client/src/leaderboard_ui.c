// client/src/leaderboard_ui.c
#include "leaderboard_ui.h"

#include <ncursesw/curses.h>

#include "network_client.h"
#include "protocol.h"

void show_leaderboard_ui() {
  LeaderboardResponse res;
  int ret = send_leaderboard_request(&res);

  clear();
  mvprintw(2, 10, "===== LEADERBOARD =====");

  if (ret != 0 || res.count == -1) {  // Assuming count = -1 for server error in getting data
    mvprintw(4, 10, "Failed to load leaderboard: %s", (ret != 0 ? "Network error" : res.message));
  } else if (res.count == 0) {
    mvprintw(4, 10, "Leaderboard is empty or %s", res.message);
  } else {
    mvprintw(4, 10, "Rank  ID              Score");
    mvprintw(5, 10, "-------------------------------");
    for (int i = 0; i < res.count && i < MAX_LEADERBOARD_ENTRIES; i++) {
      mvprintw(6 + i, 10, "%2d    %-15s %5d", i + 1, res.entries[i].username, res.entries[i].score);
    }
  }

  mvprintw(18, 10, "Press any key to return...");
  refresh();
  getch();
}