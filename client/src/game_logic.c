#include "game_logic.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client_globals.h"

/* ======= 전역 변수 정의 ======= */
WordManager g_word_manager = {0};
ActiveWordHashTable g_active_words = {0};

static int FRAME_TOP_Y, FRAME_BOTTOM_Y;
static int GAME_AREA_START_Y, GAME_AREA_END_Y, GAME_AREA_HEIGHT;
static int FRAME_LEFT_X, FRAME_RIGHT_X;
static int GAME_AREA_START_X, GAME_AREA_END_X, GAME_AREA_WIDTH;

static int input_pos = 0, current_game_score = 0, current_game_lives = INITIAL_LIVES;
static bool game_is_over = false;
static int screen_width_cache, screen_height_cache;

static pthread_mutex_t screen_mutex;
static pthread_mutex_t words_mutex;
static pthread_mutex_t game_state_mutex;

static Word words[MAX_WORDS];
static char current_input_buffer[INPUT_BUFFER_LEN];

/* ---------- 타입별 드롭 속도 기본값 ---------- */
int drop_interval_ms[WORD_TYPE_COUNT] = {
    0,   /* WORD_NORMAL */
    350, /* WORD_KILL */
    200  /* WORD_BONUS */
};

void set_drop_interval(WordType type, int interval_ms) {
  if (type >= 0 && type < WORD_TYPE_COUNT && type != WORD_NORMAL && interval_ms > 50) drop_interval_ms[type] = interval_ms;
}

/* ========== 메모리 관리 함수 구현 ========== */

int init_word_manager(void) {
  if (g_word_manager.is_initialized) {
    return 1;  // 이미 초기화됨
  }

  g_word_manager.capacity = 100;  // 초기 용량
  g_word_manager.words = malloc(sizeof(char*) * g_word_manager.capacity);
  if (!g_word_manager.words) {
    return 0;  // 메모리 할당 실패
  }

  g_word_manager.count = 0;
  g_word_manager.is_initialized = true;
  return 1;
}

void cleanup_word_manager(void) {
  if (!g_word_manager.is_initialized) {
    return;
  }

  // 모든 단어 문자열 해제
  for (int i = 0; i < g_word_manager.count; i++) {
    if (g_word_manager.words[i]) {
      free(g_word_manager.words[i]);
      g_word_manager.words[i] = NULL;
    }
  }

  // 단어 배열 해제
  if (g_word_manager.words) {
    free(g_word_manager.words);
    g_word_manager.words = NULL;
  }

  g_word_manager.count = 0;
  g_word_manager.capacity = 0;
  g_word_manager.is_initialized = false;
}

int load_words_from_response(const char* words[], int count) {
  if (!g_word_manager.is_initialized) {
    if (!init_word_manager()) {
      return 0;
    }
  }

  // 기존 단어들 정리
  cleanup_word_manager();
  if (!init_word_manager()) {
    return 0;
  }

  // 필요하면 용량 확장
  if (count > g_word_manager.capacity) {
    int new_capacity = count + 10;
    char** new_words = realloc(g_word_manager.words, sizeof(char*) * new_capacity);
    if (!new_words) {
      return 0;
    }
    g_word_manager.words = new_words;
    g_word_manager.capacity = new_capacity;
  }

  // 단어들 복사
  for (int i = 0; i < count; i++) {
    g_word_manager.words[i] = strdup(words[i]);
    if (!g_word_manager.words[i]) {
      // 메모리 할당 실패 시 이미 할당된 것들 정리
      for (int j = 0; j < i; j++) {
        free(g_word_manager.words[j]);
      }
      g_word_manager.count = 0;
      return 0;
    }
  }

  g_word_manager.count = count;
  return 1;
}

/* ========== 활성 단어 해시 테이블 구현 ========== */

static unsigned int hash_function(const char* str) {
  unsigned int hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash % HASH_TABLE_SIZE;
}

int init_active_word_table(void) {
  // 해시 테이블 초기화
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    g_active_words.buckets[i] = NULL;
  }
  g_active_words.total_count = 0;

  if (pthread_mutex_init(&g_active_words.hash_mutex, NULL) != 0) {
    return 0;
  }
  return 1;
}

void cleanup_active_word_table(void) {
  pthread_mutex_lock(&g_active_words.hash_mutex);

  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    ActiveWordNode* current = g_active_words.buckets[i];
    while (current) {
      ActiveWordNode* temp = current;
      current = current->next;
      free(temp);
    }
    g_active_words.buckets[i] = NULL;
  }
  g_active_words.total_count = 0;

  pthread_mutex_unlock(&g_active_words.hash_mutex);
  pthread_mutex_destroy(&g_active_words.hash_mutex);
}

