#include <string.h>
#include "memutil.h"
#include <stdlib.h>


void 
str_redup(char **str, const char *newstr)
{
        size_t len;

        if (!newstr) {
                free(*str);
                *str = NULL;
                return;
        }
        len = strlen(newstr) + 1;
        *str = (char *) realloc(*str, len);
        memcpy(*str, newstr, len);
}

char *
strldup(const char *entry, size_t maxlen)
{
  size_t slen = strlen(entry);
  size_t len = slen < maxlen ? slen : maxlen;
  char *target = (char *) calloc(1, len + 1);
  if (target == NULL) return NULL;
  target[len] = 0;
  return (char *) memcpy(target, entry, len);
}

/* vim: set sts=2 sw=2 ts=8 et: */
