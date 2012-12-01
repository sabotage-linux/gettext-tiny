#ifndef LIBINTL_H
#define LIBINTL_H

#define gettext(X) (X)
#define dgettext(dom, X) (X)
#define dcgettext(dom, X, cat) (X)
#define ngettext(X, Y, N) ((N == 1) ? X : Y)
#define dngettext(dom, X, Y, N) ((N == 1) ? X : Y)
#define dcngettext(dom, X, Y, N, cat) ((N == 1) ? X : Y)

#define gettext_noop(X) (X)

#if 0
// FIXME these should return something
#define bindtextdomain(X, Y)
#define textdomain(X)
#define bind_textdomain_codeset(dom, codeset)
#else
char *textdomain(const char *domainname);
char *bind_textdomain_codeset(const char *domainname, const char *codeset);
char *bindtextdomain(const char *domainname, const char *dirname);
#endif

#include <stdio.h>
#define gettext_printf(args...) printf(args)

/* to supply LC_MESSAGES and other stuff GNU expects to be exported when
   including libintl.h */
#include <locale.h>

#endif

