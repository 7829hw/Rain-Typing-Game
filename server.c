#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>  // 로그 시간 추가용
#include <unistd.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

// 사용자 상태 정의
typedef enum { STATUS_OFFLINE = 0, STATUS_ONLINE = 1 } UserStatus;

// 뮤텍스 추가 - 사용자 데이터 접근 동기화
pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;

// 활성 클라이언트 수 추적
int active_clients = 0;
pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;

// 간단한 사용자 데이터베이스
typedef struct {
  char username[50];
  char password[50];
  UserStatus status;                 // 온라인 상태 추적
  int socket_fd;                     // 현재 연결된 소켓 (온라인인 경우)
  pthread_t thread_id;               // 사용자 처리 스레드
  char ip_address[INET_ADDRSTRLEN];  // 로그인한 IP 주소
  time_t login_time;                 // 로그인 시간
} User;

User users[] = {{"user1", "pass1", STATUS_OFFLINE, -1, 0, "", 0},
                {"user2", "pass2", STATUS_OFFLINE, -1, 0, "", 0},
                {"admin", "admin123", STATUS_OFFLINE, -1, 0, "", 0}};

int num_users = sizeof(users) / sizeof(User);

// 클라이언트 처리 스레드에 전달할 구조체
typedef struct {
  int socket;
  struct sockaddr_in address;
  int user_index;  // 로그인한 사용자 인덱스 (로그인 성공 시)
} client_data;

// 현재 시간을 문자열로 반환
char *get_time_string() {
  static char time_buffer[30];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", t);
  return time_buffer;
}

// 인증 함수 - 로그인 시도
int authenticate(char *username, char *password, int *user_index) {
  int result = -1;  // -1: 사용자 없음, 0: 비밀번호 틀림, 1: 이미 로그인, 2: 성공

  pthread_mutex_lock(&user_mutex);  // 뮤텍스 락

  for (int i = 0; i < num_users; i++) {
    if (strcmp(users[i].username, username) == 0) {
      *user_index = i;  // 사용자 인덱스 저장

      if (strcmp(users[i].password, password) != 0) {
        result = 0;  // 비밀번호 틀림
      } else if (users[i].status == STATUS_ONLINE) {
        result = 1;  // 이미 로그인 상태
      } else {
        result = 2;  // 인증 성공
      }
      break;
    }
  }

  pthread_mutex_unlock(&user_mutex);  // 뮤텍스 언락
  return result;
}

// 사용자 로그아웃 처리
void logout_user(int user_index, const char *reason) {
  if (user_index < 0 || user_index >= num_users) return;

  pthread_mutex_lock(&user_mutex);

  if (users[user_index].status == STATUS_ONLINE) {
    printf("[%s] 로그아웃: %s (%s) - %s\n", get_time_string(), users[user_index].username, users[user_index].ip_address, reason);

    users[user_index].status = STATUS_OFFLINE;
    users[user_index].socket_fd = -1;
    users[user_index].thread_id = 0;
    users[user_index].ip_address[0] = '\0';
    users[user_index].login_time = 0;
  }

  pthread_mutex_unlock(&user_mutex);
}

// 시그널 핸들러 - 서버 종료 시 모든 사용자 로그아웃
void handle_shutdown(int sig) {
  printf("\n[%s] 서버 종료 중... 모든 사용자 로그아웃 처리\n", get_time_string());

  pthread_mutex_lock(&user_mutex);
  for (int i = 0; i < num_users; i++) {
    if (users[i].status == STATUS_ONLINE) {
      int sock = users[i].socket_fd;
      users[i].status = STATUS_OFFLINE;

      if (sock != -1) {
        // 클라이언트에게 서버 종료 메시지 전송
        const char *msg = "서버가 종료되었습니다.";
        send(sock, msg, strlen(msg), 0);
        close(sock);
      }

      printf("[%s] 강제 로그아웃: %s (서버 종료)\n", get_time_string(), users[i].username);
    }
  }
  pthread_mutex_unlock(&user_mutex);

  exit(EXIT_SUCCESS);
}

