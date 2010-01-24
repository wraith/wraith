#include <string.h>
#include "memcpy.h"
#include "memutil.h"
#include "src/main.h"
#include <stdlib.h>


void 
str_redup(char **str, const char *newstr)
{
        size_t len;

        if (!newstr) {
                if (*str) free(*str);
                *str = NULL;
                return;
        }
        len = strlen(newstr) + 1;
        *str = (char *) my_realloc(*str, len);
        memcpy(*str, newstr, len);
}

char *
strdup(const char *entry)
{
  size_t len = strlen(entry);
  char *target = (char *) my_calloc(1, len + 1);
  if (target == NULL) return NULL;
  target[len] = 0;
  return (char *) memcpy(target, entry, len);
}

char *
strldup(const char *entry, size_t maxlen)
{
  size_t slen = strlen(entry);
  size_t len = slen < maxlen ? slen : maxlen;
  char *target = (char *) my_calloc(1, len + 1);
  if (target == NULL) return NULL;
  target[len] = 0;
  return (char *) memcpy(target, entry, len);
}


void *my_calloc(size_t nmemb, size_t size)
{
  void *ptr = calloc(nmemb, size);

  if (ptr == NULL)
    exit(5);
  
  return ptr;
}

void *my_realloc(void *ptr, size_t size)
{
  void *x = realloc(ptr, size);

  if (x == NULL && size > 0)
    exit(5);

  return x;
}

