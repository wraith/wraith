#include "common.h"
#include <errno.h>
#include <string.h>
#include <sys/param.h>

char *
dirname(const char *path)
{
        static char bname[MAXPATHLEN] = "";
        register const char *endp = NULL;

        /* Empty or NULL string gets treated as "." */
        if (path == NULL || *path == '\0') {
                strlcpy(bname, ".", sizeof bname);
                return(bname);
        }

        /* Strip trailing slashes */
        endp = path + strlen(path) - 1;
        while (endp > path && *endp == '/')
                endp--;

        /* Find the start of the dir */
        while (endp > path && *endp != '/')
                endp--;

        /* Either the dir is "/" or there are no slashes */
        if (endp == path) {
                strlcpy(bname, *endp == '/' ? "/" : ".", sizeof bname);
                return(bname);
        } else {
                do {
                        endp--;
                } while (endp > path && *endp == '/');
        }

        if (endp - path + 2 > (signed) sizeof(bname)) {
                errno = ENAMETOOLONG;
                return(NULL);
        }
        strlcpy(bname, path, endp - path + 2);
        return(bname);
}

