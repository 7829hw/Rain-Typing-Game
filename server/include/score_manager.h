#ifndef SCORE_MANAGER_H
#define SCORE_MANAGER_H
#include "protocol.h"
void init_score_system();
int submit_score_impl(const char* username, int score, char* response_msg);
void get_leaderboard_impl(LeaderboardEntry* entries, int* count, int max_entries);
#endif
