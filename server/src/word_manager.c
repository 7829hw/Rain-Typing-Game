/********************************************************************
 *  server/src/word_manager.c
 *  ────────────────────────────────────────────────────────────────
 *  words.txt(UTF-8) 파일에서 단어 목록을 읽어 전역 g_wordlist 에 저장
 ********************************************************************/
#include "word_manager.h"

#include <stdio.h>
#include <string.h>

/* 전역 단어 리스트 (프로토콜 정의) */
WordListResponse g_wordlist;

/* ---------------------------------------------------------------
 *  load_wordlist_from_file
 *  - path 위치의 텍스트 파일을 한 줄씩 읽어 단어 배열에 저장
 *  - 성공 시 읽은 단어 개수(>=1), 실패 시 음수 반환
 * ------------------------------------------------------------- */
int load_wordlist_from_file(const char* path)
{
    FILE* fp = fopen(path, "r");
    if (!fp) return -1;

    g_wordlist.count = 0;

    while (g_wordlist.count < MAX_WORDLIST_WORDS &&
           fgets(g_wordlist.words[g_wordlist.count],
                 MAX_WORD_STR_LEN, fp))
    {
        /* --- 줄 끝 개행(LF) 제거 --- */
        char* nl = strchr(g_wordlist.words[g_wordlist.count], '\n');
        if (nl) *nl = '\0';

        /* --- 추가: CR(Carriage-Return) 제거 --- */
        char* cr = strchr(g_wordlist.words[g_wordlist.count], '\r');
        if (cr) *cr = '\0';

        /* 공백 줄은 건너뜀 */
        if (g_wordlist.words[g_wordlist.count][0] != '\0')
            g_wordlist.count++;
    }

    fclose(fp);
    return g_wordlist.count;      /* 읽은 단어 수 (0 가능) */
}

