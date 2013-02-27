#ifndef LIBINTL_H
#define LIBINTL_H

char *gettext(const char *msgid);
char *dgettext(const char *domainname, const char *msgid);
char *dcgettext(const char *domainname, const char *msgid, int category);
char *ngettext(const char *msgid1, const char *msgid2, unsigned long n);
char *dngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long n);
char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long n, int category);

char *textdomain(const char *domainname);
char *bind_textdomain_codeset(const char *domainname, const char *codeset);
char *bindtextdomain(const char *domainname, const char *dirname);

#define gettext_noop(X) (X)

#ifndef LIBINTL_NO_MACROS
/* if these macros are defined, configure checks will detect libintl as
 * built into the libc because test programs will work without -lintl.
 * for example: 
 * checking for ngettext in libc ... yes
 * the consequence is that -lintl will not be added to the LDFLAGS.
 * so if for some reason you want that libintl.a gets linked,
 * add -DLIBINTL_NO_MACROS=1 to your CPPFLAGS. */

#define gettext(X) ((char*) (X))
#define dgettext(dom, X) ((char*) (X))
#define dcgettext(dom, X, cat) ((char*) (X))
#define ngettext(X, Y, N) ((char*) ((N == 1) ? X : Y))
#define dngettext(dom, X, Y, N) ((char*) ((N == 1) ? X : Y))
#define dcngettext(dom, X, Y, N, cat) ((char*) ((N == 1) ? X : Y))
#define bindtextdomain(X, Y) ((char*) "/")
#define bind_textdomain_codeset(dom, codeset) ((char*) 0)
#define textdomain(X) ((char*) "messages")

#endif

#include <stdio.h>
#define gettext_printf(args...) printf(args)

/* to supply LC_MESSAGES and other stuff GNU expects to be exported when
   including libintl.h */
#include <locale.h>

#endif

