#ifndef _MEMUTIL_H
#define _MEMUTIL_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>

//#undef str_redup

#define str_redup my_str_redup

#ifdef __cplusplus
extern "C" {
#endif

void str_redup(char **, const char *);
char *strldup(const char *, size_t);

#ifdef __cplusplus
}
#endif

#endif /* !_MEMUTIL_H */
