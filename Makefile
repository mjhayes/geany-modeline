# vim: noexpandtab:ts=8

PREFIX ?= $(shell pkg-config --variable=prefix geany)
CC      = gcc
CFLAGS  = -g -O2 -Wall -fPIC
LDFLAGS = -shared
LIBS    = $(shell pkg-config --libs geany)
INCLUDE = $(shell pkg-config --cflags geany)
PLUGDIR = $(shell echo geany)

PROG    = modeline.so
OBJS    = modeline.o
HEADERS =

all: modeline.so

$(PROG): $(OBJS)
	echo "LD $@"
	$(CC) $(OBJS) $(LIBS) $(LDFLAGS) -o $@

$(OBJS): %.o: %.c $(HEADERS)
	echo "CC $<"
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

install: all
	echo "INSTALL $(DESTDIR)$(PREFIX)/lib64/$(PLUGDIR)/$(PROG)"
	install -s $(PROG) $(DESTDIR)$(PREFIX)/lib64/$(PLUGDIR)

userinstall: all
	echo "INSTALL ${HOME}/.config/geany/plugins"
	install -s $(PROG) ${HOME}/.config/geany/plugins

clean:
	rm -f $(OBJS) $(PROG)

.SILENT:
