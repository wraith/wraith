#ifndef _GARBLE_H
#define _GARBLE_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define STR(x) x

#ifdef S_GARBLESTRINGS
char *degarble(int, char *);
#endif /* S_GARBLESTRINGS */


#endif /* !_GARBLE_H */
