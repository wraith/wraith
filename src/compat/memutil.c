#include <stdlib.h>
#include <string.h>
#include "memcpy.h"
#include "src/main.h"

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
strdup(const char *entry)
{
  size_t len = strlen(entry) + 1;
  char *target = (char *) calloc(1, len);
  if (target == NULL) return NULL;
  return (char *) egg_memcpy(target, entry, len);
}