bool add_active_word(const char* word) {
  if (!word) return false;

  unsigned int index = hash_function(word);

  pthread_mutex_lock(&g_active_words.hash_mutex);

  // 이미 존재하는지 확인
  ActiveWordNode* current = g_active_words.buckets[index];
  while (current) {
    if (strcmp(current->word, word) == 0) {
      pthread_mutex_unlock(&g_active_words.hash_mutex);
      return false;  // 이미 존재
    }
    current = current->next;
  }

  // 새 노드 생성
  ActiveWordNode* new_node = malloc(sizeof(ActiveWordNode));
  if (!new_node) {
    pthread_mutex_unlock(&g_active_words.hash_mutex);
    return false;
  }

  strncpy(new_node->word, word, MAX_WORD_LEN - 1);
  new_node->word[MAX_WORD_LEN - 1] = '\0';
  new_node->next = g_active_words.buckets[index];
  g_active_words.buckets[index] = new_node;
  g_active_words.total_count++;

  pthread_mutex_unlock(&g_active_words.hash_mutex);
  return true;
}

bool remove_active_word(const char* word) {
  if (!word) return false;

  unsigned int index = hash_function(word);

  pthread_mutex_lock(&g_active_words.hash_mutex);

  ActiveWordNode* current = g_active_words.buckets[index];
  ActiveWordNode* prev = NULL;

  while (current) {
    if (strcmp(current->word, word) == 0) {
      if (prev) {
        prev->next = current->next;
      } else {
        g_active_words.buckets[index] = current->next;
      }
      free(current);
      g_active_words.total_count--;
      pthread_mutex_unlock(&g_active_words.hash_mutex);
      return true;
    }
    prev = current;
    current = current->next;
  }

  pthread_mutex_unlock(&g_active_words.hash_mutex);
  return false;
}

bool is_word_in_active_table(const char* word) {
  if (!word) return false;

  unsigned int index = hash_function(word);

  pthread_mutex_lock(&g_active_words.hash_mutex);

  ActiveWordNode* current = g_active_words.buckets[index];
  while (current) {
    if (strcmp(current->word, word) == 0) {
      pthread_mutex_unlock(&g_active_words.hash_mutex);
      return true;
    }
    current = current->next;
  }

  pthread_mutex_unlock(&g_active_words.hash_mutex);
  return false;
}

/* ========== 게임 로직 (기존 함수들 수정) ========== */

static int get_current_level(void) {
  pthread_mutex_lock(&game_state_mutex);
  int s = current_game_score;
  pthread_mutex_unlock(&game_state_mutex);

  if (s >= 200) return 5;
  if (s >= 150) return 4;
  if (s >= 100) return 3;
  if (s >= 50) return 2;
  return 1;
}

static int get_normal_drop_interval(void) {
  pthread_mutex_lock(&game_state_mutex);
  int s = current_game_score;
  pthread_mutex_unlock(&game_state_mutex);

  if (s >= 200) return 200;
  if (s >= 150) return 275;
  if (s >= 100) return 350;
  if (s >= 50) return 425;
  return 500;
}

static void* word_thread_func(void* arg) {
  Word* w = (Word*)arg;
  struct timespec ts_init, ts_now;
  clock_gettime(CLOCK_MONOTONIC, &ts_init);
  long last_drop_ms = ts_init.tv_sec * 1000 + ts_init.tv_nsec / 1000000;

  while (true) {
    if (sigint_received || sigint_game_exit_requested) {
      pthread_mutex_lock(&game_state_mutex);
      game_is_over = true;
      pthread_mutex_unlock(&game_state_mutex);
    }

    pthread_mutex_lock(&words_mutex);
    bool active = w->active;
    pthread_mutex_unlock(&words_mutex);

    pthread_mutex_lock(&game_state_mutex);
    bool over = game_is_over;
    pthread_mutex_unlock(&game_state_mutex);

    if (!active || over) break;

    clock_gettime(CLOCK_MONOTONIC, &ts_now);
    long now_ms = ts_now.tv_sec * 1000 + ts_now.tv_nsec / 1000000;

    int drop_interval = (w->wtype == WORD_NORMAL) ? get_normal_drop_interval() : drop_interval_ms[w->wtype];

    if (now_ms - last_drop_ms >= drop_interval) {
      pthread_mutex_lock(&words_mutex);
      if (w->active) {
        w->y++;
        if (w->y >= GAME_AREA_HEIGHT) {
          w->active = false;
          // 활성 단어 테이블에서 제거
          remove_active_word(w->text);
          pthread_mutex_lock(&game_state_mutex);
          if (!game_is_over) {
            if (w->wtype != WORD_KILL) current_game_lives--;
            if (current_game_lives <= 0) game_is_over = true;
          }
          pthread_mutex_unlock(&game_state_mutex);
        }
      }
      pthread_mutex_unlock(&words_mutex);
      last_drop_ms = now_ms;
    }
    usleep(8000);
  }

  pthread_mutex_lock(&words_mutex);
  w->active = false;
  w->to_remove = true;
  // 활성 단어 테이블에서 제거
  remove_active_word(w->text);
  pthread_mutex_unlock(&words_mutex);
  return NULL;
}

