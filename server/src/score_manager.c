#include "score_manager.h"

#include <stdio.h>
#include <stdlib.h>  // For qsort
#include <string.h>

#include "db_handler.h"  // DB 핸들러 사용

void init_score_system() { printf("[SCORE] Score system initialized (using file DB).\n"); }

int submit_score_impl(const char* username, int score, char* response_msg) {
  if (username == NULL || strlen(username) == 0) {
    snprintf(response_msg, MAX_MSG_LEN, "Cannot submit score for an anonymous user.");
    return 0;  // 실패
  }

  if (add_score_to_file(username, score)) {
    snprintf(response_msg, MAX_MSG_LEN, "Score %d submitted successfully for '%s'.", score, username);
    return 1;  // 성공
  } else {
    snprintf(response_msg, MAX_MSG_LEN, "Failed to save score for '%s'.", username);
    return 0;  // 실패
  }
}

// qsort를 위한 비교 함수 (ScoreRecord 용)
// 1. 사용자 ID 오름차순
// 2. (ID가 같을 경우) 점수 내림차순 (높은 점수가 먼저 오도록)
static int compare_score_records_for_user_best(const void* a, const void* b) {
  ScoreRecord* recordA = (ScoreRecord*)a;
  ScoreRecord* recordB = (ScoreRecord*)b;

  int user_cmp = strcmp(recordA->username, recordB->username);
  if (user_cmp != 0) {
    return user_cmp;  // 사용자 ID가 다르면 ID 기준으로 정렬
  }
  // 사용자 ID가 같으면 점수 내림차순 정렬
  if (recordB->score > recordA->score) return 1;
  if (recordB->score < recordA->score) return -1;
  return 0;
}

// qsort를 위한 비교 함수 (LeaderboardEntry 용 - 최종 리더보드 정렬)
// 점수 내림차순
static int compare_leaderboard_entries_final(const void* a, const void* b) {
  LeaderboardEntry* entryA = (LeaderboardEntry*)a;
  LeaderboardEntry* entryB = (LeaderboardEntry*)b;
  if (entryB->score > entryA->score) return 1;
  if (entryB->score < entryA->score) return -1;
  return 0;
}

void get_leaderboard_impl(LeaderboardEntry* final_leaderboard_entries, int* final_count, int max_final_entries) {
  ScoreRecord all_scores[MAX_TOTAL_SCORES];  // db_handler.h에 정의된 최대값
  int num_scores_loaded = load_all_scores_from_file(all_scores, MAX_TOTAL_SCORES);

  *final_count = 0;  // 최종 리더보드 개수 초기화

  if (num_scores_loaded <= 0) {
    return;  // 점수 없음 또는 로드 오류
  }

  // 1. 모든 점수를 사용자 ID 기준 오름차순, 동일 사용자 내 점수 내림차순으로 정렬
  qsort(all_scores, num_scores_loaded, sizeof(ScoreRecord), compare_score_records_for_user_best);

  // 2. 사용자별 최고 점수만 추출하여 임시 리더보드에 저장
  //    (실제로는 final_leaderboard_entries에 바로 저장하고, 나중에 최종 정렬)
  char last_processed_user[MAX_ID_LEN] = {0};  // 마지막으로 처리한 사용자 ID 저장

  for (int i = 0; i < num_scores_loaded && *final_count < max_final_entries; i++) {
    // 현재 사용자가 이전에 처리한 사용자와 다른 경우, 이 점수가 해당 사용자의 최고 점수
    if (strcmp(all_scores[i].username, last_processed_user) != 0) {
      strncpy(final_leaderboard_entries[*final_count].username, all_scores[i].username, MAX_ID_LEN - 1);
      final_leaderboard_entries[*final_count].username[MAX_ID_LEN - 1] = '\0';
      final_leaderboard_entries[*final_count].score = all_scores[i].score;
      (*final_count)++;

      // 현재 처리한 사용자 ID 업데이트
      strncpy(last_processed_user, all_scores[i].username, MAX_ID_LEN - 1);
      last_processed_user[MAX_ID_LEN - 1] = '\0';
    }
    // 같은 사용자의 다음 점수들은 (이미 점수 내림차순 정렬되었으므로) 최고 점수가 아니므로 건너뜀
  }

  // 3. 추출된 사용자별 최고 점수들을 최종적으로 점수 내림차순으로 정렬
  if (*final_count > 0) {
    qsort(final_leaderboard_entries, *final_count, sizeof(LeaderboardEntry), compare_leaderboard_entries_final);
  }

  // max_final_entries를 초과하는 경우 잘라내기 (위 로직에서 이미 처리됨, *final_count가 max_final_entries를 넘지 않음)
  // if (*final_count > max_final_entries) {
  //     *final_count = max_final_entries;
  // }
}