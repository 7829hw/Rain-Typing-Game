CC = gcc
CFLAGS = -Wall -Wextra -pedantic
LDFLAGS = -pthread -lncursesw
TARGET = rain_typing_game
SRC = rain_typing_game.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean