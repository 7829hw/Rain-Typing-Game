#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORDS 30
#define MAX_WORD_LEN 20
#define INPUT_BUFFER_LEN 80
#define INITIAL_LIVES 5
#define WORD_DROP_INTERVAL_MS 500
#define WORD_SPAWN_RATE_MS 1500
#define BORDER_CHAR '#'

// UI 레이아웃 관련 전역 변수 (main에서 초기화)
int FRAME_TOP_Y;
int FRAME_BOTTOM_Y;
int GAME_AREA_START_Y;  // 단어가 그려지기 시작하는 Y 좌표 (테두리 안쪽)
int GAME_AREA_END_Y;    // 단어가 그려지는 마지막 Y 좌표 (테두리 안쪽)
int GAME_AREA_HEIGHT;   // 실제 단어가 움직일 수 있는 높이

int FRAME_LEFT_X;
int FRAME_RIGHT_X;
int GAME_AREA_START_X;  // 단어가 그려지기 시작하는 X 좌표 (테두리 안쪽)
int GAME_AREA_END_X;    // 단어가 그려지는 마지막 X 좌표 (테두리 안쪽)
int GAME_AREA_WIDTH;    // 실제 단어가 움직일 수 있는 너비

typedef struct {
  char text[MAX_WORD_LEN];
  int x, y;  // 게임 영역 내부 기준 좌표
  bool active;
  pthread_t thread_id;
  bool to_remove;
} Word;

Word words[MAX_WORDS];
char current_input[INPUT_BUFFER_LEN];
int input_pos = 0;
int score = 0;
int lives = INITIAL_LIVES;        // 로직은 유지
int screen_width, screen_height;  // 실제 터미널 크기
bool game_over = false;

pthread_mutex_t screen_mutex;
pthread_mutex_t words_mutex;
pthread_mutex_t game_state_mutex;

const char *word_list[] = {"Apple",   "Banana",     "Orange",   "Grape",    "Strawberry", "Peach",  "Watermelon", "Mango",
                           "Coconut", "Grapefruit", "Hello",    "World",    "Ncurses",    "Thread", "Mutex",      "Programming",
                           "Rain",    "Typing",     "Game",     "Keyboard", "Terminal",   "Linux",  "Code",       "Computer",
                           "Science", "Developer",  "Software", "Engineer", "System"};  // 이미지에 있는 단어들 추가
int num_total_words = sizeof(word_list) / sizeof(char *);

void *word_thread_func(void *arg) {
  Word *word_ptr = (Word *)arg;
  long last_drop_time = 0;
  struct timespec ts;

  while (true) {
    bool current_word_active = false;
    bool is_game_over_globally = false;

    pthread_mutex_lock(&words_mutex);
    current_word_active = word_ptr->active;
    pthread_mutex_unlock(&words_mutex);

    pthread_mutex_lock(&game_state_mutex);
    is_game_over_globally = game_over;
    pthread_mutex_unlock(&game_state_mutex);

    if (!current_word_active || is_game_over_globally) {
      break;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    long current_time_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    if (current_time_ms - last_drop_time >= WORD_DROP_INTERVAL_MS) {
      pthread_mutex_lock(&words_mutex);
      if (word_ptr->active) {
        word_ptr->y++;
        // GAME_AREA_HEIGHT는 단어가 존재할 수 있는 y 인덱스 범위 (0 ~ GAME_AREA_HEIGHT-1)
        if (word_ptr->y >= GAME_AREA_HEIGHT) {  // 게임 영역 바닥에 닿음
          word_ptr->active = false;

          pthread_mutex_lock(&game_state_mutex);
          if (!game_over) {
            lives--;
            if (lives <= 0) {
              lives = 0;
              game_over = true;
            }
          }
          pthread_mutex_unlock(&game_state_mutex);
        }
      }
      pthread_mutex_unlock(&words_mutex);
      last_drop_time = current_time_ms;
    }
    usleep(10000);
  }

  pthread_mutex_lock(&words_mutex);
  word_ptr->active = false;
  word_ptr->to_remove = true;
  pthread_mutex_unlock(&words_mutex);
  return NULL;
}

void spawn_word() {
  pthread_mutex_lock(&game_state_mutex);
  bool is_game_over = game_over;
  pthread_mutex_unlock(&game_state_mutex);
  if (is_game_over) return;

  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].thread_id == 0) {
      strncpy(words[i].text, word_list[rand() % num_total_words], MAX_WORD_LEN - 1);
      words[i].text[MAX_WORD_LEN - 1] = '\0';

      words[i].y = 0;  // 게임 영역의 맨 위
      int word_len = strlen(words[i].text);
      if (GAME_AREA_WIDTH > word_len) {  // 단어가 게임 영역 너비보다 작을 경우에만 스폰
        words[i].x = rand() % (GAME_AREA_WIDTH - word_len);
      } else {
        words[i].x = 0;  // 너무 길면 왼쪽에 붙임 (또는 스폰 안함)
      }

      words[i].active = true;
      words[i].to_remove = false;

      if (pthread_create(&words[i].thread_id, NULL, word_thread_func, &words[i]) != 0) {
        perror("Failed to create word thread");
        words[i].active = false;
        words[i].thread_id = 0;
      }
      break;
    }
  }
  pthread_mutex_unlock(&words_mutex);
}

