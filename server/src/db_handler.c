// server/src/db_handler.c
#include "db_handler.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>  // mutex를 위해 추가
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>  // flock() 함수를 위해 추가
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DATA_DIR_PATH "data"
#define USERS_FILE_PATH DATA_DIR_PATH "/users.txt"
#define SCORES_FILE_PATH DATA_DIR_PATH "/scores.txt"

// 파일 접근 동기화를 위한 전역 mutex들
static pthread_mutex_t users_file_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t scores_file_mutex = PTHREAD_MUTEX_INITIALIZER;

// 한 줄씩 읽기 위한 버퍼 기반 읽기 함수
static ssize_t read_line(int fd, char *buffer, size_t buffer_size) {
  size_t pos = 0;
  char ch;
  ssize_t bytes_read;

  while (pos < buffer_size - 1) {
    bytes_read = read(fd, &ch, 1);
    if (bytes_read == 0) {     // EOF
      if (pos == 0) return 0;  // 아무것도 읽지 못함
      break;
    }
    if (bytes_read < 0) {  // 에러
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

static int create_directory_if_not_exists(const char *path) {
  struct stat st = {0};

  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return 0;  // 디렉터리 존재
    } else {
      fprintf(stderr, "[DB_HANDLER] Error: Path exists but is not a directory: %s\n", path);
      return -1;
    }
  } else {
    // 디렉터리 생성 시도
    if (mkdir(path, 0755) == 0) {
      printf("[DB_HANDLER] Directory created: %s\n", path);
      return 0;
    } else {
      if (errno == EEXIST) {
        // 다른 프로세스가 생성했을 수 있음, 재확인
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

  // 사용자 파일 생성/확인 (O_CREAT으로 없으면 생성)
  pthread_mutex_lock(&users_file_mutex);
  int fd_users = open(USERS_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd_users == -1) {
    perror("[DB_HANDLER] Failed to open/create users.txt");
  } else {
    close(fd_users);
  }
  pthread_mutex_unlock(&users_file_mutex);

  // 점수 파일 생성/확인
  pthread_mutex_lock(&scores_file_mutex);
  int fd_scores = open(SCORES_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd_scores == -1) {
    perror("[DB_HANDLER] Failed to open/create scores.txt");
  } else {
    close(fd_scores);
  }
  pthread_mutex_unlock(&scores_file_mutex);

  printf("[DB_HANDLER] Checked/Initialized data files: %s, %s\n", USERS_FILE_PATH, SCORES_FILE_PATH);
}

int find_user_in_file(const char *username, UserData *found_user) {
  pthread_mutex_lock(&users_file_mutex);

  int fd = open(USERS_FILE_PATH, O_RDONLY);
  if (fd == -1) {
    pthread_mutex_unlock(&users_file_mutex);
    if (errno == ENOENT) {
      return 0;  // 파일이 없음 = 사용자 없음
    }
    return -1;  // 다른 에러
  }

  // 파일 락 적용
  if (flock(fd, LOCK_SH) == -1) {
    perror("[DB_HANDLER] Failed to acquire shared lock on users file");
    close(fd);
    pthread_mutex_unlock(&users_file_mutex);
    return -1;
  }

  UserData current_user;
  int found = 0;
  char line_buffer[MAX_ID_LEN + MAX_PW_LEN + 3];  // username:password\n\0
  ssize_t line_length;

  while ((line_length = read_line(fd, line_buffer, sizeof(line_buffer))) > 0) {
    // username:password 형태 파싱
    char *colon_pos = strchr(line_buffer, ':');
    if (colon_pos == NULL) {
      continue;  // 잘못된 형식의 줄은 건너뛰기
    }

    *colon_pos = '\0';  // username 부분 분리
    char *password_part = colon_pos + 1;

    // 길이 체크 및 복사
    if (strlen(line_buffer) < MAX_ID_LEN && strlen(password_part) < MAX_PW_LEN) {
      strncpy(current_user.username, line_buffer, MAX_ID_LEN - 1);
      current_user.username[MAX_ID_LEN - 1] = '\0';
      strncpy(current_user.password, password_part, MAX_PW_LEN - 1);
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

  flock(fd, LOCK_UN);  // 락 해제
  close(fd);
  pthread_mutex_unlock(&users_file_mutex);
  return found;
}

int add_user_to_file(const UserData *user) {
  pthread_mutex_lock(&users_file_mutex);

  int fd = open(USERS_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd == -1) {
    perror("[DB_HANDLER] add_user_to_file: open users.txt");
    pthread_mutex_unlock(&users_file_mutex);
    return 0;
  }

  // 파일 락 적용 (배타적 락)
  if (flock(fd, LOCK_EX) == -1) {
    perror("[DB_HANDLER] Failed to acquire exclusive lock on users file");
    close(fd);
    pthread_mutex_unlock(&users_file_mutex);
    return 0;
  }

  int bytes_written = dprintf(fd, "%s:%s\n", user->username, user->password);
  if (bytes_written <= 0) {
    perror("[DB_HANDLER] add_user_to_file: write users.txt");
    flock(fd, LOCK_UN);
    close(fd);
    pthread_mutex_unlock(&users_file_mutex);
    return 0;
  }

  // 즉시 디스크에 쓰기 (안전성 향상)
  if (fsync(fd) != 0) {
    perror("[DB_HANDLER] add_user_to_file: fsync users.txt");
  }

  flock(fd, LOCK_UN);  // 락 해제
  if (close(fd) != 0) {
    perror("[DB_HANDLER] add_user_to_file: close users.txt");
    pthread_mutex_unlock(&users_file_mutex);
    return 0;
  }

  pthread_mutex_unlock(&users_file_mutex);
  return 1;
}

int add_score_to_file(const char *username, int score) {
  pthread_mutex_lock(&scores_file_mutex);

  int fd = open(SCORES_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd == -1) {
    perror("[DB_HANDLER] add_score_to_file: open scores.txt");
    pthread_mutex_unlock(&scores_file_mutex);
    return 0;
  }

  // 파일 락 적용 (배타적 락)
  if (flock(fd, LOCK_EX) == -1) {
    perror("[DB_HANDLER] Failed to acquire exclusive lock on scores file");
    close(fd);
    pthread_mutex_unlock(&scores_file_mutex);
    return 0;
  }

  int bytes_written = dprintf(fd, "%s:%d\n", username, score);
  if (bytes_written <= 0) {
    perror("[DB_HANDLER] add_score_to_file: write scores.txt");
    flock(fd, LOCK_UN);
    close(fd);
    pthread_mutex_unlock(&scores_file_mutex);
    return 0;
  }

  // 즉시 디스크에 쓰기 (안전성 향상)
  if (fsync(fd) != 0) {
    perror("[DB_HANDLER] add_score_to_file: fsync scores.txt");
  }

  flock(fd, LOCK_UN);  // 락 해제
  if (close(fd) != 0) {
    perror("[DB_HANDLER] add_score_to_file: close scores.txt");
    pthread_mutex_unlock(&scores_file_mutex);
    return 0;
  }

  pthread_mutex_unlock(&scores_file_mutex);
  return 1;
}

int load_all_scores_from_file(ScoreRecord scores[], int max_records) {
  pthread_mutex_lock(&scores_file_mutex);

  int fd = open(SCORES_FILE_PATH, O_RDONLY);
  if (fd == -1) {
    pthread_mutex_unlock(&scores_file_mutex);
    if (errno == ENOENT) {
      return 0;  // 파일이 없음 = 점수 없음
    }
    return -1;  // 다른 에러
  }

  // 파일 락 적용 (공유 락)
  if (flock(fd, LOCK_SH) == -1) {
    perror("[DB_HANDLER] Failed to acquire shared lock on scores file");
    close(fd);
    pthread_mutex_unlock(&scores_file_mutex);
    return -1;
  }

  int count = 0;
  char line_buffer[MAX_ID_LEN + 16];  // username:score\n\0
  ssize_t line_length;

  while (count < max_records && (line_length = read_line(fd, line_buffer, sizeof(line_buffer))) > 0) {
    // username:score 형태 파싱
    char *colon_pos = strchr(line_buffer, ':');
    if (colon_pos == NULL) {
      continue;  // 잘못된 형식의 줄은 건너뛰기
    }

    *colon_pos = '\0';  // username 부분 분리
    char *score_part = colon_pos + 1;

    // 사용자명 길이 체크 및 복사
    if (strlen(line_buffer) < MAX_ID_LEN) {
      strncpy(scores[count].username, line_buffer, MAX_ID_LEN - 1);
      scores[count].username[MAX_ID_LEN - 1] = '\0';

      // 점수 파싱
      char *endptr;
      long score_value = strtol(score_part, &endptr, 10);
      if (endptr != score_part && *endptr == '\0') {  // 성공적으로 파싱됨
        scores[count].score = (int)score_value;
        count++;
      }
    }
  }

  flock(fd, LOCK_UN);  // 락 해제
  close(fd);
  pthread_mutex_unlock(&scores_file_mutex);
  return count;
}