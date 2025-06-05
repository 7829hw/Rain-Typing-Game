/********************************************************************
  server/src/word_manager.c
  ─ words.txt(UTF-8) → g_wordlist 로드

   1) 파일이 없으면 data/words.txt 를 새로 만들고 기본 단어 목록 저장
   2) 파일이 있지만 내용이 비어 있으면 기본 목록으로 채움
 ********************************************************************/
#include "word_manager.h"

#include <errno.h> /* ENOENT 확인용 */
#include <fcntl.h> /* open() 플래그들 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> /* stat() */
#include <unistd.h>   /* read(), write(), close() */

/* 전역 단어 리스트 (프로토콜 정의) */
WordListResponse g_wordlist;

/* ─── 기본 단어 목록 (원하면 자유롭게 수정) ─── */
static const char* default_words[] = {"hello", "world",  "rain",    "typing", "keyboard",  "program", "linux", "thread",
                                      "mutex", "socket", "network", "coding", "algorithm", "pointer", "system"};
#define DEFAULT_WORD_COUNT (sizeof(default_words) / sizeof(default_words[0]))

/* ---------------------------------------------------------------
 *  유틸리티 함수들
 * ------------------------------------------------------------- */

// 한 줄씩 읽기 위한 함수
static ssize_t read_line_direct(int fd, char* buffer, size_t buffer_size) {
  size_t pos = 0;
  char ch;
  ssize_t bytes_read;

  while (pos < buffer_size - 1) {
    bytes_read = read(fd, &ch, 1);
    if (bytes_read == 0) {     // EOF
      if (pos == 0) return 0;  // 아무것도 읽지 못함
      break;
    }
    if (bytes_read < 0) {  // 에러
      return -1;
    }

    if (ch == '\n') {
      break;
    }

    buffer[pos++] = ch;
  }

  buffer[pos] = '\0';
  return pos;
}

// 기본 단어 목록을 파일에 쓰기
static int write_default_words_to_file(const char* path) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    perror("[WORD_MANAGER] Failed to create default words file");
    return -1;
  }

  for (size_t i = 0; i < DEFAULT_WORD_COUNT; ++i) {
    if (dprintf(fd, "%s\n", default_words[i]) <= 0) {
      perror("[WORD_MANAGER] Failed to write default word");
      close(fd);
      return -1;
    }
  }

  if (close(fd) != 0) {
    perror("[WORD_MANAGER] Failed to close default words file");
    return -1;
  }

  return 0;
}

// 파일이 비어있는지 확인
static int is_file_empty(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return -1;  // stat 실패
  }
  return (st.st_size == 0) ? 1 : 0;
}

/* ---------------------------------------------------------------
 *  load_wordlist_from_file
 *  - path 위치의 텍스트 파일을 한 줄씩 읽어 단어 배열에 저장
 *  - 파일이 없거나(ENOENT) 0개 읽힌 경우 → 기본 단어 목록을
 *    파일에 써 넣고 다시 읽는다.
 *  - 성공 시 읽은 단어 개수(>=1), 실패 시 음수 반환
 * ------------------------------------------------------------- */
int load_wordlist_from_file(const char* path) {
reload:
  // 파일 존재 및 크기 확인
  if (is_file_empty(path) == 1) {
    // 파일이 비어있으면 기본 목록으로 채우고 다시 로드
    printf("[WORD_MANAGER] Empty words file detected. Writing default words.\n");
    if (write_default_words_to_file(path) != 0) {
      return -1;
    }
  }

  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    /* 파일이 없으면 새로 만들고 기본 목록 기록 */
    if (errno == ENOENT) {
      printf("[WORD_MANAGER] Words file not found. Creating with default words.\n");
      if (write_default_words_to_file(path) != 0) {
        return -1;
      }
      /* 다시 읽기 시도 */
      fd = open(path, O_RDONLY);
      if (fd == -1) {
        perror("[WORD_MANAGER] Failed to reopen created words file");
        return -1;
      }
    } else {
      perror("[WORD_MANAGER] Failed to open words file");
      return -1; /* 기타 에러 */
    }
  }

  g_wordlist.count = 0;
  char line_buffer[MAX_WORD_STR_LEN + 2];  // 단어 + 개행 + null
  ssize_t line_length;

  while (g_wordlist.count < MAX_WORDLIST_WORDS && (line_length = read_line_direct(fd, line_buffer, sizeof(line_buffer))) > 0) {
    /* --- 줄 끝 개행‧CR 제거 --- */
    char* nl = strpbrk(line_buffer, "\r\n");
    if (nl) *nl = '\0';

    /* 빈 줄이 아니고 유효한 길이면 추가 */
    if (line_buffer[0] != '\0' && strlen(line_buffer) < MAX_WORD_STR_LEN) {
      strncpy(g_wordlist.words[g_wordlist.count], line_buffer, MAX_WORD_STR_LEN - 1);
      g_wordlist.words[g_wordlist.count][MAX_WORD_STR_LEN - 1] = '\0';
      g_wordlist.count++;
    }
  }

  close(fd);

  /* 읽은 단어가 0개라면 → 기본 목록 파일에 덮어쓰고 다시 로드 */
  if (g_wordlist.count == 0) {
    printf("[WORD_MANAGER] No valid words loaded. Writing default words and retrying.\n");
    if (write_default_words_to_file(path) != 0) {
      return -1;
    }
    goto reload;
  }

  printf("[WORD_MANAGER] Successfully loaded %d words from %s\n", g_wordlist.count, path);
  return g_wordlist.count; /* ≥1 보장 */
}