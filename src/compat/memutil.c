#include <stdlib.h>
#include <string.h>
#include "memcpy.h"

void 
__wrap_str_redup(char **str, const char *newstr)
{
        int len;

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

