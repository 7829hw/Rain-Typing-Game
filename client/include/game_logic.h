// client/include/game_logic.h
#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <ncurses.h>
#include <pthread.h>

#define MAX_WORDS 20
#define MAX_WORD_LEN 30
#define INPUT_BUFFER_LEN (MAX_WORD_LEN + 10)
#define INITIAL_LIVES 5
#define BORDER_CHAR ACS_BLOCK
#define WORD_SPAWN_RATE_MS 1500

typedef struct {
  char text[MAX_WORD_LEN];
  int x, y;
  bool active;
  bool to_remove;
  pthread_t thread_id;
} Word;

int run_rain_typing_game(const char* user_id);

#endif  // GAME_LOGIC_H