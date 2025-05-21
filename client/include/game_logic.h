#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_WORDS 20
#define MAX_WORD_LEN 30
#define INPUT_BUFFER_LEN (MAX_WORD_LEN + 10)
#define INITIAL_LIVES 5
#define BORDER_CHAR ACS_BLOCK
#define WORD_SPAWN_RATE_MS 1500   /* 스폰 주기 (공통) */

/* ---------- Color‑pair 번호 ---------- */
#define COLOR_PAIR_KILL  2  /* 빨간 단어 */
#define COLOR_PAIR_BONUS 3  /* 파란 단어 */

extern char** dynamic_word_list;
extern int    dynamic_word_count;

/* ---------- Word 타입 구분 ---------- */
typedef enum {
  WORD_NORMAL = 0,
  WORD_KILL   = 1,   /* 입력 → 즉시 게임 종료 */
  WORD_BONUS  = 2,   /* 입력 → +50점 */
  WORD_TYPE_COUNT = 3
} WordType;

/* ---------- Word 구조체 ---------- */
typedef struct {
  char      text[MAX_WORD_LEN];
  int       x, y;
  bool      active;
  bool      to_remove;
  WordType  wtype;          /* NEW */
  pthread_t thread_id;
} Word;

/* ---------- 타입별 드롭 속도 (ms) ----------
   - WORD_NORMAL 은 난이도(점수)에 따라 자동 조절
   - WORD_KILL / WORD_BONUS 는 이 배열값을 사용 (수동 조절)
   → 필요 시 run_rain_typing_game() 호출 전 set_drop_interval() 로 변경 가능 */
extern int drop_interval_ms[WORD_TYPE_COUNT];
void set_drop_interval(WordType type, int interval_ms);

int run_rain_typing_game(const char* user_id);

#endif /* GAME_LOGIC_H */
