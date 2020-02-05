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
	echo "INSTALL $(DESTDIR)$(PREFIX)/lib/$(PLUGDIR)/$(PROG)"
	install -s $(PROG) $(DESTDIR)$(PREFIX)/lib/$(PLUGDIR)

clean:
	rm -f $(OBJS) $(PROG)

.SILENT:
