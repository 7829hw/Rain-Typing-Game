/********************************************************************
  server/src/word_manager.c
  ─ words.txt(UTF-8) → g_wordlist 로드

   1) 파일이 없으면 data/words.txt 를 새로 만들고 기본 단어 목록 저장
   2) 파일이 있지만 내용이 비어 있으면 기본 목록으로 채움
   3) 모든 파일 접근을 mutex로 보호
 ********************************************************************/
#include "word_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* 전역 단어 리스트 (프로토콜 정의) */
WordListResponse g_wordlist;

/* words.txt 파일 접근 보호용 mutex */
static pthread_mutex_t words_file_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 전역 단어 리스트 접근 보호용 mutex */
static pthread_mutex_t wordlist_data_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 기본 단어 목록 */
static const char* default_words[] = {"hello", "world",  "rain",    "typing", "keyboard",  "program", "linux", "thread",
                                      "mutex", "socket", "network", "coding", "algorithm", "pointer", "system"};
#define DEFAULT_WORD_COUNT (sizeof(default_words) / sizeof(default_words[0]))

/* 유틸리티 함수들 */
static ssize_t read_line_direct(int fd, char* buffer, size_t buffer_size) {
  size_t pos = 0;
  char ch;
  ssize_t bytes_read;

  while (pos < buffer_size - 1) {
    bytes_read = read(fd, &ch, 1);
    if (bytes_read == 0) {
      if (pos == 0) return 0;
      break;
    }
    if (bytes_read < 0) {
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

/* 기본 단어 목록을 파일에 쓰기 - 락 보호됨 */
static int write_default_words_to_file(const char* path) {
  /* 이 함수는 이미 words_file_mutex가 잡힌 상태에서 호출되므로 추가 락 불필요 */
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

/* 파일이 비어있는지 확인 */
static int is_file_empty(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return -1;
  }
  return (st.st_size == 0) ? 1 : 0;
}

int load_wordlist_from_file(const char* path) {
  /* 파일 접근과 전역 데이터 모두 보호 */
  pthread_mutex_lock(&words_file_mutex);
  pthread_mutex_lock(&wordlist_data_mutex);

reload:
  /* 파일 존재 및 크기 확인 */
  if (is_file_empty(path) == 1) {
    printf("[WORD_MANAGER] Empty words file detected. Writing default words.\n");
    if (write_default_words_to_file(path) != 0) {
      pthread_mutex_unlock(&wordlist_data_mutex);
      pthread_mutex_unlock(&words_file_mutex);
      return -1;
    }
  }

  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    /* 파일이 없으면 새로 만들고 기본 목록 기록 */
    if (errno == ENOENT) {
      printf("[WORD_MANAGER] Words file not found. Creating with default words.\n");
      if (write_default_words_to_file(path) != 0) {
        pthread_mutex_unlock(&wordlist_data_mutex);
        pthread_mutex_unlock(&words_file_mutex);
        return -1;
      }
      /* 다시 읽기 시도 */
      fd = open(path, O_RDONLY);
      if (fd == -1) {
        perror("[WORD_MANAGER] Failed to reopen created words file");
        pthread_mutex_unlock(&wordlist_data_mutex);
        pthread_mutex_unlock(&words_file_mutex);
        return -1;
      }
    } else {
      perror("[WORD_MANAGER] Failed to open words file");
      pthread_mutex_unlock(&wordlist_data_mutex);
      pthread_mutex_unlock(&words_file_mutex);
      return -1;
    }
  }

  g_wordlist.count = 0;
  char line_buffer[MAX_WORD_STR_LEN + 2];
  ssize_t line_length;

  while (g_wordlist.count < MAX_WORDLIST_WORDS && (line_length = read_line_direct(fd, line_buffer, sizeof(line_buffer))) > 0) {
    /* 줄 끝 개행‧CR 제거 */
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
      pthread_mutex_unlock(&wordlist_data_mutex);
      pthread_mutex_unlock(&words_file_mutex);
      return -1;
    }
    goto reload;
  }

  printf("[WORD_MANAGER] Successfully loaded %d words from %s (thread-safe)\n", g_wordlist.count, path);

  pthread_mutex_unlock(&wordlist_data_mutex);
  pthread_mutex_unlock(&words_file_mutex);
  return g_wordlist.count;
}

/* 안전한 단어 리스트 접근 함수들 */
int get_wordlist_count_safe(void) {
  pthread_mutex_lock(&wordlist_data_mutex);
  int count = g_wordlist.count;
  pthread_mutex_unlock(&wordlist_data_mutex);
  return count;
}

int copy_wordlist_safe(WordListResponse* dest) {
  if (!dest) return 0;

  pthread_mutex_lock(&wordlist_data_mutex);
  memcpy(dest, &g_wordlist, sizeof(WordListResponse));
  pthread_mutex_unlock(&wordlist_data_mutex);
  return 1;
}

/* 서버 종료 시 mutex 정리 */
void cleanup_word_manager_mutexes(void) {
  pthread_mutex_destroy(&words_file_mutex);
  pthread_mutex_destroy(&wordlist_data_mutex);
  printf("[WORD_MANAGER] Word manager mutexes cleaned up\n");
}