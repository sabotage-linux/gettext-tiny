#ifndef LIBINTL_H
#define LIBINTL_H

#define gettext(X) (X)
#define dgettext(dom, X) (X)
#define dcgettext(dom, X, cat) (X)
#define ngettext(X, Y, N) ((N == 1) ? X : Y)
#define dngettext(dom, X, Y, N) ((N == 1) ? X : Y)
#define dcngettext(dom, X, Y, N, cat) ((N == 1) ? X : Y)

#define gettext_noop(X) (X)

// FIXME these should probably return something
#define bindtextdomain(X, Y)
#define textdomain(X)
#define bind_textdomain_codeset(dom, codeset)

#include <stdio.h>
#define gettext_printf(args...) printf(args)

#endif

