CC = gcc

# Directories
COMMON_INCLUDE_DIR = common/include
BIN_DIR = bin
OBJ_BASE_DIR = obj
DATA_DIR = data # 프로그램에 의해 생성될 데이터 디렉토리 (clean 시 참조)

# Common Headers
COMMON_PROTOCOL_H = $(COMMON_INCLUDE_DIR)/protocol.h

# --- Client Configuration ---
# (변경 없음)
CLIENT_SRC_DIR = client/src
CLIENT_INCLUDE_DIR = client/include
CLIENT_OBJ_DIR = $(OBJ_BASE_DIR)/client
CLIENT_TARGET_NAME = rain_client
CLIENT_TARGET = $(BIN_DIR)/$(CLIENT_TARGET_NAME)
NCURSES_CFLAGS_CMD = $(shell pkg-config --cflags ncursesw 2>/dev/null)
NCURSES_LIBS_CMD = $(shell pkg-config --libs ncursesw 2>/dev/null)
CLIENT_EXTRA_CFLAGS = $(if $(NCURSES_CFLAGS_CMD),$(NCURSES_CFLAGS_CMD),-I/usr/include/ncursesw)
CLIENT_EXTRA_LIBS = $(if $(NCURSES_LIBS_CMD),$(NCURSES_LIBS_CMD),-lncursesw)
CLIENT_CFLAGS = -Wall -Wextra -g -I$(CLIENT_INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR) $(CLIENT_EXTRA_CFLAGS)
CLIENT_LDFLAGS =
CLIENT_LIBS = $(CLIENT_EXTRA_LIBS) -lpthread # -lpthread 추가
CLIENT_SRCS_NAMES = main_client.c auth_ui.c leaderboard_ui.c network_client.c game_logic.c
CLIENT_SRCS = $(addprefix $(CLIENT_SRC_DIR)/, $(CLIENT_SRCS_NAMES))
CLIENT_OBJS = $(patsubst $(CLIENT_SRC_DIR)/%.c, $(CLIENT_OBJ_DIR)/%.o, $(CLIENT_SRCS))

# --- Server Configuration ---
# (변경 없음)
SERVER_SRC_DIR = server/src
SERVER_INCLUDE_DIR = server/include
SERVER_OBJ_DIR = $(OBJ_BASE_DIR)/server
SERVER_TARGET_NAME = rain_server
SERVER_TARGET = $(BIN_DIR)/$(SERVER_TARGET_NAME)
SERVER_CFLAGS = -Wall -Wextra -g -I$(SERVER_INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR) -pthread
SERVER_LDFLAGS =
SERVER_LIBS = -lpthread
SERVER_SRCS_NAMES = main_server.c network_server.c auth.c score_manager.c db_handler.c
SERVER_SRCS = $(addprefix $(SERVER_SRC_DIR)/, $(SERVER_SRCS_NAMES))
SERVER_OBJS = $(patsubst $(SERVER_SRC_DIR)/%.c, $(SERVER_OBJ_DIR)/%.o, $(SERVER_SRCS))


.PHONY: all client server clean clean_client clean_server directories

all: directories client server

# 'directories' 타겟은 빌드에 필요한 디렉토리만 생성
directories:
	@echo "Creating build directories..."
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(CLIENT_OBJ_DIR)
	@mkdir -p $(SERVER_OBJ_DIR)
	# DATA_DIR 생성은 프로그램(db_handler.c)에서 처리

client: $(CLIENT_TARGET)

server: $(SERVER_TARGET)

# --- Client Build Rules ---
# (변경 없음)
$(CLIENT_TARGET): $(CLIENT_OBJS)
	@echo "Linking Client: $@"
	$(CC) $(CLIENT_CFLAGS) $^ -o $@ $(CLIENT_LDFLAGS) $(CLIENT_LIBS)
	@echo "Client built successfully: $@"

$(CLIENT_OBJ_DIR)/main_client.o: $(CLIENT_SRC_DIR)/main_client.c $(CLIENT_INCLUDE_DIR)/auth_ui.h $(CLIENT_INCLUDE_DIR)/leaderboard_ui.h $(CLIENT_INCLUDE_DIR)/network_client.h $(CLIENT_INCLUDE_DIR)/game_logic.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Client): $<"
	$(CC) $(CLIENT_CFLAGS) -c $< -o $@

