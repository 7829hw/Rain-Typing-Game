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
#define WORD_SPAWN_RATE_MS 1500

/* ---------- Color‑pair 번호 ---------- */
#define COLOR_PAIR_KILL 2  /* 빨간 단어 */
#define COLOR_PAIR_BONUS 3 /* 파란 단어 */

/* ---------- 메모리 관리 구조체 ---------- */
typedef struct {
  char** words;
  int count;
  int capacity;
  bool is_initialized;
} WordManager;

/* ---------- 활성 단어 해시 테이블 ---------- */
#define HASH_TABLE_SIZE 64
typedef struct ActiveWordNode {
  char word[MAX_WORD_LEN];
  struct ActiveWordNode* next;
} ActiveWordNode;

typedef struct {
  ActiveWordNode* buckets[HASH_TABLE_SIZE];
  int total_count;
  pthread_mutex_t hash_mutex;
} ActiveWordHashTable;

/* ---------- Word 타입 구분 ---------- */
typedef enum { WORD_NORMAL = 0, WORD_KILL = 1, WORD_BONUS = 2, WORD_TYPE_COUNT = 3 } WordType;

/* ---------- Word 구조체 ---------- */
typedef struct {
  char text[MAX_WORD_LEN];
  int x, y;
  bool active;
  bool to_remove;
  WordType wtype;
  pthread_t thread_id;
} Word;

/* ---------- 전역 변수 선언 ---------- */
extern WordManager g_word_manager;
extern ActiveWordHashTable g_active_words;

/* ---------- 타입별 드롭 속도 ---------- */
extern int drop_interval_ms[WORD_TYPE_COUNT];
void set_drop_interval(WordType type, int interval_ms);

/* ---------- 메모리 관리 함수들 ---------- */
int init_word_manager(void);
void cleanup_word_manager(void);
int load_words_from_response(const char* words[], int count);

/* ---------- 활성 단어 관리 함수들 ---------- */
int init_active_word_table(void);
void cleanup_active_word_table(void);
bool add_active_word(const char* word);
bool remove_active_word(const char* word);
bool is_word_in_active_table(const char* word);

/* ---------- 게임 실행 함수 ---------- */
int run_rain_typing_game(const char* user_id);

/* ---------- 안전한 리소스 관리 ---------- */
void safe_game_cleanup(void);

#endif /* GAME_LOGIC_H */