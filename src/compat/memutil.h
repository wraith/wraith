#ifndef _MEMUTIL_H
#define _MEMUTIL_H

#undef strdup
#undef str_redup

void str_redup(char **, const char *);
char *strdup(const char *);

#endif /* !_MEMUTIL_H */