# ... (다른 클라이언트 .o 파일 규칙 생략, 이전과 동일) ...
$(CLIENT_OBJ_DIR)/auth_ui.o: $(CLIENT_SRC_DIR)/auth_ui.c $(CLIENT_INCLUDE_DIR)/auth_ui.h $(CLIENT_INCLUDE_DIR)/network_client.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Client): $<"
	$(CC) $(CLIENT_CFLAGS) -c $< -o $@

$(CLIENT_OBJ_DIR)/leaderboard_ui.o: $(CLIENT_SRC_DIR)/leaderboard_ui.c $(CLIENT_INCLUDE_DIR)/leaderboard_ui.h $(CLIENT_INCLUDE_DIR)/network_client.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Client): $<"
	$(CC) $(CLIENT_CFLAGS) -c $< -o $@

$(CLIENT_OBJ_DIR)/network_client.o: $(CLIENT_SRC_DIR)/network_client.c $(CLIENT_INCLUDE_DIR)/network_client.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Client): $<"
	$(CC) $(CLIENT_CFLAGS) -c $< -o $@

$(CLIENT_OBJ_DIR)/game_logic.o: $(CLIENT_SRC_DIR)/game_logic.c $(CLIENT_INCLUDE_DIR)/game_logic.h
	@echo "Compiling (Client): $<"
	$(CC) $(CLIENT_CFLAGS) -c $< -o $@

# --- Server Build Rules ---
# (변경 없음)
$(SERVER_TARGET): $(SERVER_OBJS)
	@echo "Linking Server: $@"
	$(CC) $(SERVER_CFLAGS) $^ -o $@ $(SERVER_LDFLAGS) $(SERVER_LIBS)
	@echo "Server built successfully: $@"

$(SERVER_OBJ_DIR)/main_server.o: $(SERVER_SRC_DIR)/main_server.c $(SERVER_INCLUDE_DIR)/network_server.h $(SERVER_INCLUDE_DIR)/auth.h $(SERVER_INCLUDE_DIR)/score_manager.h $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Server): $<"
	$(CC) $(SERVER_CFLAGS) -c $< -o $@

# ... (다른 서버 .o 파일 규칙 생략, 이전과 동일) ...
$(SERVER_OBJ_DIR)/network_server.o: $(SERVER_SRC_DIR)/network_server.c $(SERVER_INCLUDE_DIR)/network_server.h $(SERVER_INCLUDE_DIR)/auth.h $(SERVER_INCLUDE_DIR)/score_manager.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Server): $<"
	$(CC) $(SERVER_CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/auth.o: $(SERVER_SRC_DIR)/auth.c $(SERVER_INCLUDE_DIR)/auth.h $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Server): $<"
	$(CC) $(SERVER_CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/score_manager.o: $(SERVER_SRC_DIR)/score_manager.c $(SERVER_INCLUDE_DIR)/score_manager.h $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Server): $<"
	$(CC) $(SERVER_CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/db_handler.o: $(SERVER_SRC_DIR)/db_handler.c $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling (Server): $<"
	$(CC) $(SERVER_CFLAGS) -c $< -o $@


# --- Cleaning Rules ---
clean_client:
	@echo "Cleaning client objects and target..."
	rm -f $(CLIENT_OBJS) $(CLIENT_TARGET)

clean_server:
	@echo "Cleaning server objects and target..."
	rm -f $(SERVER_OBJS) $(SERVER_TARGET)

# 'clean' 타겟은 빌드 결과물 및 프로그램에 의해 생성된 DATA_DIR도 삭제
clean:
	@echo "Cleaning all generated files..."
	rm -f $(CLIENT_OBJS) $(CLIENT_TARGET)
	rm -f $(SERVER_OBJS) $(SERVER_TARGET)
	rm -rf $(OBJ_BASE_DIR)
	rm -rf $(BIN_DIR)
	# 프로그램이 생성한 데이터 디렉토리 삭제
	rm -rf $(DATA_DIR)
	@echo "Clean complete. Data in $(DATA_DIR) (if existed) has been removed."