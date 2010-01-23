#ifndef _STRSEP_H
#define _STRSEP_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

//#undef strsep

#define strsep my_strsep

char *strsep(char **, const char *);

#endif /* !_STRSEP_H */