void draw_screen() {
  pthread_mutex_lock(&screen_mutex);
  erase();

  // 상단 정보 (점수, 목숨, 제목)
  pthread_mutex_lock(&game_state_mutex);
  int current_score = score;
  int current_lives = lives;  // 현재 목숨 수 가져오기
  bool current_game_over_status = game_over;
  pthread_mutex_unlock(&game_state_mutex);

  char score_buf[30];
  char lives_buf[30];
  snprintf(score_buf, sizeof(score_buf), "Score: %d", current_score);
  snprintf(lives_buf, sizeof(lives_buf), "Lives: %d", current_lives);  // 목숨 표시 문자열
  const char *title = "Rain Typing Game";
  int title_len = strlen(title);

  // 상단 라인 클리어 (혹시 모를 잔상 제거)
  move(0, 0);
  clrtoeol();

  int current_x_pos = 1;  // 현재 x 좌표 커서

  // 1. Score 표시
  mvprintw(0, current_x_pos, "%s", score_buf);
  current_x_pos += strlen(score_buf);

  // 2. Lives 표시 (Score와 약간의 간격)
  current_x_pos += 3;  // Score와 Lives 사이 간격
  mvprintw(0, current_x_pos, "%s", lives_buf);
  current_x_pos += strlen(lives_buf);

  // 3. Title 표시 (화면 중앙에 위치 시도, Lives와 겹치지 않게)
  int title_ideal_start_x = (screen_width - title_len) / 2;
  int title_actual_start_x;

  if (title_ideal_start_x > current_x_pos + 3) {  // 중앙 위치가 Lives 뒤 + 간격보다 크면
    title_actual_start_x = title_ideal_start_x;
  } else {  // 아니면 Lives 뒤 + 간격 위치에 표시
    title_actual_start_x = current_x_pos + 3;
  }

  // 화면 오른쪽을 넘어가지 않도록 방지
  if (title_actual_start_x + title_len >= screen_width) {
    title_actual_start_x = screen_width - title_len - 1;
    if (title_actual_start_x < 0) title_actual_start_x = 0;  // 왼쪽도 넘어가지 않도록
  }
  // 추가: title_actual_start_x 가 current_x_pos (Lives 끝나는 지점) 보다 작아지는 경우 방지
  if (title_actual_start_x < current_x_pos + 1) {
    title_actual_start_x = current_x_pos + 1;
  }

  mvprintw(0, title_actual_start_x, "%s", title);

  // 게임 영역 테두리 그리기 (BORDER_CHAR 사용)
  mvhline(FRAME_TOP_Y, FRAME_LEFT_X, BORDER_CHAR, screen_width);
  mvhline(FRAME_BOTTOM_Y, FRAME_LEFT_X, BORDER_CHAR, screen_width);
  mvvline(FRAME_TOP_Y + 1, FRAME_LEFT_X, BORDER_CHAR, FRAME_BOTTOM_Y - FRAME_TOP_Y - 1);
  mvvline(FRAME_TOP_Y + 1, FRAME_RIGHT_X, BORDER_CHAR, FRAME_BOTTOM_Y - FRAME_TOP_Y - 1);

  // 모서리도 BORDER_CHAR로 통일 (ACS_코너 문자 대신)
  mvaddch(FRAME_TOP_Y, FRAME_LEFT_X, BORDER_CHAR);
  mvaddch(FRAME_TOP_Y, FRAME_RIGHT_X, BORDER_CHAR);
  mvaddch(FRAME_BOTTOM_Y, FRAME_LEFT_X, BORDER_CHAR);
  mvaddch(FRAME_BOTTOM_Y, FRAME_RIGHT_X, BORDER_CHAR);

  // 활성화된 단어들 그리기 (게임 영역 내부 좌표 사용)
  pthread_mutex_lock(&words_mutex);
  for (int i = 0; i < MAX_WORDS; ++i) {
    if (words[i].active) {
      mvprintw(GAME_AREA_START_Y + words[i].y, GAME_AREA_START_X + words[i].x, "%s", words[i].text);
    }
  }
  pthread_mutex_unlock(&words_mutex);

  // 입력 프롬프트 (화면 맨 아래, "입력: "으로 변경)
  mvprintw(screen_height - 1, 1, "입력: %s", current_input);

  if (current_game_over_status) {
    const char *game_over_msg = "GAME OVER!";
    int msg_x = GAME_AREA_START_X + (GAME_AREA_WIDTH - strlen(game_over_msg)) / 2;
    int msg_y = GAME_AREA_START_Y + GAME_AREA_HEIGHT / 2;
    if (msg_x < 0) msg_x = 0;  // 화면밖으로 나가는것 방지
    mvprintw(msg_y, msg_x, "%s", game_over_msg);
  }

  refresh();
  pthread_mutex_unlock(&screen_mutex);
}

