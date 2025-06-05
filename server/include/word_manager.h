#ifndef WORD_MANAGER_H
#define WORD_MANAGER_H
#include "protocol.h"

extern WordListResponse g_wordlist;
int load_wordlist_from_file(const char* path);

#endif

