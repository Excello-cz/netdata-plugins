CFLAGS ?= -O2
CFLAGS += -pipe -Wall -pedantic

all: smtpd.plugin

smtpd.plugin: err.o
