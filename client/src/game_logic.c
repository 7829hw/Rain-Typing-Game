#include "game_logic.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client_globals.h"

/* ======= 전역/정적 변수 ======= */
static int FRAME_TOP_Y, FRAME_BOTTOM_Y;
static int GAME_AREA_START_Y, GAME_AREA_END_Y, GAME_AREA_HEIGHT;
static int FRAME_LEFT_X, FRAME_RIGHT_X;
static int GAME_AREA_START_X, GAME_AREA_END_X, GAME_AREA_WIDTH;

static int  input_pos = 0, current_game_score = 0, current_game_lives = INITIAL_LIVES;
static bool game_is_over = false;
static int  screen_width_cache, screen_height_cache;

static pthread_mutex_t screen_mutex;
static pthread_mutex_t words_mutex;
static pthread_mutex_t game_state_mutex;

static Word words[MAX_WORDS];
static char current_input_buffer[INPUT_BUFFER_LEN];

/* ---------- 타입별 드롭 속도 기본값 ---------- */
int drop_interval_ms[WORD_TYPE_COUNT] = {
  0,        /* WORD_NORMAL 은 점수 기반 → 0 사용 */
  350,      /* WORD_KILL  (빨간 글자) */
  200       /* WORD_BONUS (파란 글자) */
};

void set_drop_interval(WordType type, int interval_ms) {
  if (type >= 0 && type < WORD_TYPE_COUNT && type != WORD_NORMAL && interval_ms > 50)
    drop_interval_ms[type] = interval_ms;
}

/* ==== 동적 단어 배열 ==== */
char** dynamic_word_list = NULL;
int    dynamic_word_count = 0;


/* 점수 0~49 → Lv1, 50~99 → Lv2, … 200 이상 → Lv5 */
static int get_current_level(void) {
  pthread_mutex_lock(&game_state_mutex);
  int s = current_game_score;
  pthread_mutex_unlock(&game_state_mutex);

  if (s >= 200) return 5;
  if (s >= 150) return 4;
  if (s >= 100) return 3;
  if (s >= 50)  return 2;
  return 1;
}
/* ---------- 현재 난이도(점수) 기반 일반 단어 드롭 속도 ---------- */
static int get_normal_drop_interval(void) {
  pthread_mutex_lock(&game_state_mutex);
  int s = current_game_score;
  pthread_mutex_unlock(&game_state_mutex);

  if (s >= 200) return 200;
  if (s >= 150) return 275;
  if (s >= 100) return 350;
  if (s >= 50)  return 425;
  return 500;
}

