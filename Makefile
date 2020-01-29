PREFIX ?= /usr/local
SRC = src/mon_sched.c deps/ms.c deps/json.c deps/config.c deps/status.c
OBJ = $(SRC:.c=.o)
CFLAGS = -D_GNU_SOURCE -std=c99 -I deps/
BIN = mon_sched

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@

.SUFFIXES: .c .o
.c.o:
	$(CC) $< $(CFLAGS) -c -o $@

install:
	cp -f mon $(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN) $(OBJ) *.log *.pid

.PHONY: clean install uninstall