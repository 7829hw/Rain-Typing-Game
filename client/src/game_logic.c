#include "game_logic.h"  // 자신의 헤더 파일 포함

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// #include <ncurses.h> // game_logic.h 에 이미 포함됨

/* ---------- 전역 상태 (파일 내부에서만 사용) ---------- */
static int FRAME_TOP_Y, FRAME_BOTTOM_Y;
static int GAME_AREA_START_Y, GAME_AREA_END_Y, GAME_AREA_HEIGHT;
static int FRAME_LEFT_X, FRAME_RIGHT_X;
static int GAME_AREA_START_X, GAME_AREA_END_X, GAME_AREA_WIDTH;

static int input_pos = 0, current_game_score = 0, current_game_lives = INITIAL_LIVES;
static int screen_width_cache, screen_height_cache;  // main_client에서 얻어온 화면 크기 저장
static bool game_is_over = false;

static pthread_mutex_t screen_mutex;
static pthread_mutex_t words_mutex;
static pthread_mutex_t game_state_mutex;

static Word words[MAX_WORDS];
static char current_input_buffer[INPUT_BUFFER_LEN];

/* ---------- 단어 리스트 ---------- */
static const char *word_list[] = {"Apple",   "Banana",     "Orange",   "Grape",    "Strawberry", "Peach",  "Watermelon", "Mango",
                                  "Coconut", "Grapefruit", "Hello",    "World",    "Ncurses",    "Thread", "Mutex",      "Programming",
                                  "Rain",    "Typing",     "Game",     "Keyboard", "Terminal",   "Linux",  "Code",       "Computer",
                                  "Science", "Developer",  "Software", "Engineer", "System"};
static int num_total_words = sizeof(word_list) / sizeof(char *);

/* ---------- 난이도(낙하 간격) ---------- */
static int get_current_drop_interval(void) {
  // current_game_score를 사용하도록 수정
  if (current_game_score >= 200) return 200; /* 5단계 */
  if (current_game_score >= 150) return 275; /* 4단계 */
  if (current_game_score >= 100) return 350; /* 3단계 */
  if (current_game_score >= 50) return 425;  /* 2단계 */
  return 500;                                /* 1단계 */
}

/* ---------- 단어 스레드 ---------- */
static void *word_thread_func(void *arg) {
  Word *w = (Word *)arg;
  long last_drop_ms = 0;
  struct timespec ts;

  // 스레드 시작 시 현재 시간으로 last_drop_ms 초기화 (바로 떨어지지 않도록)
  clock_gettime(CLOCK_MONOTONIC, &ts);
  last_drop_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

  while (true) {
    pthread_mutex_lock(&words_mutex);
    bool active = w->active;
    pthread_mutex_unlock(&words_mutex);

    pthread_mutex_lock(&game_state_mutex);
    bool over = game_is_over;
    pthread_mutex_unlock(&game_state_mutex);

    if (!active || over) break;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    long now_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    if (now_ms - last_drop_ms >= get_current_drop_interval()) {
      pthread_mutex_lock(&words_mutex);
      if (w->active) {  // 뮤텍스 잠금 후 다시 확인
        w->y++;
        if (w->y >= GAME_AREA_HEIGHT) {
          w->active = false;  // 먼저 비활성화
          // game_state_mutex는 lives 변경 시에만 잠금
          pthread_mutex_lock(&game_state_mutex);
          if (!game_is_over) {  // 게임 오버가 아닐 때만 생명 감소
            current_game_lives--;
            if (current_game_lives <= 0) {
              game_is_over = true;
            }
          }
          pthread_mutex_unlock(&game_state_mutex);
        }
      }
      pthread_mutex_unlock(&words_mutex);
      last_drop_ms = now_ms;
    }
    usleep(10000);  // 10ms (CPU 사용량 줄이기)
  }

  // 스레드 종료 처리
  pthread_mutex_lock(&words_mutex);
  w->active = false;    // 확실히 비활성화
  w->to_remove = true;  // 정리 대상으로 표시
  pthread_mutex_unlock(&words_mutex);
  return NULL;
}

