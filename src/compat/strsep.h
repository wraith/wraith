#ifndef _STRSEP_H
#define _STRSEP_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

//#undef strsep

#define strsep my_strsep

#ifdef __cplusplus
extern "C" {
#endif
char *strsep(char **, const char *);

#ifdef __cplusplus
}
#endif
#endif /* !_STRSEP_H */
