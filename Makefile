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
qmail.plugin: qmail.plugin.o flush.o fs.o netdata.o send.o signal.o smtp.o timer.o vector.o
smtpd.plugin: err.o flush.o smtpd.plugin.o netdata.o smtp.o signal.o timer.o

qmail.plugin.o: flush.h fs.h send.h signal.h timer.h vector.h
smtpd.plugin.o: err.h flush.h netdata.h smtp.h timer.o

err.o: err.c err.h
flush.o: flush.c flush.h
fs.o: fs.c fs.h
netdata.o: netdata.c netdata.h
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
