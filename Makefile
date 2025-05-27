# =============================
# Makefile - Rain Typing Game
# =============================
# 클라이언트/서버를 컴파일하고 실행파일을 생성합니다.
# SHA-256 해시를 위한 OpenSSL 라이브러리 포함

CC = gcc
CFLAGS = -Wall -Wextra -g -O2

# 디렉토리 경로
CLIENT_SRC = client/src
SERVER_SRC = server/src
COMMON_SRC = common/src
CLIENT_INC = client/include
SERVER_INC = server/include
COMMON_INC = common/include
OBJ_DIR = obj
BIN_DIR = bin

# 클라이언트 오브젝트 목록
CLIENT_OBJS = \
	$(OBJ_DIR)/client/client_main.o \
	$(OBJ_DIR)/client/auth_ui.o \
	$(OBJ_DIR)/client/leaderboard_ui.o \
	$(OBJ_DIR)/client/client_network.o \
	$(OBJ_DIR)/client/game_logic.o \
	$(OBJ_DIR)/common/hash_util.o  # SHA-256 해시 유틸 포함

# 서버 오브젝트 목록
SERVER_OBJS = \
	$(OBJ_DIR)/server/server_main.o \
	$(OBJ_DIR)/server/server_network.o \
	$(OBJ_DIR)/server/auth_manager.o \
	$(OBJ_DIR)/server/score_manager.o \
	$(OBJ_DIR)/server/db_handler.o \
	$(OBJ_DIR)/server/word_manager.o \
	$(OBJ_DIR)/common/hash_util.o  # SHA-256 해시 유틸 포함

# 링크 라이브러리
LIBS_CLIENT = -lncursesw -lpthread -lssl -lcrypto
LIBS_SERVER = -lpthread -lssl -lcrypto

# 최종 빌드 대상
all: $(BIN_DIR)/rain_client $(BIN_DIR)/rain_server

# 클라이언트 빌드
$(BIN_DIR)/rain_client: $(CLIENT_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS_CLIENT)

# 서버 빌드
$(BIN_DIR)/rain_server: $(SERVER_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS_SERVER)

# 오브젝트 빌드 규칙
$(OBJ_DIR)/client/%.o: $(CLIENT_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(CLIENT_INC) -I$(COMMON_INC) -c $< -o $@

$(OBJ_DIR)/server/%.o: $(SERVER_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SERVER_INC) -I$(COMMON_INC) -c $< -o $@

$(OBJ_DIR)/common/%.o: $(COMMON_SRC)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(COMMON_INC) -c $< -o $@

# 클린 명령어
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)/rain_client $(BIN_DIR)/rain_server
	@echo ">>> Cleaning build artifacts (users.txt, scores.txt 보존)…"
	@echo "=== Cleanup complete ==="
