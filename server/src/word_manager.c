/********************************************************************
  server/src/word_manager.c
  ─ words.txt(UTF-8) → g_wordlist 로드
 
   1) 파일이 없으면 data/words.txt 를 새로 만들고 기본 단어 목록 저장
   2) 파일이 있지만 내용이 비어 있으면 기본 목록으로 채움
 ********************************************************************/
 #include "word_manager.h"

 #include <stdio.h>
 #include <string.h>
 #include <errno.h>        /* ENOENT 확인용 */

/* 전역 단어 리스트 (프로토콜 정의) */
WordListResponse g_wordlist;

/* ─── 기본 단어 목록 (원하면 자유롭게 수정) ─── */
static const char* default_words[] = {
    "hello","world","rain","typing","keyboard","program","linux","thread",
    "mutex","socket","network","coding","algorithm","pointer","system"
};
#define DEFAULT_WORD_COUNT  (sizeof(default_words)/sizeof(default_words[0]))

/* ---------------------------------------------------------------
 *  load_wordlist_from_file
 *  - path 위치의 텍스트 파일을 한 줄씩 읽어 단어 배열에 저장
 *  - 파일이 없거나(ENOENT) 0개 읽힌 경우 → 기본 단어 목록을
 *    파일에 써 넣고 다시 읽는다.
 *  - 성공 시 읽은 단어 개수(>=1), 실패 시 음수 반환
 * ------------------------------------------------------------- */
int load_wordlist_from_file(const char* path)
{
reload:
    FILE* fp = fopen(path, "r");
    if (!fp) {
        /* 파일이 없으면 새로 만들고 기본 목록 기록 */
        if (errno == ENOENT) {
            fp = fopen(path, "w");
            if (!fp) return -1;                          /* 생성 실패 */
            for (size_t i = 0; i < DEFAULT_WORD_COUNT; ++i)
                fprintf(fp, "%s\n", default_words[i]);
            fclose(fp);
            /* 다시 읽기 */
            fp = fopen(path, "r");
            if (!fp) return -1;
        } else {
            return -1;                                   /* 기타 에러 */
        }
    }

    g_wordlist.count = 0;

    while (g_wordlist.count < MAX_WORDLIST_WORDS &&
           fgets(g_wordlist.words[g_wordlist.count],
                 MAX_WORD_STR_LEN, fp))
    {
        /* --- 줄 끝 개행‧CR 제거 --- */
        char* nl = strpbrk(g_wordlist.words[g_wordlist.count], "\r\n");
        if (nl) *nl = '\0';

        if (g_wordlist.words[g_wordlist.count][0] != '\0')
            g_wordlist.count++;
    }
    fclose(fp);

    /* 읽은 단어가 0개라면 → 기본 목록 파일에 덮어쓰고 다시 로드 */
    if (g_wordlist.count == 0) {
        fp = fopen(path, "w");
        if (!fp) return -1;
        for (size_t i = 0; i < DEFAULT_WORD_COUNT; ++i)
            fprintf(fp, "%s\n", default_words[i]);
        fclose(fp);
        goto reload;
    }
    return g_wordlist.count;      /* ≥1 보장 */
}