void process_input(int ch) {
  pthread_mutex_lock(&game_state_mutex);
  bool current_game_over_status = game_over;
  pthread_mutex_unlock(&game_state_mutex);
  // 게임오버시 'q' 제외한 입력은 무시하지 않음 (종료 메시지 후 키 입력 위해)
  // 그러나 단어 매칭은 게임 중에만.
  // if (ch == ERR) return; // 이 조건은 유지

  if (ch == ERR) return;

  if (!current_game_over_status) {  // 게임 중일 때만 단어 입력 처리
    if (ch == '\n' || ch == ' ') {
      if (input_pos > 0) {
        pthread_mutex_lock(&words_mutex);
        for (int i = 0; i < MAX_WORDS; ++i) {
          if (words[i].active && strcmp(current_input, words[i].text) == 0) {
            words[i].active = false;

            pthread_mutex_lock(&game_state_mutex);
            score += strlen(words[i].text);
            pthread_mutex_unlock(&game_state_mutex);
            break;
          }
        }
        pthread_mutex_unlock(&words_mutex);

        current_input[0] = '\0';
        input_pos = 0;
      }
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (input_pos > 0) {
        input_pos--;
        current_input[input_pos] = '\0';
      }
    } else if (ch >= 32 && ch <= 126 && input_pos < INPUT_BUFFER_LEN - 1) {
      current_input[input_pos++] = ch;
      current_input[input_pos] = '\0';
    }
  }
}

void cleanup_finished_threads() {
  for (int i = 0; i < MAX_WORDS; ++i) {
    bool should_join = false;
    pthread_t tid_to_join = 0;

    pthread_mutex_lock(&words_mutex);
    if (words[i].to_remove && words[i].thread_id != 0) {
      should_join = true;
      tid_to_join = words[i].thread_id;
    }
    pthread_mutex_unlock(&words_mutex);

    if (should_join) {
      void *res;
      pthread_join(tid_to_join, &res);

      pthread_mutex_lock(&words_mutex);
      words[i].to_remove = false;
      words[i].active = false;
      words[i].text[0] = '\0';
      words[i].thread_id = 0;
      pthread_mutex_unlock(&words_mutex);
    }
  }
}

