CC = gcc

# Directories - 최상단에 정의
COMMON_INCLUDE_DIR = common/include
BIN_DIR = bin
OBJ_BASE_DIR = obj
DATA_DIR = data

# --- Client Configuration --- - 최상단에 정의
CLIENT_SRC_DIR = client/src
CLIENT_INCLUDE_DIR = client/include

# --- Server Configuration --- - 최상단에 정의
SERVER_SRC_DIR = server/src
SERVER_INCLUDE_DIR = server/include

# Common Headers - 의존하는 변수 정의 이후
COMMON_PROTOCOL_H = $(COMMON_INCLUDE_DIR)/protocol.h

# Client Specific Headers - 의존하는 변수 정의 이후
CLIENT_GLOBALS_H = $(CLIENT_INCLUDE_DIR)/client_globals.h
CLIENT_NETWORK_H = $(CLIENT_INCLUDE_DIR)/client_network.h

# Server Specific Headers - 의존하는 변수 정의 이후
SERVER_AUTH_MANAGER_H = $(SERVER_INCLUDE_DIR)/auth_manager.h
SERVER_NETWORK_H = $(SERVER_INCLUDE_DIR)/server_network.h


# Client Build Config - 의존하는 변수 정의 이후
CLIENT_OBJ_DIR = $(OBJ_BASE_DIR)/client
CLIENT_TARGET_NAME = rain_client
CLIENT_TARGET = $(BIN_DIR)/$(CLIENT_TARGET_NAME)
NCURSES_CFLAGS_CMD = $(shell pkg-config --cflags ncursesw 2>/dev/null)
NCURSES_LIBS_CMD = $(shell pkg-config --libs ncursesw 2>/dev/null)
CLIENT_EXTRA_CFLAGS = $(if $(NCURSES_CFLAGS_CMD),$(NCURSES_CFLAGS_CMD),-I/usr/include/ncursesw)
CLIENT_EXTRA_LIBS = $(if $(NCURSES_LIBS_CMD),$(NCURSES_LIBS_CMD),-lncurses)
CLIENT_CFLAGS = -Wall -Wextra -g -I$(CLIENT_INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR) $(CLIENT_EXTRA_CFLAGS)
CLIENT_LDFLAGS =
CLIENT_LIBS = $(CLIENT_EXTRA_LIBS) -lpthread
CLIENT_SRCS_NAMES = client_main.c auth_ui.c leaderboard_ui.c client_network.c game_logic.c
CLIENT_SRCS = $(addprefix $(CLIENT_SRC_DIR)/, $(CLIENT_SRCS_NAMES))
CLIENT_OBJS = $(patsubst $(CLIENT_SRC_DIR)/%.c, $(CLIENT_OBJ_DIR)/%.o, $(CLIENT_SRCS))

# Server Build Config - 의존하는 변수 정의 이후
SERVER_OBJ_DIR = $(OBJ_BASE_DIR)/server
SERVER_TARGET_NAME = rain_server
SERVER_TARGET = $(BIN_DIR)/$(SERVER_TARGET_NAME)
SERVER_CFLAGS = -Wall -Wextra -g -I$(SERVER_INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR) -pthread
SERVER_LDFLAGS =
SERVER_LIBS = -lpthread
SERVER_SRCS_NAMES = server_main.c server_network.c auth_manager.c score_manager.c db_handler.c
SERVER_SRCS = $(addprefix $(SERVER_SRC_DIR)/, $(SERVER_SRCS_NAMES))
SERVER_OBJS = $(patsubst $(SERVER_SRC_DIR)/%.c, $(SERVER_OBJ_DIR)/%.o, $(SERVER_SRCS))


.PHONY: all client server clean clean_client clean_server directories

# 변수 값 확인용 echo (모든 변수 정의 이후에 위치)
# $(info CLIENT_INCLUDE_DIR is [$(CLIENT_INCLUDE_DIR)])
# $(info COMMON_INCLUDE_DIR is [$(COMMON_INCLUDE_DIR)])
# $(info CLIENT_GLOBALS_H is [$(CLIENT_GLOBALS_H)])
# $(info CLIENT_NETWORK_H is [$(CLIENT_NETWORK_H)])
# $(info SERVER_INCLUDE_DIR is [$(SERVER_INCLUDE_DIR)])
# $(info SERVER_AUTH_MANAGER_H is [$(SERVER_AUTH_MANAGER_H)])
# $(info SERVER_NETWORK_H is [$(SERVER_NETWORK_H)])
# $(info COMMON_PROTOCOL_H is [$(COMMON_PROTOCOL_H)])


all: directories client server
	@echo "=== Build completed successfully ==="

directories:
	@echo ">>> Creating build directories..."
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(CLIENT_OBJ_DIR)
	@mkdir -p $(SERVER_OBJ_DIR)

