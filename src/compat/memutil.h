#ifndef _MEMUTIL_H
#define _MEMUTIL_H

#undef malloc
#undef realloc
#undef calloc
#undef str_redup
#undef strdup

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t) __THROW __attribute_malloc__;
void *realloc(void *, size_t) __THROW __attribute_malloc__;
void *calloc(size_t, size_t) __THROW __attribute_malloc__;
void str_redup(char **, const char *);
char *strdup(const char *);

#ifdef __cplusplus
}
#endif

#endif /* !_MEMUTIL_H */
