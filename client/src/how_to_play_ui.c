// client/src/how_to_play_ui.c
#include "how_to_play_ui.h"

#include <ncurses.h>

#include "client_globals.h"
#include "game_logic.h" /* COLOR_PAIR_KILL, COLOR_PAIR_BONUS 상수를 위해 추가 */

/* client_main.c에서 선언된 함수 */
void wait_for_key_or_signal(int y, int x, const char* prompt);

/* 페이지 1: 기본 게임 규칙 */
static void show_page_1(void) {
  clear();

  /* 타이틀 */
  mvprintw(Y_TITLE, X_DEFAULT_POS, "=== Rain Typing Game - How to Play (1/2) ===");

  int current_line = Y_OPTIONS_START;

  /* 게임 목표 */
  mvprintw(current_line++, X_DEFAULT_POS, "GAME OBJECTIVE:");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Type falling words to destroy them before they hit the bottom");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Survive as long as possible and achieve the highest score");
  current_line++;

  /* 게임 플레이 */
  mvprintw(current_line++, X_DEFAULT_POS, "GAMEPLAY:");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Words fall from top to bottom of the screen");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Type the complete word and press ENTER or SPACE");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Use BACKSPACE to correct typing mistakes");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- You start with 5 lives");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Lose a life when a word reaches the bottom");
  current_line++;

  /* 단어 타입 */
  mvprintw(current_line++, X_DEFAULT_POS, "WORD TYPES:");
  if (has_colors()) {
    attron(COLOR_PAIR(COLOR_PAIR_KILL));
    mvprintw(current_line++, X_DEFAULT_POS + 2, "- RED WORDS: DANGER! End game immediately");
    attroff(COLOR_PAIR(COLOR_PAIR_KILL));

    attron(COLOR_PAIR(COLOR_PAIR_BONUS));
    mvprintw(current_line++, X_DEFAULT_POS + 2, "- BLUE WORDS: BONUS! +50 points + word length");
    attroff(COLOR_PAIR(COLOR_PAIR_BONUS));

    mvprintw(current_line++, X_DEFAULT_POS + 2, "- WHITE WORDS: Normal, score = word length");
  } else {
    mvprintw(current_line++, X_DEFAULT_POS + 2, "- KILL WORDS: End game immediately");
    mvprintw(current_line++, X_DEFAULT_POS + 2, "- BONUS WORDS: +50 points + word length");
    mvprintw(current_line++, X_DEFAULT_POS + 2, "- NORMAL WORDS: Score = word length");
  }

  refresh();

  /* 다음 페이지로 */
  wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key for next page (2/2)...");
}

/* 페이지 2: 레벨, 조작법, 전략 */
static void show_page_2(void) {
  clear();

  /* 타이틀 */
  mvprintw(Y_TITLE, X_DEFAULT_POS, "=== Rain Typing Game - How to Play (2/2) ===");

  int current_line = Y_OPTIONS_START;

  /* 레벨 시스템 */
  mvprintw(current_line++, X_DEFAULT_POS, "LEVEL SYSTEM:");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Level 1: 0-49 pts (Slow)    Level 2: 50-99 pts (Medium)");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Level 3: 100-149 pts (Fast) Level 4: 150-199 pts (V.Fast)");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Level 5: 200+ pts (Maximum speed)");
  current_line++;

  /* 조작법 */
  mvprintw(current_line++, X_DEFAULT_POS, "CONTROLS:");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Type letters to spell words");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- ENTER or SPACE: Submit typed word");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- BACKSPACE: Delete last character");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Ctrl+C: Exit game (return to menu)");
  current_line++;

  /* 전략 팁 */
  mvprintw(current_line++, X_DEFAULT_POS, "STRATEGY TIPS:");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Focus on words closest to the bottom first");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Avoid RED words at all costs!");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Prioritize BLUE bonus words when safe");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Speed increases with score - stay alert!");
  mvprintw(current_line++, X_DEFAULT_POS + 2, "- Practice typing accuracy over speed");

  refresh();

  /* 메뉴로 돌아가기 */
  wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key to return to the main menu...");
}

void show_how_to_play_ui(void) {
  /* 페이지 1 표시 */
  show_page_1();

  /* SIGINT 체크 */
  if (sigint_received) return;

  /* 페이지 2 표시 */
  show_page_2();
}