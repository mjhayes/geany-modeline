
PREFIX  = /usr
CC      = gcc
LIBTOOL	= libtool
PKG_CONFIG ?= pkg-config
#PKG_CONFIG ?= PKG_CONFIG_PATH=$(HOME)/lib/pkgconfig pkg-config

CFLAGS  = -g -O2 -Wall -fPIC
LDFLAGS = -rpath $(HOME)/lib


LIBS    = $(shell $(PKG_CONFIG) --libs geany)
INCLUDE = $(shell $(PKG_CONFIG) --cflags geany)

PROG    = libmodeline.la
OBJS    = modeline.lo
HEADERS =

all: $(PROG)

$(PROG): $(OBJS)
	echo "LD $@"
	$(LIBTOOL) --mode=link $(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(OBJS): %.lo: %.c $(HEADERS)
	echo "CC $<"
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

install: all
	echo "INSTALL $(DESTDIR)$(PREFIX)/lib/geany/$(PROG)"
	mkdir -p $(DESTDIR)$(PREFIX)/lib/geany
	install -s $(PROG) $(DESTDIR)$(PREFIX)/lib/geany

clean:
	rm -fr $(OBJS) $(PROG) .libs

.SILENT:
