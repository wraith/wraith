#ifndef _MEMUTIL_H
#define _MEMUTIL_H

#include <sys/types.h>

#undef str_redup
#undef strdup
#undef calloc
#undef realloc

void str_redup(char **, const char *);
char * strdup(const char *);

void *my_calloc(size_t, size_t);
void *my_realloc(void *, size_t);

#endif /* !_MEMUTIL_H */
