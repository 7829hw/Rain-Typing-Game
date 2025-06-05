// server/include/db_handler.h
#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include "protocol.h"

#define MAX_USERS 100
#define MAX_TOTAL_SCORES 500

typedef struct {
  char username[MAX_ID_LEN];
  char password[MAX_PW_LEN];
} UserData;

typedef struct {
  char username[MAX_ID_LEN];
  int score;
} ScoreRecord;

void init_db_files();
int find_user_in_file(const char* username, UserData* found_user);
int add_user_to_file(const UserData* user);
int add_score_to_file(const char* username, int score);
int load_all_scores_from_file(ScoreRecord scores[], int max_records);

/* Thread-safe cleanup function */
void cleanup_db_mutexes(void);

#endif  // DB_HANDLER_H