static void spawn_word(void) {
  if (!g_word_manager.is_initialized || g_word_manager.count == 0) {
    return;
  }

  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].thread_id == 0 || words[i].to_remove) {
      if (words[i].thread_id && words[i].to_remove) {
        words[i].thread_id = 0;
      }

      // 해시 테이블을 이용한 중복 확인으로 성능 개선
      const char* pick;
      int tries = 0;
      do {
        pick = g_word_manager.words[rand() % g_word_manager.count];
        tries++;
        if (tries > g_word_manager.count) break;
      } while (is_word_in_active_table(pick));

      strncpy(words[i].text, pick, MAX_WORD_LEN - 1);
      words[i].text[MAX_WORD_LEN - 1] = '\0';
      words[i].y = 0;
      int len = strlen(words[i].text);
      words[i].x = (GAME_AREA_WIDTH > len) ? rand() % (GAME_AREA_WIDTH - len + 1) : 0;
      words[i].active = true;
      words[i].to_remove = false;
      words[i].wtype = (rand() % 100 < 33) ? WORD_KILL : (rand() % 100 < 66) ? WORD_BONUS : WORD_NORMAL;

      // 활성 단어 테이블에 추가
      add_active_word(words[i].text);

      if (pthread_create(&words[i].thread_id, NULL, word_thread_func, &words[i]) != 0) {
        words[i].active = false;
        words[i].thread_id = 0;
        remove_active_word(words[i].text);
      }

      break;
    }
  }
  pthread_mutex_unlock(&words_mutex);
}

/* ========== 안전한 정리 함수 ========== */

void safe_game_cleanup(void) {
  // 모든 스레드 정리
  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].thread_id != 0) {
      words[i].to_remove = true;
    }
  }
  pthread_mutex_unlock(&words_mutex);

  // 스레드 종료 대기
  int cleanup_attempts = 0;
  int active_threads_exist;
  do {
    active_threads_exist = 0;
    pthread_mutex_lock(&words_mutex);
    for (int i = 0; i < MAX_WORDS; ++i) {
      if (words[i].thread_id != 0 && words[i].to_remove) {
        if (pthread_join(words[i].thread_id, NULL) == 0) {
          words[i].thread_id = 0;
          words[i].active = false;
          words[i].to_remove = false;
          words[i].text[0] = '\0';
        } else {
          active_threads_exist = 1;
        }
      }
    }
    pthread_mutex_unlock(&words_mutex);
    if (active_threads_exist) usleep(20000);
    cleanup_attempts++;
  } while (active_threads_exist && cleanup_attempts < 100);

  // 뮤텍스 정리
  pthread_mutex_destroy(&screen_mutex);
  pthread_mutex_destroy(&words_mutex);
  pthread_mutex_destroy(&game_state_mutex);

  // 단어 관리자 정리
  cleanup_word_manager();
  cleanup_active_word_table();
}

/* ========== 게임 실행 함수 (수정) ========== */

