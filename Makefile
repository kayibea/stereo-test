CC = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Werror -O2
LDFLAGS = -lasound -lm

SRC = stereo-test.c
BIN = stereo-test

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN)
