// server/src/score_manager.c
#include "score_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db_handler.h"

void init_score_system() { printf("[SCORE_MANAGER] Score system initialized (using file DB).\n"); }

int submit_score_impl(const char* username, int score, char* response_msg) {
  if (username == NULL || strlen(username) == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "Cannot submit score for an anonymous user.");
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 0;
  }

  if (add_score_to_file(username, score)) {
    snprintf(response_msg, MAX_MSG_LEN, "Score %d submitted successfully for '%s'.", score, username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 1;
  } else {
    snprintf(response_msg, MAX_MSG_LEN, "Failed to save score for '%s'.", username);
    response_msg[MAX_MSG_LEN - 1] = '\0';
    return 0;
  }
}

static int compare_score_records_for_user_best(const void* a, const void* b) {
  ScoreRecord* recordA = (ScoreRecord*)a;
  ScoreRecord* recordB = (ScoreRecord*)b;

  int user_cmp = strcmp(recordA->username, recordB->username);
  if (user_cmp != 0) {
    return user_cmp;
  }
  if (recordB->score > recordA->score) return 1;
  if (recordB->score < recordA->score) return -1;
  return 0;
}

static int compare_leaderboard_entries_final(const void* a, const void* b) {
  LeaderboardEntry* entryA = (LeaderboardEntry*)a;
  LeaderboardEntry* entryB = (LeaderboardEntry*)b;
  if (entryB->score > entryA->score) return 1;
  if (entryB->score < entryA->score) return -1;
  return 0;
}

void get_leaderboard_impl(LeaderboardEntry* final_leaderboard_entries, int* final_count, int max_final_entries) {
  ScoreRecord all_scores[MAX_TOTAL_SCORES];
  int num_scores_loaded = load_all_scores_from_file(all_scores, MAX_TOTAL_SCORES);

  *final_count = 0;

  if (num_scores_loaded <= 0) {
    if (num_scores_loaded == -1) *final_count = -1;
    return;
  }

  qsort(all_scores, num_scores_loaded, sizeof(ScoreRecord), compare_score_records_for_user_best);

  char last_processed_user[MAX_ID_LEN] = {0};

  for (int i = 0; i < num_scores_loaded; i++) {
    if (strcmp(all_scores[i].username, last_processed_user) != 0) {
      if (*final_count < max_final_entries) {
        size_t copy_len = strlen(all_scores[i].username);
        if (copy_len >= MAX_ID_LEN) {
          copy_len = MAX_ID_LEN - 1;
        }
        memcpy(final_leaderboard_entries[*final_count].username, all_scores[i].username, copy_len);
        final_leaderboard_entries[*final_count].username[copy_len] = '\0';
        final_leaderboard_entries[*final_count].username[MAX_ID_LEN - 1] = '\0';
        final_leaderboard_entries[*final_count].score = all_scores[i].score;
        (*final_count)++;

        copy_len = strlen(all_scores[i].username);
        if (copy_len >= MAX_ID_LEN) {
          copy_len = MAX_ID_LEN - 1;
        }
        memcpy(last_processed_user, all_scores[i].username, copy_len);
        last_processed_user[copy_len] = '\0';
        last_processed_user[MAX_ID_LEN - 1] = '\0';
      }
    }
  }

  if (*final_count > 0) {
    qsort(final_leaderboard_entries, *final_count, sizeof(LeaderboardEntry), compare_leaderboard_entries_final);
  }
}