// 화면 그리기 함수 (기존 코드와 동일)
static void draw_game_screen(void) {
  pthread_mutex_lock(&screen_mutex);
  erase();

  pthread_mutex_lock(&game_state_mutex);
  int sc = current_game_score, li = current_game_lives;
  bool over = game_is_over;
  pthread_mutex_unlock(&game_state_mutex);

  int level = get_current_level();
  mvprintw(0, 1, "Score: %d   Lives: %d   Level: %d", sc, li, level);
  mvprintw(0, screen_width_cache / 2 - 8, "Rain Typing Game");
  mvprintw(screen_height_cache - 2, 1, "Quit Game: Ctrl+C");

  // 프레임 그리기
  mvhline(FRAME_TOP_Y, FRAME_LEFT_X, BORDER_CHAR, screen_width_cache);
  mvhline(FRAME_BOTTOM_Y, FRAME_LEFT_X, BORDER_CHAR, screen_width_cache);
  mvvline(FRAME_TOP_Y + 1, FRAME_LEFT_X, BORDER_CHAR, FRAME_BOTTOM_Y - FRAME_TOP_Y - 1);
  mvvline(FRAME_TOP_Y + 1, FRAME_RIGHT_X, BORDER_CHAR, FRAME_BOTTOM_Y - FRAME_TOP_Y - 1);
  mvaddch(FRAME_TOP_Y, FRAME_LEFT_X, BORDER_CHAR);
  mvaddch(FRAME_TOP_Y, FRAME_RIGHT_X, BORDER_CHAR);
  mvaddch(FRAME_BOTTOM_Y, FRAME_LEFT_X, BORDER_CHAR);
  mvaddch(FRAME_BOTTOM_Y, FRAME_RIGHT_X, BORDER_CHAR);

  // 활성 단어들 그리기
  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i)
    if (words[i].active) {
      if (has_colors()) {
        if (words[i].wtype == WORD_KILL)
          attron(COLOR_PAIR(COLOR_PAIR_KILL));
        else if (words[i].wtype == WORD_BONUS)
          attron(COLOR_PAIR(COLOR_PAIR_BONUS));
      }
      mvprintw(GAME_AREA_START_Y + words[i].y, GAME_AREA_START_X + words[i].x, "%s", words[i].text);
      if (has_colors()) {
        if (words[i].wtype == WORD_KILL)
          attroff(COLOR_PAIR(COLOR_PAIR_KILL));
        else if (words[i].wtype == WORD_BONUS)
          attroff(COLOR_PAIR(COLOR_PAIR_BONUS));
      }
    }
  pthread_mutex_unlock(&words_mutex);

  mvprintw(screen_height_cache - 1, 1, "Input: %s", current_input_buffer);

  if (over) {
    const char* msg = sigint_received ? "EXITING APPLICATION (Ctrl+C)" : (sigint_game_exit_requested ? "GAME EXITED (Ctrl+C)" : "GAME OVER!");
    mvprintw(GAME_AREA_START_Y + GAME_AREA_HEIGHT / 2, GAME_AREA_START_X + (GAME_AREA_WIDTH - strlen(msg)) / 2, "%s", msg);
    mvprintw(GAME_AREA_START_Y + GAME_AREA_HEIGHT / 2 + 1, GAME_AREA_START_X + (GAME_AREA_WIDTH - strlen("Press any key to continue...")) / 2,
             "Press any key to continue...");
  }
  refresh();
  pthread_mutex_unlock(&screen_mutex);
}

// 입력 처리 함수 (기존과 유사하지만 해시 테이블 사용)
static void process_game_input(int ch) {
  if (ch == ERR) return;

  pthread_mutex_lock(&game_state_mutex);
  bool over = game_is_over;
  pthread_mutex_unlock(&game_state_mutex);
  if (over) return;

  if (ch == '\n' || ch == ' ') {
    if (input_pos > 0) {
      int target_idx = -1;
      int best_prio = 3;
      int best_y = -1;

      pthread_mutex_lock(&words_mutex);
      for (int i = 0; i < MAX_WORDS; ++i) {
        if (!words[i].active) continue;
        if (strcmp(current_input_buffer, words[i].text) != 0) continue;

        int prio = (words[i].wtype == WORD_KILL) ? 0 : (words[i].wtype == WORD_BONUS) ? 1 : 2;

        if (prio < best_prio || (prio == best_prio && words[i].y > best_y)) {
          best_prio = prio;
          best_y = words[i].y;
          target_idx = i;
        }
      }

      if (target_idx != -1) {
        WordType t = words[target_idx].wtype;
        words[target_idx].active = false;
        remove_active_word(words[target_idx].text);  // 해시 테이블에서 제거
        pthread_mutex_unlock(&words_mutex);

        pthread_mutex_lock(&game_state_mutex);
        if (t == WORD_KILL)
          game_is_over = true;
        else {
          if (t == WORD_BONUS) current_game_score += 50;
          current_game_score += strlen(words[target_idx].text);
        }
        pthread_mutex_unlock(&game_state_mutex);
      } else {
        pthread_mutex_unlock(&words_mutex);
      }

      current_input_buffer[0] = '\0';
      input_pos = 0;
    }
  } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
    if (input_pos > 0) current_input_buffer[--input_pos] = '\0';
  } else if (ch >= 32 && ch <= 126 && input_pos < INPUT_BUFFER_LEN - 1) {
    current_input_buffer[input_pos++] = (char)ch;
    current_input_buffer[input_pos] = '\0';
  }
}

