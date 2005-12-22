#ifndef _STRLCPY_H
#define _STRLCPY_H

#include <sys/types.h>

//#undef strlcpy
//#undef strlcat

#define strlcpy my_strlcpy
#define strlcat my_strlcat

size_t strlcpy(char *, const char *, size_t);
size_t strlcat(char *, const char *, size_t);

#endif /* !_STRLCPY_H */

