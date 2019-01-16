CFLAGS ?= -O2 -pipe
CFLAGS += -Wall -pedantic

BIN = smtpd.plugin

all: $(BIN)

smtpd.plugin: err.o

.PHONY: clean
clean:
	$(RM) *.o $(BIN)
