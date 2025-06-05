#ifndef WORD_MANAGER_H
#define WORD_MANAGER_H
#include "protocol.h"

extern WordListResponse g_wordlist;

/* 기존 함수 */
int load_wordlist_from_file(const char* path);

/* Thread-safe 접근 함수들 */
int get_wordlist_count_safe(void);
int copy_wordlist_safe(WordListResponse* dest);

/* Cleanup function */
void cleanup_word_manager_mutexes(void);

#endif