client: $(CLIENT_TARGET)
	@echo ">>> Client build complete: $(CLIENT_TARGET_NAME)"

server: $(SERVER_TARGET)
	@echo ">>> Server build complete: $(SERVER_TARGET_NAME)"

# --- Client Build Rules ---
$(CLIENT_TARGET): $(CLIENT_OBJS)
	@echo ">>> Linking client executable..."
	@$(CC) $(CLIENT_CFLAGS) $^ -o $@ $(CLIENT_LDFLAGS) $(CLIENT_LIBS)

$(CLIENT_OBJ_DIR)/client_main.o: $(CLIENT_SRC_DIR)/client_main.c $(CLIENT_INCLUDE_DIR)/auth_ui.h $(CLIENT_INCLUDE_DIR)/leaderboard_ui.h $(CLIENT_NETWORK_H) $(CLIENT_INCLUDE_DIR)/game_logic.h $(COMMON_PROTOCOL_H) $(CLIENT_GLOBALS_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(CLIENT_CFLAGS) -c $< -o $@

$(CLIENT_OBJ_DIR)/auth_ui.o: $(CLIENT_SRC_DIR)/auth_ui.c $(CLIENT_INCLUDE_DIR)/auth_ui.h $(CLIENT_NETWORK_H) $(COMMON_PROTOCOL_H) $(CLIENT_GLOBALS_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(CLIENT_CFLAGS) -c $< -o $@

$(CLIENT_OBJ_DIR)/leaderboard_ui.o: $(CLIENT_SRC_DIR)/leaderboard_ui.c $(CLIENT_INCLUDE_DIR)/leaderboard_ui.h $(CLIENT_NETWORK_H) $(COMMON_PROTOCOL_H) $(CLIENT_GLOBALS_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(CLIENT_CFLAGS) -c $< -o $@

$(CLIENT_OBJ_DIR)/client_network.o: $(CLIENT_SRC_DIR)/client_network.c $(CLIENT_NETWORK_H) $(COMMON_PROTOCOL_H) $(CLIENT_GLOBALS_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(CLIENT_CFLAGS) -c $< -o $@

$(CLIENT_OBJ_DIR)/game_logic.o: $(CLIENT_SRC_DIR)/game_logic.c $(CLIENT_INCLUDE_DIR)/game_logic.h $(CLIENT_GLOBALS_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(CLIENT_CFLAGS) -c $< -o $@

# --- Server Build Rules ---
$(SERVER_TARGET): $(SERVER_OBJS)
	@echo ">>> Linking server executable..."
	@$(CC) $(SERVER_CFLAGS) $^ -o $@ $(SERVER_LDFLAGS) $(SERVER_LIBS)

$(SERVER_OBJ_DIR)/server_main.o: $(SERVER_SRC_DIR)/server_main.c $(SERVER_NETWORK_H) $(SERVER_AUTH_MANAGER_H) $(SERVER_INCLUDE_DIR)/score_manager.h $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(SERVER_CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/server_network.o: $(SERVER_SRC_DIR)/server_network.c $(SERVER_NETWORK_H) $(SERVER_AUTH_MANAGER_H) $(SERVER_INCLUDE_DIR)/score_manager.h $(COMMON_PROTOCOL_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(SERVER_CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/auth_manager.o: $(SERVER_SRC_DIR)/auth_manager.c $(SERVER_AUTH_MANAGER_H) $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(SERVER_CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/score_manager.o: $(SERVER_SRC_DIR)/score_manager.c $(SERVER_INCLUDE_DIR)/score_manager.h $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(SERVER_CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/db_handler.o: $(SERVER_SRC_DIR)/db_handler.c $(SERVER_INCLUDE_DIR)/db_handler.h $(COMMON_PROTOCOL_H)
	@echo "Compiling: $(<F)"
	@$(CC) $(SERVER_CFLAGS) -c $< -o $@

# --- Cleaning Rules ---
clean_client:
	@echo ">>> Cleaning client files..."
	@rm -f $(CLIENT_OBJS) $(CLIENT_TARGET)
	@echo "Client cleanup complete"

clean_server:
	@echo ">>> Cleaning server files..."
	@rm -f $(SERVER_OBJS) $(SERVER_TARGET)
	@echo "Server cleanup complete"

clean:
	@echo ">>> Cleaning all build artifacts..."
	@rm -f $(CLIENT_OBJS) $(CLIENT_TARGET)
	@rm -f $(SERVER_OBJS) $(SERVER_TARGET)
	@rm -rf $(OBJ_BASE_DIR)
	@rm -rf $(BIN_DIR)
	@if [ -d "$(DATA_DIR)" ]; then \
		echo "Removing data directory: $(DATA_DIR)"; \
		rm -rf $(DATA_DIR); \
	fi
	@echo "=== Cleanup complete ==="