static void cleanup_finished_threads(void) {
  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].thread_id != 0 && words[i].to_remove) {
      pthread_join(words[i].thread_id, NULL);
      words[i].thread_id = 0;
      words[i].active = false;
      words[i].to_remove = false;
      words[i].text[0] = '\0';
    }
  }
  pthread_mutex_unlock(&words_mutex);
}

int run_rain_typing_game(const char* user_id) {
  (void)user_id;

  // 초기화
  current_game_score = 0;
  current_game_lives = INITIAL_LIVES;
  game_is_over = false;
  input_pos = 0;
  current_input_buffer[0] = '\0';
  memset(words, 0, sizeof(words));

  srand(time(NULL));

  // 활성 단어 테이블 초기화
  if (!init_active_word_table()) {
    return -2;
  }

  nodelay(stdscr, TRUE);
  getmaxyx(stdscr, screen_height_cache, screen_width_cache);

  // 화면 영역 계산
  FRAME_TOP_Y = 1;
  FRAME_BOTTOM_Y = screen_height_cache - 3;
  GAME_AREA_START_Y = FRAME_TOP_Y + 1;
  GAME_AREA_END_Y = FRAME_BOTTOM_Y - 1;
  GAME_AREA_HEIGHT = GAME_AREA_END_Y - GAME_AREA_START_Y + 1;

  FRAME_LEFT_X = 0;
  FRAME_RIGHT_X = screen_width_cache - 1;
  GAME_AREA_START_X = FRAME_LEFT_X + 1;
  GAME_AREA_END_X = FRAME_RIGHT_X - 1;
  GAME_AREA_WIDTH = GAME_AREA_END_X - GAME_AREA_START_X + 1;

  if (GAME_AREA_HEIGHT < 5 || GAME_AREA_WIDTH < 20) {
    cleanup_active_word_table();
    clear();
    mvprintw(screen_height_cache / 2, (screen_width_cache - strlen("Screen too small.")) / 2, "Screen too small.");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);
    return -1;
  }

  if (pthread_mutex_init(&screen_mutex, NULL) != 0 || pthread_mutex_init(&words_mutex, NULL) != 0 ||
      pthread_mutex_init(&game_state_mutex, NULL) != 0) {
    cleanup_active_word_table();
    clear();
    mvprintw(screen_height_cache / 2, (screen_width_cache - strlen("Mutex init failed.")) / 2, "Mutex init failed.");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);
    return -2;
  }

  // 게임 메인 루프
  long last_spawn_ms = 0;
  struct timespec ts_loop_start;
  clock_gettime(CLOCK_MONOTONIC, &ts_loop_start);
  last_spawn_ms = ts_loop_start.tv_sec * 1000 + ts_loop_start.tv_nsec / 1000000;

  bool loop_condition_check_game_over = false;
  while (!loop_condition_check_game_over) {
    if (sigint_received) {
      pthread_mutex_lock(&game_state_mutex);
      if (!game_is_over) game_is_over = true;
      loop_condition_check_game_over = true;
      pthread_mutex_unlock(&game_state_mutex);
    } else if (sigint_game_exit_requested) {
      pthread_mutex_lock(&game_state_mutex);
      if (!game_is_over) game_is_over = true;
      loop_condition_check_game_over = true;
      pthread_mutex_unlock(&game_state_mutex);
    }

    pthread_mutex_lock(&game_state_mutex);
    loop_condition_check_game_over = game_is_over;
    pthread_mutex_unlock(&game_state_mutex);

    int ch = getch();
    process_game_input(ch);

    pthread_mutex_lock(&game_state_mutex);
    bool can_spawn = !game_is_over;
    pthread_mutex_unlock(&game_state_mutex);

    if (can_spawn) {
      struct timespec ts_now;
      clock_gettime(CLOCK_MONOTONIC, &ts_now);
      long now_ms = ts_now.tv_sec * 1000 + ts_now.tv_nsec / 1000000;
      if (now_ms - last_spawn_ms >= WORD_SPAWN_RATE_MS) {
        spawn_word();
        last_spawn_ms = now_ms;
      }
    }

    draw_game_screen();
    cleanup_finished_threads();

    usleep(30000);
  }

  draw_game_screen();
  nodelay(stdscr, FALSE);
  if (!sigint_received) {
    getch();
  }

  // 게임 종료 시 안전한 정리
  safe_game_cleanup();

  return current_game_score;
}