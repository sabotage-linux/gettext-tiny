prefix=/usr/local
bindir=$(prefix)/bin
includedir=$(prefix)/include
libdir=$(prefix)/lib
sysconfdir=$(prefix)/etc
datadir=$(prefix)/share/gettext-tiny

ifeq ($(LIBINTL), MUSL)
	LIBSRC = libintl/libintl-musl.c
	HEADERS =
else ifeq ($(LIBINTL), NONE)
	LIBSRC =
	HEADERS =
else
	LIBSRC = libintl/libintl.c
	HEADERS = libintl.h
endif
PROGSRC = $(sort $(wildcard src/*.c))

PARSEROBJS = src/poparser.o src/StringEscape.o
PROGOBJS = $(PROGSRC:.c=.o)
LIBOBJS = $(LIBSRC:.c=.o)
OBJS = $(PROGOBJS) $(LIBOBJS)

ALL_INCLUDES = $(HEADERS)
ifneq ($(LIBINTL), NONE)
ALL_LIBS=libintl.a
endif
ALL_TOOLS=msgfmt msgmerge xgettext autopoint
ALL_M4S=$(sort $(wildcard m4/*.m4))

CFLAGS=-O0 -fPIC

AR      ?= $(CROSS_COMPILE)ar
RANLIB  ?= $(CROSS_COMPILE)ranlib
CC      ?= $(CROSS_COMPILE)cc

-include config.mak

BUILDCFLAGS=$(CFLAGS)

all: $(ALL_LIBS) $(ALL_TOOLS)

install: $(ALL_LIBS:lib%=$(DESTDIR)$(libdir)/lib%) $(ALL_INCLUDES:%=$(DESTDIR)$(includedir)/%) $(ALL_TOOLS:%=$(DESTDIR)$(bindir)/%) $(ALL_M4S:%=$(DESTDIR)$(datadir)/%)

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

msgmerge: $(OBJS)
	$(CC) $(LDFLAGS) -static -o $@ src/msgmerge.o $(PARSEROBJS)

msgfmt: $(OBJS)
	$(CC) $(LDFLAGS) -static -o $@ src/msgfmt.o $(PARSEROBJS)

xgettext:
	cp src/xgettext.sh ./xgettext

autopoint: src/autopoint.in
	cat $< | sed 's,@datadir@,$(datadir),' > $@

$(DESTDIR)$(libdir)/%.a: %.a
	install -D -m 755 $< $@

$(DESTDIR)$(includedir)/%.h: include/%.h
	install -D -m 644 $< $@

$(DESTDIR)$(bindir)/%: %
	install -D -m 755 $< $@

$(DESTDIR)$(datadir)/%: %
	install -D -m 644 $< $@

.PHONY: all clean install



