# Rain Typing Game (빗물 타이핑 게임)

## 📖 프로젝트 개요 (Project Overview)

"Rain Typing Game"은 화면 상단에서 단어들이 빗방울처럼 떨어지는 고전적인 타이핑 게임입니다. 플레이어는 각 단어가 화면 바닥에 닿기 전에 정확히 타이핑하여 제거해야 합니다. 이 게임은 C언어로 작성되었으며, 터미널 UI를 위해 `ncurses` 라이브러리를 사용하고, 각 단어의 독립적인 움직임을 위해 `pthreads` (POSIX Threads)를 활용합니다. 플레이어는 게임 시작 전 로그인을 해야 하며, 다른 플레이어들과의 점수를 리더보드에서 확인할 수 있습니다.

This "Rain Typing Game" is a classic typing game where words fall from the top of the screen like raindrops. Players must accurately type each word before it hits the bottom of the screen to eliminate it. The game is written in C, utilizing the `ncurses` library for the terminal user interface and `pthreads` (POSIX Threads) for the independent movement of each word. Players must log in before starting the game, and can check their scores against others on a leaderboard.

## ✨ 주요 기능 (Key Features)

*   **단어 낙하 (Falling Words):** 화면 상단에서 단어들이 무작위로 생성되어 아래로 떨어집니다.
*   **타이핑 입력 (Typing Input):** 플레이어는 떨어지는 단어와 일치하는 문자열을 입력하여 단어를 제거합니다.
*   **점수 시스템 (Scoring System):** 성공적으로 단어를 제거할 때마다 점수를 얻습니다.
*   **생명 시스템 (Lives System):** 단어가 바닥에 닿으면 생명을 잃습니다. 생명이 모두 소진되면 게임이 종료됩니다.
*   **로그인 기능 (Login Functionality):** 플레이어는 게임 시작 전에 사용자 이름을 입력하여 로그인합니다.
*   **리더보드 (Leaderboard):** 게임 종료 후 또는 메뉴에서 역대 최고 점수들을 확인할 수 있습니다.
*   **멀티스레딩 (Multithreading):** 각 떨어지는 단어는 개별 스레드에서 처리되어 동시에 여러 단어가 부드럽게 움직입니다.
*   **Ncurses 기반 UI (Ncurses-based UI):** 모든 게임 인터페이스는 터미널 환경에서 `ncurses`를 통해 구현됩니다.

## 🛠️ 기술 스택 (Tech Stack)

*   **언어 (Language):** C
*   **라이브러리 (Libraries):**
    *   `ncurses`: 터미널 기반 사용자 인터페이스 (Terminal-based User Interface)
    *   `pthread`: 멀티스레딩 (Multithreading)
*   **시스템 콜 (System Calls):** 파일 I/O (`open`, `read`, `write`, `close` 등), 시간 관련 (`sleep`, `usleep`), 프로세스/스레드 관리 등 다양한 시스템 콜이 사용될 수 있습니다. (Various system calls for file I/O, time, process/thread management, etc., may be used.)

## ⚙️ 시스템 요구사항 (System Requirements)

*   Linux 또는 macOS (Windows의 경우 WSL2 권장)
*   GCC (GNU Compiler Collection) 또는 Clang
*   `ncurses` 개발 라이브러리 (`libncurses-dev` 또는 `ncurses-devel`)
*   `make` (빌드 자동화를 위해)

## 🚀 빌드 및 실행 (Build & Run)

### 1. 저장소 복제 (Clone Repository)

```bash
git clone git@github.com:7829hw/Rain-Typing-Game.git
cd rain-typing-game
```

### 2. 의존성 설치 (Install Dependencies)

*   **Debian/Ubuntu 기반:**
    ```bash
    sudo apt-get update
    sudo apt-get install libncurses5-dev libncursesw5-dev build-essential
    ```
*   **Fedora/RHEL/CentOS 기반:**
    ```bash
    sudo dnf install ncurses-devel ncurses-libs make gcc
    ```
*   **macOS (Homebrew 사용):**
    ```bash
    brew install ncurses
    # macOS는 보통 기본적으로 개발 도구가 설치되어 있습니다.
    ```

### 3. 빌드 (Build)

프로젝트 루트 디렉토리에서 다음 명령어를 실행합니다:
(Execute the following command in the project root directory:)

```bash
make
```

또는 `Makefile`이 없다면 직접 컴파일:
(Or compile directly if there is no `Makefile`:)

```bash
gcc -o rain_typing_game -lncurses -lpthread
```

### 4. 실행 (Run)

```bash
./rain_typing_game
```

## 룰 및 게임플레이 (Rules & Gameplay)

1.  **로그인 (Login):** 게임을 시작하면 사용자 이름을 입력하라는 메시지가 표시됩니다. 기존 사용자이거나 새 사용자일 수 있습니다.
2.  **게임 시작 (Start Game):** 로그인 후, 메인 메뉴에서 '게임 시작'을 선택합니다.
3.  **단어 입력 (Type Words):** 단어들이 화면 상단에서 떨어지기 시작합니다. 현재 입력해야 할 활성 단어(예: 가장 먼저 입력해야 하거나, 특정 색상으로 강조된 단어)가 표시될 수 있습니다.
4.  **단어 제거 (Eliminate Words):** 단어를 정확히 입력하고 `Enter` 키(또는 스페이스바, 게임 설계에 따라 다름)를 누르면 해당 단어가 사라지고 점수를 얻습니다.
5.  **생명 감소 (Lose Life):** 단어가 화면 바닥에 닿으면 생명(Life)이 하나 줄어듭니다.
6.  **게임 오버 (Game Over):** 모든 생명을 잃으면 게임이 종료됩니다.
7.  **리더보드 확인 (Check Leaderboard):** 게임 종료 후 또는 메인 메뉴에서 리더보드를 선택하여 최고 점수들을 확인할 수 있습니다.

## 💡 향후 개선 사항 (Future Enhancements)

*   **난이도 조절 (Difficulty Levels):** 단어 떨어지는 속도, 단어 길이 등을 조절.
*   **다양한 단어 목록 (Various Word Lists):** 주제별 또는 언어별 단어 목록 선택 기능.
*   **특수 효과/아이템 (Special Effects/Items):** 게임 플레이에 영향을 주는 아이템 (예: 시간 느리게, 모든 단어 제거).
*   **보다 정교한 UI (More Sophisticated UI):** 색상, 애니메이션 등 개선.
