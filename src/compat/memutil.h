#ifndef _MEMUTIL_H
#define _MEMUTIL_H

#undef malloc
#undef realloc
#undef calloc
#undef str_redup
#undef strdup

void *malloc(size_t);
void *realloc(void *, size_t);
void *calloc(size_t, size_t);
void str_redup(char **, const char *);
char *strdup(const char *);

#endif /* !_MEMUTIL_H */
