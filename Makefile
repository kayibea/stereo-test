CC = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Werror -O2
LDFLAGS = -lasound -lm

SRC = stereo-test.c
BIN = stereo-test

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: all clean install uninstall

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -f $(BIN)
