CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm
INCLUDES = -Iinclude
LDFLAGS = 

SRC_DIR = src
INC_DIR = include
BIN = shell.out

SRCS = $(wildcard $(SRC_DIR)/*.c)

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRCS) $(INC_DIR)/*.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f $(BIN)


