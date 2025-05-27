// ���� ���� �� ���� ����� ���Ϸ� �����ϰ� �о���� �Լ����� �����մϴ�.
// ��й�ȣ�� SHA-256 �ؽ÷� ����Ǹ�, �ٹٲ� ���� ���� ó�� ����.

#include "db_handler.h"
#include "hash_util.h"  //  SHA-256 �ؽ� �Լ� ����

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define USERS_FILE_PATH "data/users.txt"
#define SCORES_FILE_PATH "data/scores.txt"

// ���Ͽ��� �� ���� �о���� ��ƿ �Լ�
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

//  users.txt �� scores.txt ������ ������ �����մϴ�.
void init_db_files() {
    mkdir("data", 0755);
    int fd1 = open(USERS_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd1 >= 0) close(fd1);
    int fd2 = open(SCORES_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd2 >= 0) close(fd2);
}

// ���̵�� ���� ������ �˻��մϴ� (��й�ȣ�� �ؽ� ���ڿ�)
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

        // ���� ���� ���� (�� ��Ȯ�� ����)
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

// ������ users.txt�� �߰��մϴ� (��й�ȣ�� �ؽ÷� ����)
int add_user_to_file(const UserData* user) {
    int fd = open(USERS_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return 0;

    char hashed_pw[65];
    compute_sha256(user->password, hashed_pw);  // ��й�ȣ �ؽ� ó��

    dprintf(fd, "%s:%s\n", user->username, hashed_pw);
    close(fd);
    return 1;
}

// ������ scores.txt�� �߰��մϴ�
int add_score_to_file(const char* username, int score) {
    int fd = open(SCORES_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return 0;
    dprintf(fd, "%s:%d\n", username, score);
    close(fd);
    return 1;
}

// scores.txt���� ��� ������ �о�ɴϴ�
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