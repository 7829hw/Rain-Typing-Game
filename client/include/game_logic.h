#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <ncursesw/curses.h>  // bool, WINDOW 등 ncurses 타입 사용
#include <pthread.h>          // pthread_mutex_t, pthread_t

#define MAX_WORDS 20
#define MAX_WORD_LEN 30
#define INPUT_BUFFER_LEN (MAX_WORD_LEN + 10)  // 충분한 버퍼
#define INITIAL_LIVES 5
#define BORDER_CHAR ACS_BLOCK    // 테두리 문자 (또는 원하는 다른 문자)
#define WORD_SPAWN_RATE_MS 1500  // 단어 생성 간격 (밀리초)

// 단어 구조체
typedef struct {
  char text[MAX_WORD_LEN];
  int x, y;
  bool active;
  bool to_remove;  // 스레드 종료 후 정리 대상 플래그
  pthread_t thread_id;
} Word;

/* ---------- 전역 상태 (다른 파일에서 접근할 필요는 없지만, game_logic.c 내부에서 사용) ---------- */
// 이 변수들은 game_logic.c 내부에 static으로 두거나,
// 굳이 헤더에 둘 필요는 없습니다. 여기서는 .c 파일에 두는 것으로 가정합니다.
// extern int FRAME_TOP_Y, FRAME_BOTTOM_Y;
// extern int GAME_AREA_START_Y, GAME_AREA_END_Y, GAME_AREA_HEIGHT;
// ... (다른 전역 변수들도 마찬가지)

/**
 * @brief Runs the main rain typing game.
 *
 * @param user_id The ID of the currently logged-in user (currently unused in game logic).
 * @return The score achieved by the player.
 */
int run_rain_typing_game(const char* user_id);

#endif  // GAME_LOGIC_H