/* ---------- 워드 스레드 ---------- */
static void* word_thread_func(void* arg) {
  Word* w = (Word*)arg;
  struct timespec ts_init, ts_now;
  clock_gettime(CLOCK_MONOTONIC, &ts_init);
  long last_drop_ms = ts_init.tv_sec*1000 + ts_init.tv_nsec/1000000;

  while (true) {
    if (sigint_received || sigint_game_exit_requested) {
      pthread_mutex_lock(&game_state_mutex); game_is_over = true; pthread_mutex_unlock(&game_state_mutex);
    }

    pthread_mutex_lock(&words_mutex);
    bool active = w->active;
    pthread_mutex_unlock(&words_mutex);

    pthread_mutex_lock(&game_state_mutex);
    bool over = game_is_over;
    pthread_mutex_unlock(&game_state_mutex);

    if (!active || over) break;

    clock_gettime(CLOCK_MONOTONIC, &ts_now);
    long now_ms = ts_now.tv_sec*1000 + ts_now.tv_nsec/1000000;

    int drop_interval = (w->wtype == WORD_NORMAL) ? get_normal_drop_interval()
                                                 : drop_interval_ms[w->wtype];

    if (now_ms - last_drop_ms >= drop_interval) {
      pthread_mutex_lock(&words_mutex);
      if (w->active) {
        w->y++;
        if (w->y >= GAME_AREA_HEIGHT) {
          w->active = false;
          pthread_mutex_lock(&game_state_mutex);
          if (!game_is_over) {
            if (w->wtype != WORD_KILL)   /* 빨간 단어면 생명 그대로 */
              current_game_lives--; 
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
  w->active = false; w->to_remove = true;
  pthread_mutex_unlock(&words_mutex);
  return NULL;
}

/* ---------- 단어 스폰 ---------- */
static void spawn_word(void)
{
    /* … 생략: 게임 종료 여부 검사 … */

    pthread_mutex_lock(&words_mutex);
    for (int i = 0; i < MAX_WORDS; ++i) {
        if (words[i].thread_id == 0 || words[i].to_remove) {

            /* 이전 쓰레드 정리 */
            if (words[i].thread_id && words[i].to_remove)
                words[i].thread_id = 0;

            /* ─── 단어 정보 초기화 ─── */
            const char* pick = dynamic_word_list[rand() % dynamic_word_count];
            strncpy(words[i].text, pick, MAX_WORD_LEN - 1);
            words[i].text[MAX_WORD_LEN - 1] = '\0';
            words[i].y = 0;
            int len = strlen(words[i].text);
            words[i].x = (GAME_AREA_WIDTH > len) ? rand() % (GAME_AREA_WIDTH - len + 1) : 0;
            words[i].active    = true;
            words[i].to_remove = false;
            words[i].wtype     = (rand() % 100 < 33) ? WORD_KILL :
                                 (rand() % 100 < 66) ? WORD_BONUS : WORD_NORMAL;

            /* ─── 스레드 시작 (★ 반드시 필요) ─── */
            if (pthread_create(&words[i].thread_id,
                               NULL,
                               word_thread_func,
                               &words[i]) != 0) {
                /* 실패하면 슬롯을 다시 비워 둡니다 */
                words[i].active = false;
                words[i].thread_id = 0;
            }

            break;   /* 한 단어만 스폰하고 for 탈출 */
        }
    }
    pthread_mutex_unlock(&words_mutex);
}


/* ---------- 화면 그리기 (색상 포함) ---------- */
static void draw_game_screen(void) {
  pthread_mutex_lock(&screen_mutex); erase();

  pthread_mutex_lock(&game_state_mutex);
  int sc=current_game_score, li=current_game_lives; bool over=game_is_over;
  pthread_mutex_unlock(&game_state_mutex);

  int level = get_current_level();   
  mvprintw(0, 1, "Score: %d   Lives: %d   Level: %d",
            sc, li, level);           /* ← 레벨 포함해 출력 */
  mvprintw(0,screen_width_cache/2-8,"Rain Typing Game");
  mvprintw(screen_height_cache-2,1,"Quit Game: Ctrl+C");

  mvhline(FRAME_TOP_Y,FRAME_LEFT_X,BORDER_CHAR,screen_width_cache);
  mvhline(FRAME_BOTTOM_Y,FRAME_LEFT_X,BORDER_CHAR,screen_width_cache);
  mvvline(FRAME_TOP_Y+1,FRAME_LEFT_X,BORDER_CHAR,FRAME_BOTTOM_Y-FRAME_TOP_Y-1);
  mvvline(FRAME_TOP_Y+1,FRAME_RIGHT_X,BORDER_CHAR,FRAME_BOTTOM_Y-FRAME_TOP_Y-1);
  mvaddch(FRAME_TOP_Y,FRAME_LEFT_X,BORDER_CHAR);  mvaddch(FRAME_TOP_Y,FRAME_RIGHT_X,BORDER_CHAR);
  mvaddch(FRAME_BOTTOM_Y,FRAME_LEFT_X,BORDER_CHAR); mvaddch(FRAME_BOTTOM_Y,FRAME_RIGHT_X,BORDER_CHAR);

  pthread_mutex_lock(&words_mutex);
  for (int i=0;i<MAX_WORDS;++i) if (words[i].active) {
    if (has_colors()) {
      if (words[i].wtype==WORD_KILL)  attron(COLOR_PAIR(COLOR_PAIR_KILL));
      else if (words[i].wtype==WORD_BONUS) attron(COLOR_PAIR(COLOR_PAIR_BONUS));
    }
    mvprintw(GAME_AREA_START_Y+words[i].y, GAME_AREA_START_X+words[i].x, "%s", words[i].text);
    if (has_colors()) {
      if (words[i].wtype==WORD_KILL)      attroff(COLOR_PAIR(COLOR_PAIR_KILL));
      else if (words[i].wtype==WORD_BONUS) attroff(COLOR_PAIR(COLOR_PAIR_BONUS));
    }
  }
  pthread_mutex_unlock(&words_mutex);

  mvprintw(screen_height_cache-1,1,"Input: %s",current_input_buffer);

  if (over) {
    const char* msg = sigint_received ? "EXITING APPLICATION (Ctrl+C)" :
                       (sigint_game_exit_requested ? "GAME EXITED (Ctrl+C)" : "GAME OVER!");
    mvprintw(GAME_AREA_START_Y+GAME_AREA_HEIGHT/2,
             GAME_AREA_START_X+(GAME_AREA_WIDTH-strlen(msg))/2,"%s",msg);
    const char* msg2="Press any key to continue...";
    mvprintw(GAME_AREA_START_Y+GAME_AREA_HEIGHT/2+1,
             GAME_AREA_START_X+(GAME_AREA_WIDTH-strlen(msg2))/2,"%s",msg2);
  }
  refresh(); pthread_mutex_unlock(&screen_mutex);
}

/* ---------- 입력 처리 ---------- */
static void process_game_input(int ch) {
  if (ch == ERR) return;

  pthread_mutex_lock(&game_state_mutex);
  bool over = game_is_over;
  pthread_mutex_unlock(&game_state_mutex);
  if (over) return;

  if ((ch == '\n') || (ch == ' ')) {
    if (input_pos > 0) {

      int target_idx = -1;
      int best_prio  = 3;   /* 0=RED, 1=BLUE, 2=WHITE */
      int best_y     = -1;

      /* 잠금금 words[] 보호 */
      pthread_mutex_lock(&words_mutex);
      for (int i = 0; i < MAX_WORDS; ++i) {
        if (!words[i].active) continue;
        if (strcmp(current_input_buffer, words[i].text) != 0) continue;

        int prio = (words[i].wtype == WORD_KILL)  ? 0 : // 낮을 수록 우선 순위 먼저(현재 빨강 -> 파랑 -> 맨 아래 흰색)
                   (words[i].wtype == WORD_BONUS) ? 1 : 2;

        if (prio < best_prio || (prio == best_prio && words[i].y > best_y)) {
          best_prio  = prio;
          best_y     = words[i].y;
          target_idx = i;
        }
      }

      if (target_idx != -1) {
        /* 단어 비활성화는 락 안에서 수행 */
        WordType t = words[target_idx].wtype;
        words[target_idx].active = false;
        pthread_mutex_unlock(&words_mutex);     /* 잠금 */

        if (t == WORD_KILL) {
          pthread_mutex_lock(&game_state_mutex);
          game_is_over = true;
          pthread_mutex_unlock(&game_state_mutex);
        } else {
          pthread_mutex_lock(&game_state_mutex);
          if (t == WORD_BONUS) current_game_score += 50;
          current_game_score += strlen(words[target_idx].text);
          pthread_mutex_unlock(&game_state_mutex);
        }
      } else {
        pthread_mutex_unlock(&words_mutex);     /* 잠금금 */
      }

      current_input_buffer[0] = '\0';
      input_pos = 0;
    }

  } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
    if (input_pos > 0) current_input_buffer[--input_pos] = '\0';

  } else if (ch >= 32 && ch <= 126 && input_pos < INPUT_BUFFER_LEN - 1) {
    current_input_buffer[input_pos++] = (char)ch;
    current_input_buffer[input_pos]   = '\0';
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

int run_rain_typing_game(const char *user_id) {
  (void)user_id;

  current_game_score = 0;
  current_game_lives = INITIAL_LIVES;
  game_is_over = false;  // Reset game over state for this game instance
  // sigint_game_exit_requested is reset in client_main's menu loop
  // sigint_received is for full app exit and not reset here

  input_pos = 0;
  current_input_buffer[0] = '\0';
  memset(words, 0, sizeof(words));

  srand(time(NULL));

  nodelay(stdscr, TRUE);  // Non-blocking input for game loop
  getmaxyx(stdscr, screen_height_cache, screen_width_cache);

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
    clear();
    mvprintw(screen_height_cache / 2, (screen_width_cache - strlen("Screen too small.")) / 2, "Screen too small.");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);  // Briefly wait
    return -1;              // Error code for screen too small
  }

  if (pthread_mutex_init(&screen_mutex, NULL) != 0 || pthread_mutex_init(&words_mutex, NULL) != 0 ||
      pthread_mutex_init(&game_state_mutex, NULL) != 0) {
    clear();
    mvprintw(screen_height_cache / 2, (screen_width_cache - strlen("Mutex init failed.")) / 2, "Mutex init failed.");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);  // Briefly wait
    return -2;              // Error code for mutex init failure
  }

  long last_spawn_ms = 0;
  struct timespec ts_loop_start;
  clock_gettime(CLOCK_MONOTONIC, &ts_loop_start);
  last_spawn_ms = ts_loop_start.tv_sec * 1000 + ts_loop_start.tv_nsec / 1000000;

  bool loop_condition_check_game_over = false;  // Local copy of game_is_over for loop condition
  while (!loop_condition_check_game_over) {
    // Check for SIGINT signals first (full program exit takes precedence)
    if (sigint_received) {
      pthread_mutex_lock(&game_state_mutex);
      if (!game_is_over) game_is_over = true;  // Ensure game state reflects this
      loop_condition_check_game_over = true;   // Update loop condition
      pthread_mutex_unlock(&game_state_mutex);
      // Don't break immediately; let draw_game_screen show the "EXITING APPLICATION" message
    } else if (sigint_game_exit_requested) {
      pthread_mutex_lock(&game_state_mutex);
      if (!game_is_over) game_is_over = true;  // Mark game as over due to game-specific SIGINT
      loop_condition_check_game_over = true;   // Update loop condition
      pthread_mutex_unlock(&game_state_mutex);
      // Don't break immediately; let draw_game_screen show the "GAME EXITED" message
    }

    // Update loop condition based on game state (lives, etc.)
    pthread_mutex_lock(&game_state_mutex);
    loop_condition_check_game_over = game_is_over;
    pthread_mutex_unlock(&game_state_mutex);

    // Get input, 'q' quit is removed
    int ch = getch();
    process_game_input(ch);  // Does not handle 'q' anymore

    // Spawn words if game is not over
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

    usleep(30000);  // Approx 33 FPS
  }

  // Game over sequence
  draw_game_screen();      // Final draw with "GAME OVER" or "Ctrl+C" message
  nodelay(stdscr, FALSE);  // Blocking getch to wait for key press

  // Only wait for a key press if the game didn't end due to a full application SIGINT
  // If it was a game-only SIGINT, we still wait for a key to return to menu.
  if (!sigint_received) {
    getch();
  }
  // The global timeout(100) will be in effect when returning to client_main's menu loop.

  // Final cleanup of all threads
  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].thread_id != 0) {
      words[i].to_remove = true;  // Signal all active word threads to terminate
    }
  }
  pthread_mutex_unlock(&words_mutex);

  int cleanup_attempts = 0;
  int active_threads_exist;
  do {
    cleanup_finished_threads();  // Join terminated threads
    active_threads_exist = 0;
    pthread_mutex_lock(&words_mutex);
    for (int i = 0; i < MAX_WORDS; ++i) {
      if (words[i].thread_id != 0) {  // Check if any thread ID is still non-zero
        active_threads_exist = 1;
        break;
      }
    }
    pthread_mutex_unlock(&words_mutex);
    if (active_threads_exist) usleep(20000);  // Wait a bit for threads to exit
    cleanup_attempts++;
  } while (active_threads_exist && cleanup_attempts < 100);  // Timeout after ~2s

  pthread_mutex_destroy(&screen_mutex);
  pthread_mutex_destroy(&words_mutex);
  pthread_mutex_destroy(&game_state_mutex);

  // Return the score, even if exited by game-specific SIGINT
  return current_game_score;
}