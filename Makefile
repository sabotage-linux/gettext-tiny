prefix=/usr/local
bindir=$(prefix)/bin
includedir=$(prefix)/include
libdir=$(prefix)/lib
sysconfdir=$(prefix)/etc

LIBSRC = $(sort $(wildcard libintl/*.c))

SRCS = $(LIBSRC)
OBJS = $(SRCS:.c=.o)
LIBOBJS = $(LIBSRC:.c=.o)

HEADERS = libintl.h
ALL_INCLUDES = $(HEADERS)

ALL_LIBS=libintl.a
ALL_TOOLS=msgfmt

CFLAGS=-O0 -fPIC

AR      ?= $(CROSS_COMPILE)ar
RANLIB  ?= $(CROSS_COMPILE)ranlib
CC      ?= $(CROSS_COMPILE)cc

-include config.mak

BUILDCFLAGS=$(CFLAGS)

all: $(ALL_LIBS) $(ALL_TOOLS)

install: $(ALL_LIBS:lib%=$(DESTDIR)$(libdir)/lib%) $(ALL_INCLUDES:%=$(DESTDIR)$(includedir)/%) $(ALL_TOOLS:%=$(DESTDIR)$(bindir)/%)

clean:
	rm -f $(ALL_LIBS)
	rm -f $(OBJS)
	rm -f $(ALL_TOOLS)

%.o: %.c
	$(CC) $(BUILDCFLAGS) -c -o $@ $<

libintl.a: $(LIBOBJS)
	rm -f $@
	$(AR) rc $@ $(LIBOBJS)
	$(RANLIB) $@

msgfmt:
	$(CC) -O0 -g -static -o $@ src/$@.c
	

$(DESTDIR)$(libdir)/%.a: %.a
	install -D -m 755 $< $@

$(DESTDIR)$(includedir)/%.h: include/%.h
	install -D -m 644 $< $@

$(DESTDIR)$(bindir)/%: %
	install -D -m 755 $< $@

.PHONY: all clean install



