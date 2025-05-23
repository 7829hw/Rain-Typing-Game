###############################################################################
# Rain-Typing-Game  ──  unified Makefile
#  * 빌드 결과
#       bin/rain_client      ← ncurses 클라이언트
#       bin/rain_server      ← TCP 서버
###############################################################################

# ───── 공통 ────────────────────────────────────────────────────────────────────
CC       := gcc
CFLAGS   := -Wall -Wextra -g -O2
LDFLAGS  :=
LIBS     := -lpthread

COMMON_INC := common/include
CLIENT_INC := client/include
SERVER_INC := server/include

OBJ_DIR := obj
BIN_DIR := bin

# 빌드 디렉터리 생성
$(shell mkdir -p $(OBJ_DIR)/client $(OBJ_DIR)/server $(BIN_DIR))

# ───── 클라이언트 ─────────────────────────────────────────────────────────────
CLIENT_SRC := \
    client/src/client_main.c \
    client/src/auth_ui.c \
    client/src/leaderboard_ui.c \
    client/src/client_network.c \
    client/src/game_logic.c

CLIENT_OBJS := $(patsubst client/src/%.c,$(OBJ_DIR)/client/%.o,$(CLIENT_SRC))
CLIENT_CFLAGS := $(CFLAGS) -I$(CLIENT_INC) -I$(COMMON_INC)
CLIENT_LIBS   := -lncursesw -lpthread
CLIENT_BIN    := $(BIN_DIR)/rain_client

# ───── 서버 ───────────────────────────────────────────────────────────────────
SERVER_SRC := \
    server/src/server_main.c \
    server/src/server_network.c \
    server/src/auth_manager.c \
    server/src/score_manager.c \
    server/src/db_handler.c \
    server/src/word_manager.c

SERVER_OBJS := $(patsubst server/src/%.c,$(OBJ_DIR)/server/%.o,$(SERVER_SRC))
SERVER_CFLAGS := $(CFLAGS) -I$(SERVER_INC) -I$(COMMON_INC)
SERVER_BIN    := $(BIN_DIR)/rain_server

# ───── 기본 타깃 ──────────────────────────────────────────────────────────────
.PHONY: all client server clean

all: $(CLIENT_BIN) $(SERVER_BIN)
	@echo "=== Build finished successfully ==="

# ───── 클라이언트 빌드 ────────────────────────────────────────────────────────
$(CLIENT_BIN): $(CLIENT_OBJS)
	@echo ">>> Linking client executable..."
	$(CC) $^ -o $@ $(LDFLAGS) $(CLIENT_LIBS)

$(OBJ_DIR)/client/%.o: client/src/%.c
	@echo "Compiling (Client): $<"
	$(CC) $(CLIENT_CFLAGS) -c $< -o $@

client: $(CLIENT_BIN)

# ───── 서버 빌드 ──────────────────────────────────────────────────────────────
$(SERVER_BIN): $(SERVER_OBJS)
	@echo ">>> Linking server executable..."
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(OBJ_DIR)/server/%.o: server/src/%.c
	@echo "Compiling (Server): $<"
	$(CC) $(SERVER_CFLAGS) -c $< -o $@

server: $(SERVER_BIN)

# ───── 클린업 ─────────────────────────────────────────────────────────────────
clean:
	@echo ">>> Cleaning build artifacts (words.txt, users.txt, scores.txt 보존)…"
	@rm -rf $(OBJ_DIR)
	@rm -f $(CLIENT_BIN) $(SERVER_BIN)
	@find $(BIN_DIR) -type f ! \( -name 'words.txt' -o -name 'users.txt' -o -name 'scores.txt' \) -delete 2>/dev/null || true
	@rmdir --ignore-fail-on-non-empty $(BIN_DIR) 2>/dev/null || true
	@if [ -d data ]; then \
		find data -type f ! \( -name 'words.txt' -o -name 'users.txt' -o -name 'scores.txt' \) -delete 2>/dev/null || true; \
		rmdir --ignore-fail-on-non-empty data 2>/dev/null || true; \
	fi
	@echo "=== Cleanup complete ==="

###############################################################################
# 사용 예:
#   $ make          # 클라이언트 + 서버 전체 빌드
#   $ make client   # 클라이언트만
#   $ make server   # 서버만
#   $ make clean    # words.txt, users.txt, scores.txt 제외 모든 산출물 삭제
###############################################################################