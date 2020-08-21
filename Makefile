# SPDX-License-Identifier: GPL-3.0-or-later

PREFIX  ?= /usr
LIB_DIR ?= lib

PLUGIN_DIR = ${DESTDIR}${PREFIX}/libexec/netdata/plugins.d
CONF_DIR   = ${DESTDIR}${PREFIX}/${LIB_DIR}/netdata/conf.d
HEALTH_DIR = ${CONF_DIR}/health.d

VERSION = 0.7

CFLAGS ?= -O2 -pipe
CFLAGS += -Wall -pedantic
CFLAGS += -std=c11
CFLAGS += -Werror=implicit-function-declaration

CPPFLAGS += -D_GNU_SOURCE

BIN = \
	qmail.plugin \
	scanner.plugin \
	svstat.plugin \
	parser.plugin

OBJS_COMMON = flush.o fs.o netdata.o signal.o timer.o vector.o

HEADERS_COMMON = fs.h err.h timer.h vector.h

.PHONY: all
all: $(BIN)

## Dependencies
qmail.plugin: qmail.plugin.o $(OBJS_COMMON) queue.o send.o smtp.o
scanner.plugin: scanner.plugin.o $(OBJS_COMMON) scanner.o
svstat.plugin: fs.o netdata.o timer.o vector.o
parser.plugin: parser.plugin.o $(OBJS_COMMON) parser.o

qmail.plugin.o: $(HEADERS_COMMON) flush.h signal.h queue.h send.h smtp.h
scanner.plugin.o: $(HEADERS_COMMON) flush.h signal.h scanner.h
svstat.plugin.o: $(HEADERS_COMMON) netdata.h
parser.plugin.o: flush.h fs.h signal.h timer.h vector.h

flush.o: flush.c flush.h
fs.o: fs.c fs.h err.h callbacks.h
netdata.o: netdata.c netdata.h
queue.o: queue.c queue.h callbacks.h netdata.h err.h fs.h
send.o: send.c send.h callbacks.h netdata.h
signal.o: signal.c signal.h
smtp.o: smtp.c smtp.h callbacks.h netdata.h
timer.o: timer.c timer.h
vector.o: vector.c vector.h err.h
parser.o: parser.c parser.h

.PHONY: install
install: all
	@echo installing executables to $(PLUGIN_DIR)
	install -d $(PLUGIN_DIR)
	install $(BIN) $(PLUGIN_DIR)
	install -d $(HEALTH_DIR)
	install health.d/* $(HEALTH_DIR)

.PHONY: clean
clean:
	$(RM) *.o $(BIN)
