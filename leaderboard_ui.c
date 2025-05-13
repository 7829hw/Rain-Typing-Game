// client/src/leaderboard_ui.c
#include <ncursesw/curses.h>      // ncurses: 터미널 기반 텍스트 UI 라이브러리
#include "network_client.h"       // 서버와 통신하는 함수들 (request_leaderboard 등)
#include "../common/protocol.h"   // LeaderboardResponse 구조체 정의 포함

// 서버에서 리더보드 정보를 받아와 화면에 출력하는 함수
void show_leaderboard_ui() {
    // 서버에 리더보드 요청을 보내고 응답 받기
    LeaderboardResponse res = request_leaderboard();

    // 화면 초기화
    clear();
    mvprintw(2, 10, "===== LEADERBOARD =====");

    // 컬럼 헤더 출력
    mvprintw(4, 10, "Rank  ID              Score");
    mvprintw(5, 10, "-------------------------------");

    // 리더보드 상위 10개 항목까지 출력
    for (int i = 0; i < res.count && i < 10; i++) {
        // 각 항목을 순위, ID, 점수 순서로 출력 (정렬은 서버에서 처리)
        mvprintw(6 + i, 10, "%2d    %-15s %5d",
            i + 1,                         // Rank (1부터 시작)
            res.entries[i].username,      // 사용자 ID
            res.entries[i].score);        // 점수
    }

    // 사용자 입력 대기 메시지
    mvprintw(18, 10, "Press any key to return..."); // 메인 메뉴로 돌아감
    refresh();  // 화면 갱신
    getch();    // 사용자 키 입력 대기 후 종료
}