/* ---------- 새 단어 스폰 ---------- */
static void spawn_word(void) {
  pthread_mutex_lock(&game_state_mutex);
  if (game_is_over) {
    pthread_mutex_unlock(&game_state_mutex);
    return;
  }
  pthread_mutex_unlock(&game_state_mutex);

  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    // thread_id가 0이거나, to_remove가 true인 슬롯을 재사용
    if (words[i].thread_id == 0 || words[i].to_remove) {
      if (words[i].thread_id != 0 && words[i].to_remove) {
        // 이전 스레드가 완전히 정리되지 않았을 수 있으므로, join 시도
        // 하지만 이 함수는 메인 루프에서 호출되므로 여기서 join하면 블로킹됨.
        // cleanup_finished_threads에서 처리하도록 둔다.
        // 여기서는 thread_id를 0으로 만들어 새 스레드 생성 가능하게 함.
        // words[i].thread_id = 0; // cleanup에서 처리하므로 불필요할 수 있음
      }

      strncpy(words[i].text, word_list[rand() % num_total_words], MAX_WORD_LEN - 1);
      words[i].text[MAX_WORD_LEN - 1] = '\0';
      words[i].y = 0;  // 게임 영역 상단에서 시작
      int len = strlen(words[i].text);
      words[i].x = (GAME_AREA_WIDTH > len) ? rand() % (GAME_AREA_WIDTH - len + 1) : 0;
      words[i].active = true;
      words[i].to_remove = false;

      if (pthread_create(&words[i].thread_id, NULL, word_thread_func, &words[i]) != 0) {
        perror("pthread_create for word");
        words[i].active = false;  // 생성 실패 시 비활성화
        words[i].thread_id = 0;   // ID 초기화
      }
      break;  // 하나만 스폰
    }
  }
  pthread_mutex_unlock(&words_mutex);
}

/* ---------- 화면 갱신 ---------- */
static void draw_game_screen(void) {
  pthread_mutex_lock(&screen_mutex);
  erase();  // 전체 화면 지우기 (main_client.c에서 clear()를 호출하지 않는다고 가정)

  pthread_mutex_lock(&game_state_mutex);
  int local_score = current_game_score;
  int local_lives = current_game_lives;
  bool local_game_over = game_is_over;
  pthread_mutex_unlock(&game_state_mutex);

  // 상단 정보 표시 (화면 크기는 캐시된 값 사용)
  mvprintw(0, 1, "Score: %d   Lives: %d", local_score, local_lives);
  mvprintw(0, screen_width_cache / 2 - 8, "Rain Typing Game");
  mvprintw(screen_height_cache - 2, 1, "Quit: Q");

  // 게임 테두리
  mvhline(FRAME_TOP_Y, FRAME_LEFT_X, BORDER_CHAR, screen_width_cache);
  mvhline(FRAME_BOTTOM_Y, FRAME_LEFT_X, BORDER_CHAR, screen_width_cache);
  mvvline(FRAME_TOP_Y + 1, FRAME_LEFT_X, BORDER_CHAR, FRAME_BOTTOM_Y - FRAME_TOP_Y - 1);
  mvvline(FRAME_TOP_Y + 1, FRAME_RIGHT_X, BORDER_CHAR, FRAME_BOTTOM_Y - FRAME_TOP_Y - 1);
  // 모서리 문자 (mvhline/mvvline이 덮어쓸 수 있으므로 나중에 그림)
  mvaddch(FRAME_TOP_Y, FRAME_LEFT_X, BORDER_CHAR);
  mvaddch(FRAME_TOP_Y, FRAME_RIGHT_X, BORDER_CHAR);
  mvaddch(FRAME_BOTTOM_Y, FRAME_LEFT_X, BORDER_CHAR);
  mvaddch(FRAME_BOTTOM_Y, FRAME_RIGHT_X, BORDER_CHAR);

  // 단어 출력
  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].active) {
      mvprintw(GAME_AREA_START_Y + words[i].y, GAME_AREA_START_X + words[i].x, "%s", words[i].text);
    }
  }
  pthread_mutex_unlock(&words_mutex);

  // 입력창
  mvprintw(screen_height_cache - 1, 1, "입력: %s", current_input_buffer);

  if (local_game_over) {
    const char *msg = "GAME OVER!";
    mvprintw(GAME_AREA_START_Y + GAME_AREA_HEIGHT / 2, GAME_AREA_START_X + (GAME_AREA_WIDTH - strlen(msg)) / 2, "%s", msg);
    const char *msg2 = "Press any key to continue...";
    mvprintw(GAME_AREA_START_Y + GAME_AREA_HEIGHT / 2 + 1, GAME_AREA_START_X + (GAME_AREA_WIDTH - strlen(msg2)) / 2, "%s", msg2);
  }

  refresh();
  pthread_mutex_unlock(&screen_mutex);
}