int main() {
  srand(time(NULL));

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  curs_set(0);
  getmaxyx(stdscr, screen_height, screen_width);

  // UI 레이아웃 변수 초기화
  FRAME_TOP_Y = 1;                       // 테두리 상단 Y
  FRAME_BOTTOM_Y = screen_height - 2;    // 테두리 하단 Y (입력 줄 바로 위)
  GAME_AREA_START_Y = FRAME_TOP_Y + 1;   // 단어 시작 Y
  GAME_AREA_END_Y = FRAME_BOTTOM_Y - 1;  // 단어 끝 Y
  GAME_AREA_HEIGHT = GAME_AREA_END_Y - GAME_AREA_START_Y + 1;

  FRAME_LEFT_X = 0;                      // 테두리 좌측 X
  FRAME_RIGHT_X = screen_width - 1;      // 테두리 우측 X
  GAME_AREA_START_X = FRAME_LEFT_X + 1;  // 단어 시작 X
  GAME_AREA_END_X = FRAME_RIGHT_X - 1;   // 단어 끝 X
  GAME_AREA_WIDTH = GAME_AREA_END_X - GAME_AREA_START_X + 1;

  // 화면 크기 확인
  if (GAME_AREA_HEIGHT < 1 || GAME_AREA_WIDTH < 10) {  // 최소 게임 영역 크기
    endwin();
    fprintf(stderr, "Screen is too small to run the game.\n");
    fprintf(stderr, "Required game area height > 0 (current: %d based on screen height %d)\n", GAME_AREA_HEIGHT, screen_height);
    fprintf(stderr, "Required game area width > 10 (current: %d based on screen width %d)\n", GAME_AREA_WIDTH, screen_width);
    return 1;
  }

  pthread_mutex_init(&screen_mutex, NULL);
  pthread_mutex_init(&words_mutex, NULL);
  pthread_mutex_init(&game_state_mutex, NULL);

  for (int i = 0; i < MAX_WORDS; ++i) {
    words[i].active = false;
    words[i].to_remove = false;
    words[i].text[0] = '\0';
    words[i].thread_id = 0;
  }
  current_input[0] = '\0';

  long last_spawn_time = 0;
  struct timespec ts_main;
  bool local_game_over = false;

  while (!local_game_over) {
    int ch = getch();
    if (ch == 'q' || ch == 'Q') {
      pthread_mutex_lock(&game_state_mutex);
      game_over = true;
      pthread_mutex_unlock(&game_state_mutex);
    }

    // process_input은 게임 중일 때만 입력을 처리하도록 수정됨
    process_input(ch);

    clock_gettime(CLOCK_MONOTONIC, &ts_main);
    long current_time_ms_main = ts_main.tv_sec * 1000 + ts_main.tv_nsec / 1000000;

    if (current_time_ms_main - last_spawn_time >= WORD_SPAWN_RATE_MS) {
      spawn_word();
      last_spawn_time = current_time_ms_main;
    }

    draw_screen();
    cleanup_finished_threads();

    pthread_mutex_lock(&game_state_mutex);
    local_game_over = game_over;
    pthread_mutex_unlock(&game_state_mutex);

    usleep(30000);
  }

  draw_screen();  // 마지막 "GAME OVER!" 메시지 표시

  for (int i = 0; i < MAX_WORDS; ++i) {
    pthread_t tid_to_join = 0;
    pthread_mutex_lock(&words_mutex);
    if (words[i].thread_id != 0) {
      tid_to_join = words[i].thread_id;
    }
    pthread_mutex_unlock(&words_mutex);

    if (tid_to_join != 0) {
      pthread_join(tid_to_join, NULL);
      pthread_mutex_lock(&words_mutex);
      words[i].to_remove = false;
      words[i].active = false;
      words[i].thread_id = 0;
      pthread_mutex_unlock(&words_mutex);
    }
  }

  pthread_mutex_lock(&screen_mutex);
  const char *exit_msg = "Press any key to exit...";
  // 종료 메시지를 게임 오버 메시지 아래에 표시
  int msg_x = GAME_AREA_START_X + (GAME_AREA_WIDTH - strlen(exit_msg)) / 2;
  int msg_y = GAME_AREA_START_Y + GAME_AREA_HEIGHT / 2 + 1;  // 한 줄 아래
  if (msg_y >= FRAME_BOTTOM_Y) msg_y = FRAME_BOTTOM_Y - 1;   // 테두리 침범 방지
  if (msg_x < GAME_AREA_START_X) msg_x = GAME_AREA_START_X;  // 테두리 침범 방지

  mvprintw(msg_y, msg_x, "%s", exit_msg);
  nodelay(stdscr, FALSE);
  curs_set(1);
  refresh();
  pthread_mutex_unlock(&screen_mutex);

  getch();

  endwin();

  pthread_mutex_destroy(&screen_mutex);
  pthread_mutex_destroy(&words_mutex);
  pthread_mutex_destroy(&game_state_mutex);

  printf("Game Over! Final Score: %d\n", score);

  return 0;
}