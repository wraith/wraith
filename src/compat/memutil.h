#ifndef _MEMUTIL_H
#define _MEMUTIL_H

/*
#undef malloc
#undef realloc
#undef calloc
void *malloc(size_t);
void *realloc(void *, size_t);
void *calloc(size_t, size_t);
*/

#ifdef str_redup
#undef str_redup
#endif /* str_redup */
#undef strdup

void str_redup(char **, const char *);
char *strdup(const char *);

#endif /* !_MEMUTIL_H */