// 강제 로그아웃 처리 - 기존 세션 종료
void force_logout(int user_index) {
  if (user_index < 0 || user_index >= num_users) return;

  pthread_mutex_lock(&user_mutex);

  if (users[user_index].status == STATUS_ONLINE && users[user_index].socket_fd != -1) {
    int sock = users[user_index].socket_fd;

    // 기존 클라이언트에 강제 로그아웃 메시지 전송
    const char *msg = "다른 위치에서 로그인하여 현재 세션이 종료됩니다.";
    send(sock, msg, strlen(msg), 0);

    printf("[%s] 강제 로그아웃: %s (%s) - 다른 위치에서 로그인\n", get_time_string(), users[user_index].username, users[user_index].ip_address);

    // 소켓 닫기 (이렇게 하면 해당 클라이언트 스레드가 종료됨)
    close(sock);

    // 상태 초기화
    users[user_index].status = STATUS_OFFLINE;
    users[user_index].socket_fd = -1;
    // thread_id는 그대로 두고 해당 스레드가 정리되게 함
  }

  pthread_mutex_unlock(&user_mutex);
}

// 클라이언트 연결 처리 스레드 함수
void *handle_client(void *arg) {
  client_data *data = (client_data *)arg;
  int client_fd = data->socket;
  struct sockaddr_in client_addr = data->address;
  char buffer[BUFFER_SIZE] = {0};
  int user_index = -1;

  // 클라이언트 IP 주소 가져오기
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

  // 클라이언트 수 증가
  pthread_mutex_lock(&client_count_mutex);
  active_clients++;
  printf("[%s] 새 클라이언트 연결: %s:%d (현재 연결: %d)\n", get_time_string(), client_ip, ntohs(client_addr.sin_port), active_clients);
  pthread_mutex_unlock(&client_count_mutex);

  // 클라이언트로부터 데이터 수신
  int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
  if (bytes_read > 0) {
    buffer[bytes_read] = '\0';

    // 로그인 정보 파싱
    char username[50], password[50];
    sscanf(buffer, "%s %s", username, password);

    printf("[%s] 로그인 시도: 사용자 %s (%s:%d)\n", get_time_string(), username, client_ip, ntohs(client_addr.sin_port));

    // 인증 처리
    char response[BUFFER_SIZE];
    int auth_result = authenticate(username, password, &user_index);

    switch (auth_result) {
      case 2:  // 인증 성공
        // 이전에 중복 로그인 방지를 위해 강제 로그아웃 옵션
        /*
        if (allow_force_logout) {
            force_logout(user_index);
        }
        */

        // 사용자를 온라인 상태로 표시
        pthread_mutex_lock(&user_mutex);
        users[user_index].status = STATUS_ONLINE;
        users[user_index].socket_fd = client_fd;
        users[user_index].thread_id = pthread_self();
        strncpy(users[user_index].ip_address, client_ip, INET_ADDRSTRLEN);
        users[user_index].login_time = time(NULL);
        pthread_mutex_unlock(&user_mutex);

        // 로그인 성공 응답
        sprintf(response, "로그인 성공! 환영합니다, %s님", username);
        printf("[%s] 로그인 성공: %s (%s:%d)\n", get_time_string(), username, client_ip, ntohs(client_addr.sin_port));

        // 클라이언트 구조체에 사용자 인덱스 저장
        data->user_index = user_index;
        break;

      case 1:  // 이미 로그인 중
        sprintf(response, "로그인 실패: %s는 이미 다른 세션에서 로그인 중입니다", username);
        printf("[%s] 중복 로그인 시도: %s (%s:%d)\n", get_time_string(), username, client_ip, ntohs(client_addr.sin_port));
        break;

      case 0:  // 비밀번호 틀림
        sprintf(response, "로그인 실패: 잘못된 비밀번호입니다");
        printf("[%s] 로그인 실패: %s (%s:%d) - 잘못된 비밀번호\n", get_time_string(), username, client_ip, ntohs(client_addr.sin_port));
        break;

      default:  // 사용자 없음
        sprintf(response, "로그인 실패: 사용자 %s를 찾을 수 없습니다", username);
        printf("[%s] 로그인 실패: %s (%s:%d) - 사용자 없음\n", get_time_string(), username, client_ip, ntohs(client_addr.sin_port));
        break;
    }

    // 클라이언트에 응답 전송
    write(client_fd, response, strlen(response));

    // 성공한 경우 클라이언트와 연결 유지, 실패한 경우 바로 연결 종료
    if (auth_result == 2) {
      // 로그인 성공 - 추가 메시지 기다리기
      while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(client_fd, buffer, BUFFER_SIZE);

        if (bytes_read <= 0) {
          // 연결 끊김
          break;
        }

        // 여기서 로그아웃 메시지 등을 처리할 수 있음
        // 예: "logout" 메시지 받으면 로그아웃 처리 등
        if (strcmp(buffer, "logout") == 0) {
          printf("[%s] 로그아웃 요청: %s\n", get_time_string(), users[user_index].username);
          break;
        }

        // 추가적인 메시지 처리
        printf("[%s] %s로부터 메시지: %s\n", get_time_string(), users[user_index].username, buffer);

        // 메시지 에코 응답
        sprintf(response, "메시지 수신: %s", buffer);
        write(client_fd, response, strlen(response));
      }

      // 로그아웃 처리
      logout_user(user_index, "연결 종료");
    }
  }

  // 연결 종료
  close(client_fd);

  // 클라이언트 수 감소
  pthread_mutex_lock(&client_count_mutex);
  active_clients--;
  printf("[%s] 클라이언트 연결 종료: %s:%d (현재 연결: %d)\n", get_time_string(), client_ip, ntohs(client_addr.sin_port), active_clients);
  pthread_mutex_unlock(&client_count_mutex);

  // 메모리 해제
  free(data);

  return NULL;
}

