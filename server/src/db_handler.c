// server/src/db_handler.c
#include "db_handler.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DATA_DIR_PATH "data"
#define USERS_FILE_PATH DATA_DIR_PATH "/users.txt"
#define SCORES_FILE_PATH DATA_DIR_PATH "/scores.txt"

static int create_directory_if_not_exists(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return 0;
    } else {
      fprintf(stderr, "[DB_HANDLER] Error: Path exists but is not a directory: %s\n", path);
      return -1;
    }
  } else {
    if (mkdir(path, 0755) == 0) {
      printf("[DB_HANDLER] Directory created: %s\n", path);
      return 0;
    } else {
      if (errno == EEXIST) {
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
          return 0;
        }
      }
      perror("[DB_HANDLER] mkdir failed");
      fprintf(stderr, "[DB_HANDLER] Failed to create directory: %s (errno: %d)\n", path, errno);
      return -1;
    }
  }
}

void init_db_files() {
  if (create_directory_if_not_exists(DATA_DIR_PATH) != 0) {
    fprintf(stderr, "[DB_HANDLER] Critical: Could not ensure data directory %s exists. Exiting.\n", DATA_DIR_PATH);
    exit(EXIT_FAILURE);
  }

  FILE *fp_users = fopen(USERS_FILE_PATH, "a");
  if (fp_users == NULL) {
    perror("[DB_HANDLER] Failed to open/create users.txt");
  } else {
    fclose(fp_users);
  }

  FILE *fp_scores = fopen(SCORES_FILE_PATH, "a");
  if (fp_scores == NULL) {
    perror("[DB_HANDLER] Failed to open/create scores.txt");
  } else {
    fclose(fp_scores);
  }
  printf("[DB_HANDLER] Checked/Initialized data files: %s, %s\n", USERS_FILE_PATH, SCORES_FILE_PATH);
}

int find_user_in_file(const char *username, UserData *found_user) {
  FILE *fp = fopen(USERS_FILE_PATH, "r");
  if (fp == NULL) {
    if (errno == ENOENT) return 0;
    // perror("[DB_HANDLER] find_user_in_file: fopen users.txt");
    return -1;
  }

  UserData current_user;
  int found = 0;
  char line_buffer[MAX_ID_LEN + MAX_PW_LEN + 3];

  while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
    line_buffer[strcspn(line_buffer, "\n")] = 0;

    if (sscanf(line_buffer, "%19[^:]:%19s", current_user.username, current_user.password) == 2) {
      current_user.username[MAX_ID_LEN - 1] = '\0';
      current_user.password[MAX_PW_LEN - 1] = '\0';
      if (strcmp(current_user.username, username) == 0) {
        if (found_user != NULL) {
          *found_user = current_user;
        }
        found = 1;
        break;
      }
    }
  }
  fclose(fp);
  return found;
}

int add_user_to_file(const UserData *user) {
  FILE *fp = fopen(USERS_FILE_PATH, "a");
  if (fp == NULL) {
    perror("[DB_HANDLER] add_user_to_file: fopen users.txt");
    return 0;
  }
  if (fprintf(fp, "%s:%s\n", user->username, user->password) < 0) {
    perror("[DB_HANDLER] add_user_to_file: fprintf users.txt");
    fclose(fp);
    return 0;
  }
  if (fclose(fp) != 0) {
    perror("[DB_HANDLER] add_user_to_file: fclose users.txt");
    return 0;
  }
  return 1;
}

int add_score_to_file(const char *username, int score) {
  FILE *fp = fopen(SCORES_FILE_PATH, "a");
  if (fp == NULL) {
    perror("[DB_HANDLER] add_score_to_file: fopen scores.txt");
    return 0;
  }
  if (fprintf(fp, "%s:%d\n", username, score) < 0) {
    perror("[DB_HANDLER] add_score_to_file: fprintf scores.txt");
    fclose(fp);
    return 0;
  }
  if (fclose(fp) != 0) {
    perror("[DB_HANDLER] add_score_to_file: fclose scores.txt");
    return 0;
  }
  return 1;
}

int load_all_scores_from_file(ScoreRecord scores[], int max_records) {
  FILE *fp = fopen(SCORES_FILE_PATH, "r");
  if (fp == NULL) {
    if (errno == ENOENT) return 0;
    // perror("[DB_HANDLER] load_all_scores_from_file: fopen scores.txt");
    return -1;
  }

  int count = 0;
  char line_buffer[MAX_ID_LEN + 12];  // Max username + : + 10 digit score + newline + null
  while (count < max_records && fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
    line_buffer[strcspn(line_buffer, "\n")] = 0;
    if (sscanf(line_buffer, "%19[^:]:%d", scores[count].username, &scores[count].score) == 2) {
      scores[count].username[MAX_ID_LEN - 1] = '\0';
      count++;
    }
  }
  fclose(fp);
  return count;
}