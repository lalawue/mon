PREFIX ?= /usr/local
SRC = src/mon_sched.c deps/ms.c deps/json.c deps/config.c deps/status.c deps/cron.c deps/json_file.c
OBJ = $(SRC:.c=.o)
CFLAGS = -O2 -D_GNU_SOURCE -std=c99 -I deps/
BIN = mon_sched

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ -lm

.SUFFIXES: .c .o
.c.o:
	$(CC) $< $(CFLAGS) -c -o $@

install:
	cp -f $(BIN) $(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN) $(OBJ) *.log *.pid

.PHONY: clean install uninstall
