#ifndef LIBINTL_H
#define LIBINTL_H

#define gettext(X) (X)
#define gettext_noop(X) (X)
#define bindtextdomain(X, Y)
#define textdomain(X)

#include <stdio.h>
#define gettext_printf(args...) printf(args)

#endif

