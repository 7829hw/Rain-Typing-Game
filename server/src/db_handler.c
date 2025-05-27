// 유저 정보 및 점수 기록을 파일로 저장하고 읽어오는 함수들을 제공합니다.
// 비밀번호는 SHA-256 해시로 저장되며, 줄바꿈 문자 제거 처리 포함.

#include "db_handler.h"
#include "hash_util.h"  //  SHA-256 해시 함수 포함

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define USERS_FILE_PATH "data/users.txt"
#define SCORES_FILE_PATH "data/scores.txt"

// 파일에서 한 줄을 읽어오는 유틸 함수
static ssize_t read_line(int fd, char* buffer, size_t buffer_size) {
    size_t pos = 0;
    char ch;
    ssize_t bytes_read;

    while (pos < buffer_size - 1) {
        bytes_read = read(fd, &ch, 1);
        if (bytes_read <= 0) break;
        if (ch == '\n') break;
        buffer[pos++] = ch;
    }

    buffer[pos] = '\0';
    return pos;
}

//  users.txt 및 scores.txt 파일이 없으면 생성합니다.
void init_db_files() {
    mkdir("data", 0755);
    int fd1 = open(USERS_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd1 >= 0) close(fd1);
    int fd2 = open(SCORES_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd2 >= 0) close(fd2);
}

// 아이디로 유저 정보를 검색합니다 (비밀번호는 해시 문자열)
int find_user_in_file(const char* username, UserData* found_user) {
    int fd = open(USERS_FILE_PATH, O_RDONLY);
    if (fd == -1) return -1;

    char line[256];
    while (read_line(fd, line, sizeof(line)) > 0) {
        char* colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char* stored_username = line;
        char* stored_hash = colon + 1;

        // 개행 문자 제거 (비교 정확도 보장)
        stored_hash[strcspn(stored_hash, "\r\n")] = '\0';

        if (strcmp(stored_username, username) == 0) {
            strncpy(found_user->username, stored_username, MAX_ID_LEN);
            strncpy(found_user->password, stored_hash, MAX_PW_LEN);
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}

// 유저를 users.txt에 추가합니다 (비밀번호는 해시로 저장)
int add_user_to_file(const UserData* user) {
    int fd = open(USERS_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return 0;

    char hashed_pw[65];
    compute_sha256(user->password, hashed_pw);  // 비밀번호 해싱 처리

    dprintf(fd, "%s:%s\n", user->username, hashed_pw);
    close(fd);
    return 1;
}

// 점수를 scores.txt에 추가합니다
int add_score_to_file(const char* username, int score) {
    int fd = open(SCORES_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return 0;
    dprintf(fd, "%s:%d\n", username, score);
    close(fd);
    return 1;
}

// scores.txt에서 모든 점수를 읽어옵니다
int load_all_scores_from_file(ScoreRecord scores[], int max_records) {
    int fd = open(SCORES_FILE_PATH, O_RDONLY);
    if (fd == -1) return -1;

    int count = 0;
    char line[256];
    while (count < max_records && read_line(fd, line, sizeof(line)) > 0) {
        char* colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char* username = line;
        int score = atoi(colon + 1);

        strncpy(scores[count].username, username, MAX_ID_LEN);
        scores[count].score = score;
        count++;
    }

    close(fd);
    return count;
}