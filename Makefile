PREFIX ?= /usr
PLUGIN_DIR = ${DESTDIR}${PREFIX}/libexec/netdata/plugins.d

VERSION = 0.0

CFLAGS ?= -O2 -pipe
CFLAGS += -Wall -pedantic
CFLAGS += -std=c11
CFLAGS += -Werror=implicit-function-declaration

CPPFLAGS += -D_GNU_SOURCE

BIN = qmail.plugin smtpd.plugin

.PHONY: all
all: $(BIN)

## Dependencies
smtpd.plugin: err.o flush.o smtpd.plugin.o netdata.o signal.o timer.o
qmail.plugin: qmail.plugin.o flush.o signal.o timer.o

err.o: err.c err.h
flush.o: flush.c flush.h
smtpd.plugin.o: err.h flush.h netdata.h timer.o
qmail.plugin.o: flush.h signal.h timer.h
signal.o: signal.c signal.h
netdata.o: netdata.c netdata.h
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