int main() {
  int server_fd;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  // 시그널 핸들러 설정
  signal(SIGINT, handle_shutdown);   // Ctrl+C
  signal(SIGTERM, handle_shutdown);  // 정상 종료
  signal(SIGPIPE, SIG_IGN);          // 끊긴 소켓에 쓰기 시도 무시

  // 소켓 생성
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("소켓 생성 실패");
    exit(EXIT_FAILURE);
  }

  // 소켓 옵션 설정
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt 실패");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  // 소켓 바인딩
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("바인드 실패");
    exit(EXIT_FAILURE);
  }

  // 연결 대기
  if (listen(server_fd, MAX_CLIENTS) < 0) {
    perror("리슨 실패");
    exit(EXIT_FAILURE);
  }

  printf("[%s] 서버가 %d 포트에서 실행 중입니다...\n", get_time_string(), PORT);
  printf("[%s] 최대 %d명의 클라이언트를 동시에 처리할 수 있습니다.\n", get_time_string(), MAX_CLIENTS);

  // 클라이언트 요청 수락 루프
  while (1) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    // 클라이언트 연결 수락
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addrlen)) < 0) {
      perror("accept 실패");
      continue;  // 다음 클라이언트 시도
    }

    // 클라이언트 데이터 구조체 할당
    client_data *data = (client_data *)malloc(sizeof(client_data));
    if (!data) {
      perror("메모리 할당 실패");
      close(client_fd);
      continue;
    }

    data->socket = client_fd;
    data->address = client_addr;
    data->user_index = -1;  // 아직 로그인하지 않음

    // 클라이언트 처리용 스레드 생성
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, (void *)data) != 0) {
      perror("스레드 생성 실패");
      free(data);
      close(client_fd);
      continue;
    }

    // 스레드 분리 -
    pthread_detach(thread_id);
  }

  return 0;
}