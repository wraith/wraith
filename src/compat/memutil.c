#include <stdlib.h>
#include <string.h>
#include "memcpy.h"

void str_redup(char **str, const char *newstr)
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

