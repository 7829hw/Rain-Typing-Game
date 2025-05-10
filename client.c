#include <arpa/inet.h>
#include <netdb.h>  // DNS 조회를 위한 헤더
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_DOMAIN "elec462teamproject.duckdns.org"  // 여기에 원하는 도메인 주소를 하드코딩

int main() {
  int sock = 0;
  struct sockaddr_in serv_addr;
  char username[50];
  char password[50];
  char buffer[BUFFER_SIZE] = {0};

  // 소켓 생성 (시스템 콜)
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("소켓 생성 실패");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  // 하드코딩된 도메인 주소 처리
  struct hostent *he;
  struct in_addr **addr_list;

  printf("%s 도메인을 해석 중...\n", SERVER_DOMAIN);

  if ((he = gethostbyname(SERVER_DOMAIN)) == NULL) {
    herror("gethostbyname 실패");
    return -1;
  }

  // IP 주소 목록 가져오기
  addr_list = (struct in_addr **)he->h_addr_list;

  if (addr_list[0] != NULL) {
    // 첫 번째 IP 주소 사용
    serv_addr.sin_addr = *addr_list[0];
    printf("도메인 %s의 IP: %s\n", SERVER_DOMAIN, inet_ntoa(serv_addr.sin_addr));
  } else {
    printf("유효한 IP 주소를 찾을 수 없습니다.\n");
    return -1;
  }

  printf("서버 %s:%d에 연결 시도 중...\n", SERVER_DOMAIN, PORT);

  // 서버에 연결 (시스템 콜)
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("연결 실패");
    return -1;
  }

  printf("서버에 연결되었습니다.\n");

  // 로그인 정보 입력
  printf("사용자 이름: ");
  scanf("%s", username);

  printf("비밀번호: ");
  scanf("%s", password);

  // 로그인 요청 메시지 작성
  char login_msg[100];
  sprintf(login_msg, "%s %s", username, password);

  // 서버에 로그인 정보 전송 (시스템 콜)
  send(sock, login_msg, strlen(login_msg), 0);
  printf("로그인 정보를 서버에 전송했습니다.\n");

  // 서버로부터 응답 수신 (시스템 콜)
  int bytes_read = read(sock, buffer, BUFFER_SIZE);
  buffer[bytes_read] = '\0';

  printf("서버 응답: %s\n", buffer);

  // 연결 종료 (시스템 콜)
  close(sock);
  return 0;
}