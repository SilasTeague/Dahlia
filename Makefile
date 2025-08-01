CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
SRC = src
BIN = dahlia

SRCS = $(wildcard $(SRC)/*.c)

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(BIN)

clean:
	rm -f $(BIN)

.PHONY: all clean
