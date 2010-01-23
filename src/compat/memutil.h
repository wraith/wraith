#ifndef _MEMUTIL_H
#define _MEMUTIL_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>

//#undef str_redup
//#undef strdup
#undef calloc
#undef realloc

#define str_redup my_str_redup
#define strdup my_strdup
//#define calloc my_calloc
//#define realloc my_realloc

void str_redup(char **, const char *);
char *strdup(const char *);
char *strldup(const char *, size_t);


void *my_calloc(size_t, size_t);
void *my_realloc(void *, size_t);

#endif /* !_MEMUTIL_H */
