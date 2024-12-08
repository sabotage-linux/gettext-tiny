prefix=/usr/local
bindir=$(prefix)/bin
includedir=$(prefix)/include
libdir=$(prefix)/lib
sysconfdir=$(prefix)/etc
datarootdir=$(prefix)/share
datadir=$(datarootdir)/gettext-tiny
acdir=$(datarootdir)/aclocal

ifeq ($(LIBINTL), MUSL)
	LIBSRC = libintl/libintl-musl.c
	HEADERS =
else ifeq ($(LIBINTL), NONE)
	LIBSRC =
	HEADERS =
else
	LIBSRC = libintl/libintl.c
	HEADERS = include/libintl.h
endif

PARSEROBJS = src/poparser.o src/StringEscape.o
PROGOBJS = src/msgmerge.o src/msgfmt.o
LIBOBJS = $(LIBSRC:.c=.o)
OBJS = $(PARSEROBJS) $(PROGOBJS) $(LIBOBJS)

ALL_INCLUDES = $(HEADERS)
ifneq ($(LIBINTL), NONE)
ALL_LIBS=libintl.a
endif
ALL_TOOLS=msgfmt msgmerge xgettext autopoint

CFLAGS  ?= -O0 -fPIC

AR      ?= $(CROSS_COMPILE)ar
RANLIB  ?= $(CROSS_COMPILE)ranlib
CC      ?= $(CROSS_COMPILE)cc

INSTALL ?= ./install.sh

-include config.mak

BUILDCFLAGS=$(CFLAGS)

all: $(ALL_LIBS) $(ALL_TOOLS)

clean:
	rm -f $(ALL_LIBS)
	rm -f $(OBJS)
	rm -f $(ALL_TOOLS)
	rm -f ldlibs

.c.o:
	$(CC) $(BUILDCFLAGS) -c -o $@ $<

libintl.a: $(LIBOBJS)
	rm -f $@
	$(AR) rc $@ $(LIBOBJS)
	$(RANLIB) $@

ldlibs:
	echo "int main(){}" | $(CC) $(CFLAGS) $(LDFLAGS) -liconv -x c - >/dev/null 2>&1 && printf %s -liconv || true > ldlibs

msgmerge: src/msgmerge.o $(PARSEROBJS) ldlibs
	$(CC) -o $@ src/msgmerge.o $(PARSEROBJS) $(LDFLAGS) `cat ldlibs`

msgfmt: src/msgfmt.o $(PARSEROBJS) ldlibs
	$(CC) -o $@ src/msgfmt.o $(PARSEROBJS) $(LDFLAGS) `cat ldlibs`

xgettext:
	cp src/xgettext.sh ./xgettext

autopoint: src/autopoint.in
	cat $< | sed 's,@datadir@,$(datadir),' > $@

install: $(ALL_LIBS) $(ALL_INCLUDES) $(ALL_TOOLS)
	$(INSTALL) -D -m 755 $(ALL_LIBS) $(DESTDIR)$(libdir)/
	$(INSTALL) -D -m 644 $(ALL_INCLUDES) $(DESTDIR)$(includedir)/
	$(INSTALL) -D -m 755 $(ALL_TOOLS) $(DESTDIR)$(bindir)/
	$(INSTALL) -D -m 644 m4/*.m4 $(DESTDIR)$(datadir)/m4/
	$(INSTALL) -D -m 644 data/* $(DESTDIR)$(datadir)/data/
	for i in m4/*.m4 ; do $(INSTALL) -D -l $(datadir)/$$i $(DESTDIR)$(acdir)/$${i#m4/}; done

.PHONY: all clean install
