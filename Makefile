# SPDX-License-Identifier: GPL-3.0-or-later

PREFIX ?= /usr
PLUGIN_DIR = ${DESTDIR}${PREFIX}/libexec/netdata/plugins.d

VERSION = 0.3.2

CFLAGS ?= -O2 -pipe
CFLAGS += -Wall -pedantic
CFLAGS += -std=c11
CFLAGS += -Werror=implicit-function-declaration

CPPFLAGS += -D_GNU_SOURCE

BIN = \
	qmail.plugin \
	scanner.plugin

SHARED_OBJ = flush.o fs.o netdata.o signal.o timer.o vector.o

.PHONY: all
all: $(BIN)

## Dependencies
qmail.plugin: qmail.plugin.o $(SHARED_OBJ) queue.o send.o smtp.o
scanner.plugin: scanner.plugin.o $(SHARED_OBJ) scanner.o

qmail.plugin.o: flush.h fs.h send.h signal.h timer.h vector.h
scanner.plugin.o: flush.h

err.o: err.c err.h
flush.o: flush.c flush.h
fs.o: fs.c fs.h
netdata.o: netdata.c netdata.h
queue.o: queue.c callbacks.h netdata.h queue.h
send.o: send.c send.h
signal.o: signal.c signal.h
smtp.o: smtp.c smtp.h
timer.o: timer.c timer.h
vector.o: vector.c vector.h

.PHONY: install
install: all
	@echo installing executables to $(PLUGIN_DIR)
	install -d $(PLUGIN_DIR)
	install $(BIN) $(PLUGIN_DIR)

.PHONY: clean
clean:
	$(RM) *.o $(BIN)
