#include <stdlib.h>
#include <string.h>
#include "memcpy.h"
#include "src/main.h"

/*
extern void * __real_malloc(size_t);
extern void * __real_realloc(void *, size_t);

void *
__wrap_malloc(size_t size)
{
  void *x = NULL;

  x = (void *) __real_malloc(size);
  if (x == NULL)
    fatal("Memory allocation failed", 0);

  return x;
}

void *
__wrap_realloc(void *ptr, size_t size)
{
  void *x = NULL;

  if (ptr == NULL)
    return malloc(size);

  x = (void *) __real_realloc(ptr, size);
  if (x == NULL && size > 0)
    fatal("Memory re-allocation failed", 0);

  return x;
}

void *
__wrap_calloc(size_t nmemb, size_t size)
{
  void *x = NULL;

  x = (void *) malloc(nmemb * size);
  if (x)
    egg_bzero(x, nmemb * size);

  return x;
}
*/

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
        *str = (char *) realloc(*str, len);
        egg_memcpy(*str, newstr, len);
}

char *
__wrap_strdup(const char *entry)
{
  char *target = (char*)calloc(1, strlen(entry) + 1);
  if (target == 0) return 0;
  strcpy(target, entry);
  return target;
}