/* ---------- 입력 처리 (가장 아래 단어 우선 제거) ---------- */
static void process_game_input(int ch) {
  pthread_mutex_lock(&game_state_mutex);
  bool local_game_over = game_is_over;
  pthread_mutex_unlock(&game_state_mutex);

  if (ch == ERR) return;  // 입력 없음

  if (!local_game_over) {
    if (ch == '\n' || ch == ' ') {  // Enter 또는 Space로 단어 제출
      if (input_pos > 0) {
        int target_idx = -1;
        int lowest_y = -1;

        pthread_mutex_lock(&words_mutex);
        for (int i = 0; i < MAX_WORDS; ++i) {
          if (words[i].active && strcmp(current_input_buffer, words[i].text) == 0) {
            if (words[i].y > lowest_y) {  // 가장 아래에 있는 단어 선택
              lowest_y = words[i].y;
              target_idx = i;
            }
          }
        }

        if (target_idx != -1) {
          words[target_idx].active = false;  // 단어 비활성화 (스레드가 정리할 것임)
          // words[target_idx].to_remove = true; // 스레드 함수 내부에서 처리

          pthread_mutex_lock(&game_state_mutex);
          current_game_score += strlen(words[target_idx].text);
          pthread_mutex_unlock(&game_state_mutex);
        }
        pthread_mutex_unlock(&words_mutex);

        current_input_buffer[0] = '\0';  // 입력 버퍼 초기화
        input_pos = 0;
      }
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {  // 백스페이스
      if (input_pos > 0) {
        current_input_buffer[--input_pos] = '\0';
      }
    } else if (ch >= 32 && ch <= 126 && input_pos < INPUT_BUFFER_LEN - 1) {  // 일반 문자 입력
      current_input_buffer[input_pos++] = (char)ch;
      current_input_buffer[input_pos] = '\0';
    }
  }
}

/* ---------- 죽은 스레드 정리 ---------- */
static void cleanup_finished_threads(void) {
  pthread_mutex_lock(&words_mutex);  // words 배열 전체를 보호
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].thread_id != 0 && words[i].to_remove) {
      // to_remove가 true이면 스레드는 이미 종료되었거나 종료 중이어야 함
      // pthread_join은 해당 스레드가 완전히 종료될 때까지 기다림
      pthread_join(words[i].thread_id, NULL);
      words[i].thread_id = 0;      // 스레드 ID 초기화, 슬롯 재사용 가능
      words[i].active = false;     // 확실히 비활성
      words[i].to_remove = false;  // 정리 완료
      words[i].text[0] = '\0';     // 텍스트 클리어 (선택적)
    }
  }
  pthread_mutex_unlock(&words_mutex);
}

/* ---------- 게임 실행 함수 (기존 main 대체) ---------- */
int run_rain_typing_game(const char *user_id) {
  // user_id는 현재 게임 로직에서 사용되지 않음. 필요시 활용.
  (void)user_id;  // 컴파일러 경고 방지

  // 게임 상태 초기화
  current_game_score = 0;
  current_game_lives = INITIAL_LIVES;
  game_is_over = false;
  input_pos = 0;
  current_input_buffer[0] = '\0';
  memset(words, 0, sizeof(words));  // words 배열 전체 초기화

  // 로케일 및 난수 시드 설정 (main_client.c에서 한 번만 해도 되지만, 게임 시작 시마다 해도 무방)
  setlocale(LC_ALL, "");
  srand(time(NULL));

  // ncurses 설정은 main_client.c에서 처리
  // initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
  nodelay(stdscr, TRUE);  // 게임 루프를 위해 비차단 입력 모드 설정

  getmaxyx(stdscr, screen_height_cache, screen_width_cache);  // 현재 화면 크기 가져오기

  // 게임 영역 레이아웃 계산
  FRAME_TOP_Y = 1;
  FRAME_BOTTOM_Y = screen_height_cache - 3;  // 입력창과 Quit 메시지 공간 확보
  GAME_AREA_START_Y = FRAME_TOP_Y + 1;
  GAME_AREA_END_Y = FRAME_BOTTOM_Y - 1;
  GAME_AREA_HEIGHT = GAME_AREA_END_Y - GAME_AREA_START_Y + 1;

  FRAME_LEFT_X = 0;
  FRAME_RIGHT_X = screen_width_cache - 1;
  GAME_AREA_START_X = FRAME_LEFT_X + 1;
  GAME_AREA_END_X = FRAME_RIGHT_X - 1;
  GAME_AREA_WIDTH = GAME_AREA_END_X - GAME_AREA_START_X + 1;

  if (GAME_AREA_HEIGHT < 5 || GAME_AREA_WIDTH < 20) {  // 최소 게임 영역 크기 검사
    // endwin(); // main_client.c에서 처리
    // 에러 메시지를 화면 중앙에 표시하고 잠시 대기 후 점수 반환
    clear();
    mvprintw(screen_height_cache / 2, (screen_width_cache - strlen("Screen too small.")) / 2, "Screen too small.");
    refresh();
    nodelay(stdscr, FALSE);  // 다음 getch 대기를 위해 차단 모드로 변경
    getch();
    return -1;  // 오류를 나타내는 음수 점수 반환
  }

  // 뮤텍스 초기화
  if (pthread_mutex_init(&screen_mutex, NULL) != 0 || pthread_mutex_init(&words_mutex, NULL) != 0 ||
      pthread_mutex_init(&game_state_mutex, NULL) != 0) {
    // endwin(); // main_client.c에서 처리
    clear();
    mvprintw(screen_height_cache / 2, (screen_width_cache - strlen("Mutex init failed.")) / 2, "Mutex init failed.");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    return -1;  // 오류
  }

  long last_spawn_ms = 0;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  last_spawn_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;  // 첫 스폰 지연 방지

  bool local_game_over_for_loop = false;
  while (!local_game_over_for_loop) {
    int ch = getch();  // 비차단 입력

    pthread_mutex_lock(&game_state_mutex);
    local_game_over_for_loop = game_is_over;  // 루프 조건용 변수 업데이트
    pthread_mutex_unlock(&game_state_mutex);

    if (ch == 'q' || ch == 'Q') {
      pthread_mutex_lock(&game_state_mutex);
      game_is_over = true;
      local_game_over_for_loop = true;  // 즉시 루프 탈출
      pthread_mutex_unlock(&game_state_mutex);
    }

    process_game_input(ch);  // 입력 처리

    // 단어 스폰 (게임 오버가 아닐 때만)
    pthread_mutex_lock(&game_state_mutex);
    bool can_spawn = !game_is_over;
    pthread_mutex_unlock(&game_state_mutex);

    if (can_spawn) {
      clock_gettime(CLOCK_MONOTONIC, &ts);
      long now_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
      if (now_ms - last_spawn_ms >= WORD_SPAWN_RATE_MS) {
        spawn_word();
        last_spawn_ms = now_ms;
      }
    }

    draw_game_screen();          // 화면 그리기
    cleanup_finished_threads();  // 종료된 스레드 정리

    usleep(30000);  // 게임 루프 간격 (약 33 FPS)
  }

  // 게임 오버 후 마지막 화면 표시 및 키 입력 대기
  draw_game_screen();      // 게임 오버 메시지가 포함된 마지막 화면
  nodelay(stdscr, FALSE);  // 키 입력을 기다리기 위해 차단 모드로 변경
  getch();                 // 아무 키나 누를 때까지 대기

  // endwin(); // main_client.c에서 처리

  // 모든 단어 스레드가 확실히 종료되도록 한번 더 정리
  // (이미 game_is_over = true 상태이므로 새 스레드는 생성되지 않음)
  // 루프를 몇 번 더 돌면서 정리하거나, 모든 스레드에 종료 신호를 보내고 join 할 수 있음.
  // 여기서는 남아있는 활성 스레드에 대해 to_remove를 설정하고 cleanup을 반복.
  int active_threads_remaining = 0;
  do {
    active_threads_remaining = 0;
    pthread_mutex_lock(&words_mutex);
    for (int i = 0; i < MAX_WORDS; ++i) {
      if (words[i].thread_id != 0 && !words[i].to_remove) {
        words[i].to_remove = true;  // 아직 정리 안된 스레드 정리 요청
      }
      if (words[i].thread_id != 0) active_threads_remaining++;
    }
    pthread_mutex_unlock(&words_mutex);
    if (active_threads_remaining > 0) {
      cleanup_finished_threads();
      usleep(10000);  // 짧은 대기
    }
  } while (active_threads_remaining > 0);

  // 뮤텍스 파괴
  pthread_mutex_destroy(&screen_mutex);
  pthread_mutex_destroy(&words_mutex);
  pthread_mutex_destroy(&game_state_mutex);

  // main_client.c로 최종 점수 반환
  return current_game_score;
}